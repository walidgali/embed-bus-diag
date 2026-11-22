#define _POSIX_C_SOURCE 200809L

#include "diag_engine.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    const char *id;
    double scale;
    double offset;
    double cal_a;
    double cal_b;
    double cal_c;
} datasheet_cal_t;

static const datasheet_cal_t g_cal_table[] = {
    { "none",           0.0,        0.0,   0.0, 0.0, 0.0 },
    { "tmp102_0p0625",  0.0625,     0.0,   0.0, 0.0, 0.0 },
    { "tmp117_0p0078125",0.0078125,  0.0,   0.0, 0.0, 0.0 },
    { "sht3x_temp_c",   0.00267032, -45.0, 0.0, 0.0, 0.0 },
    { "sht3x_rh_pct",   0.00152590,  0.0,  0.0, 0.0, 0.0 },
    { "ina219_shunt_mv",0.01,       0.0,   0.0, 0.0, 0.0 },
    { "ads1115_4v096",  0.000125,   0.0,   0.0, 0.0, 0.0 }
};

static double now_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec / 1e3;
}

static void sleep_ms(uint32_t ms) {
    if (ms == 0) {
        return;
    }
    struct timespec req = {
        .tv_sec = ms / 1000u,
        .tv_nsec = (long)(ms % 1000u) * 1000000L
    };
    nanosleep(&req, NULL);
}

static void trim_line(char *s) {
    char *end = s + strlen(s);
    while (end > s && (end[-1] == '\n' || end[-1] == '\r' || isspace((unsigned char)end[-1]))) {
        *--end = '\0';
    }
}

uint8_t diag_crc8_sht3x(const uint8_t *data, size_t len) {
    uint8_t crc = 0xFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            if (crc & 0x80u) {
                crc = (uint8_t)((crc << 1) ^ 0x31u);
            } else {
                crc = (uint8_t)(crc << 1);
            }
        }
    }
    return crc;
}

static void apply_cal_profile(sensor_profile_t *prof) {
    if (prof->cal_profile[0] == '\0' || strcmp(prof->cal_profile, "none") == 0) {
        return;
    }
    size_t n = sizeof(g_cal_table) / sizeof(g_cal_table[0]);
    for (size_t i = 0; i < n; i++) {
        if (strcmp(prof->cal_profile, g_cal_table[i].id) == 0) {
            prof->scale = g_cal_table[i].scale;
            prof->offset = g_cal_table[i].offset;
            prof->cal_a = g_cal_table[i].cal_a;
            prof->cal_b = g_cal_table[i].cal_b;
            prof->cal_c = g_cal_table[i].cal_c;
            return;
        }
    }
}

int profile_load_ini(const char *path, sensor_profile_t *out) {
    if (!path || !out) {
        return -EINVAL;
    }
    memset(out, 0, sizeof(*out));
    strncpy(out->cal_profile, "none", sizeof(out->cal_profile) - 1);
    out->scale = 1.0;

    FILE *fp = fopen(path, "r");
    if (!fp) {
        return -errno;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        trim_line(line);
        if (line[0] == '\0' || line[0] == '#' || line[0] == ';') {
            continue;
        }
        char *eq = strchr(line, '=');
        if (!eq) {
            continue;
        }
        *eq = '\0';
        const char *key = line;
        const char *val = eq + 1;

        if (strcmp(key, "name") == 0) {
            strncpy(out->name, val, sizeof(out->name) - 1);
        } else if (strcmp(key, "unit") == 0) {
            strncpy(out->unit, val, sizeof(out->unit) - 1);
        } else if (strcmp(key, "cal_profile") == 0) {
            strncpy(out->cal_profile, val, sizeof(out->cal_profile) - 1);
        } else if (strcmp(key, "addr") == 0) {
            out->addr = (uint8_t)strtoul(val, NULL, 0);
        } else if (strcmp(key, "reg") == 0) {
            out->reg = (uint8_t)strtoul(val, NULL, 0);
        } else if (strcmp(key, "len") == 0) {
            out->len = (uint8_t)strtoul(val, NULL, 0);
        } else if (strcmp(key, "shift") == 0) {
            out->shift = (uint8_t)strtoul(val, NULL, 0);
        } else if (strcmp(key, "mask") == 0) {
            out->mask = (uint32_t)strtoul(val, NULL, 0);
        } else if (strcmp(key, "signed") == 0) {
            out->is_signed = (strtol(val, NULL, 0) != 0);
        } else if (strcmp(key, "endian") == 0) {
            out->little_endian = (strcmp(val, "le") == 0 || strcmp(val, "little") == 0);
        } else if (strcmp(key, "crc8") == 0) {
            out->verify_crc8 = (strtol(val, NULL, 0) != 0);
        } else if (strcmp(key, "scale") == 0) {
            out->scale = strtod(val, NULL);
        } else if (strcmp(key, "offset") == 0) {
            out->offset = strtod(val, NULL);
        } else if (strcmp(key, "cal_a") == 0) {
            out->cal_a = strtod(val, NULL);
        } else if (strcmp(key, "cal_b") == 0) {
            out->cal_b = strtod(val, NULL);
        } else if (strcmp(key, "cal_c") == 0) {
            out->cal_c = strtod(val, NULL);
        }
    }
    fclose(fp);

    apply_cal_profile(out);

    if (out->name[0] == '\0' || out->unit[0] == '\0') {
        return -EINVAL;
    }
    if (out->len == 0 || out->len > 4 || out->mask == 0 || out->scale == 0.0) {
        return -EINVAL;
    }
    if (out->verify_crc8 && out->len < 2) {
        return -EINVAL;
    }
    return 0;
}

double diag_decode_raw(const sensor_profile_t *prof, const uint8_t *buf) {
    size_t data_len = prof->verify_crc8 ? (size_t)(prof->len - 1u) : (size_t)prof->len;
    uint32_t raw = 0;

    if (prof->little_endian) {
        for (int i = (int)data_len - 1; i >= 0; i--) {
            raw = (raw << 8) | buf[i];
        }
    } else {
        for (size_t i = 0; i < data_len; i++) {
            raw = (raw << 8) | buf[i];
        }
    }

    raw = (raw >> prof->shift) & prof->mask;

    double base = 0.0;
    if (prof->is_signed) {
        uint32_t sign_bit = (prof->mask + 1u) >> 1;
        if (sign_bit > 0 && (raw & sign_bit)) {
            int32_t sraw = (int32_t)(raw | ~prof->mask);
            base = (double)sraw * prof->scale + prof->offset;
        } else {
            base = (double)raw * prof->scale + prof->offset;
        }
    } else {
        base = (double)raw * prof->scale + prof->offset;
    }

    return base + prof->cal_a * (base * base) + prof->cal_b * base + prof->cal_c;
}

int diag_run_series(hal_bus_t *bus, const sensor_profile_t *prof, size_t count, uint32_t interval_ms, diag_metrics_t *out) {
    if (!bus || !prof || count == 0 || count > 1024 || !out) {
        return -EINVAL;
    }
    memset(out, 0, sizeof(*out));

    double values[1024];
    double sum = 0.0;
    double lat_sum = 0.0;
    const double alpha = 0.35;

    for (size_t i = 0; i < count; i++) {
        uint8_t rx[4] = {0};
        double t0 = now_us();
        int rc = hal_read_reg(bus, prof->addr, prof->reg, rx, prof->len);
        double dt = now_us() - t0;

        if (rc != 0) {
            out->errors++;
            if (i + 1 < count) {
                sleep_ms(interval_ms);
            }
            continue;
        }

        if (prof->verify_crc8) {
            size_t payload_len = (size_t)(prof->len - 1u);
            uint8_t expected_crc = diag_crc8_sht3x(rx, payload_len);
            if (rx[payload_len] != expected_crc) {
                out->errors++;
                if (i + 1 < count) {
                    sleep_ms(interval_ms);
                }
                continue;
            }
        }

        double val = diag_decode_raw(prof, rx);
        size_t idx = out->samples;
        values[idx] = val;
        out->samples++;

        if (idx == 0) {
            out->min_val = val;
            out->max_val = val;
            out->ema_val = val;
            out->max_latency_us = dt;
        } else {
            if (val < out->min_val) out->min_val = val;
            if (val > out->max_val) out->max_val = val;
            out->ema_val = alpha * val + (1.0 - alpha) * out->ema_val;
            if (dt > out->max_latency_us) out->max_latency_us = dt;
        }

        sum += val;
        lat_sum += dt;

        if (i + 1 < count) {
            sleep_ms(interval_ms);
        }
    }

    if (out->samples == 0) {
        return -EIO;
    }

    out->avg_val = sum / (double)out->samples;
    out->drift_val = values[out->samples - 1] - values[0];
    out->avg_latency_us = lat_sum / (double)out->samples;

    double var_sum = 0.0;
    double x_mean = (double)(out->samples - 1) / 2.0;
    double num = 0.0;
    double den = 0.0;

    for (size_t i = 0; i < out->samples; i++) {
        double diff = values[i] - out->avg_val;
        var_sum += diff * diff;
        double dx = (double)i - x_mean;
        num += dx * diff;
        den += dx * dx;
    }

    double stddev = sqrt(var_sum / (double)out->samples);
    if (stddev > 1e-9) {
        for (size_t i = 0; i < out->samples; i++) {
            double z = fabs(values[i] - out->avg_val) / stddev;
            if (z > 2.5) {
                out->outliers++;
            }
        }
    }
    out->trend_slope = (den > 0.0) ? (num / den) : 0.0;
    return 0;
}

int diag_scan_bus(hal_bus_t *bus, uint8_t *found_addrs, size_t max_found, size_t *out_count) {
    if (!bus || !found_addrs || !out_count) {
        return -EINVAL;
    }
    *out_count = 0;
    for (uint8_t addr = 0x08u; addr <= 0x77u; addr++) {
        if (hal_probe_addr(bus, addr) == 0) {
            if (*out_count < max_found) {
                found_addrs[*out_count] = addr;
            }
            (*out_count)++;
        }
    }
    return 0;
}

int diag_export_json(const char *path, const char *backend, const sensor_profile_t *prof, const diag_metrics_t *m) {
    if (!path || !backend || !prof || !m) {
        return -EINVAL;
    }
    FILE *fp = fopen(path, "w");
    if (!fp) {
        return -errno;
    }
    fprintf(fp,
            "{\n"
            "  \"tool\": \"embed-bus-diag\",\n"
            "  \"backend\": \"%s\",\n"
            "  \"sensor\": \"%s\",\n"
            "  \"cal_profile\": \"%s\",\n"
            "  \"addr\": \"0x%02X\",\n"
            "  \"unit\": \"%s\",\n"
            "  \"samples\": %zu,\n"
            "  \"errors\": %zu,\n"
            "  \"outliers\": %zu,\n"
            "  \"min\": %.4f,\n"
            "  \"max\": %.4f,\n"
            "  \"avg\": %.4f,\n"
            "  \"ema\": %.4f,\n"
            "  \"drift\": %.4f,\n"
            "  \"trend_slope\": %.6f,\n"
            "  \"avg_latency_us\": %.2f,\n"
            "  \"max_latency_us\": %.2f\n"
            "}\n",
            backend, prof->name, prof->cal_profile, prof->addr, prof->unit,
            m->samples, m->errors, m->outliers,
            m->min_val, m->max_val, m->avg_val, m->ema_val,
            m->drift_val, m->trend_slope, m->avg_latency_us, m->max_latency_us);
    fclose(fp);
    return 0;
}

int diag_stress_bus(hal_bus_t *bus, const sensor_profile_t *prof, size_t attempts, stress_metrics_t *out) {
    if (!bus || !prof || attempts == 0 || !out) {
        return -EINVAL;
    }
    memset(out, 0, sizeof(*out));
    out->attempts = attempts;

    double lat_sum = 0.0;
    for (size_t i = 0; i < attempts; i++) {
        uint8_t rx[4] = {0};
        double t0 = now_us();
        int rc = hal_read_reg(bus, prof->addr, prof->reg, rx, prof->len);
        double dt = now_us() - t0;

        if (rc == 0 && prof->verify_crc8) {
            size_t payload_len = (size_t)(prof->len - 1u);
            if (rx[payload_len] != diag_crc8_sht3x(rx, payload_len)) {
                rc = -EIO;
            }
        }

        if (rc == 0) {
            out->ok++;
        } else {
            out->failed++;
        }

        if (i == 0 || dt > out->max_latency_us) {
            out->max_latency_us = dt;
        }
        lat_sum += dt;
    }

    out->avg_latency_us = lat_sum / (double)attempts;
    out->success_pct = ((double)out->ok / (double)attempts) * 100.0;
    return 0;
}

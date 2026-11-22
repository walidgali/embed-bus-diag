#include "diag_engine.h"
#include "hal.h"

#include <dirent.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s [options]\n"
            "Options:\n"
            "  --backend <mock|i2c|spi> Select HAL backend (default: mock)\n"
            "  --dev <path>             Device path (default: /dev/i2c-1)\n"
            "  --profile <file.ini>     Sensor INI profile (default: profiles/tmp102.ini)\n"
            "  --list-profiles          List available .ini profiles in profiles/\n"
            "  --scan                   Scan bus addresses (0x08..0x77)\n"
            "  --stress <count>         Run rapid bus stress test with N reads\n"
            "  --min-val <num>          Alert threshold lower bound (exit code 3)\n"
            "  --max-val <num>          Alert threshold upper bound (exit code 3)\n"
            "  --strict                 Fail (exit code 4) if errors > 0 or outliers > 0\n"
            "  -n <count>               Sample count (default: 5)\n"
            "  -i <ms>                  Interval in ms (default: 20)\n"
            "  -o <report.json>         Output JSON file (default: report.json)\n"
            "  -h, --help               Show this help\n",
            prog);
}

static int list_profiles_dir(const char *dir_path) {
    DIR *d = opendir(dir_path);
    if (!d) {
        fprintf(stderr, "Error: cannot open profiles directory '%s'\n", dir_path);
        return 1;
    }
    struct dirent *ent;
    size_t total = 0;
    while ((ent = readdir(d)) != NULL) {
        const char *n = ent->d_name;
        size_t len = strlen(n);
        if (len > 4 && strcmp(n + len - 4, ".ini") == 0) {
            printf("  %s/%s\n", dir_path, n);
            total++;
        }
    }
    closedir(d);
    printf("TOTAL_PROFILES=%zu\n", total);
    return 0;
}

int main(int argc, char **argv) {
    const char *backend = "mock";
    const char *dev_path = "/dev/i2c-1";
    const char *profile_path = "profiles/tmp102.ini";
    const char *json_path = "report.json";
    size_t count = 5;
    size_t stress_count = 0;
    uint32_t interval_ms = 20;
    bool scan_only = false;
    bool list_only = false;
    bool strict_mode = false;
    bool has_min = false;
    bool has_max = false;
    double min_bound = 0.0;
    double max_bound = 0.0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--list-profiles") == 0) {
            list_only = true;
        } else if (strcmp(argv[i], "--backend") == 0 && i + 1 < argc) {
            backend = argv[++i];
        } else if (strcmp(argv[i], "--dev") == 0 && i + 1 < argc) {
            dev_path = argv[++i];
        } else if (strcmp(argv[i], "--profile") == 0 && i + 1 < argc) {
            profile_path = argv[++i];
        } else if (strcmp(argv[i], "--scan") == 0) {
            scan_only = true;
        } else if (strcmp(argv[i], "--stress") == 0 && i + 1 < argc) {
            stress_count = (size_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--min-val") == 0 && i + 1 < argc) {
            min_bound = strtod(argv[++i], NULL);
            has_min = true;
        } else if (strcmp(argv[i], "--max-val") == 0 && i + 1 < argc) {
            max_bound = strtod(argv[++i], NULL);
            has_max = true;
        } else if (strcmp(argv[i], "--strict") == 0) {
            strict_mode = true;
        } else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            count = (size_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
            interval_ms = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            json_path = argv[++i];
        } else {
            fprintf(stderr, "Unknown or incomplete option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 2;
        }
    }

    if (list_only) {
        return list_profiles_dir("profiles");
    }

    sensor_profile_t prof;
    if (profile_load_ini(profile_path, &prof) != 0) {
        fprintf(stderr, "Error: failed to load profile: %s\n", profile_path);
        return 1;
    }

    const hal_ops_t *ops = NULL;
    if (strcmp(backend, "mock") == 0) {
        ops = &g_hal_mock_ops;
    } else if (strcmp(backend, "i2c") == 0) {
        ops = &g_hal_i2c_ops;
    } else if (strcmp(backend, "spi") == 0) {
        ops = &g_hal_spi_ops;
    } else {
        fprintf(stderr, "Error: unsupported backend '%s'\n", backend);
        return 2;
    }

    hal_bus_t bus;
    int rc = hal_open(&bus, ops, dev_path, prof.addr);
    if (rc != 0) {
        fprintf(stderr, "Error: hal_open(%s) failed: %d\n", backend, rc);
        return 1;
    }

    if (scan_only) {
        uint8_t addrs[128];
        size_t found = 0;
        diag_scan_bus(&bus, addrs, 128, &found);
        printf("SCAN_RESULT backend=%s found=%zu:", ops->backend_name, found);
        for (size_t i = 0; i < found; i++) {
            printf(" 0x%02X", addrs[i]);
        }
        printf("\n");
        hal_close(&bus);
        return 0;
    }

    if (stress_count > 0) {
        stress_metrics_t sm;
        rc = diag_stress_bus(&bus, &prof, stress_count, &sm);
        hal_close(&bus);
        if (rc != 0) {
            fprintf(stderr, "Error: diag_stress_bus failed: %d\n", rc);
            return 1;
        }
        printf("STRESS_RESULT sensor=%s attempts=%zu ok=%zu failed=%zu success=%.2f%% avg_lat=%.2fus max_lat=%.2fus\n",
               prof.name, sm.attempts, sm.ok, sm.failed, sm.success_pct, sm.avg_latency_us, sm.max_latency_us);
        if (strict_mode && sm.failed > 0) {
            fprintf(stderr, "STRICT_FAIL: stress test recorded %zu failed reads\n", sm.failed);
            return 4;
        }
        return 0;
    }

    diag_metrics_t metrics;
    rc = diag_run_series(&bus, &prof, count, interval_ms, &metrics);
    hal_close(&bus);

    if (rc != 0) {
        fprintf(stderr, "Error: diag_run_series failed: %d\n", rc);
        return 1;
    }

    if (diag_export_json(json_path, ops->backend_name, &prof, &metrics) != 0) {
        fprintf(stderr, "Error: failed to write JSON report: %s\n", json_path);
        return 1;
    }

    printf("Sensor=%s Addr=0x%02X Backend=%s | avg=%.4f %s ema=%.4f drift=%.4f slope=%.6f outliers=%zu lat=%.1fus\n",
           prof.name, prof.addr, ops->backend_name,
           metrics.avg_val, prof.unit, metrics.ema_val,
           metrics.drift_val, metrics.trend_slope, metrics.outliers, metrics.avg_latency_us);

    if ((has_min && metrics.avg_val < min_bound) || (has_max && metrics.avg_val > max_bound)) {
        fprintf(stderr, "ALERT: avg=%.4f %s out of bounds [%.4f .. %.4f]\n",
                metrics.avg_val, prof.unit,
                has_min ? min_bound : -INFINITY,
                has_max ? max_bound : INFINITY);
        return 3;
    }

    if (strict_mode && (metrics.errors > 0 || metrics.outliers > 0)) {
        fprintf(stderr, "STRICT_FAIL: errors=%zu outliers=%zu\n", metrics.errors, metrics.outliers);
        return 4;
    }

    return 0;
}

#ifndef EMBED_DIAG_ENGINE_H
#define EMBED_DIAG_ENGINE_H

#include "hal.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    char name[32];
    char unit[16];
    char cal_profile[32];
    uint8_t addr;
    uint8_t reg;
    uint8_t len;
    uint8_t shift;
    uint32_t mask;
    bool is_signed;
    bool little_endian;
    bool verify_crc8;
    double scale;
    double offset;
    double cal_a;
    double cal_b;
    double cal_c;
} sensor_profile_t;

typedef struct {
    size_t samples;
    size_t errors;
    size_t outliers;
    double min_val;
    double max_val;
    double avg_val;
    double drift_val;
    double ema_val;
    double trend_slope;
    double avg_latency_us;
    double max_latency_us;
} diag_metrics_t;

typedef struct {
    size_t attempts;
    size_t ok;
    size_t failed;
    double success_pct;
    double avg_latency_us;
    double max_latency_us;
} stress_metrics_t;

int profile_load_ini(const char *path, sensor_profile_t *out);
double diag_decode_raw(const sensor_profile_t *prof, const uint8_t *buf);
uint8_t diag_crc8_sht3x(const uint8_t *data, size_t len);
int diag_run_series(hal_bus_t *bus, const sensor_profile_t *prof, size_t count, uint32_t interval_ms, diag_metrics_t *out);
int diag_stress_bus(hal_bus_t *bus, const sensor_profile_t *prof, size_t attempts, stress_metrics_t *out);
int diag_scan_bus(hal_bus_t *bus, uint8_t *found_addrs, size_t max_found, size_t *out_count);
int diag_export_json(const char *path, const char *backend, const sensor_profile_t *prof, const diag_metrics_t *m);

#endif /* EMBED_DIAG_ENGINE_H */

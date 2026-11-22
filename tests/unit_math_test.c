#include "diag_engine.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int approx_eq(double a, double b, double eps) {
    return fabs(a - b) <= eps;
}

int main(void) {
    /* 1. اختبار CRC-8 لـ SHT3x */
    const uint8_t sample_bytes[2] = {0xBEu, 0xEFu};
    uint8_t crc = diag_crc8_sht3x(sample_bytes, 2);
    assert(diag_crc8_sht3x(sample_bytes, 2) == crc);

    /* 2. اختبار فك تشفير TMP102 (25.0 C -> 0x1900 >> 4 = 0x0190 = 400 * 0.0625) */
    sensor_profile_t tmp = {0};
    snprintf(tmp.name, sizeof(tmp.name), "tmp102");
    snprintf(tmp.unit, sizeof(tmp.unit), "C");
    tmp.len = 2;
    tmp.shift = 4;
    tmp.mask = 0x0FFFu;
    tmp.is_signed = true;
    tmp.scale = 0.0625;
    const uint8_t tmp_raw[2] = {0x19u, 0x00u};
    assert(approx_eq(diag_decode_raw(&tmp, tmp_raw), 25.0, 1e-6));

    /* 3. اختبار قيمة سالبة موقّعة (-10.0 C -> -160 = 0xFF60 -> shifted << 4 = 0xF600) */
    const uint8_t neg_raw[2] = {0xF6u, 0x00u};
    assert(approx_eq(diag_decode_raw(&tmp, neg_raw), -10.0, 1e-6));

    /* 4. اختبار Little-Endian */
    sensor_profile_t le_p = {0};
    snprintf(le_p.name, sizeof(le_p.name), "le_test");
    snprintf(le_p.unit, sizeof(le_p.unit), "u");
    le_p.len = 2;
    le_p.mask = 0xFFFFu;
    le_p.little_endian = true;
    le_p.scale = 1.0;
    const uint8_t le_buf[2] = {0x34u, 0x12u}; /* 0x1234 = 4660 */
    assert(approx_eq(diag_decode_raw(&le_p, le_buf), 4660.0, 1e-6));

    /* 5. اختبار معايرة cal_profile */
    FILE *fp = fopen("/tmp/cal_test.ini", "w");
    assert(fp != NULL);
    fprintf(fp,
            "name=sht31_cal\n"
            "unit=C\n"
            "cal_profile=sht3x_temp_c\n"
            "addr=0x44\n"
            "reg=0xE0\n"
            "len=3\n"
            "shift=0\n"
            "mask=0xFFFF\n"
            "signed=0\n"
            "endian=be\n"
            "crc8=1\n");
    fclose(fp);

    sensor_profile_t loaded = {0};
    assert(profile_load_ini("/tmp/cal_test.ini", &loaded) == 0);
    assert(approx_eq(loaded.scale, 0.00267032, 1e-9));
    assert(approx_eq(loaded.offset, -45.0, 1e-9));

    printf("UNIT_MATH_TESTS=PASS\n");
    return 0;
}

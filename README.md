# embed-bus-diag

[![Tests](https://github.com/walidgali/embed-bus-diag/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/walidgali/embed-bus-diag/actions/workflows/ci.yml)


Modular C11 embedded bus diagnostic CLI tool featuring:
- Multi-transport HAL (`mock`, `linux-i2c`, `linux-spi`)
- 42 sensor INI profiles (`profiles/*.ini`) with strict validation, datasheet calibration (`cal_profile`), and `CRC-8` verification
- Statistical & bus diagnostics: `min`, `max`, `avg`, `EMA`, `drift`, linear `trend_slope`, `Z-score` outliers, latency (`us`), `--scan`, `--stress`, threshold alerts (exit code `3`), strict production mode (`--strict`, exit code `4`), and `JSON` report export
- Unit math tests (`tests/unit_math_test.c`) and automated QA suite (`tests/run_qa.sh`)

## Build & Test
```bash
make test


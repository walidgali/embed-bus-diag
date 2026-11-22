#!/usr/bin/env bash
set -euo pipefail

# 1. مسح الناقل
SCAN_OUT=$(./embed-bus-diag --backend mock --scan)
echo "$SCAN_OUT" | grep -q "SCAN_RESULT backend=mock found=1: 0x48"

# 2. قائمة التعريفات (40+ حساس)
COUNT=$(ls -1 profiles/*.ini | wc -l)
[ "$COUNT" -ge 40 ]
./embed-bus-diag --list-profiles | grep -q "TOTAL_PROFILES=$COUNT"

# 3. اختبار الإجهاد مع --strict
./embed-bus-diag --backend mock --profile profiles/tmp102.ini --stress 20 --strict | grep -q "STRESS_RESULT"

# 4. قراءة وتصدير JSON في وضع --strict
./embed-bus-diag --backend mock --profile profiles/tmp102.ini -n 4 -i 0 --strict -o /tmp/qa_tmp102.json
grep -q '"sensor": "tmp102"' /tmp/qa_tmp102.json
grep -q '"errors": 0' /tmp/qa_tmp102.json

# 5. تنبيه تجاوز الحد الأقصى (exit code 3)
set +e
./embed-bus-diag --backend mock --profile profiles/tmp102.ini -n 3 -i 0 --max-val 20.0 >/dev/null 2>&1
ALERT_RC=$?
set -e
[ "$ALERT_RC" -eq 3 ]

# 6. رفض خيار خاطئ (exit code 2)
set +e
./embed-bus-diag --invalid-opt >/dev/null 2>&1
BAD_RC=$?
set -e
[ "$BAD_RC" -eq 2 ]

rm -f /tmp/qa_tmp102.json
echo "QA_SUITE=PASS (profiles=$COUNT, backends=mock|i2c|spi, strict=enabled)"

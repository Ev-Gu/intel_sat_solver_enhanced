#!/usr/bin/env bash
# Wrapper for MaxSAT-Fuzzer compare.py:
# - Intel Topor expects <instance> before -M 1 (compare.py appends instance last)
# - compare.py expects v-line as compact 0/1 string (no spaces), not "v 1 0 1 ..."
set -u

TOPOR="/mnt/c/Users/gal13/Documents/INtelMaxSat/intel_sat_solver_enhanced/topor_release"
INSTANCE="$1"

output="$("$TOPOR" "$INSTANCE" -M 1 2>&1)"
ret=$?

while IFS= read -r line; do
  if [[ "$line" == v\ * ]]; then
    # "v 1 0 1 ..." -> "v 101..." (no spaces between assignment bits)
  compact="${line#v }"
  compact="${compact// /}"
  echo "v ${compact}"
  else
    echo "$line"
  fi
done <<< "$output"

exit "$ret"

#!/usr/bin/env bash
# Wrapper for MaxSAT-Fuzzer compare.py (non-incremental .wcnf fuzzer):
# - Runs IntelTopor on a WCNF instance and normalizes output for compare.py
# - Intel Topor expects <instance> before -M 1 (compare.py appends instance last)
# - compare.py expects v-line as compact 0/1 string (no spaces), not "v 1 0 1 ..."
#
# Build the solver first (static release, from repo root):
#   make rs EXEC=intel_sat_solver_enhanced
# This produces intel_sat_solver_enhanced_static (not _release from "make r").
set -u

# Resolve the repo root from this script's location (Scripts/ -> repo root is ../../..),
# so this works on any machine. Override with TOPOR_BIN if the binary lives elsewhere.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
TOPOR="${TOPOR_BIN:-$REPO_ROOT/intel_sat_solver_enhanced_static}"
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

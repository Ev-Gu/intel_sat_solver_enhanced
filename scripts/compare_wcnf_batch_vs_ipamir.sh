#!/usr/bin/env bash
# Compare batch MaxSAT (-M 1) vs IPAMIR on the same WCNF file(s).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BATCH="$ROOT/third_party/MaxSAT-Fuzzer/Scripts/intel_topor_maxsat.sh"
IPAMIR="$ROOT/tools/bin/ipamir_wcnf_main"

usage() {
  echo "Usage: $0 <file.wcnf> [more.wcnf ...]"
  echo "Compares batch (intel_sat_solver_enhanced_static -M 1) vs IPAMIR on each file."
  exit 1
}

[ $# -ge 1 ] || usage

if [ ! -x "$IPAMIR" ]; then
  echo "Build IPAMIR tool first: bash tools/build_ipamir_wcnf.sh" >&2
  exit 1
fi

if [ ! -x "$ROOT/intel_sat_solver_enhanced_static" ]; then
  echo "Build solver first: make rs EXEC=intel_sat_solver_enhanced" >&2
  exit 1
fi

extract_so() {
  grep -E '^(s |o )' | sed 's/^v .*//' | grep -E '^(s |o )' | sort
}

failed=0
for wcnf in "$@"; do
  if [ ! -f "$wcnf" ]; then
    echo "ERROR: not a file: $wcnf" >&2
    failed=1
    continue
  fi

  echo "=== $wcnf ==="
  batch_out="$(bash "$BATCH" "$wcnf" 2>&1 || true)"
  ipamir_out="$("$IPAMIR" "$wcnf" 2>&1 || true)"

  batch_key="$(printf '%s\n' "$batch_out" | extract_so)"
  ipamir_key="$(printf '%s\n' "$ipamir_out" | extract_so)"

  if [ "$batch_key" = "$ipamir_key" ]; then
    echo "OK — batch and IPAMIR agree"
    printf '%s\n' "$batch_key" | sed 's/^/  /'
  else
    echo "MISMATCH"
    echo "--- batch ---"
    printf '%s\n' "$batch_out" | grep -E '^(s |o |c topor|c running)' || true
    echo "--- IPAMIR ---"
    printf '%s\n' "$ipamir_out" | grep -E '^(s |o )' || true
    failed=1
  fi
  echo
done

exit "$failed"

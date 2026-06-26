#!/usr/bin/env bash
# Run the WCNF differential MaxSAT fuzzer (IntelTopor vs EvalMaxSAT).
# SAT fuzzing is separate: scripts/fuzz_and_verify.csh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FUZZER="$ROOT/third_party/MaxSAT-Fuzzer"

if [ ! -x "$FUZZER/runwcnfuzz.py" ]; then
  echo "ERROR: missing $FUZZER/runwcnfuzz.py" >&2
  exit 1
fi

if [ ! -x "$ROOT/intel_sat_solver_enhanced_static" ]; then
  echo "Build the solver first (from repo root): make rs EXEC=intel_sat_solver_enhanced" >&2
  exit 1
fi

cd "$FUZZER"
exec env PYTHONUNBUFFERED=1 python3 ./runwcnfuzz.py -t 4 --timeout 30 --upperBound 4611686018427387904 "$@"

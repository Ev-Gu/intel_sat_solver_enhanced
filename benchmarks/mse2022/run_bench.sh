#!/usr/bin/env bash
# MSE2022 performance benchmark (benchmarks/mse2022 — NOT regression suite).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BENCH="$ROOT/benchmarks/mse2022"
mkdir -p "$BENCH/results"

usage() {
  echo "Usage: $0 {ref-first-wcnf|ipamir-app} [extra args...]"
  echo "  ref-first-wcnf  — EvalMaxSAT filter, IntelTopor IPAMIR only, NUWLS off by default"
  echo "  ipamir-app      — UWrMaxSat filter, IntelSatSolver, NUWLS off by default"
  echo ""
  echo "Results: benchmarks/mse2022/results/"
  exit 1
}

[ $# -ge 1 ] || usage

if [ ! -x "$ROOT/intel_sat_solver_enhanced_static" ]; then
  echo "Build: ./scripts/build_maxsat_tools.sh" >&2
  exit 1
fi

if [ ! -x "$ROOT/tools/bin/ipamir_wcnf_ours" ]; then
  echo "Build: bash tools/build_ipamir_wcnf.sh" >&2
  exit 1
fi

export TOPOR_NUWLS_TIME_LIMIT="${TOPOR_NUWLS_TIME_LIMIT:-0}"

exec python3 -u "$ROOT/tools/mse2022_benchmark.py" "$@"

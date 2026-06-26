#!/usr/bin/env bash
# Yam-style benchmarks: reference solver filters instances, then compare IntelTopor.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$ROOT/results"

usage() {
  echo "Usage: $0 {ref-first-wcnf|ipamir-app} [extra args...]"
  echo "  ref-first-wcnf  — EvalMaxSAT filters MSE22 WCNFs, then IntelTopor batch+IPAMIR"
  echo "  ipamir-app      — UWrMaxSat filters ipamirapp inputs, then IntelSatSolver"
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

exec python3 -u "$ROOT/tools/mse2022_benchmark.py" "$@"

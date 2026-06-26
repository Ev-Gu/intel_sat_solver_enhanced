#!/usr/bin/env bash
# One-shot setup for MaxSAT testing (batch + IPAMIR + fuzzer generator).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

echo "== [1/4] Solver (static release executable) =="
make rs EXEC=intel_sat_solver_enhanced

echo "== [2/4] IPAMIR WCNF loaders (ours + UWrMaxSat) =="
if [ ! -d third_party/ipamir/.git ]; then
  bash tools/install_ipamir.sh
fi
bash tools/build_ipamir_wcnf.sh

echo "== [3/4] WCNF fuzzer generator =="
make -C third_party/MaxSAT-Fuzzer/Fuzzer/wcnfuzz

echo "== [4/4] EvalMaxSAT reference (for differential fuzzer) =="
if [ ! -x third_party/MaxSAT-Fuzzer/MaxSATSolver/MSE22/EvalMaxSAT/build/EvalMaxSAT_bin ]; then
  bash third_party/MaxSAT-Fuzzer/Scripts/install_evalmaxsat.sh
fi

echo
echo "MaxSAT tools ready. See scripts/README.md"
echo "  ./scripts/fuzz_maxsat.sh              # fuzzer 1: batch WCNF vs EvalMaxSAT"
echo "  ./scripts/fuzz_maxsat_ipamir.sh       # fuzzer 2: WCNF vs UWrMaxSat via IPAMIR"
echo "  ./scripts/run_maxsat_regression.sh   (after: cd third_party/MaxSATRegressionSuite && ./install)"
echo "  ./scripts/compare_wcnf_batch_vs_ipamir.sh maxsat_regression_instances/*.wcnf"

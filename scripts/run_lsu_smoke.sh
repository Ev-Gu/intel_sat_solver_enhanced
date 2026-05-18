#!/bin/bash
# Smoke test for unweighted LSU (-M 1). Run from repo root.
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

BIN="${BIN:-./intel_sat_solver_enhanced_release}"
if [[ ! -x "$BIN" ]]; then
  echo "Building release binary..."
  make r
  BIN="./intel_sat_solver_enhanced_release"
fi

echo "=== LSU smoke: test_maxsat_lsu.cnf (expect o 0) ==="
"$BIN" regression_instances/test_maxsat_lsu.cnf -M 1

echo ""
echo "=== LSU smoke: test_maxsat_lsu_cost1.cnf (expect o 1) ==="
"$BIN" regression_instances/test_maxsat_lsu_cost1.cnf -M 1

echo ""
echo "=== Done ==="

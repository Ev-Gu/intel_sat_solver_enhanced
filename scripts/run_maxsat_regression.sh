#!/usr/bin/env bash
# Run the MSE MaxSAT regression suite on IntelTopor (batch WCNF path).
# SAT regression is separate: scripts/run_and_verify_intel_sat_on_regression.csh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SUITE="$ROOT/third_party/MaxSATRegressionSuite"
SOLVER="bash $ROOT/third_party/MaxSAT-Fuzzer/Scripts/intel_topor_maxsat.sh"

if [ ! -f "$SUITE/testSolver.py" ]; then
  echo "ERROR: missing $SUITE/testSolver.py" >&2
  exit 1
fi

if [ ! -x "$ROOT/intel_sat_solver_enhanced_static" ]; then
  echo "Build the solver first (from repo root): make rs EXEC=intel_sat_solver_enhanced" >&2
  exit 1
fi

if [ ! -f "$SUITE/MSE22+23Unique.csv" ]; then
  echo "Regression WCNFs not installed." >&2
  echo "Run: cd $SUITE && ./install" >&2
  exit 1
fi

python3 -c "import psutil" 2>/dev/null || {
  echo "Install Python dependency: pip install psutil" >&2
  exit 1
}

cd "$SUITE"
exec ./testSolver.py "$SOLVER" "$@"

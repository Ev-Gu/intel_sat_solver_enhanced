#!/usr/bin/env bash
# Performance comparison: IntelTopor vs EvalMaxSAT (batch) and UWrMaxSat (IPAMIR).
# Uses MSE regression WCNFs (install via third_party/MaxSATRegressionSuite/install).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SUITE="$ROOT/third_party/MaxSATRegressionSuite"
mkdir -p "$ROOT/results"

usage() {
  echo "Usage: $0 [--max-instances N] [--timeout SEC] [--csv FILE]"
  echo "  Default: MSE22Unique.csv, 40 instances, 30s timeout"
  exit 1
}

MAX=40
TIMEOUT=30
CSV="$SUITE/MSE22Unique.csv"

while [ $# -gt 0 ]; do
  case "$1" in
    -n|--max-instances) MAX="$2"; shift 2 ;;
    -t|--timeout) TIMEOUT="$2"; shift 2 ;;
    --csv) CSV="$2"; shift 2 ;;
    -h|--help) usage ;;
    *) echo "Unknown: $1" >&2; usage ;;
  esac
done

if [ ! -f "$SUITE/MSE22Unique.csv" ] && [ ! -f "$CSV" ]; then
  echo "Regression WCNFs not installed. Run: cd $SUITE && wget ... && unzip (see install script)" >&2
  exit 1
fi

if [ ! -x "$ROOT/intel_sat_solver_enhanced_static" ]; then
  echo "Build solver: ./scripts/build_maxsat_tools.sh" >&2
  exit 1
fi

if [ ! -x "$ROOT/tools/bin/ipamir_wcnf_ours" ]; then
  echo "Build IPAMIR drivers: bash tools/build_ipamir_wcnf.sh" >&2
  exit 1
fi

exec python3 "$ROOT/tools/maxsat_performance.py" \
  --csv "$CSV" \
  --max-instances "$MAX" \
  --timeout "$TIMEOUT"

#!/usr/bin/env bash
# Wrapper → benchmarks/mse2022/run_bench.sh (performance benchmark, not regression suite).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
exec "$ROOT/benchmarks/mse2022/run_bench.sh" "$@"

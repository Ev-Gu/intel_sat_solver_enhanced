#!/usr/bin/env bash
# Fuzzer 2: random WCNF -> IPAMIR -> compare ours vs UWrMaxSat (Yevgeny's loader).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
mkdir -p "$ROOT/results"

if [ ! -x tools/bin/ipamir_wcnf_ours ] || [ ! -x tools/bin/ipamir_wcnf_uwrmaxsat ]; then
  echo "Build first: bash tools/build_ipamir_wcnf.sh" >&2
  exit 1
fi

exec python3 -u "$ROOT/tools/wcnf_ipamir_fuzz.py" "$@"

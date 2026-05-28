#!/usr/bin/env bash
# Clone and build EvalMaxSAT for MaxSAT-Fuzzer comparison (MSE22 reference solver).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TARGET="$ROOT/MaxSATSolver/MSE22/EvalMaxSAT"

if [ ! -d "$TARGET/.git" ]; then
  mkdir -p "$(dirname "$TARGET")"
  git clone https://github.com/FlorentAvellaneda/EvalMaxSAT.git "$TARGET"
fi

mkdir -p "$TARGET/build"
cd "$TARGET/build"
cmake ..
make -j"$(nproc)"

echo "EvalMaxSAT binary: $TARGET/build/main/EvalMaxSAT_bin"

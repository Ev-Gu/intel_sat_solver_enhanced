#!/usr/bin/env bash
# Clone and build EvalMaxSAT for MaxSAT-Fuzzer comparison (MSE22 reference solver).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TARGET="$ROOT/MaxSATSolver/MSE22/EvalMaxSAT"

if [ ! -d "$TARGET/.git" ]; then
  mkdir -p "$(dirname "$TARGET")"
  git clone https://github.com/normal-account/EvalMaxSAT2022.git "$TARGET"
fi

# Upstream deleted test headers but main.cpp still includes them — remove so build succeeds.
MAINCPP="$TARGET/main.cpp"
if grep -q 'unweighted_data.h' "$MAINCPP" 2>/dev/null; then
  sed -i '/unweighted_data.h/d;/weighted_data.h/d' "$MAINCPP"
fi

mkdir -p "$TARGET/build"
cd "$TARGET/build"
# -DNBUILD makes bundled CaDiCaL skip its generated 'build.hpp'. This is required under
# clang (e.g. macOS), where __GNUC__==4 forces an unconditional #include <build.hpp>.
cmake -DCMAKE_CXX_FLAGS="-DNBUILD" ..
# 'nproc' is Linux-only; fall back to sysctl on macOS.
JOBS="$( (nproc 2>/dev/null) || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
make -j"$JOBS"

echo "EvalMaxSAT binary: $TARGET/build/EvalMaxSAT_bin"

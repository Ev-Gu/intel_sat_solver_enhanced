#!/usr/bin/env bash
# Clone the official IPAMIR scaffolding and apply UWrMaxSat compile fixes
# needed to build the incremental fuzzer reference solver on Linux/g++.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TARGET="$ROOT/third_party/ipamir"
PATCHES="$(cd "$(dirname "$0")/.." && pwd)/patches/uwrmaxsat-build.patch"
UWR_SRC="$TARGET/maxsat/uwrmaxsat14/uwrmaxsat"

if [ ! -d "$TARGET/.git" ]; then
  git clone --depth 1 https://bitbucket.org/coreo-group/ipamir.git "$TARGET"
fi

if [ -f "$PATCHES" ]; then
  ( cd "$UWR_SRC" && git apply --check "$PATCHES" 2>/dev/null && git apply "$PATCHES" ) || \
  patch -p1 -d "$UWR_SRC" < "$PATCHES" || true
fi

echo "IPAMIR ready at: $TARGET"
echo "Next: cd third_party/IncrementalFuzzer && ./build.sh"

#!/usr/bin/env bash
# Clone IPAMIR scaffolding and apply UWrMaxSat compile patches (Linux/g++).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TARGET="$ROOT/third_party/ipamir"
PATCHES="$ROOT/tools/patches/uwrmaxsat-build.patch"
UWR_SRC="$TARGET/maxsat/uwrmaxsat14/uwrmaxsat"

if [ ! -d "$TARGET/.git" ]; then
  git clone --depth 1 https://bitbucket.org/coreo-group/ipamir.git "$TARGET"
fi

if [ -f "$PATCHES" ] && [ -d "$UWR_SRC" ]; then
  patch -p1 -d "$UWR_SRC" --forward --batch < "$PATCHES" 2>/dev/null || true
fi

echo "IPAMIR ready at: $TARGET"
echo "Next: bash tools/build_ipamir_wcnf.sh"

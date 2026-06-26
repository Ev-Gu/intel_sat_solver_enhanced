#!/usr/bin/env bash
# Build IPAMIR WCNF loaders (Yevgeny's loader, two solver libraries):
#   bin/ipamir_wcnf_ours       -> IntelSatSolver
#   bin/ipamir_wcnf_uwrmaxsat  -> UWrMaxSat 1.4 (reference oracle)
#   bin/ipamir_wcnf_main       -> symlink to ipamir_wcnf_ours (batch-vs-IPAMIR script)
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$HERE/.." && pwd)"
IPAMIR="$REPO_ROOT/third_party/ipamir"
OURS_DIR="$IPAMIR/maxsat/IntelSatSolver"
UWR_DIR="$IPAMIR/maxsat/uwrmaxsat14"
COMINI="$IPAMIR/sat/cominisatps"
PATCH="$HERE/patches/uwrmaxsat-build.patch"
UWR_SRC="$UWR_DIR/uwrmaxsat"

mkdir -p "$HERE/bin"

if [ ! -d "$IPAMIR/.git" ]; then
  echo "Run: bash tools/install_ipamir.sh" >&2
  exit 1
fi

if [ -f "$PATCH" ] && [ -d "$UWR_SRC" ]; then
  patch -p1 -d "$UWR_SRC" --forward --batch < "$PATCH" 2>/dev/null || true
fi

echo "== [1/3] Reference: UWrMaxSat IPAMIR lib =="
if [[ ! -f "$UWR_DIR/libipamiruwrmaxsat14.a" ]]; then
    make -C "$UWR_DIR"
else
    echo "   already built: $UWR_DIR/libipamiruwrmaxsat14.a"
fi

echo "== [2/3] Our solver: IntelSatSolver IPAMIR lib =="
( cd "$REPO_ROOT" && make libr LIB=IntelSatSolver >/dev/null )
ar d "$REPO_ROOT/libIntelSatSolver_release.a" Main.or 2>/dev/null || true
mkdir -p "$OURS_DIR"
cp "$REPO_ROOT/libIntelSatSolver_release.a" "$OURS_DIR/libipamirIntelSatSolver.a"
printf 'g++' > "$OURS_DIR/LINK"
printf -- '-pthread' > "$OURS_DIR/LIBS"

echo "== [3/3] WCNF IPAMIR loaders =="
CXX=${CXX:-g++}
CXXFLAGS=${CXXFLAGS:--O2 -std=c++17 -Wall}
SRC="$HERE/ipamir_wcnf_main.cpp"

$CXX $CXXFLAGS "$SRC" -o "$HERE/bin/ipamir_wcnf_ours" \
    -L"$OURS_DIR" -lipamirIntelSatSolver -pthread
echo "   built bin/ipamir_wcnf_ours"

$CXX $CXXFLAGS "$SRC" -o "$HERE/bin/ipamir_wcnf_uwrmaxsat" \
    -L"$UWR_DIR" -lipamiruwrmaxsat14 -pthread -lgmp -L"$COMINI" -lipasircominisatps
echo "   built bin/ipamir_wcnf_uwrmaxsat"

ln -sf ipamir_wcnf_ours "$HERE/bin/ipamir_wcnf_main"
echo "Done. Binaries in $HERE/bin/"

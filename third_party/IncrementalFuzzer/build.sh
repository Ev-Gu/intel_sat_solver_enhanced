#!/usr/bin/env bash
# Builds the two IPAMIR incremental drivers used by the fuzzer:
#   bin/driver_ours        -> linked against our solver (IntelSatSolver)
#   bin/driver_uwrmaxsat   -> linked against the reference solver (UWrMaxSat 1.4)
#
# Our solver is built as a static release library (make libr), then linked into
# the drivers. For the non-incremental WCNF fuzzer use "make rs" instead (see
# third_party/MaxSAT-Fuzzer/Scripts/intel_topor_maxsat.sh).
#
# Re-run this after changing solver source code.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$HERE/../.." && pwd)"          # /root/bisan
IPAMIR="$REPO_ROOT/third_party/ipamir"

OURS_DIR="$IPAMIR/maxsat/IntelSatSolver"
UWR_DIR="$IPAMIR/maxsat/uwrmaxsat14"
COMINI="$IPAMIR/sat/cominisatps"

echo "== [1/3] Reference solver: UWrMaxSat IPAMIR lib =="
if [[ ! -f "$UWR_DIR/libipamiruwrmaxsat14.a" ]]; then
    make -C "$UWR_DIR"
else
    echo "   already built: $UWR_DIR/libipamiruwrmaxsat14.a"
fi

echo "== [2/3] Our solver: IntelSatSolver IPAMIR lib =="
# Build the release static library from the main repo, strip the object that
# contains main(), and publish it into the IPAMIR scaffolding directory.
( cd "$REPO_ROOT" && make libr LIB=IntelSatSolver >/dev/null )
ar d "$REPO_ROOT/libIntelSatSolver_release.a" Main.or 2>/dev/null || true
mkdir -p "$OURS_DIR"
cp "$REPO_ROOT/libIntelSatSolver_release.a" "$OURS_DIR/libipamirIntelSatSolver.a"
printf 'g++'      > "$OURS_DIR/LINK"
printf -- '-pthread' > "$OURS_DIR/LIBS"
echo "   published: $OURS_DIR/libipamirIntelSatSolver.a"

echo "== [3/3] Compiling drivers =="
CXX=${CXX:-g++}
CXXFLAGS=${CXXFLAGS:--O2 -std=c++17 -Wall}

$CXX $CXXFLAGS -I"$HERE" "$HERE/incr_driver.cc" -o "$HERE/bin/driver_ours" \
    -L"$OURS_DIR" -lipamirIntelSatSolver -pthread
echo "   built bin/driver_ours"

$CXX $CXXFLAGS -I"$HERE" "$HERE/incr_driver.cc" -o "$HERE/bin/driver_uwrmaxsat" \
    -L"$UWR_DIR" -lipamiruwrmaxsat14 -pthread -lgmp -L"$COMINI" -lipasircominisatps
echo "   built bin/driver_uwrmaxsat"

echo "Done. Drivers are in $HERE/bin/"

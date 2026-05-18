#!/bin/bash
# Build script for LSU + Totalizer test

echo "=== Building LSU Totalizer Test ==="

# Compilation flags
CXX=g++
CXXFLAGS="-std=c++20 -DSKIP_ZLIB -Wall -g -O0"
LDFLAGS="-lpthread"

# Source files needed
SOURCE_FILES="test_lsu_totalizer.cpp Topor.cc Topi.cc TopiAsg.cc TopiBacktrack.cc TopiBcp.cc TopiBitCompression.cc TopiCompression.cc TopiConflictAnalysis.cc TopiDebugPrinting.cc TopiDecision.cc TopiInprocess.cc TopiRestart.cc TopiStatistics.cc TopiVarScores.cc TopiWL.cc"

# Output executable
OUTPUT="test_lsu_totalizer"

echo "Compiling with:"
echo "  CXX: $CXX"
echo "  CXXFLAGS: $CXXFLAGS"
echo "  Output: $OUTPUT"
echo ""

$CXX $CXXFLAGS -o $OUTPUT $SOURCE_FILES $LDFLAGS

if [ $? -eq 0 ]; then
    echo "✓ Build successful!"
    echo ""
    echo "To run the test:"
    echo "  ./$OUTPUT"
else
    echo "✗ Build failed!"
    exit 1
fi

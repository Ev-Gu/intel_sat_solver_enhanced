@echo off
REM Build script for LSU + Totalizer test (Windows)

echo === Building LSU Totalizer Test ===

REM You may need to adjust the path to your compiler
REM For Visual Studio, use: cl.exe
REM For MinGW, use: g++.exe or x86_64-w64-mingw32-g++.exe

set CXX=g++.exe
set CXXFLAGS=-std=c++20 -DSKIP_ZLIB -Wall -g -O0
set LDFLAGS=-lpthread
set OUTPUT=test_lsu_totalizer.exe

echo Compiling with:
echo   CXX: %CXX%
echo   CXXFLAGS: %CXXFLAGS%
echo   Output: %OUTPUT%
echo.

set SOURCE_FILES=test_lsu_totalizer.cpp Topor.cc Topi.cc TopiAsg.cc TopiBacktrack.cc TopiBcp.cc TopiBitCompression.cc TopiCompression.cc TopiConflictAnalysis.cc TopiDebugPrinting.cc TopiDecision.cc TopiInprocess.cc TopiRestart.cc TopiStatistics.cc TopiVarScores.cc TopiWL.cc

%CXX% %CXXFLAGS% -o %OUTPUT% %SOURCE_FILES% %LDFLAGS%

if %ERRORLEVEL% EQU 0 (
    echo.
    echo [OK] Build successful!
    echo.
    echo To run the test:
    echo   %OUTPUT%
) else (
    echo.
    echo [ERROR] Build failed!
    exit /b 1
)

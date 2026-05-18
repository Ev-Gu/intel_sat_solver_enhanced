@echo off
REM Build script using Visual Studio Command Prompt (MSVC)

echo === Building LSU Totalizer Test (MSVC) ===

REM Try to find Visual Studio and run developer command prompt
REM Update the path if your Visual Studio installation is elsewhere

set VS_PATH=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat

if exist "%VS_PATH%" (
    echo Found Visual Studio at: %VS_PATH%
    call "%VS_PATH%"
) else (
    echo ERROR: Visual Studio not found at %VS_PATH%
    echo Please adjust the path in this script or install Visual Studio
    echo Alternatively, use MinGW: choco install mingw or install from https://www.mingw-w64.org/
    exit /b 1
)

echo.
echo Compiling with MSVC...
echo.

set CXX=cl.exe
set CXXFLAGS=/std:c++20 /W4 /Zi /Od /D SKIP_ZLIB
set LDFLAGS=
set OUTPUT=test_lsu_totalizer.exe

set SOURCE_FILES=test_lsu_totalizer.cpp Topor.cc Topi.cc TopiAsg.cc TopiBacktrack.cc TopiBcp.cc TopiBitCompression.cc TopiCompression.cc TopiConflictAnalysis.cc TopiDebugPrinting.cc TopiDecision.cc TopiInprocess.cc TopiRestart.cc TopiStatistics.cc TopiVarScores.cc TopiWL.cc

%CXX% %CXXFLAGS% /Fe%OUTPUT% %SOURCE_FILES% %LDFLAGS%

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

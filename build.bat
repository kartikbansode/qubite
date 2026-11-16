
@echo off
setlocal

echo ========================================================
echo   Building QUBITE - Cafe Token Management System
echo ========================================================
echo.

REM Use MSYS2 UCRT64 toolchain
set "PATH=C:\msys64\ucrt64\bin;%PATH%"

REM Configure project
cmake -B build -G "MinGW Makefiles" ^
    -DCMAKE_C_COMPILER=C:/msys64/ucrt64/bin/gcc.exe ^
    -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe ^
    -DCMAKE_MAKE_PROGRAM=C:/msys64/ucrt64/bin/mingw32-make.exe

if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed.
    pause
    exit /b 1
)

REM Build main server
cmake --build build --target CafeServer

if %errorlevel% neq 0 (
    echo [ERROR] Server compilation failed.
    pause
    exit /b 1
)

REM Build data structure tests
cmake --build build --target test_structures

if %errorlevel% neq 0 (
    echo [WARNING] Test compilation failed.
) else (
    echo.
    echo Running Data Structures Verification...
    build\test_structures.exe
)

echo.
echo ========================================================
echo   Build Successful! Run run.bat to start the server.
echo ========================================================

pause

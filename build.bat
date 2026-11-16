@echo off
echo ========================================================
echo   Building Cafe Token Management System
echo ========================================================
echo.
cmake -B build -G "MinGW Makefiles"
if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed.
    pause
    exit /b %errorlevel%
)

cmake --build build --target CafeServer
if %errorlevel% neq 0 (
    echo [ERROR] Compilation failed.
    pause
    exit /b %errorlevel%
)

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

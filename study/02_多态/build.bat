@echo off
REM ============================================================
REM module2_多态 · demo2 构建脚本
REM 需要 g++ 15.2 (MSYS2 MinGW) 在 PATH 中（g++ --version 验证）
REM ============================================================
g++ -std=c++17 -Wall -Wextra -O2 main.cpp -o demo2.exe
chcp 65001 >nul
if %errorlevel%==0 (
    echo.
    echo ===== Build OK. Running... =====
    echo.
    demo2.exe
) else (
    echo.
    echo ===== Build FAILED. See errors above. =====
)

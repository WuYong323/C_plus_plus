@echo off
REM ============================================================
REM module1_封装 · demo1 构建脚本
REM 需要 g++ 15.2 (MSYS2 MinGW) 在 PATH 中（g++ --version 验证）
REM ============================================================
chcp 65001 >nul
g++ -std=c++17 -Wall -Wextra -O2 main.cpp -o demo1.exe
if %errorlevel%==0 (
    echo.
    echo ===== Build OK. Running... =====
    echo.
    demo1.exe
) else (
    echo.
    echo ===== Build FAILED. See errors above. =====
)

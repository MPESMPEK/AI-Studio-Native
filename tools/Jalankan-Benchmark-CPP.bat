@echo off
title C++ Native Engine Benchmark (AVX2 + OpenMP)
color 0a
cd /d "%~dp0\.."

echo =========================================================
echo   MENJALANKAN BENCHMARK C++ NATIVE ENGINE (AVX2 + OpenMP)
echo =========================================================
echo.

if exist "cpp_core\bin\benchmark.exe" (
    "cpp_core\bin\benchmark.exe"
) else (
    echo [ERROR] File cpp_core\bin\benchmark.exe belum terkompilasi!
    echo Silakan jalankan 'Kompilasi-Ulang-CPP.bat' terlebih dahulu.
)

echo.
echo =========================================================
pause

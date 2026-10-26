@echo off
title Kompilasi C++ Native Engine (GCC AVX2 OpenMP)
color 0e
cd /d "%~dp0\.."

echo =========================================================
echo   MEMULAI KOMPILASI C++ NATIVE ENGINE (AVX2 + OpenMP)
echo =========================================================
echo.
echo * Catatan: Tutup jendela aplikasi AI Studio sebelum kompilasi
echo   agar file binary tidak terkunci oleh Windows.
echo.

if not exist "w64devkit\bin\g++.exe" (
    echo [ERROR] Compiler g++.exe tidak ditemukan di folder w64devkit!
    pause
    exit /b 1
)

if not exist "cpp_core\bin" (
    mkdir "cpp_core\bin"
)

echo [1/3] Mengompilasi engine_core.dll (SIMD AVX2 + OpenMP)...
".\w64devkit\bin\g++.exe" -O3 -mavx2 -mfma -fopenmp -shared -DBUILDING_ENGINE_CORE_DLL -I"cpp_core\include" "cpp_core\src\engine_core.cpp" "cpp_core\src\image_motion.cpp" -o "cpp_core\bin\engine_core.dll" -lpsapi
if %errorlevel% neq 0 (
    echo [ERROR] Gagal mengompilasi engine_core.dll!
    pause
    exit /b 1
)
copy /y "cpp_core\bin\engine_core.dll" "engine_core.dll" >nul
echo      [OK] Sukses membuat cpp_core\bin\engine_core.dll!
echo.

echo [2/3] Mengompilasi benchmark.exe...
".\w64devkit\bin\g++.exe" -O3 -mavx2 -mfma -fopenmp -I"cpp_core\include" "cpp_core\src\benchmark.cpp" -L"cpp_core\bin" -lengine_core -o "cpp_core\bin\benchmark.exe"
if %errorlevel% neq 0 (
    echo [ERROR] Gagal mengompilasi benchmark.exe!
    pause
    exit /b 1
)
echo      [OK] Sukses membuat cpp_core\bin\benchmark.exe!
echo.

echo [3/3] Mengompilasi AI-Studio.exe (Arsitektur Modular C++ dan Direct3D 11)...
call compile_step_by_step.bat
if %errorlevel% neq 0 (
    echo [ERROR] Gagal mengompilasi AI-Studio.exe!
    pause
    exit /b 1
)
copy /y "AI-Studio.exe" "cpp_core\bin\ai_studio_native.exe" >nul
echo      [OK] Sukses membuat AI-Studio.exe (100%% Pure C++ Standalone Single Binary)!
echo.

echo =========================================================
echo   KOMPILASI SELESAI SEMPURNA!
echo =========================================================
echo.
pause

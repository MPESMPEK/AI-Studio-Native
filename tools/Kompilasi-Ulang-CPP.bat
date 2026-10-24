@echo off
title 🛠️ Kompilasi C++ Native Engine (GCC 16 AVX2 OpenMP)
color 0e
cd /d "%~dp0\.."

echo =========================================================
echo   MEMULAI KOMPILASI C++ NATIVE ENGINE (AVX2 + OpenMP)
echo =========================================================
echo.
echo * Catatan: Tutup jendela aplikasi AI Studio sebelum kompilasi
echo   agar file engine_core.dll tidak terkunci oleh Windows.
echo.

if not exist "w64devkit\bin\g++.exe" (
    echo [ERROR] Compiler g++.exe tidak ditemukan di folder w64devkit!
    pause
    exit /b 1
)

if not exist "cpp_core\bin" (
    mkdir "cpp_core\bin"
)

echo [1/2] Mengompilasi engine_core.dll (SIMD AVX2 + OpenMP)...
".\w64devkit\bin\g++.exe" -O3 -mavx2 -mfma -fopenmp -shared -DBUILDING_ENGINE_CORE_DLL -I"cpp_core\include" "cpp_core\src\engine_core.cpp" "cpp_core\src\image_motion.cpp" -o "cpp_core\bin\engine_core.dll" -lpsapi
if %errorlevel% neq 0 (
    echo [ERROR] Gagal mengompilasi engine_core.dll!
    pause
    exit /b 1
)
echo      -> Sukses membuat cpp_core\bin\engine_core.dll!
echo.

echo [2/3] Mengompilasi benchmark.exe...
".\w64devkit\bin\g++.exe" -O3 -mavx2 -mfma -fopenmp -I"cpp_core\include" "cpp_core\src\benchmark.cpp" -L"cpp_core\bin" -lengine_core -o "cpp_core\bin\benchmark.exe"
if %errorlevel% neq 0 (
    echo [ERROR] Gagal mengompilasi benchmark.exe!
    pause
    exit /b 1
)
echo      -> Sukses membuat cpp_core\bin\benchmark.exe!
echo.

echo [3/3] Mengompilasi AI-Studio.exe (100%% Pure C++ Standalone Single-Binary)...
".\w64devkit\bin\g++.exe" -O3 -mavx2 -mfma -fopenmp -mwindows -s -DSTATIC_ENGINE_CORE ^
    -I"cpp_core\include" -I"cpp_core\imgui" ^
    "cpp_core\src\gui_main.cpp" ^
    "cpp_core\src\engine_core.cpp" ^
    "cpp_core\src\image_motion.cpp" ^
    "cpp_core\imgui\imgui.cpp" ^
    "cpp_core\imgui\imgui_draw.cpp" ^
    "cpp_core\imgui\imgui_tables.cpp" ^
    "cpp_core\imgui\imgui_widgets.cpp" ^
    "cpp_core\imgui\imgui_impl_win32.cpp" ^
    "cpp_core\imgui\imgui_impl_dx11.cpp" ^
    -ld3d11 -ldxgi -lgdi32 -ld3dcompiler -ldwmapi -lshell32 -lpsapi -lole32 ^
    -o "AI-Studio.exe"
if %errorlevel% neq 0 (
    echo [ERROR] Gagal mengompilasi AI-Studio.exe!
    pause
    exit /b 1
)
copy /y "AI-Studio.exe" "cpp_core\bin\ai_studio_native.exe" >nul
copy /y "cpp_core\bin\engine_core.dll" "engine_core.dll" >nul
echo      -> Sukses membuat AI-Studio.exe (100%% Pure C++ Standalone Single Binary)!
echo.

echo =========================================================
echo   KOMPILASI SELESAI SEMPURNA!
echo =========================================================
echo.
pause

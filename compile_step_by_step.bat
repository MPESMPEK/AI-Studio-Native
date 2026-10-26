@echo off
set CXX=w64devkit\bin\g++.exe
set CFLAGS=-O3 -mavx2 -mfma -fopenmp -DSTATIC_ENGINE_CORE -I"cpp_core\include" -I"cpp_core\imgui"

if not exist "cpp_core\obj" mkdir "cpp_core\obj"

echo [1/10] Compiling gui_main.cpp (-O3 SIMD)...
%CXX% %CFLAGS% -c "cpp_core\src\gui_main.cpp" -o "cpp_core\obj\gui_main.o"
if %ERRORLEVEL% NEQ 0 goto error

echo [2/10] Checking hardware_detector.o (-O3)...
%CXX% %CFLAGS% -c "cpp_core\src\hardware_detector.cpp" -o "cpp_core\obj\hardware_detector.o"
if %ERRORLEVEL% NEQ 0 goto error

echo [3/10] Checking model_manager.o (-O3)...
%CXX% %CFLAGS% -c "cpp_core\src\model_manager.cpp" -o "cpp_core\obj\model_manager.o"
if %ERRORLEVEL% NEQ 0 goto error

echo [4/11] Checking prompt_engine.o (-O3)...
%CXX% %CFLAGS% -c "cpp_core\src\prompt_engine.cpp" -o "cpp_core\obj\prompt_engine.o"
if %ERRORLEVEL% NEQ 0 goto error

echo [5/11] Checking memory_manager.o (-O3)...
%CXX% %CFLAGS% -c "cpp_core\src\memory_manager.cpp" -o "cpp_core\obj\memory_manager.o"
if %ERRORLEVEL% NEQ 0 goto error

echo [6/12] Checking hardware_telemetry.o (-O3)...
%CXX% %CFLAGS% -c "cpp_core\src\hardware_telemetry.cpp" -o "cpp_core\obj\hardware_telemetry.o"
if %ERRORLEVEL% NEQ 0 goto error

echo [7/12] Compiling engine_core.o (-O3 AVX2 Vectorized)...
%CXX% %CFLAGS% -c "cpp_core\src\engine_core.cpp" -o "cpp_core\obj\engine_core.o"
if %ERRORLEVEL% NEQ 0 goto error

echo [6/10] Compiling image_motion.o (-O3 Interpolation)...
%CXX% %CFLAGS% -c "cpp_core\src\image_motion.cpp" -o "cpp_core\obj\image_motion.o"
if %ERRORLEVEL% NEQ 0 goto error

echo [7/10] Checking imgui.o...
if not exist "cpp_core\obj\imgui.o" %CXX% %CFLAGS% -c "cpp_core\imgui\imgui.cpp" -o "cpp_core\obj\imgui.o"
if %ERRORLEVEL% NEQ 0 goto error

if not exist "cpp_core\obj\imgui_draw.o" %CXX% %CFLAGS% -c "cpp_core\imgui\imgui_draw.cpp" -o "cpp_core\obj\imgui_draw.o"
if %ERRORLEVEL% NEQ 0 goto error

if not exist "cpp_core\obj\imgui_tables.o" %CXX% %CFLAGS% -c "cpp_core\imgui\imgui_tables.cpp" -o "cpp_core\obj\imgui_tables.o"
if %ERRORLEVEL% NEQ 0 goto error

if not exist "cpp_core\obj\imgui_widgets.o" %CXX% %CFLAGS% -c "cpp_core\imgui\imgui_widgets.cpp" -o "cpp_core\obj\imgui_widgets.o"
if %ERRORLEVEL% NEQ 0 goto error

echo [8/10] Checking imgui_impl_win32.o...
if not exist "cpp_core\obj\imgui_impl_win32.o" %CXX% %CFLAGS% -c "cpp_core\imgui\imgui_impl_win32.cpp" -o "cpp_core\obj\imgui_impl_win32.o"
if %ERRORLEVEL% NEQ 0 goto error

echo [9/10] Checking imgui_impl_dx11.o...
if not exist "cpp_core\obj\imgui_impl_dx11.o" %CXX% %CFLAGS% -c "cpp_core\imgui\imgui_impl_dx11.cpp" -o "cpp_core\obj\imgui_impl_dx11.o"
if %ERRORLEVEL% NEQ 0 goto error

echo [12/12] Linking AI-Studio.exe (-O3 Native)...
%CXX% -mwindows -s "cpp_core\obj\gui_main.o" "cpp_core\obj\hardware_detector.o" "cpp_core\obj\model_manager.o" "cpp_core\obj\prompt_engine.o" "cpp_core\obj\memory_manager.o" "cpp_core\obj\hardware_telemetry.o" "cpp_core\obj\engine_core.o" "cpp_core\obj\image_motion.o" "cpp_core\obj\imgui.o" "cpp_core\obj\imgui_draw.o" "cpp_core\obj\imgui_tables.o" "cpp_core\obj\imgui_widgets.o" "cpp_core\obj\imgui_impl_win32.o" "cpp_core\obj\imgui_impl_dx11.o" -ld3d11 -ldxgi -lgdi32 -ld3dcompiler -ldwmapi -lshell32 -lpsapi -lole32 -ladvapi32 -fopenmp -lcomdlg32 -lwinmm -o "AI-Studio.exe"
if %ERRORLEVEL% NEQ 0 goto error

echo [ALL DONE SUCCESSFUL]
if exist "AI-Studio-Pro.exe" del /f /q "AI-Studio-Pro.exe" >nul 2>&1
echo [BUILD COMPLETE]
exit /b 0

:error
echo [ERROR ENCOUNTERED] Error code: %ERRORLEVEL%
exit /b 1

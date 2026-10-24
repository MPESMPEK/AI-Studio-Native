@echo off
set CXX=w64devkit\bin\g++.exe
set CFLAGS=-O2 -mavx2 -mfma -fopenmp -DSTATIC_ENGINE_CORE -I"cpp_core\include" -I"cpp_core\imgui"

if not exist "cpp_core\obj" mkdir "cpp_core\obj"

echo [1/9] Compiling gui_main.cpp...
%CXX% %CFLAGS% -c "cpp_core\src\gui_main.cpp" -o "cpp_core\obj\gui_main.o"
if %ERRORLEVEL% NEQ 0 goto error

echo [2/9] Checking engine_core.o...
if not exist "cpp_core\obj\engine_core.o" %CXX% %CFLAGS% -c "cpp_core\src\engine_core.cpp" -o "cpp_core\obj\engine_core.o"
if %ERRORLEVEL% NEQ 0 goto error

echo [3/9] Checking image_motion.o...
if not exist "cpp_core\obj\image_motion.o" %CXX% %CFLAGS% -c "cpp_core\src\image_motion.cpp" -o "cpp_core\obj\image_motion.o"
if %ERRORLEVEL% NEQ 0 goto error

echo [4/9] Checking imgui.o...
if not exist "cpp_core\obj\imgui.o" %CXX% %CFLAGS% -c "cpp_core\imgui\imgui.cpp" -o "cpp_core\obj\imgui.o"
if %ERRORLEVEL% NEQ 0 goto error

echo [5/9] Checking imgui_draw.o...
if not exist "cpp_core\obj\imgui_draw.o" %CXX% %CFLAGS% -c "cpp_core\imgui\imgui_draw.cpp" -o "cpp_core\obj\imgui_draw.o"
if %ERRORLEVEL% NEQ 0 goto error

echo [6/9] Checking imgui_tables.o...
if not exist "cpp_core\obj\imgui_tables.o" %CXX% %CFLAGS% -c "cpp_core\imgui\imgui_tables.cpp" -o "cpp_core\obj\imgui_tables.o"
if %ERRORLEVEL% NEQ 0 goto error

echo [7/9] Checking imgui_widgets.o...
if not exist "cpp_core\obj\imgui_widgets.o" %CXX% %CFLAGS% -c "cpp_core\imgui\imgui_widgets.cpp" -o "cpp_core\obj\imgui_widgets.o"
if %ERRORLEVEL% NEQ 0 goto error

echo [8/9] Checking imgui_impl_win32.o...
if not exist "cpp_core\obj\imgui_impl_win32.o" %CXX% %CFLAGS% -c "cpp_core\imgui\imgui_impl_win32.cpp" -o "cpp_core\obj\imgui_impl_win32.o"
if %ERRORLEVEL% NEQ 0 goto error

echo [9/9] Checking imgui_impl_dx11.o...
if not exist "cpp_core\obj\imgui_impl_dx11.o" %CXX% %CFLAGS% -c "cpp_core\imgui\imgui_impl_dx11.cpp" -o "cpp_core\obj\imgui_impl_dx11.o"
if %ERRORLEVEL% NEQ 0 goto error

echo [LINKING] Linking AI-Studio.exe...
%CXX% -mwindows -s "cpp_core\obj\gui_main.o" "cpp_core\obj\engine_core.o" "cpp_core\obj\image_motion.o" "cpp_core\obj\imgui.o" "cpp_core\obj\imgui_draw.o" "cpp_core\obj\imgui_tables.o" "cpp_core\obj\imgui_widgets.o" "cpp_core\obj\imgui_impl_win32.o" "cpp_core\obj\imgui_impl_dx11.o" -ld3d11 -ldxgi -lgdi32 -ld3dcompiler -ldwmapi -lshell32 -lpsapi -lole32 -ladvapi32 -fopenmp -lcomdlg32 -lwinmm -o "AI-Studio.exe"
if %ERRORLEVEL% NEQ 0 goto error

echo [ALL DONE SUCCESSFUL]
if exist "AI-Studio-Pro.exe" del /f /q "AI-Studio-Pro.exe" >nul 2>&1
echo [BUILD COMPLETE]
exit /b 0

:error
echo [ERROR ENCOUNTERED] Error code: %ERRORLEVEL%
exit /b 1

@echo off
title 🚀 AI Studio Native C++ (Direct3D 11 + Touchscreen)
color 0b
cd /d "%~dp0\.."

if exist "AI-Studio.exe" (
    start "" "AI-Studio.exe"
) else (
    echo [ERROR] AI-Studio.exe belum ditemukan.
    pause
)

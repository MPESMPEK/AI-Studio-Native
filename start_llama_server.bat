@echo off
title Llama Server 1.5B
cd /d "%~dp0llama_bin"
llama-server.exe -m "..\models\Teks_LLM\qwen2.5-coder-1.5b-instruct-q4_k_m.gguf" --port 8080 -c 8192 -ngl 99 --host 127.0.0.1
pause

#pragma once
#ifndef PROMPT_ENGINE_H
#define PROMPT_ENGINE_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

// Eksekusi CLI C++ Native secara Sinkron (digunakan untuk Smart Prompt LLM)
std::string ExecuteNativeCliSync(const std::string& cmd_str);

// AI Smart Prompt Expander (Mengubah ide singkat menjadi prompt masterpiece fotorealistis)
std::string GenerateSmartPrompt(const std::string& input_idea, int num_threads, int framing_idx = 0);

// AI Prompt Corrector (Menerjemahkan kata Indonesia ke Inggris & memperbaiki tata bahasa TANPA menambah konsep baru)
std::string GenerateCorrectedPrompt(const std::string& input_idea, int num_threads);

// Pembersih banner interaktif llama-cli dan format pemikiran DeepSeek-R1
std::string CleanLlamaStreamOutput(const std::string& raw);

#endif // PROMPT_ENGINE_H

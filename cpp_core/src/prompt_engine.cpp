#include "../include/prompt_engine.h"
#include "../include/hardware_detector.h"
#include "../include/model_manager.h"
#include <algorithm>

std::string ExecuteNativeCliSync(const std::string& cmd_str) {
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE hReadPipe = NULL, hWritePipe = NULL;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) return "";
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    ZeroMemory(&pi, sizeof(pi));

    std::string output = "";
    char* cmd_buf = _strdup(cmd_str.c_str());
    if (CreateProcessA(NULL, cmd_buf, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hWritePipe);
        char read_buf[512];
        DWORD bytes_read = 0;
        while (ReadFile(hReadPipe, read_buf, sizeof(read_buf) - 1, &bytes_read, NULL) && bytes_read > 0) {
            read_buf[bytes_read] = '\0';
            output += read_buf;
        }
        WaitForSingleObject(pi.hProcess, 35000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hReadPipe);
    } else {
        if (hWritePipe) CloseHandle(hWritePipe);
        if (hReadPipe) CloseHandle(hReadPipe);
    }
    free(cmd_buf);
    return output;
}

std::string GenerateSmartPrompt(const std::string& input_idea, int num_threads, int framing_idx) {
    std::string model_path = "models\\Teks_LLM\\qwen2.5-coder-1.5b-instruct-q4_k_m.gguf";
    if (GetFileAttributesA(model_path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        model_path = "models\\Teks_LLM\\Qwen3.8-2B-Uncensored-Q4_K_M.gguf";
    }
    if (GetFileAttributesA(model_path.c_str()) == INVALID_FILE_ATTRIBUTES && !g_detected_llms.empty()) {
        model_path = g_detected_llms[0].full_path;
    }
    if (GetFileAttributesA(model_path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        return input_idea;
    }

    std::string safe_idea = "";
    for (char c : input_idea) {
        if (c == '"') safe_idea += "\\\"";
        else if (c == '\n' || c == '\r') safe_idea += " ";
        else safe_idea += c;
    }

    std::string framing_desc = "";
    if (framing_idx == 1) {
        framing_desc = "extreme close-up portrait photograph, Canon EOS R5 85mm f/1.4 lens, tight framing on face, sharp facial focus, beautiful expressive eyes, micro skin pores, 8k uhd photorealistic";
    } else if (framing_idx == 2) {
        framing_desc = "medium shot photograph, waist-up portrait, 50mm f/1.8 lens, natural posture, sharp focus on subject and face, detailed clothing and expression, 8k uhd photorealistic";
    } else if (framing_idx == 3) {
        framing_desc = "wide-angle 35mm lens, full length body shot, standing pose, captured completely from head to toe with visible shoes and feet on the ground, full figure framing, high resolution full body photograph, (photorealistic detailed beautiful face:1.25), (sharp focused eyes, symmetrical iris:1.2), accurate human anatomy, proportionate limbs, realistic hands, 5 fingers on each hand";
    } else {
        std::string lower_idea = input_idea;
        for (char& ch : lower_idea) ch = (char)tolower(ch);
        bool is_fullbody = (lower_idea.find("full body") != std::string::npos ||
                            lower_idea.find("fullbody") != std::string::npos ||
                            lower_idea.find("seluruh badan") != std::string::npos ||
                            lower_idea.find("seluruh tubuh") != std::string::npos ||
                            lower_idea.find("full length") != std::string::npos ||
                            lower_idea.find("standing") != std::string::npos ||
                            lower_idea.find("berdiri") != std::string::npos ||
                            lower_idea.find("head to toe") != std::string::npos ||
                            lower_idea.find("kaki") != std::string::npos ||
                            lower_idea.find("sepatu") != std::string::npos);

        if (is_fullbody) {
            framing_desc = "wide-angle 35mm lens, full length body shot, standing pose, captured from head to toe with visible shoes on the ground, full figure framing, high resolution full body photograph, (photorealistic detailed beautiful face:1.25), (sharp focused eyes, symmetrical iris:1.2), accurate human anatomy, proportionate limbs, realistic hands, 5 fingers on each hand";
        } else if (lower_idea.find("wajah") != std::string::npos ||
                   lower_idea.find("close up") != std::string::npos ||
                   lower_idea.find("close-up") != std::string::npos ||
                   lower_idea.find("muka") != std::string::npos) {
            framing_desc = "close-up portrait photograph, Canon EOS R5 85mm f/1.4 lens, sharp facial focus, beautiful eyes, 8k uhd photorealistic";
        } else {
            framing_desc = "portrait photograph, Canon EOS R5 85mm f/1.4 lens, natural lighting, sharp facial focus, 8k uhd photorealistic quality";
        }
    }

    std::string sys_prompt = "<|im_start|>system\\nYou are an expert photographic prompt assistant for Stable Diffusion. Your mission is to enrich the user's idea into a master photorealistic prompt while STRICTLY PRESERVING the exact core subject, action, clothing, and setting requested. DO NOT invent random extra objects or change the scene. Enhance ONLY photographic parameters: natural lighting, authentic skin texture with visible micro pores, " + framing_desc + ", and 8k photorealistic quality. Output ONLY raw English prompt tokens separated by commas. Do not add explanations or quotes.<|im_end|>\\n<|im_start|>user\\nEnhance this prompt faithfully: " + safe_idea + "<|im_end|>\\n<|im_start|>assistant\\n";

    bool is_rtx_active = (g_hardware_backend_mode == 2 || (g_hardware_backend_mode == 0 && g_hw_profile.has_nvidia_gpu));
    int actual_gpu_layers = is_rtx_active ? (g_gpu_layers > 0 ? g_gpu_layers : 99) : 0;

    char cmd[4096];
    if (actual_gpu_layers > 0) {
        snprintf(cmd, sizeof(cmd),
            "llama_bin\\llama-cli.exe -m \"%s\" -p \"%s\" -ngl %d -n 90 -t %d --single-turn --no-display-prompt --log-disable --simple-io",
            model_path.c_str(), sys_prompt.c_str(), actual_gpu_layers, num_threads
        );
    } else {
        snprintf(cmd, sizeof(cmd),
            "llama_bin\\llama-cli.exe -m \"%s\" -p \"%s\" -n 90 -t %d --single-turn --no-display-prompt --log-disable --simple-io",
            model_path.c_str(), sys_prompt.c_str(), num_threads
        );
    }

    std::string raw_out = ExecuteNativeCliSync(cmd);
    std::string clean = "";
    size_t asst_pos = raw_out.rfind("<|im_start|>assistant");
    if (asst_pos != std::string::npos) {
        size_t nl_pos = raw_out.find('\n', asst_pos);
        clean = (nl_pos != std::string::npos) ? raw_out.substr(nl_pos + 1) : raw_out.substr(asst_pos + 21);
    } else {
        size_t p_pos = raw_out.rfind("> ");
        if (p_pos != std::string::npos) {
            size_t nl_pos = raw_out.find('\n', p_pos);
            clean = (nl_pos != std::string::npos) ? raw_out.substr(nl_pos + 1) : raw_out.substr(p_pos + 2);
        } else {
            size_t glob_pos = raw_out.find("using globbing pattern");
            clean = (glob_pos != std::string::npos) ? raw_out.substr(glob_pos + 22) : raw_out;
        }
    }

    size_t stats_pos = clean.find("[ Prompt:");
    if (stats_pos != std::string::npos) clean = clean.substr(0, stats_pos);
    size_t exit_pos = clean.find("Exiting...");
    if (exit_pos != std::string::npos) clean = clean.substr(0, exit_pos);
    size_t end_tag_pos = clean.find("<|im_end|>");
    if (end_tag_pos != std::string::npos) clean = clean.substr(0, end_tag_pos);

    while (!clean.empty() && (clean.front() == '\n' || clean.front() == '\r' || clean.front() == ' ' || clean.front() == '\t' || clean.front() == '"' || clean.front() == '>')) {
        clean.erase(clean.begin());
    }
    while (!clean.empty() && (clean.back() == '\n' || clean.back() == '\r' || clean.back() == ' ' || clean.back() == '\t' || clean.back() == '"')) {
        clean.pop_back();
    }

    if (clean.length() < 12) return input_idea;
    return clean;
}

std::string GenerateCorrectedPrompt(const std::string& input_idea, int num_threads) {
    std::string model_path = "models\\Teks_LLM\\qwen2.5-coder-1.5b-instruct-q4_k_m.gguf";
    if (GetFileAttributesA(model_path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        model_path = "models\\Teks_LLM\\Qwen3.8-2B-Uncensored-Q4_K_M.gguf";
    }
    if (GetFileAttributesA(model_path.c_str()) == INVALID_FILE_ATTRIBUTES && !g_detected_llms.empty()) {
        model_path = g_detected_llms[0].full_path;
    }
    if (GetFileAttributesA(model_path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        return input_idea;
    }

    std::string safe_idea = "";
    for (char c : input_idea) {
        if (c == '"') safe_idea += "\\\"";
        else if (c == '\n' || c == '\r') safe_idea += " ";
        else safe_idea += c;
    }

    std::string sys_prompt = "<|im_start|>system\\nYou are an accurate prompt translator and grammar cleaner for Stable Diffusion. Translate Indonesian words to clear English tags and fix spelling. STRICT RULES: DO NOT add new characters, objects, backgrounds, or actions that were not asked for. Keep the exact intent 100% faithful to the user. Output ONLY the translated, corrected English descriptive tags separated by commas. No explanations, no conversation, no quotes.<|im_end|>\\n<|im_start|>user\\nTranslate and clean this prompt faithfully without adding new concepts: " + safe_idea + "<|im_end|>\\n<|im_start|>assistant\\n";

    bool is_rtx_active = (g_hardware_backend_mode == 2 || (g_hardware_backend_mode == 0 && g_hw_profile.has_nvidia_gpu));
    int actual_gpu_layers = is_rtx_active ? (g_gpu_layers > 0 ? g_gpu_layers : 99) : 0;

    char cmd[4096];
    if (actual_gpu_layers > 0) {
        snprintf(cmd, sizeof(cmd),
            "llama_bin\\llama-cli.exe -m \"%s\" -p \"%s\" -ngl %d -n 75 -t %d --single-turn --no-display-prompt --log-disable --simple-io",
            model_path.c_str(), sys_prompt.c_str(), actual_gpu_layers, num_threads
        );
    } else {
        snprintf(cmd, sizeof(cmd),
            "llama_bin\\llama-cli.exe -m \"%s\" -p \"%s\" -n 75 -t %d --single-turn --no-display-prompt --log-disable --simple-io",
            model_path.c_str(), sys_prompt.c_str(), num_threads
        );
    }

    std::string raw_out = ExecuteNativeCliSync(cmd);
    std::string clean = "";
    size_t asst_pos = raw_out.rfind("<|im_start|>assistant");
    if (asst_pos != std::string::npos) {
        size_t nl_pos = raw_out.find('\n', asst_pos);
        clean = (nl_pos != std::string::npos) ? raw_out.substr(nl_pos + 1) : raw_out.substr(asst_pos + 21);
    } else {
        size_t p_pos = raw_out.rfind("> ");
        if (p_pos != std::string::npos) {
            size_t nl_pos = raw_out.find('\n', p_pos);
            clean = (nl_pos != std::string::npos) ? raw_out.substr(nl_pos + 1) : raw_out.substr(p_pos + 2);
        } else {
            clean = raw_out;
        }
    }

    size_t end_pos = clean.find("<|im_end|>");
    if (end_pos != std::string::npos) clean = clean.substr(0, end_pos);

    while (!clean.empty() && (clean.front() == ' ' || clean.front() == '\n' || clean.front() == '\r' || clean.front() == '"')) clean.erase(clean.begin());
    while (!clean.empty() && (clean.back() == ' ' || clean.back() == '\n' || clean.back() == '\r' || clean.back() == '"')) clean.pop_back();

    return clean.empty() ? input_idea : clean;
}

std::string CleanLlamaStreamOutput(const std::string& raw) {
    if (raw.empty()) return "";
    std::string text = raw;

    size_t last_prompt_arrow = text.rfind("\n> ");
    if (last_prompt_arrow == std::string::npos && text.rfind("> ") == 0) {
        last_prompt_arrow = 0;
    }
    if (last_prompt_arrow != std::string::npos) {
        size_t after_arrow = (last_prompt_arrow == 0) ? 2 : last_prompt_arrow + 3;
        size_t next_nl = text.find('\n', after_arrow);
        if (next_nl != std::string::npos) {
            text = text.substr(next_nl + 1);
        } else {
            text = "";
        }
    } else {
        size_t glob_pos = text.find("using globbing pattern");
        if (glob_pos != std::string::npos) {
            size_t next_nl = text.find('\n', glob_pos);
            if (next_nl != std::string::npos) {
                text = text.substr(next_nl + 1);
            }
        }
    }

    size_t stats_pos = text.find("[ Prompt:");
    if (stats_pos != std::string::npos) text = text.substr(0, stats_pos);
    size_t exit_pos = text.find("Exiting...");
    if (exit_pos != std::string::npos) text = text.substr(0, exit_pos);
    size_t eot_pos = text.find("<|im_end|>");
    if (eot_pos != std::string::npos) text = text.substr(0, eot_pos);
    size_t eos_pos = text.find("<|endoftext|>");
    if (eos_pos != std::string::npos) text = text.substr(0, eos_pos);

    auto replace_all = [](std::string& str, const std::string& from, const std::string& to) {
        size_t pos = 0;
        while ((pos = str.find(from, pos)) != std::string::npos) {
            str.replace(pos, from.length(), to);
            pos += to.length();
        }
    };
    replace_all(text, "<think>", "💭 [Pemikiran Logika DeepSeek]:\n");
    replace_all(text, "</think>", "\n\n💡 [Jawaban]:\n");
    replace_all(text, "[Start thinking]", "💭 [Pemikiran Logika DeepSeek]:\n");
    replace_all(text, "[End thinking]", "\n\n💡 [Jawaban]:\n");

    while (!text.empty() && (text.front() == '\n' || text.front() == '\r' || text.front() == ' ' || text.front() == '\t')) {
        text.erase(text.begin());
    }
    return text;
}

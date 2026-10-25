#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>
#include <d3d11.h>
#include <tchar.h>
#include <string>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <algorithm>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <fstream>
#include <sapi.h>
#include <mmsystem.h>

#define STB_IMAGE_IMPLEMENTATION
#include "../include/stb_image.h"

#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_win32.h"
#include "../imgui/imgui_impl_dx11.h"
#include "../include/engine_core.h"

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Direct3D 11 Globals
static ID3D11Device*            g_pd3dDevice = NULL;
static ID3D11DeviceContext*     g_pd3dDeviceContext = NULL;
static IDXGISwapChain*          g_pSwapChain = NULL;
static ID3D11RenderTargetView*  g_mainRenderTargetView = NULL;
static FILE*                    g_logf = NULL;

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Chat Message Struct with Direct DX11 Texture View
struct ChatMsg {
    std::string role;      // "User" or "AI Studio"
    std::string text;
    std::string timestamp;
    std::string tag;       // e.g. "LLM", "Diffusion", "Video", "NSFW Dewasa"
    std::string image_path;
    ID3D11ShaderResourceView* texture_view = NULL;
    int img_w = 0;
    int img_h = 0;
};

// UI State
static char g_input_prompt[4096] = "";
static int g_selected_mode = 0; // 0=Otomatis, 1=Chat, 2=Foto, 3=NSFW / Dewasa, 4=Video
static int g_selected_llm = 0;
static int g_selected_photo = 0;
static bool g_search_internet = false;
static bool g_voice_enabled = false;
static bool g_auto_speak = false; // Default: OFF (AI diam dan TIDAK ngomong sendiri kecuali disuruh)
static bool g_agent_mode = true;
static CoreTelemetry g_telemetry = {0};

// NSFW, Realism & Acceleration State
static bool g_nsfw_mode_is_photo = true; // true = Foto NSFW Realistis, false = Chat Bebas Uncensored
static bool g_nsfw_quality_booster = true;
static bool g_fullbody_face_fix = true; // HD Wajah & Anatomi (Anti-Muka Kabur & Cacat Anatomi)
static bool g_random_seed = true;
static int  g_custom_seed = 42;
static int  g_sampling_method_idx = 0;
static const char* g_sampling_methods[] = { "euler_a (Halus & Realistis)", "euler (Standar)", "dpm++2m (Detail Tajam)", "lcm (Ultra Cepat 4-Step)" };
static const char* g_sampling_method_flags[] = { "euler_a", "euler", "dpm++2m", "lcm" };
static char g_negative_prompt[1024] = "bad anatomy, deformed, mutated, extra limbs, bad hands, bad fingers, blurry, bad face, deformed eyes, crossed eyes, disfigured, cartoon, 3d, render, doll, plastic skin, drawing, watermark, low quality, bad proportions";

// Acceleration Controls
static bool g_use_fast_vae = true; // Gunakan TAESD (~2 detik vs 61 detik di CPU)
static bool g_turbo_mode = false;  // Mode Turbo LCM 4-Step (Off secara default untuk foto DSLR 20-step paling tajam)
static bool g_smart_prompt_auto = true; // ✨ Otomatis perluas ide singkat menjadi prompt fotorealistis AI
static std::atomic<bool> g_is_optimizing_prompt(false);
static std::thread g_optimize_thread;
static std::mutex g_input_mutex;

// Settings Modal & Parameters
static bool g_show_settings = false;
static int g_num_threads = 4;
static int g_image_steps = 20;
static float g_cfg_scale = 7.0f;
static int g_resolution_idx = 0;
static const char* g_resolutions[] = { "Rasio: 512x512 (Kotak)", "Rasio: 512x768 (Vertikal)", "Rasio: 768x512 (Lanskap)" };
static int g_framing_idx = 0; // 0=Auto, 1=Wajah (85mm), 2=Setengah Badan (50mm), 3=Seluruh Badan (35mm)
static const char* g_framing_names[] = {
    "Framing: Auto",
    "Framing: Wajah (85mm)",
    "Framing: 1/2 Badan (50mm)",
    "Framing: Full Body (35mm)"
};

// Image-to-Image (Edit Foto) & ControlNet Globals
static HWND g_hMainWnd = NULL;
static char g_init_image_path[512] = "";
static bool g_has_init_image = false;
static float g_img2img_strength = 0.65f;
static bool g_use_controlnet = false;
static float g_controlnet_strength = 0.90f;
static ID3D11ShaderResourceView* g_init_image_view = NULL;
static int g_init_image_w = 0;
static int g_init_image_h = 0;

// Document / File Attachment Globals
static char g_attached_doc_path[512] = "";
static bool g_has_attached_doc = false;
static std::string g_attached_doc_content = "";
static size_t g_attached_doc_size_bytes = 0;
static int g_attached_doc_lines = 0;

// Processing / Loading State
static bool g_is_processing = false;
static std::atomic<float> g_process_progress(0.0f);
static std::mutex g_status_mutex;
static std::string g_process_status = "";
static std::string g_pending_prompt = "";
static int g_pending_mode = 0;

static std::string g_live_stream_text = "";
static std::mutex g_stream_mutex;
static std::chrono::steady_clock::time_point g_process_start_time;

static std::thread g_worker_thread;
static std::atomic<bool> g_worker_finished(false);
static std::string g_worker_result = "";
static std::string g_worker_img_path = "";

void CopyTextToClipboard(const std::string& text) {
    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
        if (hGlob) {
            memcpy(GlobalLock(hGlob), text.c_str(), text.size() + 1);
            GlobalUnlock(hGlob);
            SetClipboardData(CF_TEXT, hGlob);
        }
        CloseClipboard();
    }
}

// Windows SAPI Voice / TTS Offline Native C++
void SpeakTextAsync(const std::string& text) {
    std::thread([text]() {
        CLSID clsid_spvoice;
        IID iid_ispvoice;
        CLSIDFromString(L"{96749377-3391-11D2-9EE3-00C04F797396}", &clsid_spvoice);
        IIDFromString(L"{6C44DF74-72B9-4992-A1EC-EF996E0422D4}", &iid_ispvoice);
        CoInitialize(NULL);
        ISpVoice* pVoice = NULL;
        HRESULT hr = CoCreateInstance(clsid_spvoice, NULL, CLSCTX_ALL, iid_ispvoice, (void**)&pVoice);
        if (SUCCEEDED(hr) && pVoice) {
            std::wstring wtext;
            for (char c : text) {
                if (c != '*' && c != '#' && c != '`' && c != '_' && c != '[' && c != ']' && c != '\\') {
                    wtext += (wchar_t)c;
                }
            }
            pVoice->Speak(wtext.c_str(), SPF_DEFAULT, NULL);
            pVoice->Release();
        }
        CoUninitialize();
    }).detach();
}

// ================= SISTEM SUARA MANUSIA ASLI (NATURAL HUMAN VOICE) =================
static std::atomic<bool> g_is_speaking(false);
static std::atomic<bool> g_stop_speaking_requested(false);
static int g_voice_character = 0; // 0=Gadis (Wanita), 1=Ardi (Pria), 2=Piper Offline
static const char* g_voice_character_names[] = { "👩 Gadis (Wanita - Ramah)", "👨 Ardi (Pria - Sopan)", "📻 Piper (Offline News)" };

void StopSpeakingVoice() {
    g_stop_speaking_requested.store(true);
    mciSendStringA("stop ai_voice_player", NULL, 0, NULL);
    mciSendStringA("close ai_voice_player", NULL, 0, NULL);
    g_is_speaking.store(false);
}

void TriggerWindowsVoiceTyping() {
    keybd_event(VK_LWIN, 0, 0, 0);
    keybd_event('H', 0, 0, 0);
    keybd_event('H', 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_LWIN, 0, KEYEVENTF_KEYUP, 0);
}

void SpeakNaturalVoiceAsync(const std::string& text, int voice_char = -1) {
    if (text.empty()) return;
    if (voice_char < 0) voice_char = g_voice_character;

    StopSpeakingVoice();
    g_stop_speaking_requested.store(false);

    std::thread([text, voice_char]() {
        g_is_speaking.store(true);

        std::string clean_spoken = "";
        bool in_code_block = false;
        for (size_t i = 0; i < text.length(); i++) {
            if (text[i] == '`') {
                if (i + 2 < text.length() && text[i+1] == '`' && text[i+2] == '`') {
                    in_code_block = !in_code_block;
                    i += 2;
                    continue;
                }
                continue;
            }
            if (in_code_block) continue;
            char c = text[i];
            if (c != '*' && c != '#' && c != '_' && c != '~' && c != '>' && c != '[' && c != ']' && c != '\\') {
                clean_spoken += c;
            }
        }

        while (!clean_spoken.empty() && (clean_spoken.front() == '\n' || clean_spoken.front() == '\r' || clean_spoken.front() == ' ')) {
            clean_spoken.erase(clean_spoken.begin());
        }

        if (clean_spoken.length() > 600) {
            clean_spoken = clean_spoken.substr(0, 597) + "... dan seterusnya.";
        }

        if (clean_spoken.empty()) {
            g_is_speaking.store(false);
            return;
        }

        CreateDirectoryA("outputs", NULL);
        std::string audio_file = "outputs\\ai_voice_response.mp3";
        std::string text_temp = "outputs\\voice_prompt_temp.txt";

        FILE* fp = fopen(text_temp.c_str(), "w");
        if (fp) {
            fputs(clean_spoken.c_str(), fp);
            fclose(fp);
        }

        char cmd[1024];
        if (voice_char == 2) {
            audio_file = "outputs\\ai_voice_response.wav";
            snprintf(cmd, sizeof(cmd), "cmd.exe /c type \"%s\" | piper.exe --model \"models\\Audio_Suara\\id_ID-news_tts-medium.onnx\" --output_file \"%s\"",
                     text_temp.c_str(), audio_file.c_str());
        } else {
            const char* vname = (voice_char == 1) ? "id-ID-ArdiNeural" : "id-ID-GadisNeural";
            snprintf(cmd, sizeof(cmd), "edge-tts --voice %s -f \"%s\" --write-media \"%s\"",
                     vname, text_temp.c_str(), audio_file.c_str());
        }

        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        ZeroMemory(&pi, sizeof(pi));

        if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 15000);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }

        DeleteFileA(text_temp.c_str());

        if (g_stop_speaking_requested.load()) {
            g_is_speaking.store(false);
            return;
        }

        if (GetFileAttributesA(audio_file.c_str()) != INVALID_FILE_ATTRIBUTES) {
            char mci_cmd[512];
            mciSendStringA("close ai_voice_player", NULL, 0, NULL);
            snprintf(mci_cmd, sizeof(mci_cmd), "open \"%s\" type %s alias ai_voice_player",
                     audio_file.c_str(), (voice_char == 2) ? "waveaudio" : "mpegvideo");
            mciSendStringA(mci_cmd, NULL, 0, NULL);
            mciSendStringA("play ai_voice_player wait", NULL, 0, NULL);
            mciSendStringA("close ai_voice_player", NULL, 0, NULL);
        } else {
            SpeakTextAsync(clean_spoken);
        }

        g_is_speaking.store(false);
    }).detach();
}

// Load Image into Direct3D 11 Texture View
ID3D11ShaderResourceView* LoadTextureFromFileDX11(ID3D11Device* pDevice, const char* filename, int* out_width, int* out_height) {
    if (!pDevice || !filename) return NULL;
    int image_width = 0, image_height = 0, channels = 0;
    unsigned char* image_data = stbi_load(filename, &image_width, &image_height, &channels, 4);
    if (!image_data) return NULL;

    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = image_width;
    desc.Height = image_height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;

    ID3D11Texture2D* pTexture = NULL;
    D3D11_SUBRESOURCE_DATA subResource;
    subResource.pSysMem = image_data;
    subResource.SysMemPitch = desc.Width * 4;
    subResource.SysMemSlicePitch = 0;
    HRESULT hr = pDevice->CreateTexture2D(&desc, &subResource, &pTexture);
    stbi_image_free(image_data);
    if (FAILED(hr) || !pTexture) return NULL;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = desc.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;

    ID3D11ShaderResourceView* pTextureView = NULL;
    hr = pDevice->CreateShaderResourceView(pTexture, &srvDesc, &pTextureView);
    pTexture->Release();
    if (FAILED(hr)) return NULL;

    if (out_width) *out_width = image_width;
    if (out_height) *out_height = image_height;
    return pTextureView;
}

void SetInitImage(const char* path) {
    if (!path || strlen(path) == 0) return;
    if (g_init_image_view) {
        g_init_image_view->Release();
        g_init_image_view = NULL;
    }
    strncpy(g_init_image_path, path, sizeof(g_init_image_path) - 1);
    g_init_image_path[sizeof(g_init_image_path) - 1] = '\0';
    if (g_pd3dDevice) {
        g_init_image_view = LoadTextureFromFileDX11(g_pd3dDevice, g_init_image_path, &g_init_image_w, &g_init_image_h);
        g_has_init_image = (g_init_image_view != NULL);
    } else {
        g_has_init_image = true;
    }
}

void ClearInitImage() {
    if (g_init_image_view) {
        g_init_image_view->Release();
        g_init_image_view = NULL;
    }
    g_init_image_path[0] = '\0';
    g_has_init_image = false;
}

void ClearAttachedDocument() {
    g_attached_doc_path[0] = '\0';
    g_has_attached_doc = false;
    g_attached_doc_content.clear();
    g_attached_doc_size_bytes = 0;
    g_attached_doc_lines = 0;
}

bool AttachDocument(const char* path) {
    if (!path || strlen(path) == 0) return false;
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    file.seekg(0, std::ios::end);
    size_t fsize = (size_t)file.tellg();
    file.seekg(0, std::ios::beg);

    g_attached_doc_size_bytes = fsize;

    const size_t MAX_READ_BYTES = 45000;
    size_t to_read = (fsize < MAX_READ_BYTES) ? fsize : MAX_READ_BYTES;

    std::string raw_content(to_read, '\0');
    file.read(&raw_content[0], to_read);
    file.close();

    std::string ext = "";
    std::string path_str = path;
    size_t dot_pos = path_str.find_last_of('.');
    if (dot_pos != std::string::npos) {
        ext = path_str.substr(dot_pos);
        for (char& c : ext) c = (char)tolower(c);
    }

    std::string clean_text = "";
    if (ext == ".pdf") {
        for (size_t i = 0; i < raw_content.size(); ++i) {
            char c = raw_content[i];
            if ((unsigned char)c >= 32 && (unsigned char)c <= 126) {
                clean_text += c;
            } else if (c == '\n' || c == '\r' || c == '\t') {
                if (!clean_text.empty() && clean_text.back() != '\n') {
                    clean_text += '\n';
                }
            }
        }
        if (clean_text.length() > 20000) clean_text = clean_text.substr(0, 20000);
    } else {
        for (char c : raw_content) {
            if (c != '\0') clean_text += c;
        }
    }

    if (fsize > MAX_READ_BYTES) {
        clean_text += "\n\n...[Isi dokumen dipotong karena ukuran melebihi batas baca awal 45 KB]...";
    }

    int lines = 1;
    for (char c : clean_text) {
        if (c == '\n') lines++;
    }

    strncpy(g_attached_doc_path, path, sizeof(g_attached_doc_path) - 1);
    g_attached_doc_path[sizeof(g_attached_doc_path) - 1] = '\0';
    g_attached_doc_content = clean_text;
    g_attached_doc_lines = lines;
    g_has_attached_doc = true;

    // Otomatis beralih ke Mode Chat jika sedang di mode foto
    if (g_selected_mode == 2 || g_selected_mode == 3) {
        g_selected_mode = 1;
    }

    return true;
}

std::string OpenDocumentFileDialog(HWND hWnd) {
    char szFile[MAX_PATH] = { 0 };
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Semua Dokumen (*.txt;*.md;*.pdf;*.csv;*.json;*.py;*.c;*.cpp;*.log)\0*.txt;*.md;*.pdf;*.csv;*.json;*.py;*.c;*.cpp;*.log;*.h;*.hpp;*.html;*.js;*.bat;*.ini;*.xml;*.yaml\0File Teks (*.txt;*.md;*.log)\0*.txt;*.md;*.log\0Kode Pemrograman (*.c;*.cpp;*.py;*.js;*.html)\0*.c;*.cpp;*.py;*.js;*.html;*.h;*.hpp;*.bat\0File Data (*.csv;*.json;*.xml)\0*.csv;*.json;*.xml\0Semua File (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn) == TRUE) {
        return std::string(szFile);
    }
    return "";
}

std::string OpenImageFileDialog(HWND hWnd) {
    char szFile[MAX_PATH] = { 0 };
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "File Gambar (*.png;*.jpg;*.jpeg;*.webp;*.bmp)\0*.png;*.jpg;*.jpeg;*.webp;*.bmp\0Semua File (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn) == TRUE) {
        return std::string(szFile);
    }
    return "";
}

std::string OpenModelFileDialog(HWND hWnd) {
    char szFile[MAX_PATH] = { 0 };
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Model AI GGUF / Safetensors (*.gguf;*.safetensors)\0*.gguf;*.safetensors\0Model GGUF (*.gguf)\0*.gguf\0Model Safetensors (*.safetensors)\0*.safetensors\0Semua File (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn) == TRUE) {
        return std::string(szFile);
    }
    return "";
}

// Eksekusi CLI C++ Native secara Sinkron (digunakan untuk Smart Prompt LLM)
static std::string ExecuteNativeCliSync(const std::string& cmd_str) {
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

// ================= HARDWARE & SYSTEM COMPATIBILITY ENGINE (RTX / CPU) =================
struct GpuInfo {
    std::string name;
    std::string vendor_name; // "NVIDIA", "Intel", "AMD", dll.
    UINT vendor_id = 0;
    int vram_mb = 0;
    int vram_gb = 0;
    bool is_nvidia = false;
    bool is_rtx = false;
    bool is_discrete = false;
};

struct HardwareDetectionResult {
    std::string cpu_brand = "";
    int cpu_cores_physical = 1;
    int cpu_threads_logical = 1;
    bool has_avx2 = false;
    bool has_fma = false;

    int total_ram_gb = 0;
    int free_ram_gb = 0;

    std::vector<GpuInfo> gpus;
    int selected_gpu_idx = 0;
    bool has_nvidia_gpu = false;
    bool has_rtx_gpu = false;

    int recommended_backend = 0; // 0=CPU AVX2, 1=NVIDIA RTX / CUDA
    int recommended_threads = 4;
    int recommended_gpu_layers = 0;
    bool recommended_fast_vae = true;
    int recommended_steps = 20;
    std::string profile_summary = "";
    std::string compatibility_status = "";
};

static HardwareDetectionResult g_hw_profile;
static bool g_hw_initialized = false;
static int  g_hardware_backend_mode = 0; // 0=Otomatis (Rekomendasi Cerdas), 1=Paksa CPU AVX2, 2=Paksa NVIDIA CUDA/RTX
static const char* g_backend_mode_names[] = {
    "Auto (Rekomendasi Cerdas)",
    "Mode Native CPU (AVX2 + OpenMP)",
    "Mode NVIDIA RTX (CUDA & Tensor Cores)"
};
static int  g_gpu_layers = 0; // Layers offload ke GPU (0 - 99)
static std::string g_hw_notification_msg = "";

static HardwareDetectionResult DetectHardwareAndCompatibility() {
    HardwareDetectionResult hw;

    // 1. CPU Brand dari Registry
    char cpuNameBuf[256] = {0};
    DWORD bufSize = sizeof(cpuNameBuf);
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExA(hKey, "ProcessorNameString", NULL, NULL, (LPBYTE)cpuNameBuf, &bufSize) == ERROR_SUCCESS) {
            hw.cpu_brand = cpuNameBuf;
        }
        RegCloseKey(hKey);
    }
    if (hw.cpu_brand.empty()) {
        hw.cpu_brand = "Intel(R) Core(TM) Processor";
    }

    // Cores dan Threads
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    hw.cpu_threads_logical = (int)sysinfo.dwNumberOfProcessors;
    hw.cpu_cores_physical = hw.cpu_threads_logical > 1 ? hw.cpu_threads_logical / 2 : 1;

    // SIMD AVX2 & FMA
    CoreTelemetry tel;
    Core_GetTelemetry(&tel);
    hw.has_avx2 = (tel.has_avx2 != 0);
    hw.has_fma  = (tel.has_fma != 0);

    // RAM Info
    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx(&mem);
    hw.total_ram_gb = (int)(mem.ullTotalPhys / (1024ULL * 1024ULL * 1024ULL));
    hw.free_ram_gb  = (int)(mem.ullAvailPhys / (1024ULL * 1024ULL * 1024ULL));

    // 2. GPU Detection via DXGI
    IDXGIFactory* pFactory = NULL;
    hw.has_nvidia_gpu = false;
    hw.has_rtx_gpu = false;
    hw.selected_gpu_idx = 0;

    if (SUCCEEDED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&pFactory))) {
        IDXGIAdapter* pAdapter = NULL;
        UINT adapterIdx = 0;
        while (pFactory->EnumAdapters(adapterIdx, &pAdapter) != DXGI_ERROR_NOT_FOUND) {
            DXGI_ADAPTER_DESC desc;
            pAdapter->GetDesc(&desc);

            char gpuName[256] = {0};
            WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, gpuName, sizeof(gpuName), NULL, NULL);

            GpuInfo info;
            info.name = gpuName;
            info.vendor_id = desc.VendorId;
            info.vram_mb = (int)(desc.DedicatedVideoMemory / (1024ULL * 1024ULL));
            info.vram_gb = (int)(desc.DedicatedVideoMemory / (1024ULL * 1024ULL * 1024ULL));

            std::string lowerName = info.name;
            for (char& c : lowerName) c = (char)tolower(c);

            if (desc.VendorId == 0x10DE || lowerName.find("nvidia") != std::string::npos || lowerName.find("geforce") != std::string::npos) {
                info.vendor_name = "NVIDIA";
                info.is_nvidia = true;
                info.is_discrete = true;
                if (lowerName.find("rtx") != std::string::npos) {
                    info.is_rtx = true;
                }
            } else if (desc.VendorId == 0x8086 || lowerName.find("intel") != std::string::npos) {
                info.vendor_name = "Intel";
                info.is_nvidia = false;
                info.is_rtx = false;
                info.is_discrete = (desc.DedicatedVideoMemory > 1024ULL * 1024ULL * 1024ULL);
            } else if (desc.VendorId == 0x1002 || lowerName.find("amd") != std::string::npos || lowerName.find("radeon") != std::string::npos) {
                info.vendor_name = "AMD";
                info.is_nvidia = false;
                info.is_rtx = false;
                info.is_discrete = (desc.DedicatedVideoMemory > 1024ULL * 1024ULL * 1024ULL);
            } else {
                info.vendor_name = "Lainnya";
                info.is_nvidia = false;
                info.is_rtx = false;
                info.is_discrete = false;
            }

            if (lowerName.find("basic render") == std::string::npos) {
                hw.gpus.push_back(info);
            }

            pAdapter->Release();
            adapterIdx++;
        }
        pFactory->Release();
    }

    if (hw.gpus.empty()) {
        GpuInfo defGpu;
        defGpu.name = "Intel(R) Integrated Graphics";
        defGpu.vendor_name = "Intel";
        defGpu.vendor_id = 0x8086;
        defGpu.vram_mb = 128;
        defGpu.vram_gb = 0;
        hw.gpus.push_back(defGpu);
    }

    // Cari GPU Terbaik (Prioritas NVIDIA RTX / Dedicated GPU)
    int bestGpu = 0;
    int highestVram = -1;
    for (size_t i = 0; i < hw.gpus.size(); i++) {
        if (hw.gpus[i].is_rtx) {
            hw.has_rtx_gpu = true;
            hw.has_nvidia_gpu = true;
            bestGpu = (int)i;
            break;
        } else if (hw.gpus[i].is_nvidia) {
            hw.has_nvidia_gpu = true;
            if (bestGpu == 0 || !hw.gpus[bestGpu].is_nvidia) bestGpu = (int)i;
        } else if (hw.gpus[i].vram_mb > highestVram) {
            highestVram = hw.gpus[i].vram_mb;
            bestGpu = (int)i;
        }
    }
    hw.selected_gpu_idx = bestGpu;

    // 3. Konfigurasi Profil Kompatibilitas Otomatis
    hw.recommended_threads = hw.cpu_threads_logical > 8 ? 8 : (hw.cpu_threads_logical > 4 ? hw.cpu_threads_logical : 4);

    if (hw.has_rtx_gpu || hw.has_nvidia_gpu) {
        const auto& activeGpu = hw.gpus[hw.selected_gpu_idx];
        hw.recommended_backend = 1; // NVIDIA RTX / CUDA
        hw.recommended_fast_vae = true;
        hw.recommended_steps = 25;

        if (activeGpu.vram_gb >= 12) {
            hw.recommended_gpu_layers = 99;
            hw.profile_summary = "NVIDIA RTX Extreme (VRAM >= 12GB): Full GPU Offload 99 Layer + Tensor Cores. Estimasi generasi gambar ~1.5 detik & chat ~90-120 token/detik!";
        } else if (activeGpu.vram_gb >= 8) {
            hw.recommended_gpu_layers = 99;
            hw.profile_summary = "NVIDIA RTX Pro (VRAM 8GB): Full Offload Model SD & 99 Layer LLM. Generasi foto ultra cepat ~2.5 - 4 detik di resolusi 512x768.";
        } else if (activeGpu.vram_gb >= 6) {
            hw.recommended_gpu_layers = 33;
            hw.profile_summary = "NVIDIA RTX Mid (VRAM 6GB): Hybrid Offload 33 Layer. Komputasi difusi penuh di GPU, VAE di VRAM, teks LLM sangat responsif.";
        } else {
            hw.recommended_gpu_layers = 20;
            hw.profile_summary = "NVIDIA Entry (VRAM 4GB): Smart VRAM Offload 20 Layer untuk mencegah Out-Of-Memory (OOM).";
        }
        hw.compatibility_status = "🚀 Siap Akselerasi Penuh NVIDIA RTX (CUDA & Tensor Cores)";
    } else {
        hw.recommended_backend = 0; // CPU AVX2
        hw.recommended_gpu_layers = 0;
        hw.recommended_fast_vae = true; // Wajib di CPU
        hw.recommended_steps = 20;
        hw.profile_summary = "Mode Native CPU (AVX2 + OpenMP): Dioptimalkan khusus prosesor laptop (" + hw.cpu_brand + "). Fast VAE TAESD otomatis aktif untuk memangkas waktu dari 61 detik ke 2.7 detik.";
        hw.compatibility_status = "⚡ Aktif: Mode CPU Laptop (AVX2 + FMA + TAESD)";
    }

    return hw;
}

static void ApplyHardwareCompatibilitySettings(bool user_override_backend = false) {
    if (!g_hw_initialized) {
        g_hw_profile = DetectHardwareAndCompatibility();
        g_hw_initialized = true;
    }

    int active_mode = g_hardware_backend_mode;
    if (active_mode == 0) {
        active_mode = g_hw_profile.recommended_backend;
    }

    if (active_mode == 0 || (g_hardware_backend_mode == 0 && !g_hw_profile.has_nvidia_gpu)) {
        g_num_threads = g_hw_profile.recommended_threads;
        g_use_fast_vae = true;
        g_gpu_layers = 0;
        g_hw_notification_msg = "Profil CPU Aktif: " + std::to_string(g_num_threads) + " Thread CPU, Fast VAE TAESD Aktif.";
    } else {
        g_num_threads = g_hw_profile.recommended_threads;
        g_gpu_layers = g_hw_profile.recommended_gpu_layers > 0 ? g_hw_profile.recommended_gpu_layers : 99;
        g_use_fast_vae = true;
        g_hw_notification_msg = "Profil NVIDIA RTX Aktif: Akselerasi CUDA Tensor Cores & GPU Offload " + std::to_string(g_gpu_layers) + " Layer.";
    }
}

// ================= REGISTRI MODEL AI & AUTO-DETEKSI GGUF / SAFETENSORS =================
struct DetectedModel {
    std::string display_name;
    std::string filename;
    std::string full_path;
    std::string desc_plus;
    std::string desc_minus;
    std::string rec;
    bool is_custom;
    int size_mb;
};

static std::vector<DetectedModel> g_detected_llms;
static std::vector<DetectedModel> g_detected_photos;
static std::vector<std::string> g_detected_llm_labels;
static std::vector<std::string> g_detected_photo_labels;
static std::vector<const char*> g_llm_items;
static std::vector<const char*> g_photo_items;

static void ScanModelsDirectory() {
    g_detected_llms.clear();
    g_detected_photos.clear();
    g_detected_llm_labels.clear();
    g_detected_photo_labels.clear();
    g_llm_items.clear();
    g_photo_items.clear();

    // 1. Pindai folder models\Teks_LLM\*.gguf
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA("models\\Teks_LLM\\*.gguf", &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                std::string fname = fd.cFileName;
                std::string path = "models\\Teks_LLM\\" + fname;
                ULARGE_INTEGER fileSize;
                fileSize.LowPart = fd.nFileSizeLow;
                fileSize.HighPart = fd.nFileSizeHigh;
                int mb = (int)(fileSize.QuadPart / (1024 * 1024));

                DetectedModel m;
                m.filename = fname;
                m.full_path = path;
                m.size_mb = mb;
                m.is_custom = false;

                std::string lower_f = fname;
                for (char& c : lower_f) c = (char)tolower(c);

                if (lower_f.find("qwen2.5-coder-1.5b") != std::string::npos) {
                    m.display_name = "Qwen2.5-Coder 1.5B (Koding & Chat Kilat)";
                    m.desc_plus = "Sangat ringan & responsif (~30-50 token/detik di CPU), hemat RAM (~1.2 GB), serta sangat lihai koding C++/Python dan ekspansi Auto Prompt.";
                    m.desc_minus = "Penalaran logika teka-teki yang sangat rumit atau esai panjang terkadang kurang mendalam dibanding model 7B.";
                    m.rec = "Model default terbaik untuk koding cepat, obrolan sehari-hari, dan Prompt Pintar AI.";
                } else if (lower_f.find("deepseek-r1") != std::string::npos) {
                    m.display_name = "DeepSeek-R1 Distill Qwen (Reasoning Mendalam)";
                    m.desc_plus = "Kemampuan berpikir mendalam (Chain-of-Thought): Luar biasa pintar dalam logika matematika, analisa algoritma, dan pemecahan bug rumit.";
                    m.desc_minus = "Menghasilkan blok pemikiran <think> sehingga respons teks sedikit lebih panjang dan butuh beberapa detik lebih lama.";
                    m.rec = "Gunakan saat memecahkan soal matematika sulit, analisa logika, atau pencarian bug rumit.";
                } else if (lower_f.find("qwen2.5-coder-7b") != std::string::npos) {
                    m.display_name = "Qwen2.5-Coder 7B (Koding Kompleks Enterprise)";
                    m.desc_plus = "Tingkat kecerdasan & kapasitas pengetahuan tertinggi (7 Miliar parameter): Mampu merancang sistem aplikasi besar dan penjelasan teknis mendalam.";
                    m.desc_minus = "Ukuran file besar (~4.5 GB), konsumsi RAM laptop lebih tinggi, dan kecepatan ketik di CPU (~8-15 token/detik) lebih lambat.";
                    m.rec = "Gunakan saat butuh bantuan perancangan kode skala besar dan penjelasan teknis lengkap.";
                } else if (lower_f.find("uncensored") != std::string::npos || lower_f.find("qwen3.8") != std::string::npos) {
                    m.display_name = "Qwen3.8-2B Uncensored (Chat Bebas / NSFW)";
                    m.desc_plus = "100% Bebas Sensor: Menjawab semua topik obrolan, cerita dewasa/NSFW, roleplay santai tanpa penolakan moral.";
                    m.desc_minus = "Keahlian sintaksis koding teknis tidak setinggi seri Coder.";
                    m.rec = "Pilihan paling pas untuk Mode NSFW, penulisan cerita bebas, dan teman ngobrol tanpa filter moral.";
                } else {
                    // File GGUF baru yang ditambahkan oleh pengguna!
                    m.is_custom = true;
                    m.display_name = "✨ [GGUF] " + fname + " (" + std::to_string(mb) + " MB)";
                    m.desc_plus = "Model LLM kustom baru (.gguf) otomatis terdeteksi dari folder models\\Teks_LLM. Kompatibel 100% dengan llama-cli C++.";
                    m.desc_minus = "Kecepatan dan penalaran bergantung pada arsitektur & tingkat kuantisasi model yang Anda tambahkan.";
                    m.rec = "Dapat langsung dipilih untuk percakapan teks, penalaran, atau bantuan koding.";
                }
                g_detected_llms.push_back(m);
            }
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }

    // Pindai juga jika ada file .gguf di root folder models\ (jika belum masuk daftar)
    hFind = FindFirstFileA("models\\*.gguf", &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                std::string fname = fd.cFileName;
                std::string lower_f = fname;
                for (char& c : lower_f) c = (char)tolower(c);
                if (lower_f.find("sd") != std::string::npos || lower_f.find("diff") != std::string::npos || lower_f.find("dream") != std::string::npos) {
                    continue; // Model difusi, akan dipindai di bagian foto
                }
                bool already = false;
                for (const auto& ex : g_detected_llms) {
                    if (ex.filename == fname) { already = true; break; }
                }
                if (!already) {
                    ULARGE_INTEGER fileSize;
                    fileSize.LowPart = fd.nFileSizeLow;
                    fileSize.HighPart = fd.nFileSizeHigh;
                    int mb = (int)(fileSize.QuadPart / (1024 * 1024));

                    DetectedModel m;
                    m.filename = fname;
                    m.full_path = "models\\" + fname;
                    m.size_mb = mb;
                    m.is_custom = true;
                    m.display_name = "✨ [GGUF] " + fname + " (" + std::to_string(mb) + " MB)";
                    m.desc_plus = "Model LLM (.gguf) kustom baru terdeteksi di folder models. Siap dijalankan dengan engine C++.";
                    m.desc_minus = "Performa tergantung arsitektur model.";
                    m.rec = "Pilih model ini untuk mengobrol atau bertanya.";
                    g_detected_llms.push_back(m);
                }
            }
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }

    // 2. Pindai folder models\Gambar_Diffusion\ untuk .safetensors dan .gguf
    auto scan_diff_ext = [](const char* search_pattern, const char* dir_prefix) {
        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA(search_pattern, &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    std::string fname = fd.cFileName;
                    bool already = false;
                    for (const auto& ex : g_detected_photos) {
                        if (ex.filename == fname) { already = true; break; }
                    }
                    if (already) continue;

                    std::string path = std::string(dir_prefix) + fname;
                    ULARGE_INTEGER fileSize;
                    fileSize.LowPart = fd.nFileSizeLow;
                    fileSize.HighPart = fd.nFileSizeHigh;
                    int mb = (int)(fileSize.QuadPart / (1024 * 1024));

                    DetectedModel m;
                    m.filename = fname;
                    m.full_path = path;
                    m.size_mb = mb;
                    m.is_custom = false;

                    std::string lower_f = fname;
                    for (char& c : lower_f) c = (char)tolower(c);

                    if (lower_f.find("cyberrealistic") != std::string::npos) {
                        m.display_name = "CyberRealistic V5.0 (Paling Nyata & NSFW)";
                        m.desc_plus = "Kualitas fotorealistis tertinggi untuk wajah, pori-pori kulit mikro, iris mata tajam, dan pencahayaan kamera DSLR 8K. Sangat unggul untuk foto manusia nyata dan mode NSFW uncensored.";
                        m.desc_minus = "Ukuran file besar (2.13 GB) & membutuhkan 20-25 steps denoising (~15-25 detik di CPU) untuk hasil maksimal.";
                        m.rec = "Pilihan UTAMA dan TERBAIK untuk membuat foto potret manusia nyata berkualitas majalah/DSLR.";
                    } else if (lower_f.find("realistic_vision") != std::string::npos) {
                        m.display_name = "Realistic Vision V5.1 (Fotorealistis Human & Alam)";
                        m.desc_plus = "Sangat seimbang dan stabil: unggul untuk foto manusia, pemandangan alam, arsitektur, dan mobil sport.";
                        m.desc_minus = "Tekstur pori-pori kulit wajah sedikit lebih halus/soft dibandingkan CyberRealistic V5.";
                        m.rec = "Sangat direkomendasikan jika ingin membuat pemandangan alam, mobil balap, interior, atau foto campuran manusia & lingkungan.";
                    } else if (lower_f.find("dreamshaper") != std::string::npos) {
                        m.display_name = "DreamShaper 8 LCM (Ultra Fast 4-Step)";
                        m.desc_plus = "SUPER KILAT: Selesai hanya dalam 4 langkah (~3-5 detik di CPU). Gaya visual artistik, sinematik, & hemat RAM.";
                        m.desc_minus = "Kurang fotorealistis untuk potret manusia jarak dekat; detail iris dan pori-pori tidak sepadat model 20-step.";
                        m.rec = "Sangat cocok untuk eksplorasi ide cepat, konsep gambar, atau saat Anda ingin gambar selesai dalam hitungan detik.";
                    } else if (lower_f.find("sakura") != std::string::npos || lower_f.find("iris") != std::string::npos) {
                        m.display_name = "Sakura Iris 3B DiT (Anime & Ilustrasi)";
                        m.desc_plus = "Arsitektur Diffusion Transformer (DiT): Kualitas terbaik untuk seni anime Jepang, 2.5D, dan karakter fiksi berwarna cerah.";
                        m.desc_minus = "Tidak dirancang untuk foto manusia fotorealistis dunia nyata.";
                        m.rec = "Gunakan khusus saat Anda ingin membuat karakter kartun/anime, fanart, atau ilustrasi fiksi.";
                    } else if (lower_f.find("qwen-image") != std::string::npos) {
                        m.display_name = "Qwen Image 2.1 Uncensored (GGUF)";
                        m.desc_plus = "Model difusi GGUF resolusi tinggi, uncensored, dan fleksibel.";
                        m.desc_minus = "Memerlukan konsumsi RAM lebih stabil.";
                        m.rec = "Pilihan alternatif yang bagus untuk generasi bebas sensor.";
                    } else if (lower_f.find("sd_model") != std::string::npos) {
                        m.display_name = "SD Model Baseline (GGUF)";
                        m.desc_plus = "Model difusi dasar terkuantisasi GGUF, ringan dan cepat dimuat di CPU.";
                        m.desc_minus = "Gaya umum standar SD 1.5.";
                        m.rec = "Bagus untuk pengujian performa difusi ringan di laptop.";
                    } else {
                        // File model baru yang ditambahkan oleh user!
                        m.is_custom = true;
                        std::string tag = (lower_f.find(".gguf") != std::string::npos) ? "✨ [GGUF] " : "✨ [Safetensors] ";
                        m.display_name = tag + fname + " (" + std::to_string(mb) + " MB)";
                        m.desc_plus = "Model difusi kustom baru berhasil dideteksi otomatis dari folder models\\Gambar_Diffusion. Kompatibel penuh dengan engine sd-cli C++.";
                        m.desc_minus = "Kecepatan dan gaya gambar mengikuti arsitektur checkpoint model ini.";
                        m.rec = "Dapat langsung dipilih untuk menghasilkan gambar sesuai karakteristik model.";
                    }
                    g_detected_photos.push_back(m);
                }
            } while (FindNextFileA(hFind, &fd));
            FindClose(hFind);
        }
    };

    scan_diff_ext("models\\Gambar_Diffusion\\*.safetensors", "models\\Gambar_Diffusion\\");
    scan_diff_ext("models\\Gambar_Diffusion\\*.gguf", "models\\Gambar_Diffusion\\");
    scan_diff_ext("models\\*.safetensors", "models\\");

    // Fallbacks if empty
    if (g_detected_llms.empty()) {
        DetectedModel def;
        def.display_name = "Qwen2.5-Coder 1.5B (Default)";
        def.filename = "qwen2.5-coder-1.5b-instruct-q4_k_m.gguf";
        def.full_path = "models\\Teks_LLM\\qwen2.5-coder-1.5b-instruct-q4_k_m.gguf";
        def.desc_plus = "Model default.";
        def.desc_minus = "Tidak ada model lain ditemukan.";
        def.rec = "Salin file .gguf ke folder models\\Teks_LLM.";
        def.is_custom = false;
        def.size_mb = 980;
        g_detected_llms.push_back(def);
    }
    if (g_detected_photos.empty()) {
        DetectedModel def;
        def.display_name = "CyberRealistic V5.0 (Default)";
        def.filename = "CyberRealistic_V5_FP16.safetensors";
        def.full_path = "models\\Gambar_Diffusion\\CyberRealistic_V5_FP16.safetensors";
        def.desc_plus = "Model default.";
        def.desc_minus = "Tidak ada model lain ditemukan.";
        def.rec = "Salin file .safetensors / .gguf ke folder models\\Gambar_Diffusion.";
        def.is_custom = false;
        def.size_mb = 2130;
        g_detected_photos.push_back(def);
    }

    // Bangun daftar pointer untuk ImGui::Combo
    g_detected_llm_labels.resize(g_detected_llms.size());
    g_llm_items.resize(g_detected_llms.size());
    for (size_t i = 0; i < g_detected_llms.size(); i++) {
        g_detected_llm_labels[i] = g_detected_llms[i].display_name;
        g_llm_items[i] = g_detected_llm_labels[i].c_str();
    }

    g_detected_photo_labels.resize(g_detected_photos.size());
    g_photo_items.resize(g_detected_photos.size());
    for (size_t i = 0; i < g_detected_photos.size(); i++) {
        g_detected_photo_labels[i] = g_detected_photos[i].display_name;
        g_photo_items[i] = g_detected_photo_labels[i].c_str();
    }

    if (g_selected_llm >= (int)g_detected_llms.size()) g_selected_llm = 0;
    if (g_selected_photo >= (int)g_detected_photos.size()) g_selected_photo = 0;
}

// ================= DIAGNOSTIK & IMPORT MODEL GGUF / SAFETENSORS =================
enum ModelCategory {
    MODEL_CAT_UNKNOWN = 0,
    MODEL_CAT_TEXT_LLM,
    MODEL_CAT_IMAGE_DIFFUSION,
    MODEL_CAT_VISION_MULTIMODAL,
    MODEL_CAT_VAE,
    MODEL_CAT_LORA
};

struct ModelInspectionResult {
    bool is_valid_file = false;
    bool is_gguf = false;
    bool is_safetensors = false;
    std::string filepath = "";
    std::string filename = "";
    int file_size_mb = 0;
    
    ModelCategory category = MODEL_CAT_UNKNOWN;
    std::string category_name = "";
    std::string target_folder = "";
    
    bool is_critical_error = false;
    std::string critical_reason = "";
    
    std::vector<std::string> dependencies_status;
    std::vector<std::string> consequences;
    std::string recommendation = "";
};

static ModelInspectionResult InspectModelFile(const std::string& filepath) {
    ModelInspectionResult res;
    res.filepath = filepath;
    
    size_t last_slash = filepath.find_last_of("\\/");
    res.filename = (last_slash != std::string::npos) ? filepath.substr(last_slash + 1) : filepath;

    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExA(filepath.c_str(), GetFileExInfoStandard, &fad)) {
        res.is_critical_error = true;
        res.critical_reason = "File tidak ditemukan atau tidak dapat diakses.";
        res.consequences.push_back("Aplikasi tidak dapat membaca file dari lokasi penyimpanan ini.");
        return res;
    }

    ULARGE_INTEGER fileSize;
    fileSize.LowPart = fad.nFileSizeLow;
    fileSize.HighPart = fad.nFileSizeHigh;
    res.file_size_mb = (int)(fileSize.QuadPart / (1024 * 1024));

    if (fileSize.QuadPart == 0) {
        res.is_critical_error = true;
        res.critical_reason = "File berukuran 0 Byte (Kosong). Kemungkinan download gagal atau terputus.";
        res.consequences.push_back("Engine C++ lokal (llama-cli / sd-cli) akan langsung crash / gagal jika memuat file kosong.");
        return res;
    }

    FILE* f = fopen(filepath.c_str(), "rb");
    if (!f) {
        res.is_critical_error = true;
        res.critical_reason = "Tidak dapat membuka file (file sedang digunakan oleh program lain atau izin ditolak).";
        res.consequences.push_back("Proses impor tidak dapat membaca data model.");
        return res;
    }

    char header[65536];
    size_t bytes_read = fread(header, 1, sizeof(header), f);
    fclose(f);

    if (bytes_read >= 4 && header[0] == 'G' && header[1] == 'G' && header[2] == 'U' && header[3] == 'F') {
        res.is_gguf = true;
        res.is_valid_file = true;
    } else if (res.filename.find(".safetensors") != std::string::npos) {
        res.is_safetensors = true;
        res.is_valid_file = true;
    } else if (res.filename.find(".gguf") != std::string::npos) {
        res.is_critical_error = true;
        res.critical_reason = "Header magic bukan 'GGUF' yang valid. File kemungkinan bukan format GGUF resmi atau data header awal rusak.";
        res.consequences.push_back("Runner llama-cli / sd-cli menolak memuat file ini karena struktur tensor tidak valid.");
        return res;
    } else {
        res.is_critical_error = true;
        res.critical_reason = "Ekstensi file tidak didukung. Engine C++ hanya mendukung format .gguf atau .safetensors.";
        res.consequences.push_back("File bertipe ini tidak dapat dieksekusi oleh mesin offline studio.");
        return res;
    }

    std::string header_str = "";
    for (size_t i = 0; i < bytes_read; i++) {
        char c = header[i];
        if (c >= 32 && c <= 126) header_str += (char)tolower(c);
        else header_str += ' ';
    }

    std::string lower_fn = res.filename;
    for (char& c : lower_fn) c = (char)tolower(c);

    // Deteksi Kategori Model
    if (header_str.find("clip_vision") != std::string::npos || lower_fn.find("mmproj") != std::string::npos) {
        res.category = MODEL_CAT_VISION_MULTIMODAL;
        res.category_name = "Vision Multimodal (Proyektor Gambar)";
        res.target_folder = "models\\Multimodal_Vision";
    } else if (lower_fn.find("vae") != std::string::npos) {
        res.category = MODEL_CAT_VAE;
        res.category_name = "VAE (AutoEncoder Citra)";
        res.target_folder = "models\\VAE";
    } else if (lower_fn.find("lora") != std::string::npos) {
        res.category = MODEL_CAT_LORA;
        res.category_name = "LoRA (Adaptor Gaya / Akselerasi)";
        res.target_folder = "models\\LoRA";
    } else if (header_str.find("stable-diffusion") != std::string::npos ||
               header_str.find("flux") != std::string::npos ||
               header_str.find("sd15") != std::string::npos ||
               header_str.find("diffusion") != std::string::npos ||
               lower_fn.find("sd") != std::string::npos ||
               lower_fn.find("diff") != std::string::npos ||
               lower_fn.find("realistic") != std::string::npos ||
               lower_fn.find("cyberrealistic") != std::string::npos ||
               lower_fn.find("dreamshaper") != std::string::npos ||
               lower_fn.find("anime") != std::string::npos ||
               lower_fn.find("pony") != std::string::npos ||
               lower_fn.find("checkpoint") != std::string::npos ||
               res.is_safetensors) {
        res.category = MODEL_CAT_IMAGE_DIFFUSION;
        res.category_name = "Model Foto / Diffusion";
        res.target_folder = "models\\Gambar_Diffusion";
    } else {
        res.category = MODEL_CAT_TEXT_LLM;
        res.category_name = "Model Bahasa / Chat (LLM)";
        res.target_folder = "models\\Teks_LLM";
    }

    // Evaluasi Dependensi & Penjelasan Konsekuensi
    if (res.category == MODEL_CAT_IMAGE_DIFFUSION) {
        // Cek VAE
        if (GetFileAttributesA("models\\VAE\\taesd.safetensors") != INVALID_FILE_ATTRIBUTES) {
            res.dependencies_status.push_back("✅ Fast VAE (TAESD): Terpasang di models\\VAE\\taesd.safetensors.");
        } else {
            res.dependencies_status.push_back("⚠️ [Non-Kritis] Fast VAE (taesd.safetensors) belum ada di folder models\\VAE.");
            res.consequences.push_back("KONSEKUENSI VAE: Foto tetap bisa digenerasi normal menggunakan VAE standar bawaan model. Namun, tahap decoding akhir gambar di CPU memakan waktu ~1 menit per foto (jika ada TAESD, hanya ~2.7 detik).");
        }

        // Cek LCM
        if (lower_fn.find("lcm") != std::string::npos) {
            if (GetFileAttributesA("models\\LoRA\\lcm_lora_sd15.safetensors") != INVALID_FILE_ATTRIBUTES) {
                res.dependencies_status.push_back("✅ LoRA LCM: Terpasang untuk akselerasi 4-step kilat.");
            } else {
                res.dependencies_status.push_back("ℹ️ [Info] LoRA LCM belum terpasang.");
                res.consequences.push_back("KONSEKUENSI LCM: Model akan berjalan dengan jumlah langkah standar 20 steps (~20 detik di CPU), bukan 4 langkah.");
            }
        }

        // Cek Ukuran RAM
        if (res.file_size_mb > 4000) {
            res.dependencies_status.push_back("⚠️ [Hardware] Ukuran model di atas 4 GB.");
            res.consequences.push_back("KONSEKUENSI RAM: Membutuhkan RAM laptop sekitar 8-12 GB saat proses komputasi difusi. Disarankan menutup aplikasi berat lain saat rendering.");
        } else {
            res.dependencies_status.push_back("✅ Ukuran model sangat ideal untuk RAM prosesor laptop.");
        }

        res.recommendation = "Model difusi siap dipasang ke folder models\\Gambar_Diffusion. Otomatis muncul di menu Mode Foto & NSFW.";
    }
    else if (res.category == MODEL_CAT_TEXT_LLM) {
        // Cek Vision
        if (lower_fn.find("vl") != std::string::npos || lower_fn.find("vision") != std::string::npos) {
            bool has_mmproj = false;
            WIN32_FIND_DATAA fd_mm;
            HANDLE hMM = FindFirstFileA("models\\Multimodal_Vision\\*.gguf", &fd_mm);
            if (hMM != INVALID_HANDLE_VALUE) {
                has_mmproj = true;
                FindClose(hMM);
            }
            if (has_mmproj) {
                res.dependencies_status.push_back("✅ Proyektor Vision (mmproj): Ditemukan di models\\Multimodal_Vision.");
            } else {
                res.dependencies_status.push_back("⚠️ [Peringatan Fitur Gambar] Model memiliki arsitektur Vision, namun proyektor mmproj belum ada.");
                res.consequences.push_back("KONSEKUENSI VISION: Model HANYA BISA digunakan untuk obrolan teks dan koding biasa. Fitur membaca/menganalisa gambar tidak aktif sampai file mmproj ditambahkan.");
            }
        }

        if (res.file_size_mb > 6000) {
            res.dependencies_status.push_back("⚠️ [Hardware] Model berukuran besar (> 6 GB).");
            res.consequences.push_back("KONSEKUENSI UKURAN: Mengonsumsi RAM sekitar 6-8 GB. Di CPU laptop, kecepatan ketik AI akan berjalan santai (~6-12 token/detik).");
        } else {
            res.dependencies_status.push_back("✅ Ukuran model ringan & optimal (~1.2 - 4 GB), sangat responsif di CPU laptop.");
        }

        res.recommendation = "Model bahasa siap dipasang ke folder models\\Teks_LLM. Otomatis muncul di menu Mode Chat LLM & Auto Prompt.";
    }
    else if (res.category == MODEL_CAT_VAE) {
        res.dependencies_status.push_back("✅ Komponen VAE: Digunakan untuk mempercepat proses decoding gambar citra difusi.");
        res.recommendation = "File VAE akan otomatis disimpan ke folder models\\VAE.";
    }
    else if (res.category == MODEL_CAT_LORA) {
        res.dependencies_status.push_back("✅ Komponen LoRA: Digunakan untuk memodifikasi gaya seni atau akselerasi LCM.");
        res.recommendation = "File LoRA akan otomatis disimpan ke folder models\\LoRA.";
    }
    else {
        res.dependencies_status.push_back("ℹ️ Format GGUF umum yang kompatibel dengan mesin C++.");
        res.recommendation = "Model akan ditempatkan di folder models\\Teks_LLM untuk pengujian obrolan teks.";
    }

    return res;
}

// Modal & Worker State for Model Import & Deletion
static bool g_show_import_modal = false;
static bool g_is_importing = false;
static float g_import_progress = 0.0f;
static std::string g_import_status_text = "";
static ModelInspectionResult g_current_inspection;
static std::thread g_import_thread;

static bool g_show_delete_confirm_modal = false;
static std::string g_delete_target_path = "";
static std::string g_delete_target_name = "";

static void StartImportModelWorker(const ModelInspectionResult& insp) {
    g_is_importing = true;
    g_import_progress = 0.10f;
    g_import_status_text = "Mempersiapkan penyalinan ke " + insp.target_folder + "...";

    if (g_import_thread.joinable()) g_import_thread.join();
    g_import_thread = std::thread([insp]() {
        CreateDirectoryA("models", NULL);
        CreateDirectoryA(insp.target_folder.c_str(), NULL);

        std::string dest_path = insp.target_folder + "\\" + insp.filename;
        g_import_progress = 0.30f;
        g_import_status_text = "Menyalin file model (" + std::to_string(insp.file_size_mb) + " MB)...";

        BOOL ok = CopyFileA(insp.filepath.c_str(), dest_path.c_str(), FALSE);
        if (ok) {
            g_import_progress = 0.90f;
            g_import_status_text = "Memindai ulang registri model C++...";
            ScanModelsDirectory();

            if (insp.category == MODEL_CAT_IMAGE_DIFFUSION) {
                for (size_t i = 0; i < g_detected_photos.size(); i++) {
                    if (g_detected_photos[i].filename == insp.filename) {
                        g_selected_photo = (int)i;
                        break;
                    }
                }
            } else if (insp.category == MODEL_CAT_TEXT_LLM) {
                for (size_t i = 0; i < g_detected_llms.size(); i++) {
                    if (g_detected_llms[i].filename == insp.filename) {
                        g_selected_llm = (int)i;
                        break;
                    }
                }
            }

            g_import_progress = 1.0f;
            g_import_status_text = "Selesai! Model berhasil dipasang dan langsung aktif.";
        } else {
            g_import_progress = 1.0f;
            g_import_status_text = "Gagal menyalin file (pastikan ruang hardisk laptop mencukupi).";
        }
        g_is_importing = false;
    });
}

static void DeleteModelFile(const std::string& path_to_del) {
    if (GetFileAttributesA(path_to_del.c_str()) != INVALID_FILE_ATTRIBUTES) {
        DeleteFileA(path_to_del.c_str());
        ScanModelsDirectory();
    }
}

static void RenderModelImportModal() {
    if (!g_show_import_modal) return;

    ImGui::OpenPopup("📥 Pemasangan Model AI Baru (Auto-Deteksi & Diagnostik)");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(640, 0), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("📥 Pemasangan Model AI Baru (Auto-Deteksi & Diagnostik)", &g_show_import_modal, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(0.22f, 0.74f, 0.97f, 1.0f), "Hasil Analisis Arsitektur & Pemeriksaan Dependensi File");
        ImGui::Separator();
        ImGui::Spacing();

        // 1. Info File Dasar
        ImGui::Text("Nama File:");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "%s", g_current_inspection.filename.c_str());

        ImGui::Text("Ukuran File:");
        ImGui::SameLine();
        if (g_current_inspection.file_size_mb >= 1024) {
            ImGui::Text("%.2f GB (%d MB)", g_current_inspection.file_size_mb / 1024.0f, g_current_inspection.file_size_mb);
        } else {
            ImGui::Text("%d MB", g_current_inspection.file_size_mb);
        }

        ImGui::Text("Format Tensor:");
        ImGui::SameLine();
        if (g_current_inspection.is_gguf) {
            ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.4f, 1.0f), "GGUF Native (Kuantisasi CPU/AVX2)");
        } else if (g_current_inspection.is_safetensors) {
            ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Safetensors (HuggingFace FP16/FP8)");
        } else {
            ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "Format Tidak Dikenal");
        }

        ImGui::Text("Deteksi Kategori:");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.95f, 0.60f, 0.20f, 1.0f), "%s", g_current_inspection.category_name.c_str());

        ImGui::Text("Lokasi Pemasangan:");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 1.0f, 1.0f), "%s", g_current_inspection.target_folder.c_str());

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 2. Diagnostik & Kompatibilitas
        if (g_current_inspection.is_critical_error) {
            // KRITIKAL ERROR - Penolakan Impor
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.35f, 0.08f, 0.08f, 0.90f));
            ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.95f, 0.20f, 0.20f, 0.90f));
            ImGui::BeginChild("CriticalErrorBox", ImVec2(600, 130), true);
            ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.25f, 1.0f), "⛔ KESALAHAN KRITIKAL: MODEL DITOLAK UNTUK DI-IMPORT!");
            ImGui::Spacing();
            ImGui::TextWrapped("Alasan: %s", g_current_inspection.critical_reason.c_str());
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.75f, 1.0f), "Efek jika tetap dipaksakan:");
            for (const auto& c : g_current_inspection.consequences) {
                ImGui::BulletText("%s", c.c_str());
            }
            ImGui::EndChild();
            ImGui::PopStyleColor(2);
        } else {
            // NON-KRITIKAL / STATUS NORMAL
            ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.4f, 1.0f), "Pemeriksaan Dependensi & Fitur Pendukung:");
            for (const auto& st : g_current_inspection.dependencies_status) {
                ImGui::BulletText("%s", st.c_str());
            }

            if (!g_current_inspection.consequences.empty()) {
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.20f, 0.16f, 0.05f, 0.85f));
                ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.85f, 0.65f, 0.10f, 0.85f));
                ImGui::BeginChild("ConsequencesBox", ImVec2(600, 105), true);
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "ℹ️ Keterangan Pengaruh & Dampak (Non-Kritikal):");
                for (const auto& c : g_current_inspection.consequences) {
                    ImGui::TextWrapped("• %s", c.c_str());
                }
                ImGui::EndChild();
                ImGui::PopStyleColor(2);
            }

            if (!g_current_inspection.recommendation.empty()) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.3f, 0.85f, 1.0f, 1.0f), "💡 Rekomendasi: %s", g_current_inspection.recommendation.c_str());
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 3. Tombol Aksi / Indikator Progress
        if (g_is_importing) {
            ImGui::TextColored(ImVec4(0.22f, 0.74f, 0.97f, 1.0f), "%s", g_import_status_text.c_str());
            ImGui::ProgressBar(g_import_progress, ImVec2(600, 22), "");
            ImGui::Spacing();
            ImGui::TextDisabled("Mohon tunggu, file sedang disalin ke folder models secara asinkron...");
        } else {
            if (g_import_progress >= 1.0f && !g_import_status_text.empty()) {
                ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.4f, 1.0f), "%s", g_import_status_text.c_str());
                ImGui::Spacing();
                if (ImGui::Button("✔️ Selesai & Aktifkan Model", ImVec2(240, 36))) {
                    g_show_import_modal = false;
                    g_import_progress = 0.0f;
                    g_import_status_text = "";
                    ImGui::CloseCurrentPopup();
                }
            } else {
                if (!g_current_inspection.is_critical_error) {
                    if (ImGui::Button("📥 Pasang Model Sekarang", ImVec2(220, 36))) {
                        StartImportModelWorker(g_current_inspection);
                    }
                    ImGui::SameLine();
                }
                if (ImGui::Button(g_current_inspection.is_critical_error ? "Tutup" : "Batal", ImVec2(120, 36))) {
                    g_show_import_modal = false;
                    ImGui::CloseCurrentPopup();
                }
            }
        }

        ImGui::EndPopup();
    }
}

static void RenderDeleteConfirmModal() {
    if (!g_show_delete_confirm_modal) return;

    ImGui::OpenPopup("Konfirmasi Hapus Model AI");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500, 0), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Konfirmasi Hapus Model AI", &g_show_delete_confirm_modal, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.35f, 0.35f, 1.0f));
        ImGui::Text("⚠️ PERINGATAN: Menghapus Model dari Komputer!");
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Model yang akan dihapus:");
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "%s", g_delete_target_name.c_str());
        ImGui::Spacing();
        ImGui::TextDisabled("Lokasi: %s", g_delete_target_path.c_str());

        ImGui::Spacing();
        ImGui::TextWrapped("Setelah dihapus, file model akan langsung hilang secara otomatis dari daftar dropdown aplikasi ini.");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("🗑️ Ya, Hapus File Model", ImVec2(190, 34))) {
            DeleteModelFile(g_delete_target_path);
            g_show_delete_confirm_modal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Batal", ImVec2(110, 34))) {
            g_show_delete_confirm_modal = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

// ✨ AI Smart Prompt Expander (Mengubah ide singkat menjadi prompt masterpiece fotorealistis)
static std::string GenerateSmartPrompt(const std::string& input_idea, int num_threads, int framing_idx = 0) {
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

    // Penentuan sudut framing berdasarkan opsi manual atau deteksi prompt
    std::string framing_desc = "";
    if (framing_idx == 1) {
        // Opsi Manual: Wajah (Close-up)
        framing_desc = "extreme close-up portrait photograph, Canon EOS R5 85mm f/1.4 lens, tight framing on face, sharp facial focus, beautiful expressive eyes, micro skin pores, 8k uhd photorealistic";
    } else if (framing_idx == 2) {
        // Opsi Manual: Setengah Badan (Medium shot)
        framing_desc = "medium shot photograph, waist-up portrait, 50mm f/1.8 lens, natural posture, sharp focus on subject and face, detailed clothing and expression, 8k uhd photorealistic";
    } else if (framing_idx == 3) {
        // Opsi Manual: Seluruh Badan (Full Body)
        framing_desc = "wide-angle 35mm lens, full length body shot, standing pose, captured completely from head to toe with visible shoes and feet on the ground, full figure framing, high resolution full body photograph, (photorealistic detailed beautiful face:1.25), (sharp focused eyes, symmetrical iris:1.2), accurate human anatomy, proportionate limbs, realistic hands, 5 fingers on each hand";
    } else {
        // Opsi Manual: Auto (Menganalisis kata-kata dalam ide pengguna)
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

    std::string sys_prompt = "<|im_start|>system\\nYou are an expert prompt engineer for photorealistic Stable Diffusion models. Convert the user's short idea into a rich, detailed English prompt describing: subject appearance, beautiful natural face, realistic skin texture with visible micro pores, " + framing_desc + ", cinematic lighting (golden hour or studio rim light), composition, and 8k photorealistic quality. Output ONLY the raw prompt tokens separated by commas, in English. Do not add explanations, conversational text, or quotes.<|im_end|>\\n<|im_start|>user\\nConvert this idea into a photorealistic prompt: " + safe_idea + "<|im_end|>\\n<|im_start|>assistant\\n";

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

    // Ambil HANYA teks balasan asisten setelah tag <|im_start|>assistant
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

    // Hapus tanda kutip atau prefix yang tidak perlu
    while (!clean.empty() && (clean.front() == '\n' || clean.front() == '\r' || clean.front() == ' ' || clean.front() == '\t' || clean.front() == '"' || clean.front() == '>')) {
        clean.erase(clean.begin());
    }
    while (!clean.empty() && (clean.back() == '\n' || clean.back() == '\r' || clean.back() == ' ' || clean.back() == '\t' || clean.back() == '"')) {
        clean.pop_back();
    }

    if (clean.length() < 12) return input_idea;
    return clean;
}

static std::string CleanLlamaStreamOutput(const std::string& raw) {
    if (raw.empty()) return "";
    std::string text = raw;
    size_t p_pos = text.find("> ");
    if (p_pos != std::string::npos) {
        size_t nl_pos = text.find('\n', p_pos);
        text = (nl_pos != std::string::npos) ? text.substr(nl_pos + 1) : text.substr(p_pos + 2);
    } else {
        size_t cmd_pos = text.find("using globbing pattern");
        if (cmd_pos != std::string::npos) {
            text = text.substr(cmd_pos + 22);
        }
    }
    size_t stats_pos = text.find("[ Prompt:");
    if (stats_pos != std::string::npos) text = text.substr(0, stats_pos);
    size_t exit_pos = text.find("Exiting...");
    if (exit_pos != std::string::npos) text = text.substr(0, exit_pos);

    while (!text.empty() && (text.front() == '\n' || text.front() == '\r')) {
        text.erase(text.begin());
    }
    return text;
}

void RunInferenceWorker(std::string prompt, int mode, int model_idx, int photo_idx, int num_threads, int steps, float cfg, std::string init_img_path = "", float img_strength = 0.65f, bool use_controlnet = false, float cnet_strength = 0.90f) {
    g_process_progress.store(0.05f);
    {
        std::lock_guard<std::mutex> lock(g_status_mutex);
        if (!init_img_path.empty()) {
            g_process_status = use_controlnet ? "Mengekstrak Garis Pose & Inisialisasi ControlNet Canny..." : "Mempersiapkan Edit Foto (Image-to-Image)...";
        } else {
            g_process_status = "Inisialisasi Pure C++ Engine...";
        }
    }

    // 1. Deteksi Mode Otomatis jika mode == 0
    int actual_mode = mode;
    if (prompt.find("[DOKUMEN LAMPIRAN:") != std::string::npos) {
        actual_mode = 1; // Analisis dokumen wajib diarahkan ke Chat LLM
    } else if (!init_img_path.empty()) {
        if (actual_mode == 0 || actual_mode == 1) {
            actual_mode = 2; // Jika ada foto asal, otomatis beralih ke mode Foto / Edit
        }
    } else if (actual_mode == 0) {
        std::string lower_p = prompt;
        for (char& ch : lower_p) ch = (char)tolower(ch);
        if (lower_p.find("nsfw") != std::string::npos ||
            lower_p.find("nude") != std::string::npos ||
            lower_p.find("telanjang") != std::string::npos ||
            lower_p.find("dewasa") != std::string::npos ||
            lower_p.find("sensual") != std::string::npos ||
            lower_p.find("seksi") != std::string::npos ||
            lower_p.find("sexy") != std::string::npos ||
            lower_p.find("bugil") != std::string::npos) {
            actual_mode = 3; // Auto NSFW / Dewasa
        } else if (lower_p.find("gambar") != std::string::npos ||
            lower_p.find("foto") != std::string::npos ||
            lower_p.find("lukis") != std::string::npos ||
            lower_p.find("image") != std::string::npos ||
            lower_p.find("draw") != std::string::npos ||
            lower_p.find("photo") != std::string::npos) {
            actual_mode = 2; // Auto Photo
        } else {
            actual_mode = 1; // Auto Chat
        }
    }

    bool is_photo = false;
    if (prompt.find("[DOKUMEN LAMPIRAN:") != std::string::npos) {
        is_photo = false;
    } else if (actual_mode == 2) {
        is_photo = true;
    } else if (actual_mode == 3) {
        std::string lower_p = prompt;
        for (char& ch : lower_p) ch = (char)tolower(ch);
        if (lower_p.find("cerita") != std::string::npos ||
            lower_p.find("chat") != std::string::npos ||
            lower_p.find("tulis") != std::string::npos ||
            lower_p.find("kisah") != std::string::npos ||
            lower_p.find("roleplay") != std::string::npos ||
            !g_nsfw_mode_is_photo) {
            is_photo = false;
        } else {
            is_photo = true;
        }
    }

    std::string original_idea = prompt;
    std::string smart_prompt = prompt;

    // 2. AI Smart Prompt Optimizer (Jika Auto Prompt Pintar Aktif)
    if (is_photo && g_smart_prompt_auto) {
        std::string lower_p = prompt;
        for (char& ch : lower_p) ch = (char)tolower(ch);
        bool needs_expansion = false;
        if (lower_p.find("photorealistic") == std::string::npos &&
            lower_p.find("8k") == std::string::npos &&
            lower_p.find("canon") == std::string::npos &&
            prompt.length() < 180) {
            needs_expansion = true;
        }

        if (needs_expansion) {
            g_process_progress.store(0.10f);
            {
                std::lock_guard<std::mutex> lock(g_status_mutex);
                g_process_status = "✨ Mengoptimalkan ide menjadi Prompt Pintar (AI LLM)...";
            }
            std::string enriched = GenerateSmartPrompt(prompt, num_threads, g_framing_idx);
            if (!enriched.empty() && enriched != prompt) {
                smart_prompt = enriched;
            }
        }
    }

    // Enhance prompt dengan keywords realistik jika mode NSFW / Foto aktif (Detail Wajah & Mata 8K)
    std::string final_prompt = smart_prompt;
    if (is_photo && (actual_mode == 3 || actual_mode == 2) && g_nsfw_quality_booster) {
        std::string lower_p = final_prompt;
        for (char& ch : lower_p) ch = (char)tolower(ch);
        if (lower_p.find("photorealistic") == std::string::npos && 
            lower_p.find("realistic") == std::string::npos) {
            final_prompt += ", ultra realistic, 8k uhd, photorealistic, natural skin texture, realistic detailed face, beautiful expressive eyes, micro skin pores, masterpiece, raw candid photo";
        }
    }

    // Terapkan framing eksplisit jika opsi manual dipilih oleh pengguna
    if (is_photo && (actual_mode == 3 || actual_mode == 2)) {
        std::string lower_p = final_prompt;
        for (char& ch : lower_p) ch = (char)tolower(ch);
        if (g_framing_idx == 3) {
            if (lower_p.find("full body") == std::string::npos &&
                lower_p.find("full length") == std::string::npos &&
                lower_p.find("head to toe") == std::string::npos) {
                final_prompt += ", wide-angle 35mm lens, full length body shot, head to toe, visible shoes on ground, standing pose, full figure framing";
            }
        } else if (g_framing_idx == 1) {
            if (lower_p.find("close-up") == std::string::npos &&
                lower_p.find("portrait") == std::string::npos) {
                final_prompt += ", extreme close-up portrait, Canon EOS R5 85mm f/1.4 lens, sharp facial focus";
            }
        } else if (g_framing_idx == 2) {
            if (lower_p.find("medium shot") == std::string::npos &&
                lower_p.find("waist-up") == std::string::npos) {
                final_prompt += ", medium shot photograph, waist-up portrait, 50mm f/1.8 lens";
            }
        }
    }

    // HD Wajah & Anatomi: Injeksi token penajam berbobot tinggi untuk mencegah muka buram dan anatomi cacat pada foto Full Body
    bool is_fullbody_shot = false;
    if (is_photo && (actual_mode == 3 || actual_mode == 2)) {
        std::string lower_p = final_prompt;
        for (char& ch : lower_p) ch = (char)tolower(ch);
        if (g_framing_idx == 3 ||
            lower_p.find("full body") != std::string::npos ||
            lower_p.find("fullbody") != std::string::npos ||
            lower_p.find("full length") != std::string::npos ||
            lower_p.find("head to toe") != std::string::npos ||
            lower_p.find("seluruh badan") != std::string::npos ||
            lower_p.find("standing pose") != std::string::npos) {
            is_fullbody_shot = true;
        }

        if (is_fullbody_shot && g_fullbody_face_fix) {
            if (lower_p.find("detailed beautiful face") == std::string::npos &&
                lower_p.find("symmetrical iris") == std::string::npos) {
                final_prompt += ", (photorealistic detailed beautiful face:1.25), (sharp focused clear eyes, symmetrical facial features, detailed iris, sharp pupils:1.25), natural lips, highly detailed skin texture, accurate body anatomy, natural limbs, perfect hands, 5 fingers on each hand";
            }
        }
    }

    // Escape prompt untuk command line
    std::string safe_prompt = "";
    for (char c : final_prompt) {
        if (c == '"') safe_prompt += "\\\"";
        else if (c == '\n' || c == '\r') safe_prompt += " ";
        else safe_prompt += c;
    }

    char cmd[4096] = {0};
    std::string out_img_path = "";
    int out_w = 512;
    int out_h = 512;

    int actual_steps = (steps > 0) ? steps : 20;

    if (is_photo) {
        // Mode Foto & NSFW: PURE C++ SD-CLI
        std::string model_path = "models\\Gambar_Diffusion\\CyberRealistic_V5_FP16.safetensors";
        if (photo_idx >= 0 && photo_idx < (int)g_detected_photos.size()) {
            model_path = g_detected_photos[photo_idx].full_path;
        } else if (actual_mode == 3) {
            // Prioritas #1 Paling Realistis: CyberRealistic V5.0
            if (GetFileAttributesA("models\\Gambar_Diffusion\\CyberRealistic_V5_FP16.safetensors") != INVALID_FILE_ATTRIBUTES) {
                model_path = "models\\Gambar_Diffusion\\CyberRealistic_V5_FP16.safetensors";
            } else {
                model_path = "models\\Gambar_Diffusion\\Realistic_Vision_V5.1_fp16-no-ema.safetensors";
            }
        }

        // Cek jika file model ada, jika tidak fallback ke model yang tersedia
        if (GetFileAttributesA(model_path.c_str()) == INVALID_FILE_ATTRIBUTES) {
            model_path = "models\\Gambar_Diffusion\\CyberRealistic_V5_FP16.safetensors";
            if (GetFileAttributesA(model_path.c_str()) == INVALID_FILE_ATTRIBUTES) {
                model_path = "models\\Gambar_Diffusion\\Realistic_Vision_V5.1_fp16-no-ema.safetensors";
                if (GetFileAttributesA(model_path.c_str()) == INVALID_FILE_ATTRIBUTES && !g_detected_photos.empty()) {
                    model_path = g_detected_photos[0].full_path;
                }
            }
        }

        CreateDirectoryA("outputs", NULL);
        CreateDirectoryA("outputs\\foto", NULL);
        
        char timestamp_buf[64];
        time_t t_now = time(NULL);
        snprintf(timestamp_buf, sizeof(timestamp_buf), "%lld", (long long)t_now);
        out_img_path = std::string("outputs\\foto\\img_") + timestamp_buf + ".png";

        // Resolusi sepenuhnya mematuhi pilihan pengguna (TIDAK ADA override otomatis diam-diam)
        if (g_resolution_idx == 0) { out_w = 512; out_h = 512; }
        else if (g_resolution_idx == 1) { out_w = 512; out_h = 768; }
        else if (g_resolution_idx == 2) { out_w = 768; out_h = 512; }

        actual_steps = (steps > 0) ? steps : 20;
        if (!g_turbo_mode && model_path.find("LCM") == std::string::npos && model_path.find("lcm") == std::string::npos) {
            if (is_fullbody_shot && g_fullbody_face_fix && actual_steps < 25) {
                actual_steps = 25; // Optimalkan steps ke 25 agar UNet memiliki iterasi cukup untuk mendetailkan wajah & jari pada shot full body
            }
        }
        float actual_cfg = cfg;
        std::string lora_arg = "";

        if (g_turbo_mode || model_path.find("LCM") != std::string::npos || model_path.find("lcm") != std::string::npos) {
            actual_steps = 4;
            actual_cfg = 1.8f;
            // Jika model standar SD 1.5 dijalankan di mode Turbo, lampirkan LoRA LCM agar proses 4-step tajam
            if (model_path.find("LCM") == std::string::npos && model_path.find("lcm") == std::string::npos && GetFileAttributesA("models\\LoRA\\lcm_lora_sd15.safetensors") != INVALID_FILE_ATTRIBUTES) {
                lora_arg = "--lora \"models\\LoRA\\lcm_lora_sd15.safetensors\"";
            }
        }

        int seed_val = g_random_seed ? -1 : g_custom_seed;
        const char* sampler_flag = (g_sampling_method_idx >= 0 && g_sampling_method_idx < 4) ?
                                   g_sampling_method_flags[g_sampling_method_idx] : "euler_a";

        std::string safe_neg = "";
        std::string neg_src = g_negative_prompt;
        if (is_photo && is_fullbody_shot) {
            if (neg_src.find("cropped") == std::string::npos) {
                neg_src += ", cropped, out of frame, cut off head, cut off feet, missing feet";
            }
            if (g_fullbody_face_fix) {
                neg_src += ", blurry face, out of focus face, low-res face, deformed eyes, crossed eyes, bad eyes, poorly drawn face, poorly drawn eyes, disfigured face, mutated face, melted face, bad anatomy, deformed anatomy, extra limbs, missing limbs, floating limbs, malformed limbs, disconnected limbs, mutated hands, bad hands, missing fingers, extra fingers, fused fingers, too many fingers, poorly drawn hands, long neck, duplicate";
            }
        }
        for (int i = 0; neg_src[i] != '\0'; i++) {
            char c = neg_src[i];
            if (c == '"') safe_neg += "\\\"";
            else if (c == '\n' || c == '\r') safe_neg += " ";
            else safe_neg += c;
        }

        std::string controlnet_arg = "";
        std::string img2img_arg = "";
        if (!init_img_path.empty() && GetFileAttributesA(init_img_path.c_str()) != INVALID_FILE_ATTRIBUTES) {
            std::string cnet_model = "models\\ControlNet\\control_sd15_canny.safetensors";
            if (use_controlnet && GetFileAttributesA(cnet_model.c_str()) != INVALID_FILE_ATTRIBUTES) {
                char cbuf[700];
                snprintf(cbuf, sizeof(cbuf), "--control-net \"%s\" --control-image \"%s\" --image-preprocess target=control,canny=true --control-strength %.2f",
                    cnet_model.c_str(), init_img_path.c_str(), cnet_strength);
                controlnet_arg = cbuf;
            } else {
                char buf[600];
                snprintf(buf, sizeof(buf), "-i \"%s\" --strength %.2f", init_img_path.c_str(), img_strength);
                img2img_arg = buf;
            }
        }

        bool is_rtx_active = (g_hardware_backend_mode == 2 || (g_hardware_backend_mode == 0 && g_hw_profile.has_nvidia_gpu));
        std::string backend_arg = "";
        if (is_rtx_active) {
            backend_arg = "--backend cuda0 --auto-fit on";
        }

        bool is_dit = (model_path.find("Sakura") != std::string::npos ||
                       model_path.find("Iris") != std::string::npos ||
                       model_path.find("qwen-image") != std::string::npos ||
                       model_path.find("DiT") != std::string::npos);

        if (is_dit) {
            std::string text_enc = "models\\Multimodal_Vision\\Sakura-TextEncoder-Qwen3VL-4B-Uncensored-Q4_K_M-2.33GiB.gguf";
            if (GetFileAttributesA(text_enc.c_str()) == INVALID_FILE_ATTRIBUTES) {
                text_enc = "models\\Multimodal_Vision\\mmproj-Qwen-Image-2.1-Turbo-Uncensored-F16.gguf";
            }
            std::string vae_path = "models\\VAE\\qwen_image_2.1_vae_bf16.safetensors";

            snprintf(cmd, sizeof(cmd),
                "sd_bin\\sd-cli.exe --diffusion-model \"%s\" --llm \"%s\" --vae \"%s\" %s %s %s -p \"%s\" --steps %d -W %d -H %d -t %d --cfg-scale %.1f -s %d -o \"%s\"",
                model_path.c_str(), text_enc.c_str(), vae_path.c_str(), backend_arg.c_str(), controlnet_arg.c_str(), img2img_arg.c_str(),
                safe_prompt.c_str(), actual_steps, out_w, out_h, num_threads, actual_cfg, seed_val, out_img_path.c_str()
            );
        } else {
            std::string taesd_arg = "";
            if (g_use_fast_vae && GetFileAttributesA("models\\VAE\\taesd.safetensors") != INVALID_FILE_ATTRIBUTES) {
                taesd_arg = "--taesd \"models\\VAE\\taesd.safetensors\"";
            }

            snprintf(cmd, sizeof(cmd),
                "sd_bin\\sd-cli.exe -m \"%s\" %s %s %s %s %s -p \"%s\" --negative-prompt \"%s\" --steps %d -W %d -H %d -t %d --cfg-scale %.1f -s %d --sampling-method %s -o \"%s\"",
                model_path.c_str(), backend_arg.c_str(), taesd_arg.c_str(), lora_arg.c_str(), controlnet_arg.c_str(), img2img_arg.c_str(), safe_prompt.c_str(), safe_neg.c_str(), actual_steps, out_w, out_h, num_threads, actual_cfg, seed_val, sampler_flag, out_img_path.c_str()
            );
        }
    } else {
        // Mode Chat LLM: PURE C++ LLAMA-CLI
        std::string model_path = "models\\Teks_LLM\\qwen2.5-coder-1.5b-instruct-q4_k_m.gguf";
        if (actual_mode == 3 && model_idx == 0) {
            // Mode NSFW: prioritaskan model uncensored jika ada
            for (const auto& dm : g_detected_llms) {
                std::string lf = dm.filename;
                for (char& ch : lf) ch = (char)tolower(ch);
                if (lf.find("uncensored") != std::string::npos || lf.find("qwen3.8") != std::string::npos) {
                    model_path = dm.full_path;
                    break;
                }
            }
        } else if (model_idx >= 0 && model_idx < (int)g_detected_llms.size()) {
            model_path = g_detected_llms[model_idx].full_path;
        }

        if (GetFileAttributesA(model_path.c_str()) == INVALID_FILE_ATTRIBUTES && !g_detected_llms.empty()) {
            model_path = g_detected_llms[0].full_path;
        }

        // Simpan prompt ke file sementara agar bebas dari batasan panjang baris perintah Windows (CreateProcess 8KB limit)
        std::string prompt_file_path = "llama_prompt.txt";
        std::ofstream pfile(prompt_file_path, std::ios::binary);
        if (pfile.is_open()) {
            pfile.write(prompt.data(), prompt.size());
            pfile.close();
        }

        bool is_rtx_active = (g_hardware_backend_mode == 2 || (g_hardware_backend_mode == 0 && g_hw_profile.has_nvidia_gpu));
        int actual_gpu_layers = is_rtx_active ? (g_gpu_layers > 0 ? g_gpu_layers : 99) : 0;

        if (actual_gpu_layers > 0) {
            snprintf(cmd, sizeof(cmd),
                "llama_bin\\llama-cli.exe -m \"%s\" -f \"%s\" -ngl %d -c 4096 -n 512 -t %d --single-turn --no-display-prompt --log-disable --simple-io",
                model_path.c_str(), prompt_file_path.c_str(), actual_gpu_layers, num_threads
            );
        } else {
            snprintf(cmd, sizeof(cmd),
                "llama_bin\\llama-cli.exe -m \"%s\" -f \"%s\" -c 4096 -n 512 -t %d --single-turn --no-display-prompt --log-disable --simple-io",
                model_path.c_str(), prompt_file_path.c_str(), num_threads
            );
        }
    }

    // Eksekusi PURE C++ Native via Anonymous Pipe
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE hReadPipe = NULL;
    HANDLE hWritePipe = NULL;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        g_worker_result = "Gagal membuat pipe native C++.";
        g_worker_finished = true;
        return;
    }
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

    if (g_logf) { fprintf(g_logf, "[PURE C++ EXEC]: %s\n", cmd); fflush(g_logf); }

    if (CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hWritePipe);
        hWritePipe = NULL;

        std::string full_output = "";
        char read_buf[512];
        DWORD bytes_read = 0;
        int total_steps = actual_steps;
        auto start_time = std::chrono::high_resolution_clock::now();

        while (ReadFile(hReadPipe, read_buf, sizeof(read_buf) - 1, &bytes_read, NULL) && bytes_read > 0) {
            read_buf[bytes_read] = '\0';
            full_output += read_buf;

            if (is_photo) {
                // Parse step progress: pastikan hanya saat denoising sampling, bukan "generating image: 1/1"
                if (strstr(read_buf, "s/it") || strstr(read_buf, "it/s") || strstr(read_buf, "sampling")) {
                    char* match = strstr(read_buf, "/");
                    if (match && match > read_buf) {
                        char* step_ptr = match - 1;
                        while (step_ptr > read_buf && (*step_ptr >= '0' && *step_ptr <= '9')) {
                            step_ptr--;
                        }
                        if (*step_ptr < '0' || *step_ptr > '9') step_ptr++;
                        
                        int s = atoi(step_ptr);
                        int tot = atoi(match + 1);
                        if (s > 0 && tot > 0 && tot == actual_steps) {
                            total_steps = tot;
                            float pct = (float)s / (float)tot;
                            g_process_progress.store(0.15f + pct * 0.75f);
                            
                            char status_msg[128];
                            snprintf(status_msg, sizeof(status_msg), "Step %d/%d (%.0f%%) - Melakukan Denoising...", s, tot, pct * 100.0f);
                            std::lock_guard<std::mutex> lock(g_status_mutex);
                            g_process_status = status_msg;
                        }
                    }
                }
                if (strstr(read_buf, "decoding") || strstr(read_buf, "decode_first_stage")) {
                    g_process_progress.store(0.95f);
                    std::lock_guard<std::mutex> lock(g_status_mutex);
                    g_process_status = "Decoding Latents ke Gambar Citra Asli...";
                }
            } else {
                float p = g_process_progress.load() + 0.04f;
                if (p > 0.94f) p = 0.94f;
                g_process_progress.store(p);

                std::string cleaned_live = CleanLlamaStreamOutput(full_output);
                {
                    std::lock_guard<std::mutex> lock(g_stream_mutex);
                    g_live_stream_text = cleaned_live;
                }

                std::lock_guard<std::mutex> lock(g_status_mutex);
                g_process_status = "Menulis jawaban teks AI...";
            }
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hReadPipe);

        if (is_photo) {
            if (GetFileAttributesA(out_img_path.c_str()) != INVALID_FILE_ATTRIBUTES) {
                g_worker_img_path = out_img_path;
                g_process_progress.store(1.0f);
                {
                    std::lock_guard<std::mutex> lock(g_status_mutex);
                    g_process_status = "Selesai 100%!";
                }

                auto end_time = std::chrono::high_resolution_clock::now();
                double elapsed_sec = std::chrono::duration<double>(end_time - start_time).count();
                char elapsed_str[32];
                snprintf(elapsed_str, sizeof(elapsed_str), "%.1fs", elapsed_sec);

                const char* used_mname = strstr(cmd, "CyberRealistic") ? "CyberRealistic V5.0 (Paling Nyata & Realistis)" :
                                         strstr(cmd, "Realistic_Vision") ? "Realistic Vision V5.1 (Fotorealistis Human)" :
                                         strstr(cmd, "DreamShaper") ? "DreamShaper 8 LCM (Kilat)" : "Sakura DiT";
                
                std::string prompt_info = "";
                if (original_idea != final_prompt) {
                    prompt_info = "Ide Awal: \"" + original_idea + "\"\n"
                                  "Prompt Pintar AI: \"" + final_prompt + "\"\n\n";
                } else {
                    prompt_info = "Prompt: \"" + final_prompt + "\"\n\n";
                }

                g_worker_result = "[Foto Paling Realistis Berhasil Dibuat (100% C++ Native Engine)]\n\n"
                                  "Model: " + std::string(used_mname) + "\n"
                                  "Resolusi: " + std::to_string(out_w) + " x " + std::to_string(out_h) + " px | Steps: " + std::to_string(actual_steps) + " langkah | Waktu: " + elapsed_str + "\n"
                                  "Lokasi File: " + out_img_path + "\n\n"
                                  + prompt_info +
                                  "Gunakan tombol di bawah untuk langsung membuka atau melihat foto.";
            } else {
                g_worker_img_path = "";
                g_worker_result = "Gagal menyimpan foto. Log engine:\n" + full_output.substr(0, 500);
            }
        } else {
            std::string clean_resp = "";
            size_t p_pos = full_output.find("> ");
            if (p_pos != std::string::npos) {
                size_t nl_pos = full_output.find('\n', p_pos);
                if (nl_pos != std::string::npos) {
                    clean_resp = full_output.substr(nl_pos + 1);
                } else {
                    clean_resp = full_output.substr(p_pos + 2);
                }
            } else {
                size_t cmd_pos = full_output.find("using globbing pattern");
                if (cmd_pos != std::string::npos) {
                    clean_resp = full_output.substr(cmd_pos + 22);
                } else {
                    clean_resp = full_output;
                }
            }

            size_t stats_pos = clean_resp.find("[ Prompt:");
            if (stats_pos != std::string::npos) {
                clean_resp = clean_resp.substr(0, stats_pos);
            }
            size_t exit_pos = clean_resp.find("Exiting...");
            if (exit_pos != std::string::npos) {
                clean_resp = clean_resp.substr(0, exit_pos);
            }

            while (!clean_resp.empty() && (clean_resp.front() == '\n' || clean_resp.front() == '\r' || clean_resp.front() == ' ' || clean_resp.front() == '\t')) {
                clean_resp.erase(clean_resp.begin());
            }
            while (!clean_resp.empty() && (clean_resp.back() == '\n' || clean_resp.back() == '\r' || clean_resp.back() == ' ' || clean_resp.back() == '\t')) {
                clean_resp.pop_back();
            }

            if (clean_resp.empty()) {
                clean_resp = full_output;
            }
            g_worker_result = clean_resp;
            g_process_progress.store(1.0f);
            {
                std::lock_guard<std::mutex> lock(g_status_mutex);
                g_process_status = "Selesai 100%!";
            }
        }
    } else {
        if (hWritePipe) CloseHandle(hWritePipe);
        CloseHandle(hReadPipe);
        g_worker_result = "Gagal menjalankan executable C++ native: " + std::string(cmd);
    }

    g_worker_finished = true;
}

static const char* g_mode_names[] = { "Otomatis", "Chat Teks", "Buat Foto", "NSFW / Dewasa", "Buat Video", "🗣️ Suara AI" };

static std::string GetCurrentTimestamp() {
    time_t rawtime;
    struct tm* timeinfo;
    char buffer[16];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    if (timeinfo) {
        strftime(buffer, sizeof(buffer), "%H:%M", timeinfo);
        return std::string(buffer);
    }
    return "12:00";
}

// ================= SISTEM PENYIMPANAN PERCAKAPAN PERMANEN (PERSISTENT CHAT HISTORY) =================
struct ChatSession {
    std::string id;
    std::string title;
    std::string last_time;
    std::vector<ChatMsg> messages;
};

static std::vector<ChatSession> g_sessions;
static int g_active_session_idx = 0;

static void SaveSessionToFile(const ChatSession& session) {
    CreateDirectoryA("history", NULL);
    std::string filepath = "history\\" + session.id + ".chat";
    FILE* fp = fopen(filepath.c_str(), "w");
    if (!fp) return;

    for (const auto& msg : session.messages) {
        fprintf(fp, "[MSG]\n");
        fprintf(fp, "ROLE:%s\n", msg.role.c_str());
        fprintf(fp, "TIME:%s\n", msg.timestamp.c_str());
        fprintf(fp, "TAG:%s\n", msg.tag.c_str());
        fprintf(fp, "IMG:%s\n", msg.image_path.c_str());
        fprintf(fp, "TEXT:\n%s\n[END_TEXT]\n", msg.text.c_str());
        fprintf(fp, "[END_MSG]\n");
    }
    fclose(fp);
}

static void SaveSessionsIndex() {
    CreateDirectoryA("history", NULL);
    FILE* fp = fopen("history\\sessions_index.txt", "w");
    if (!fp) return;

    for (const auto& s : g_sessions) {
        fprintf(fp, "%s|%s|%s\n", s.id.c_str(), s.title.c_str(), s.last_time.c_str());
    }
    fclose(fp);
}

static void LoadSessionsFromDisk(ID3D11Device* pDevice) {
    g_sessions.clear();
    CreateDirectoryA("history", NULL);

    FILE* fp = fopen("history\\sessions_index.txt", "r");
    if (fp) {
        char line[1024];
        while (fgets(line, sizeof(line), fp)) {
            std::string sline = line;
            while (!sline.empty() && (sline.back() == '\n' || sline.back() == '\r')) sline.pop_back();
            if (sline.empty()) continue;

            size_t p1 = sline.find('|');
            size_t p2 = (p1 != std::string::npos) ? sline.find('|', p1 + 1) : std::string::npos;
            if (p1 != std::string::npos) {
                ChatSession sess;
                sess.id = sline.substr(0, p1);
                if (p2 != std::string::npos) {
                    sess.title = sline.substr(p1 + 1, p2 - (p1 + 1));
                    sess.last_time = sline.substr(p2 + 1);
                } else {
                    sess.title = sline.substr(p1 + 1);
                    sess.last_time = "Baru";
                }

                std::string chatfile = "history\\" + sess.id + ".chat";
                FILE* cfp = fopen(chatfile.c_str(), "r");
                if (cfp) {
                    char cline[2048];
                    ChatMsg curMsg;
                    bool in_msg = false;
                    bool in_text = false;
                    std::string text_accum = "";

                    while (fgets(cline, sizeof(cline), cfp)) {
                        std::string cl = cline;
                        while (!cl.empty() && (cl.back() == '\n' || cl.back() == '\r')) cl.pop_back();

                        if (cl == "[MSG]") {
                            in_msg = true;
                            in_text = false;
                            curMsg = ChatMsg();
                            text_accum = "";
                        } else if (cl == "[END_MSG]") {
                            if (in_msg) {
                                curMsg.text = text_accum;
                                if (!curMsg.image_path.empty() && pDevice != NULL) {
                                    if (GetFileAttributesA(curMsg.image_path.c_str()) != INVALID_FILE_ATTRIBUTES) {
                                        curMsg.texture_view = LoadTextureFromFileDX11(pDevice, curMsg.image_path.c_str(), &curMsg.img_w, &curMsg.img_h);
                                    }
                                }
                                sess.messages.push_back(curMsg);
                            }
                            in_msg = false;
                            in_text = false;
                        } else if (in_msg) {
                            if (cl == "TEXT:") {
                                in_text = true;
                                text_accum = "";
                            } else if (cl == "[END_TEXT]") {
                                in_text = false;
                            } else if (in_text) {
                                if (!text_accum.empty()) text_accum += "\n";
                                text_accum += cl;
                            } else if (cl.rfind("ROLE:", 0) == 0) {
                                curMsg.role = cl.substr(5);
                            } else if (cl.rfind("TIME:", 0) == 0) {
                                curMsg.timestamp = cl.substr(5);
                            } else if (cl.rfind("TAG:", 0) == 0) {
                                curMsg.tag = cl.substr(4);
                            } else if (cl.rfind("IMG:", 0) == 0) {
                                curMsg.image_path = cl.substr(4);
                            }
                        }
                    }
                    fclose(cfp);
                }
                g_sessions.push_back(sess);
            }
        }
        fclose(fp);
    }

    if (g_sessions.empty()) {
        ChatSession def;
        char id_buf[64];
        snprintf(id_buf, sizeof(id_buf), "session_%lld", (long long)time(NULL));
        def.id = id_buf;
        def.title = "Percakapan Baru";
        def.last_time = GetCurrentTimestamp();

        ChatMsg welcome;
        welcome.role = "AI Studio (100% Pure C++)";
        welcome.text = "Selamat datang di AI Studio versi 100% Pure Native C++ (Direct3D 11).\n"
                       "- Eksekusi Chat: llama-cli.exe (Pure C++ / AVX2 Standalone)\n"
                       "- Eksekusi Foto Realistis: sd-cli.exe dengan CyberRealistic V5.0 & Realistic Vision V5.1\n"
                       "- Menu NSFW & Edit Foto: Tersedia langsung di layar utama\n"
                       "- Riwayat Percakapan: Otomatis tersimpan permanen di laptop Anda.";
        welcome.timestamp = GetCurrentTimestamp();
        welcome.tag = "Pure C++";
        welcome.image_path = "";
        def.messages.push_back(welcome);

        g_sessions.push_back(def);
        SaveSessionToFile(def);
        SaveSessionsIndex();
    }

    g_active_session_idx = 0;
}

static void CreateNewSession() {
    ChatSession sess;
    char id_buf[64];
    snprintf(id_buf, sizeof(id_buf), "session_%lld", (long long)time(NULL));
    sess.id = id_buf;
    char title_buf[64];
    snprintf(title_buf, sizeof(title_buf), "Percakapan #%d", (int)g_sessions.size() + 1);
    sess.title = title_buf;
    sess.last_time = GetCurrentTimestamp();

    ChatMsg welcome;
    welcome.role = "AI Studio";
    welcome.text = "Sesi percakapan baru dimulai. Silakan ketik prompt atau masukkan foto untuk mulai berkreasi!";
    welcome.timestamp = sess.last_time;
    welcome.tag = "Pure C++";
    sess.messages.push_back(welcome);

    g_sessions.push_back(sess);
    g_active_session_idx = (int)g_sessions.size() - 1;
    SaveSessionToFile(sess);
    SaveSessionsIndex();
}

static void DeleteSession(int idx) {
    if (idx < 0 || idx >= (int)g_sessions.size()) return;
    for (auto& msg : g_sessions[idx].messages) {
        if (msg.texture_view) {
            msg.texture_view->Release();
            msg.texture_view = NULL;
        }
    }
    std::string chatfile = "history\\" + g_sessions[idx].id + ".chat";
    DeleteFileA(chatfile.c_str());

    g_sessions.erase(g_sessions.begin() + idx);
    if (g_sessions.empty()) {
        CreateNewSession();
    } else {
        if (g_active_session_idx >= (int)g_sessions.size()) {
            g_active_session_idx = (int)g_sessions.size() - 1;
        }
        SaveSessionsIndex();
    }
}

void ApplyModernDarkTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding    = 10.0f;
    style.FrameRounding     = 6.0f;
    style.PopupRounding     = 8.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabRounding      = 6.0f;
    style.TabRounding       = 6.0f;
    style.FramePadding      = ImVec2(12, 8); // Touch-friendly padding
    style.ItemSpacing       = ImVec2(10, 10);
    style.TouchExtraPadding = ImVec2(6, 6);  // Touchscreen Friendly!

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg]             = ImVec4(0.043f, 0.059f, 0.098f, 1.00f); // #0b0f19
    colors[ImGuiCol_ChildBg]              = ImVec4(0.059f, 0.090f, 0.165f, 1.00f); // #0f172a
    colors[ImGuiCol_PopupBg]              = ImVec4(0.075f, 0.114f, 0.180f, 1.00f); // #131d2e
    colors[ImGuiCol_Border]               = ImVec4(0.200f, 0.255f, 0.333f, 0.50f); // #334155
    colors[ImGuiCol_FrameBg]              = ImVec4(0.118f, 0.161f, 0.231f, 1.00f); // #1e293b
    colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.200f, 0.255f, 0.333f, 1.00f);
    colors[ImGuiCol_FrameBgActive]        = ImVec4(0.145f, 0.388f, 0.922f, 0.60f);
    colors[ImGuiCol_TitleBg]              = ImVec4(0.059f, 0.090f, 0.165f, 1.00f);
    colors[ImGuiCol_TitleBgActive]        = ImVec4(0.145f, 0.388f, 0.922f, 0.90f);
    colors[ImGuiCol_Button]               = ImVec4(0.145f, 0.388f, 0.922f, 1.00f); // #2563eb
    colors[ImGuiCol_ButtonHovered]        = ImVec4(0.114f, 0.306f, 0.847f, 1.00f); // #1d4ed8
    colors[ImGuiCol_ButtonActive]         = ImVec4(0.090f, 0.251f, 0.690f, 1.00f);
    colors[ImGuiCol_Header]               = ImVec4(0.118f, 0.161f, 0.231f, 1.00f);
    colors[ImGuiCol_HeaderHovered]        = ImVec4(0.145f, 0.388f, 0.922f, 0.80f);
    colors[ImGuiCol_HeaderActive]         = ImVec4(0.145f, 0.388f, 0.922f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.043f, 0.059f, 0.098f, 0.60f);
    colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.200f, 0.255f, 0.333f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.145f, 0.388f, 0.922f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.145f, 0.388f, 0.922f, 1.00f);
    colors[ImGuiCol_CheckMark]            = ImVec4(0.220f, 0.741f, 0.973f, 1.00f); // #38bdf8
    colors[ImGuiCol_Text]                 = ImVec4(0.941f, 0.961f, 0.980f, 1.00f); // #f1f5f9
}

// ================= BADGE BANTUAN INTERAKTIF (?) [JIKA AKTIF vs NON-AKTIF] =================
static void HelpBadge(const char* title, const char* if_on, const char* if_off, const char* recommendation) {
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.38f, 0.78f, 1.0f, 1.0f));
    ImGui::Text("(?)");
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(400.0f);
        ImGui::TextColored(ImVec4(0.95f, 0.85f, 0.35f, 1.0f), "PENJELASAN: %s", title);
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.60f, 1.0f), "[+] JIKA AKTIF (ON):");
        ImGui::TextWrapped("%s", if_on);
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.98f, 0.45f, 0.45f, 1.0f), "[-] JIKA NON-AKTIF (OFF):");
        ImGui::TextWrapped("%s", if_off);
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.50f, 0.80f, 0.98f, 1.0f), "[*] REKOMENDASI:");
        ImGui::TextWrapped("%s", recommendation);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

// ================= BADGE DETAIL MODEL AI (KELEBIHAN & KEKURANGAN) =================
static void HelpBadgePhotoModels(int selected_photo) {
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.38f, 0.78f, 1.0f, 1.0f));
    ImGui::Text("(?)");
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(540.0f);
        ImGui::TextColored(ImVec4(0.95f, 0.85f, 0.35f, 1.0f), "PANDUAN MODEL FOTO (KELEBIHAN & KEKURANGAN)");
        ImGui::Separator();
        ImGui::Spacing();

        for (size_t i = 0; i < g_detected_photos.size(); i++) {
            const auto& m = g_detected_photos[i];
            bool is_sel = ((int)i == selected_photo);
            ImGui::TextColored(is_sel ? ImVec4(0.38f, 0.88f, 1.0f, 1.0f) : ImVec4(0.90f, 0.90f, 0.90f, 1.0f), 
                "%s %d. %s", is_sel ? ">>> [AKTIF]" : "   ", (int)i + 1, m.display_name.c_str());
            ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.60f, 1.0f), "    [+] KELEBIHAN:");
            ImGui::TextWrapped("        %s", m.desc_plus.c_str());
            ImGui::TextColored(ImVec4(0.98f, 0.45f, 0.45f, 1.0f), "    [-] KEKURANGAN:");
            ImGui::TextWrapped("        %s", m.desc_minus.c_str());
            ImGui::Spacing();
        }

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.50f, 0.80f, 0.98f, 1.0f), "[*] REKOMENDASI:");
        ImGui::TextWrapped("Pilih CyberRealistic V5.0 untuk foto manusia/NSFW paling nyata, atau DreamShaper 8 jika butuh gambar kilat 4 detik. Setiap file model baru (.gguf / .safetensors) yang Anda masukkan ke folder models\\Gambar_Diffusion otomatis terdeteksi!");
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

static void HelpBadgeLlmModels(int selected_llm) {
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.38f, 0.78f, 1.0f, 1.0f));
    ImGui::Text("(?)");
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(540.0f);
        ImGui::TextColored(ImVec4(0.95f, 0.85f, 0.35f, 1.0f), "PANDUAN MODEL CHAT LLM (KELEBIHAN & KEKURANGAN)");
        ImGui::Separator();
        ImGui::Spacing();

        for (size_t i = 0; i < g_detected_llms.size(); i++) {
            const auto& m = g_detected_llms[i];
            bool is_sel = ((int)i == selected_llm);
            ImGui::TextColored(is_sel ? ImVec4(0.38f, 0.88f, 1.0f, 1.0f) : ImVec4(0.90f, 0.90f, 0.90f, 1.0f), 
                "%s %d. %s", is_sel ? ">>> [AKTIF]" : "   ", (int)i + 1, m.display_name.c_str());
            ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.60f, 1.0f), "    [+] KELEBIHAN:");
            ImGui::TextWrapped("        %s", m.desc_plus.c_str());
            ImGui::TextColored(ImVec4(0.98f, 0.45f, 0.45f, 1.0f), "    [-] KEKURANGAN:");
            ImGui::TextWrapped("        %s", m.desc_minus.c_str());
            ImGui::Spacing();
        }

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.50f, 0.80f, 0.98f, 1.0f), "[*] REKOMENDASI:");
        ImGui::TextWrapped("Gunakan Qwen2.5 1.5B untuk koding & Auto Prompt sehari-hari, DeepSeek-R1 untuk logika matematika, atau Qwen3.8 Uncensored untuk obrolan santai tanpa batas. Setiap file .gguf baru yang Anda tambahkan ke folder models\\Teks_LLM otomatis terdeteksi!");
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

static void HelpBadgeSimple(const char* title, const char* explanation, const char* recommendation) {
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.38f, 0.78f, 1.0f, 1.0f));
    ImGui::Text("(?)");
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(400.0f);
        ImGui::TextColored(ImVec4(0.95f, 0.85f, 0.35f, 1.0f), "PENJELASAN: %s", title);
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextWrapped("%s", explanation);
        if (recommendation && strlen(recommendation) > 0) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.50f, 0.80f, 0.98f, 1.0f), "[*] REKOMENDASI:");
            ImGui::TextWrapped("%s", recommendation);
        }
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

// Render WhatsApp-Style Chat Bubble (Kanan: User, Kiri: AI)
void RenderWhatsAppBubble(const ChatMsg& msg, float chat_w, int idx) {
    bool is_user = (msg.role == "User");
    float max_bubble_w = chat_w * 0.75f;
    if (max_bubble_w < 320.0f) max_bubble_w = chat_w - 36.0f;

    float inner_text_w = max_bubble_w - 36.0f;
    ImVec2 text_size = ImGui::CalcTextSize(msg.text.c_str(), NULL, false, inner_text_w);
    
    float bubble_w = text_size.x + 40.0f;
    if (bubble_w < 240.0f) bubble_w = 240.0f;
    if (bubble_w > max_bubble_w) bubble_w = max_bubble_w;

    float img_h_extra = 0.0f;
    float preview_w = bubble_w - 28.0f;
    if (preview_w > 400.0f) preview_w = 400.0f;
    if (preview_w < 200.0f) preview_w = 200.0f;
    float aspect = (msg.img_w > 0 && msg.img_h > 0) ? ((float)msg.img_h / (float)msg.img_w) : 1.0f;
    if (msg.texture_view != NULL) {
        img_h_extra = preview_w * aspect + 18.0f;
    }
    float action_btn_extra = !msg.image_path.empty() ? 46.0f : 0.0f;
    float voice_btn_extra = (!is_user && msg.image_path.empty()) ? 38.0f : 0.0f;
    float bubble_h = text_size.y + 72.0f + img_h_extra + action_btn_extra + voice_btn_extra;

    if (is_user) {
        float pos_x = chat_w - bubble_w - 24.0f;
        if (pos_x < 16.0f) pos_x = 16.0f;
        ImGui::SetCursorPosX(pos_x);

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.04f, 0.35f, 0.26f, 0.95f)); // #005c4b
        ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.08f, 0.52f, 0.38f, 0.85f));
    } else {
        ImGui::SetCursorPosX(16.0f);

        if (msg.tag.find("NSFW") != std::string::npos) {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.22f, 0.08f, 0.14f, 0.95f)); // Ruby Wine Tint
            ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.55f, 0.15f, 0.28f, 0.85f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.11f, 0.15f, 0.22f, 0.95f)); // #1e293b
            ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.20f, 0.28f, 0.40f, 0.85f));
        }
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 10));

    char child_id[64];
    snprintf(child_id, sizeof(child_id), "bubble_%d", idx);
    ImGui::BeginChild(child_id, ImVec2(bubble_w, bubble_h), true, ImGuiWindowFlags_None);

    if (is_user) {
        ImGui::TextColored(ImVec4(0.40f, 0.95f, 0.70f, 1.0f), "Anda");
    } else {
        if (msg.tag.find("NSFW") != std::string::npos) {
            ImGui::TextColored(ImVec4(0.98f, 0.35f, 0.55f, 1.0f), "AI Studio (NSFW Uncensored)");
        } else {
            ImGui::TextColored(ImVec4(0.38f, 0.74f, 0.98f, 1.0f), "AI Studio");
        }
        ImGui::SameLine();
        ImGui::TextDisabled("| %s", msg.tag.c_str());
    }

    ImGui::Spacing();
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + bubble_w - 28.0f);
    ImGui::TextUnformatted(msg.text.c_str());
    ImGui::PopTextWrapPos();
    ImGui::Spacing();

    // 1. Render Gambar Langsung di Layar Chat Direct3D 11
    if (msg.texture_view != NULL) {
        ImGui::Spacing();
        ImGui::Image((ImTextureID)msg.texture_view, ImVec2(preview_w, preview_w * aspect));
        ImGui::Spacing();
    }

    // 2. Tombol Aksi Gambar (Layar Penuh, Folder, Salin Path)
    if (!msg.image_path.empty()) {
        ImGui::Separator();
        ImGui::Spacing();

        char btn_view[64], btn_folder[64], btn_copy[64];
        snprintf(btn_view, sizeof(btn_view), "Layar Penuh##%d", idx);
        snprintf(btn_folder, sizeof(btn_folder), "Folder Output##%d", idx);
        snprintf(btn_copy, sizeof(btn_copy), "Salin Path##%d", idx);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.50f, 0.35f, 1.0f));
        if (ImGui::Button(btn_view, ImVec2(110, 30))) {
            ShellExecuteA(NULL, "open", msg.image_path.c_str(), NULL, NULL, SW_SHOWNORMAL);
        }
        ImGui::PopStyleColor();

        ImGui::SameLine();
        if (ImGui::Button(btn_folder, ImVec2(110, 30))) {
            ShellExecuteA(NULL, "open", "outputs\\foto", NULL, NULL, SW_SHOWNORMAL);
        }

        ImGui::SameLine();
        if (ImGui::Button(btn_copy, ImVec2(90, 30))) {
            CopyTextToClipboard(msg.image_path);
        }

        ImGui::Spacing();
    }

    // 3. Tombol Suara Alami (Natural Human Voice) untuk Chat Teks AI
    if (!is_user && msg.image_path.empty()) {
        ImGui::Separator();
        ImGui::Spacing();

        char btn_gadis[64], btn_ardi[64];
        snprintf(btn_gadis, sizeof(btn_gadis), "🔊 Gadis (Wanita)##%d", idx);
        snprintf(btn_ardi, sizeof(btn_ardi), "🔊 Ardi (Pria)##%d", idx);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.08f, 0.48f, 0.42f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.12f, 0.65f, 0.55f, 1.0f));
        if (ImGui::Button(btn_gadis, ImVec2(130, 26))) {
            SpeakNaturalVoiceAsync(msg.text, 0);
        }
        ImGui::PopStyleColor(2);

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.35f, 0.55f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.48f, 0.72f, 1.0f));
        if (ImGui::Button(btn_ardi, ImVec2(120, 26))) {
            SpeakNaturalVoiceAsync(msg.text, 1);
        }
        ImGui::PopStyleColor(2);

        if (g_is_speaking.load()) {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.70f, 0.15f, 0.15f, 0.85f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.20f, 0.20f, 1.0f));
            char btn_stop[64];
            snprintf(btn_stop, sizeof(btn_stop), "⏹️ Stop##%d", idx);
            if (ImGui::Button(btn_stop, ImVec2(65, 26))) {
                StopSpeakingVoice();
            }
            ImGui::PopStyleColor(2);
        }
        ImGui::Spacing();
    }

    // Waktu di pojok
    ImGui::TextDisabled("%s", msg.timestamp.c_str());

    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);

    ImGui::Dummy(ImVec2(0, 6)); // Spacing antar bubble
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    g_logf = fopen("cpp_core/native_gui.log", "w");
    if (g_logf) { fprintf(g_logf, "[LOG] AI Studio Native Starting...\n"); fflush(g_logf); }

    // 1. Inisialisasi C++ Native Core & Auto-Deteksi Hardware (CPU / RAM / GPU RTX)
    Core_Init(0);
    ScanModelsDirectory(); // ✨ Auto-deteksi semua model .gguf & .safetensors di folder models\
    g_hw_profile = DetectHardwareAndCompatibility();
    g_hw_initialized = true;
    ApplyHardwareCompatibilitySettings();

    // 2. Buat Windows Class & Window Native
    WNDCLASSEXW wc = {
        sizeof(WNDCLASSEXW), CS_CLASSDC, WndProc, 0L, 0L,
        GetModuleHandle(NULL), NULL, NULL, NULL, NULL,
        L"AIStudioCppNativeClass", NULL
    };
    RegisterClassExW(&wc);

    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);
    int win_w = (int)(screen_w * 0.90);
    int win_h = (int)(screen_h * 0.88);
    int win_x = (screen_w - win_w) / 2;
    int win_y = (screen_h - win_h) / 2;

    HWND hwnd = CreateWindowW(
        wc.lpszClassName,
        L"AI Studio (100% Pure C++ Native - Direct3D 11 & Touchscreen)",
        WS_OVERLAPPEDWINDOW,
        win_x, win_y, win_w, win_h,
        NULL, NULL, wc.hInstance, NULL
    );

    g_hMainWnd = hwnd;
    DragAcceptFiles(hwnd, TRUE);

    // Aktifkan Windows Touch API Native (Touchscreen Multi-Gesture)
    RegisterTouchWindow(hwnd, 0);

    // 3. Inisialisasi Direct3D 11
    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    // 4. Inisialisasi Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = "cpp_core/imgui.ini";

    // Muat font Segoe UI Windows untuk tampilan bersih, lega, dan nyaman (Ukuran 19.0f)
    if (GetFileAttributesA("C:\\Windows\\Fonts\\segoeui.ttf") != INVALID_FILE_ATTRIBUTES) {
        io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 19.0f);
    } else {
        io.Fonts->AddFontDefault();
    }

    ApplyModernDarkTheme();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Muat riwayat percakapan permanen dari disk
    LoadSessionsFromDisk(g_pd3dDevice);

    // 5. Main Loop (60 FPS VSync)
    bool bDone = false;
    while (!bDone) {
        MSG msg;
        while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                bDone = true;
        }
        if (bDone) break;

        // Update Inferensi Nyata & Animasi di Background (100% Pure C++)
        if (g_is_processing) {
            if (g_worker_finished.load()) {
                if (g_worker_thread.joinable()) {
                    g_worker_thread.join();
                }

                ChatMsg aiMsg;
                aiMsg.role = "AI Studio";
                aiMsg.tag = (g_pending_mode == 5) ? "Asisten Suara (C++)" : (g_pending_mode == 3) ? "NSFW Dewasa (C++)" : (g_pending_mode == 2) ? "Foto Diffusion (C++)" : (g_pending_mode == 4) ? "Video AI" : "AI LLM (C++)";
                aiMsg.timestamp = GetCurrentTimestamp();
                aiMsg.text = g_worker_result;
                aiMsg.image_path = g_worker_img_path;

                if (!g_worker_img_path.empty()) {
                    int iw = 0, ih = 0;
                    aiMsg.texture_view = LoadTextureFromFileDX11(g_pd3dDevice, g_worker_img_path.c_str(), &iw, &ih);
                    aiMsg.img_w = iw;
                    aiMsg.img_h = ih;
                }

                if (g_active_session_idx >= 0 && g_active_session_idx < (int)g_sessions.size()) {
                    auto& cur_sess = g_sessions[g_active_session_idx];
                    cur_sess.messages.push_back(aiMsg);
                    cur_sess.last_time = aiMsg.timestamp;
                    SaveSessionToFile(cur_sess);
                }

                // Hanya bicara jika user mengaktifkan 'Auto Bicara' atau eksplisit menyuruh AI bersuara di prompt
                bool user_requested_voice = false;
                std::string lower_prompt = g_pending_prompt;
                for (char& c : lower_prompt) c = (char)tolower(c);
                if (lower_prompt.find("bicara") != std::string::npos ||
                    lower_prompt.find("suara") != std::string::npos ||
                    lower_prompt.find("baca") != std::string::npos ||
                    lower_prompt.find("ngomong") != std::string::npos ||
                    lower_prompt.find("ucapkan") != std::string::npos) {
                    user_requested_voice = true;
                }

                if ((g_auto_speak || user_requested_voice) && (g_pending_mode == 5 || (g_voice_enabled && g_pending_mode == 1))) {
                    SpeakNaturalVoiceAsync(aiMsg.text, g_voice_character);
                }

                g_worker_img_path = "";
                g_is_processing = false;
                g_worker_finished = false;
                g_process_progress.store(0.0f);
                {
                    std::lock_guard<std::mutex> lock(g_stream_mutex);
                    g_live_stream_text = "";
                }
            }
        }

        // Optimasi UI: Saat AI sedang melakukan inferensi berat di latar belakang,
        // turunkan frekuensi render UI sejenak agar CPU 100% fokus mengerjakan model AI
        if (g_is_processing) {
            Sleep(12);
        }

        // Start ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Ambil ukuran window saat ini (Auto-Responsive)
        RECT rect;
        GetClientRect(hwnd, &rect);
        float app_width = (float)(rect.right - rect.left);
        float app_height = (float)(rect.bottom - rect.top);

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(app_width, app_height));
        ImGuiWindowFlags root_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                      ImGuiWindowFlags_NoBringToFrontOnFocus;

        ImGui::Begin("RootWindow", NULL, root_flags);

        float sidebar_width = 280.0f;
        if (app_width < 800.0f) sidebar_width = 230.0f;
        float right_panel_width = app_width - sidebar_width - 24.0f;
        if (right_panel_width < 280.0f) right_panel_width = 280.0f;

        // ================= 1. SIDEBAR KIRI (FULL HEIGHT) =================
        ImGui::BeginChild("SidebarPanel", ImVec2(sidebar_width, app_height - 16.0f), true);

        // Logo & Brand Header
        ImGui::TextColored(ImVec4(0.22f, 0.74f, 0.97f, 1.0f), "AI STUDIO");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.50f, 0.80f, 0.95f, 0.9f), "C++");
        ImGui::TextDisabled("100%% Pure Native * Direct3D 11");
        ImGui::Spacing();

        // Tombol Percakapan Baru
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.38f, 0.65f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.50f, 0.82f, 1.0f));
        if (ImGui::Button("+ Percakapan Baru", ImVec2(-1, 38))) {
            CreateNewSession();
        }
        ImGui::PopStyleColor(2);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Daftar Percakapan / Topics (Scrollable)
        float topics_h = app_height - 315.0f;
        if (topics_h < 100.0f) topics_h = 100.0f;
        ImGui::BeginChild("TopicsList", ImVec2(-1, topics_h), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
        for (size_t t = 0; t < g_sessions.size(); t++) {
            bool selected = ((int)t == g_active_session_idx);
            ImGui::PushID((int)t);

            if (selected) {
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.16f, 0.28f, 0.44f, 0.95f));
                ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.35f, 0.65f, 0.98f, 0.90f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.13f, 0.18f, 0.70f));
                ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.18f, 0.24f, 0.32f, 0.60f));
            }
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 6));

            std::string card_name = "SessionCard_" + std::to_string(t);
            ImGui::BeginChild(card_name.c_str(), ImVec2(-1, 46), true, ImGuiWindowFlags_NoScrollbar);

            if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) {
                g_active_session_idx = (int)t;
            }

            std::string title_disp = g_sessions[t].title;
            if (title_disp.length() > 20) title_disp = title_disp.substr(0, 17) + "...";
            ImGui::TextColored(selected ? ImVec4(0.45f, 0.85f, 1.0f, 1.0f) : ImVec4(0.90f, 0.92f, 0.96f, 1.0f), "💬 %s", title_disp.c_str());

            int msg_count = (int)g_sessions[t].messages.size();
            ImGui::TextDisabled("%d pesan • %s", msg_count, g_sessions[t].last_time.c_str());

            if (g_sessions.size() > 1) {
                ImGui::SameLine(sidebar_width - 64.0f);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.15f, 0.15f, 0.8f));
                if (ImGui::SmallButton("❌")) {
                    DeleteSession((int)t);
                    ImGui::PopStyleColor(2);
                    ImGui::EndChild();
                    ImGui::PopStyleVar(2);
                    ImGui::PopStyleColor(2);
                    ImGui::PopID();
                    break;
                }
                ImGui::PopStyleColor(2);
            }

            ImGui::EndChild();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);
            ImGui::PopID();
            ImGui::Spacing();
        }
        ImGui::EndChild();

        ImGui::Separator();
        ImGui::Spacing();

        // Telemetri Hardware Card
        Core_GetTelemetry(&g_telemetry);
        double total_ram_gb = (double)g_telemetry.total_ram_bytes / (1024.0 * 1024.0 * 1024.0);
        double free_ram_gb  = (double)g_telemetry.free_ram_bytes / (1024.0 * 1024.0 * 1024.0);
        double used_ram_gb  = total_ram_gb - free_ram_gb;
        float ram_fraction  = total_ram_gb > 0 ? (float)(used_ram_gb / total_ram_gb) : 0.0f;

        bool is_rtx_active = (g_hardware_backend_mode == 2 || (g_hardware_backend_mode == 0 && g_hw_profile.has_nvidia_gpu));

        ImGui::TextDisabled("STATUS HARDWARE");
        ImGui::Text("RAM: %.1f / %.1f GB", used_ram_gb, total_ram_gb);
        ImGui::ProgressBar(ram_fraction, ImVec2(-1, 6), "");
        if (is_rtx_active) {
            std::string gpu_label = g_hw_profile.gpus.empty() ? "NVIDIA RTX" : g_hw_profile.gpus[g_hw_profile.selected_gpu_idx].name;
            if (gpu_label.length() > 24) gpu_label = gpu_label.substr(0, 22) + "..";
            ImGui::TextColored(ImVec4(0.2f, 0.95f, 0.4f, 1.0f), "GPU: %s", gpu_label.c_str());
            ImGui::TextDisabled("Akselerasi: CUDA RTX Active 🚀");
        } else {
            ImGui::TextDisabled("CPU: %d Thread | AVX2 + TAESD ⚡", g_num_threads);
        }
        ImGui::Spacing();

        // Tombol Pengaturan & Outputs di Footer Sidebar
        float footer_btn_w = (sidebar_width - 24.0f) * 0.48f;
        if (ImGui::Button("Pengaturan", ImVec2(footer_btn_w, 32))) {
            g_show_settings = !g_show_settings;
        }
        ImGui::SameLine();
        if (ImGui::Button("Output", ImVec2(footer_btn_w, 32))) {
            ShellExecuteA(NULL, "open", "outputs", NULL, NULL, SW_SHOWNORMAL);
        }

        ImGui::EndChild(); // SidebarPanel

        ImGui::SameLine();

        // ================= 2. PANEL UTAMA CHAT (KANAN) =================
        ImGui::BeginGroup();

        // A. TOP NAVBAR (Tinggi 42px)
        ImGui::BeginChild("TopNavbar", ImVec2(right_panel_width, 42), true, ImGuiWindowFlags_NoScrollbar);
        
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Mode:");
        ImGui::SameLine();

        for (int m = 0; m < 6; m++) {
            if (m > 0) ImGui::SameLine();
            bool is_active = (g_selected_mode == m);
            if (m == 3) {
                // Tombol NSFW / Dewasa dengan warna Rose Crimson Menarik
                if (is_active) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.88f, 0.14f, 0.38f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.98f, 0.22f, 0.46f, 1.0f));
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.38f, 0.08f, 0.18f, 0.9f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.55f, 0.12f, 0.26f, 1.0f));
                }
            } else if (m == 5) {
                // Tombol Suara AI dengan warna Emerald / Mint Berkilau
                if (is_active) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.08f, 0.65f, 0.52f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.12f, 0.78f, 0.62f, 1.0f));
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.06f, 0.25f, 0.22f, 0.85f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.10f, 0.36f, 0.30f, 1.0f));
                }
            } else {
                if (is_active) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.145f, 0.388f, 0.922f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.45f, 0.95f, 1.0f));
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.14f, 0.20f, 0.8f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.20f, 0.30f, 1.0f));
                }
            }
            if (ImGui::Button(g_mode_names[m], ImVec2(0, 26))) {
                g_selected_mode = m;
            }
            ImGui::PopStyleColor(2);
        }

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        if (g_selected_mode == 2 || g_selected_mode == 3) {
            ImGui::Text(g_selected_mode == 3 ? "Model NSFW:" : "Model Foto:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(240);
            if (!g_photo_items.empty()) {
                ImGui::Combo("##PhotoCombo", &g_selected_photo, g_photo_items.data(), (int)g_photo_items.size());
            }
            HelpBadgePhotoModels(g_selected_photo);
        } else {
            ImGui::Text("Model Chat:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(240);
            if (!g_llm_items.empty()) {
                ImGui::Combo("##LLMCombo", &g_selected_llm, g_llm_items.data(), (int)g_llm_items.size());
            }
            HelpBadgeLlmModels(g_selected_llm);
        }

        ImGui::SameLine();
        if (ImGui::Button("+ GGUF", ImVec2(58, 26))) {
            std::string chosen = OpenModelFileDialog(g_hMainWnd);
            if (!chosen.empty()) {
                g_current_inspection = InspectModelFile(chosen);
                g_show_import_modal = true;
                g_is_importing = false;
                g_import_progress = 0.0f;
                g_import_status_text = "";
            }
        }
        HelpBadge(
            "Import Model AI (.GGUF / .safetensors)",
            "Menambah model baru secara instan: pilih file .gguf dari folder laptop Anda, sistem akan otomatis membaca arsitektur model, mengecek kelengkapan VAE, lalu mengaktifkannya.",
            "Menyalin file secara manual melalui Windows Explorer.",
            "Tips Cepat: Anda juga bisa langsung men-drag & drop file .gguf ke dalam jendela aplikasi ini kapan saja!"
        );

        ImGui::SameLine();
        ImGui::Checkbox("Web", &g_search_internet);
        HelpBadge(
            "Pencarian Web Otomatis",
            "AI akan mencari informasi online terkini dari internet sebelum menjawab pertanyaan Anda.",
            "AI berjalan 100% OFFLINE tanpa menyentuh internet. Menjawab murni dari data lokal model LLM (lebih cepat & privasi aman).",
            "AKTIFKAN jika menanyakan informasi/berita terbaru. NON-AKTIFKAN untuk koding, percakapan umum, atau mode offline."
        );

        ImGui::EndChild(); // TopNavbar

        // Sub-Toolbar Khusus Mode 2 (Foto) & Mode 3 (NSFW / Dewasa)
        float subbar_h = 0.0f;
        if (g_selected_mode == 3) {
            subbar_h = 42.0f;
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.06f, 0.10f, 0.90f));
            ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.50f, 0.12f, 0.24f, 0.70f));
            ImGui::BeginChild("NsfwSubNavbar", ImVec2(right_panel_width, 38), true, ImGuiWindowFlags_NoScrollbar);

            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(ImVec4(0.98f, 0.35f, 0.55f, 1.0f), "PRESET NSFW:");
            ImGui::SameLine();

            if (ImGui::Button("Sensual Bedroom", ImVec2(0, 24))) {
                strcpy(g_input_prompt, "Sensual woman portrait in cozy bedroom, soft morning sunlight, ultra realistic skin texture, 8k uhd, photorealistic, raw candid photo");
            }
            ImGui::SameLine();
            if (ImGui::Button("Pantai Glamour", ImVec2(0, 24))) {
                strcpy(g_input_prompt, "Realistic beautiful Asian model posing on tropical beach during golden hour sunset, wet skin, natural sunlight, highly detailed eyes and body, 8k photography");
            }
            ImGui::SameLine();
            if (ImGui::Button("Fine Art Nude", ImVec2(0, 24))) {
                strcpy(g_input_prompt, "Fine art female aesthetic nude silhouette, dramatic chiaroscuro rim lighting, soft shadows, detailed skin pores, award winning masterpiece");
            }
            ImGui::SameLine();
            if (ImGui::Button("Fitness Model", ImVec2(0, 24))) {
                strcpy(g_input_prompt, "Sensual fitness girl in studio sportswear, athletic toned body, dramatic rim light, sweat glistening, photorealistic, 8k");
            }
            ImGui::SameLine();
            if (ImGui::Button("Full Body", ImVec2(0, 24))) {
                g_framing_idx = 3;    // Set Framing ke Full Body
                g_resolution_idx = 1; // Set Rasio ke 512 x 768 Potret Vertikal
                strcpy(g_input_prompt, "Full length body shot of an attractive woman standing gracefully, full body view from head to toe, aesthetic lingerie, bare feet, wide angle 35mm lens, full figure framing, (detailed beautiful face:1.2), sharp detailed eyes, natural facial expression, accurate anatomy, natural proportions, 5 fingers, soft ambient lighting, photorealistic, 8k");
            }

            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();

            ImGui::Checkbox("Booster Realistis", &g_nsfw_quality_booster);
            HelpBadge(
                "Booster Realistis (Quality Booster)",
                "Otomatis menyuntikkan token fotografi 8K: natural skin texture, micro pores, beautiful expressive eyes, Canon EOS R5 85mm f/1.4, photorealistic, raw photo ke prompt.",
                "Hanya menggunakan kata-kata prompt yang Anda ketik secara apa adanya tanpa penambahan kualitas.",
                "SELALU AKTIFKAN (ON) agar wajah dan tekstur kulit tampak seperti manusia sungguhan dan tidak tampak seperti kartun/3D."
            );

            ImGui::SameLine();
            ImGui::Checkbox("HD Wajah", &g_fullbody_face_fix);
            HelpBadge(
                "HD Wajah & Anatomi (Anti-Muka Kabur / Cacat)",
                "Otomatis menyuntikkan token penajam wajah berbobot tinggi (detailed beautiful face:1.25), (sharp focused eyes:1.25) dan filter negative prompt anti-cacat anatomi (mencegah tangan bercabang, jari berlebih/kurang, dan muka buram/meleleh pada foto Full Body).\nJuga menaikkan sampling ke 25 langkah agar detail mikro mata & jari terbentuk sempurna.",
                "Hanya menggunakan prompt polos biasa tanpa penyempurnaan anatomi otomatis.",
                "SELALU AKTIFKAN (ON) terutama saat memilih sudut 'Full Body' agar wajah tetap jernih dan jari tangan utuh 5 sempurna."
            );

            ImGui::SameLine();
            ImGui::Checkbox("Acak Seed", &g_random_seed);
            HelpBadge(
                "Acak Seed (Random Variation)",
                "Setiap kali Anda klik Kirim, AI menggunakan angka seed acak sehingga menghasilkan variasi pose, wajah, dan sudut foto yang selalu berbeda dan segar.",
                "Mengunci angka seed ke nilai statis. Gambar yang dihasilkan akan selalu sama persis selama prompt dan pengaturannya tidak berubah.",
                "AKTIFKAN (ON) untuk mengeksplorasi banyak variasi foto. NON-AKTIFKAN jika ingin mereproduksi foto yang sama."
            );

            ImGui::SameLine();
            ImGui::Checkbox("Fast VAE", &g_use_fast_vae);
            HelpBadge(
                "Fast VAE (Akselerasi TAESD)",
                "Menggunakan Tiny AutoEncoder SD. Waktu tahap akhir decoding gambar di CPU dipangkas drastis dari 61 detik menjadi HANYA ~2.7 detik tanpa penurunan kualitas kasat mata.",
                "Menggunakan VAE standar bawaan yang sangat lambat di CPU (menambah waktu tunggu sekitar 1 menit penuh di setiap generasi).",
                "SANGAT DIREKOMENDASIKAN SELALU AKTIF (ON) agar generasi foto di laptop Anda cepat selesai."
            );

            ImGui::SameLine();
            ImGui::Checkbox("Turbo LCM", &g_turbo_mode);
            HelpBadge(
                "Turbo LCM (Akselerasi 4-Step Kilat)",
                "Proses denoising dipangkas menjadi HANYA 4 LANGKAH (~3-5 detik) dengan otomatis menyertakan LCM LoRA. Sangat cepat untuk membuat sketsa ide atau preview instan.",
                "Denoising berjalan PENUH 20-30 LANGKAH (CFG 7.0). Menghasilkan ketajaman maksimal, tekstur kulit mikro berpori, bayangan halus, dan mata yang sangat jernih layaknya kamera DSLR.",
                "NON-AKTIFKAN (OFF) jika Anda ingin hasil fotorealistis terbaik sekelas majalah/DSLR pada model CyberRealistic V5. AKTIFKAN jika butuh hasil cepat dalam hitungan detik."
            );

            ImGui::EndChild(); // NsfwSubNavbar
            ImGui::PopStyleColor(2);
        } else if (g_selected_mode == 2) {
            subbar_h = 42.0f;
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.12f, 0.18f, 0.90f));
            ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.20f, 0.40f, 0.65f, 0.70f));
            ImGui::BeginChild("PhotoSubNavbar", ImVec2(right_panel_width, 38), true, ImGuiWindowFlags_NoScrollbar);

            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(ImVec4(0.38f, 0.75f, 0.98f, 1.0f), "PRESET FOTO:");
            ImGui::SameLine();

            if (ImGui::Button("Gadis Cantik", ImVec2(0, 24))) {
                strcpy(g_input_prompt, "Close-up portrait of a gorgeous Indonesian woman smiling outdoors in nature, golden hour warm lighting, natural skin pores, Canon EOS R5 85mm f/1.4, 8k");
            }
            ImGui::SameLine();
            if (ImGui::Button("Pemandangan Alam", ImVec2(0, 24))) {
                strcpy(g_input_prompt, "Breathtaking landscape photography of Mount Bromo sunrise with sea of clouds, epic morning sun rays, ultra detailed, award winning nature photo, 8k uhd");
            }
            ImGui::SameLine();
            if (ImGui::Button("Cyberpunk Kota", ImVec2(0, 24))) {
                strcpy(g_input_prompt, "Futuristic neon cyberpunk city street at rainy night, wet asphalt reflections, vibrant blue and purple holographic billboards, cinematic 8k");
            }
            ImGui::SameLine();
            if (ImGui::Button("Mobil Sport", ImVec2(0, 24))) {
                strcpy(g_input_prompt, "Hypercar luxury sports car parked on coastal cliff highway at sunset, sleek carbon fiber body reflections, automotive photography, 8k");
            }
            ImGui::SameLine();
            if (ImGui::Button("Full Body", ImVec2(0, 24))) {
                g_framing_idx = 3;    // Set Framing ke Full Body
                g_resolution_idx = 1; // Set Rasio ke 512 x 768 Potret Vertikal
                strcpy(g_input_prompt, "Full length body shot of a beautiful model standing gracefully outdoors, head to toe view, fashionable elegant dress, visible shoes on ground, wide angle 35mm lens, full figure framing, (detailed beautiful face:1.2), sharp detailed eyes, natural facial expression, accurate anatomy, natural proportions, 5 fingers, natural lighting, 8k uhd, photorealistic, masterpiece");
            }

            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();

            ImGui::Checkbox("Booster Realistis", &g_nsfw_quality_booster);
            HelpBadge(
                "Booster Realistis (Quality Booster)",
                "Otomatis menyuntikkan kata kunci fotografi 8K: natural skin texture, micro pores, beautiful expressive eyes, Canon EOS R5 85mm f/1.4, photorealistic, raw photo ke prompt.",
                "Hanya menggunakan teks prompt mentah apa adanya.",
                "SELALU AKTIFKAN (ON) untuk potret manusia atau objek fotorealistis."
            );

            ImGui::SameLine();
            ImGui::Checkbox("HD Wajah", &g_fullbody_face_fix);
            HelpBadge(
                "HD Wajah & Anatomi (Anti-Muka Kabur / Cacat)",
                "Otomatis menyuntikkan token penajam wajah berbobot tinggi (detailed beautiful face:1.25), (sharp focused eyes:1.25) dan filter negative prompt anti-cacat anatomi (mencegah tangan bercabang, jari berlebih/kurang, dan muka buram/meleleh pada foto Full Body).\nJuga menaikkan sampling ke 25 langkah agar detail mikro mata & jari terbentuk sempurna.",
                "Hanya menggunakan prompt polos biasa tanpa penyempurnaan anatomi otomatis.",
                "SELALU AKTIFKAN (ON) terutama saat memilih sudut 'Full Body' agar wajah tetap jernih dan jari tangan utuh 5 sempurna."
            );

            ImGui::SameLine();
            ImGui::Checkbox("Acak Seed", &g_random_seed);
            HelpBadge(
                "Acak Seed (Random Variation)",
                "Setiap klik Kirim menghasilkan variasi gambar baru yang segar.",
                "Mengunci seed agar hasil gambar identik/konsisten.",
                "AKTIFKAN (ON) untuk eksplorasi variasi."
            );

            ImGui::SameLine();
            ImGui::Checkbox("Fast VAE", &g_use_fast_vae);
            HelpBadge(
                "Fast VAE (Akselerasi TAESD)",
                "Decoding gambar di CPU dipangkas dari 61 detik menjadi ~2.7 detik.",
                "Decoding VAE bawaan yang lambat di CPU (~61 detik).",
                "SELALU AKTIFKAN (ON)."
            );

            ImGui::SameLine();
            ImGui::Checkbox("Turbo LCM", &g_turbo_mode);
            HelpBadge(
                "Turbo LCM (Akselerasi 4-Step Kilat)",
                "Generasi kilat hanya 4 langkah (~3-5 detik).",
                "Generasi penuh 20-30 langkah DSLR paling tajam & realistis.",
                "NON-AKTIFKAN (OFF) untuk kualitas foto DSLR terbaik sekelas CyberRealistic V5."
            );

            ImGui::EndChild(); // PhotoSubNavbar
            ImGui::PopStyleColor(2);
        } else if (g_selected_mode == 5) {
            subbar_h = 42.0f;
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.05f, 0.16f, 0.15f, 0.90f));
            ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.12f, 0.55f, 0.48f, 0.70f));
            ImGui::BeginChild("VoiceSubNavbar", ImVec2(right_panel_width, 38), true, ImGuiWindowFlags_NoScrollbar);

            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.82f, 1.0f), "🎙️ SUARA:");
            ImGui::SameLine();

            for (int vc = 0; vc < 3; vc++) {
                if (vc > 0) ImGui::SameLine();
                bool active = (g_voice_character == vc);
                if (active) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.65f, 0.52f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.78f, 0.62f, 1.0f));
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.08f, 0.22f, 0.20f, 0.7f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.12f, 0.32f, 0.28f, 1.0f));
                }
                if (ImGui::Button(g_voice_character_names[vc], ImVec2(0, 24))) {
                    g_voice_character = vc;
                }
                ImGui::PopStyleColor(2);
            }

            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();

            ImGui::Checkbox("Auto Bicara", &g_auto_speak);
            HelpBadge(
                "Mode Bicara Otomatis",
                "Jika DICENTANG: AI akan otomatis membacakan setiap jawaban begitu selesai menulis.",
                "Jika TIDAK DICENTANG (Default): AI tetap hening/diam. AI hanya akan bersuara jika Anda mengklik tombol [🔊 Suara Gadis] / [🔊 Suara Ardi], atau jika di prompt Anda menyuruhnya 'bacakan' / 'bicara'.",
                "Tips: Biarkan mati jika Anda ingin membaca teks secara santai tanpa suara mengejutkan."
            );

            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();

            if (g_is_speaking.load()) {
                static float anim_dot = 0.0f;
                anim_dot += 0.05f;
                int wave = (int)(sinf(anim_dot * 3.0f) * 2.0f + 2.0f);
                const char* waves[] = { " ~ ~ ~ ", " ∿ ~ ∿ ", " ∿ ∿ ∿ ", " ~ ∿ ~ ", " ~ ~ ~ " };
                const char* wstr = waves[(wave >= 0 && wave < 5) ? wave : 0];
                ImGui::TextColored(ImVec4(0.35f, 1.0f, 0.65f, 1.0f), "🔊 Sedang Bicara%s", wstr);
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.18f, 0.18f, 0.9f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.25f, 0.25f, 1.0f));
                if (ImGui::Button("⏹️ Hentikan", ImVec2(0, 24))) {
                    StopSpeakingVoice();
                }
                ImGui::PopStyleColor(2);
            } else {
                if (g_auto_speak) {
                    ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.70f, 0.9f), "🔊 Auto Bicara: Aktif");
                } else {
                    ImGui::TextColored(ImVec4(0.65f, 0.75f, 0.85f, 0.85f), "🔇 AI Diam (Klik tombol suara untuk mendengar)");
                }
            }

            ImGui::EndChild(); // VoiceSubNavbar
            ImGui::PopStyleColor(2);
        }

        // B. CHAT CANVAS (Scrollable Area)
        float input_card_h = 0.0f;
        if (g_has_init_image) input_card_h += 80.0f;
        if (g_has_attached_doc) input_card_h += 52.0f;
        float input_area_h = 92.0f + input_card_h;
        float chat_canvas_h = app_height - 42.0f - subbar_h - input_area_h - 32.0f;
        if (chat_canvas_h < 120.0f) chat_canvas_h = 120.0f;

        ImGui::BeginChild("ChatCanvas", ImVec2(right_panel_width, chat_canvas_h), true,
            ImGuiWindowFlags_AlwaysVerticalScrollbar);

        if (g_active_session_idx >= 0 && g_active_session_idx < (int)g_sessions.size()) {
            auto& cur_messages = g_sessions[g_active_session_idx].messages;
            for (size_t i = 0; i < cur_messages.size(); i++) {
                RenderWhatsAppBubble(cur_messages[i], right_panel_width, (int)i);
            }
        }

        if (g_is_processing) {
            bool is_photo_job = (g_pending_mode == 2 || g_pending_mode == 3);

            if (!is_photo_job) {
                // ================= MODE TEKS (LLM / CHAT / CODING) =================
                ImGui::SetCursorPosX(16.0f);
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.16f, 0.24f, 0.95f));
                ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.25f, 0.50f, 0.85f, 0.80f));
                ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 12));

                float bubble_w = right_panel_width * 0.85f;
                if (bubble_w < 340.0f) bubble_w = 340.0f;
                if (bubble_w > 820.0f) bubble_w = 820.0f;

                std::string live_text = "";
                {
                    std::lock_guard<std::mutex> lock(g_stream_mutex);
                    live_text = g_live_stream_text;
                }

                double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - g_process_start_time).count();
                int anim_frame = (int)(ImGui::GetTime() * 3.5f) % 4;
                const char* dot_anim = (anim_frame == 0) ? "" : (anim_frame == 1) ? "." : (anim_frame == 2) ? ".." : "...";
                bool cursor_blink = ((int)(ImGui::GetTime() * 2.5f) % 2) == 0;

                ImVec2 text_sz = ImGui::CalcTextSize(live_text.c_str(), NULL, false, bubble_w - 32.0f);
                float live_bubble_h = text_sz.y + 70.0f;
                if (live_bubble_h < 85.0f) live_bubble_h = 85.0f;

                ImGui::BeginChild("ai_writing_bubble", ImVec2(bubble_w, live_bubble_h), true, ImGuiWindowFlags_NoScrollbar);
                
                // Header Penulisan
                ImGui::TextColored(ImVec4(0.22f, 0.74f, 0.97f, 1.0f), "AI sedang menulis jawaban%s", dot_anim);
                ImGui::SameLine();
                ImGui::TextDisabled("| Waktu: %.1fs", elapsed);
                ImGui::Separator();
                ImGui::Spacing();

                if (live_text.empty()) {
                    ImGui::TextColored(ImVec4(0.70f, 0.85f, 1.0f, 0.85f), "Sedang memproses ide dan menyusun kalimat respons (AVX2 Engine)...");
                    ImGui::Spacing();
                    ImGui::ProgressBar(g_process_progress.load(), ImVec2(-1, 4), "");
                } else {
                    // Tampilkan teks yang sedang diketik secara live!
                    ImGui::TextWrapped("%s%s", live_text.c_str(), cursor_blink ? " _" : "  ");
                }

                ImGui::EndChild();
                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor(2);
                ImGui::Dummy(ImVec2(0, 6));
            } else {
                // ================= MODE GAMBAR (DIFFUSION / FOTO STUDIO / NSFW) =================
                ImGui::SetCursorPosX(16.0f);
                bool is_nsfw = (g_pending_mode == 3);
                
                if (is_nsfw) {
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.18f, 0.08f, 0.14f, 0.95f));
                    ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.85f, 0.20f, 0.45f, 0.85f));
                } else {
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.14f, 0.22f, 0.95f));
                    ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.20f, 0.65f, 0.95f, 0.85f));
                }
                ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 14));

                float bubble_w = right_panel_width * 0.82f;
                if (bubble_w < 360.0f) bubble_w = 360.0f;
                if (bubble_w > 720.0f) bubble_w = 720.0f;

                double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - g_process_start_time).count();
                float cur_prog = g_process_progress.load();
                std::string cur_status;
                {
                    std::lock_guard<std::mutex> lock(g_status_mutex);
                    cur_status = g_process_status;
                }

                // Animated spinner symbols
                const char* spin_chars[] = { "/", "-", "\\", "|" };
                int spin_idx = (int)(ImGui::GetTime() * 4.0f) % 4;

                ImGui::BeginChild("photo_render_card", ImVec2(bubble_w, 145), true, ImGuiWindowFlags_NoScrollbar);
                
                // Judul Animasi
                ImGui::TextColored(is_nsfw ? ImVec4(1.0f, 0.4f, 0.65f, 1.0f) : ImVec4(0.35f, 0.85f, 1.0f, 1.0f),
                    "[%s] Studio Foto AI: Sedang Merender Gambar...", spin_chars[spin_idx]
                );
                ImGui::SameLine();
                ImGui::TextDisabled("| Waktu: %.1fs", elapsed);

                ImGui::Spacing();
                
                // Status tahapan & Bar Progres
                ImGui::TextColored(ImVec4(1.0f, 0.88f, 0.35f, 1.0f), "Tahapan: %s", cur_status.c_str());
                ImGui::ProgressBar(cur_prog, ImVec2(-1, 10), "");

                ImGui::Spacing();

                // Dynamic Photographic Tips ticker (berganti setiap 4 detik)
                int tip_idx = ((int)(elapsed / 4.0)) % 4;
                const char* photo_tips[] = {
                    "Tips: Fast VAE (TAESD) memangkas proses decode foto menjadi hanya ~2.7 detik.",
                    "Framing HD: Penajam wajah & simetri mata otomatis diinjeksi untuk foto jernih.",
                    "CyberRealistic: Menghasilkan pori-pori kulit DSLR nyata pada 20-25 steps.",
                    "Komputasi: Instruksi AVX2 + FMA bekerja paralel di seluruh thread CPU Anda."
                };
                ImGui::TextDisabled("%s", photo_tips[tip_idx]);

                ImGui::EndChild();
                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor(2);
                ImGui::Dummy(ImVec2(0, 6));
            }
        }

        if (g_is_processing || ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 35) {
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild(); // ChatCanvas

        // C. BOTTOM INPUT AREA (Hanya di Kolom Kanan)
        ImGui::BeginChild("BottomInput", ImVec2(right_panel_width, input_area_h), false, ImGuiWindowFlags_NoScrollbar);

        // Preview Lampiran Dokumen jika ada file dokumen yang dipilih
        if (g_has_attached_doc) {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.16f, 0.20f, 0.95f));
            ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.20f, 0.70f, 0.60f, 0.75f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
            ImGui::BeginChild("AttachedDocCard", ImVec2(right_panel_width - 12.0f, 44.0f), true, ImGuiWindowFlags_NoScrollbar);

            std::string fname = g_attached_doc_path;
            size_t slash = fname.find_last_of("\\/");
            if (slash != std::string::npos) fname = fname.substr(slash + 1);
            if (fname.length() > 30) fname = fname.substr(0, 27) + "...";

            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.75f, 1.0f), "📄 DOKUMEN: %s", fname.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();

            float kb_size = (float)g_attached_doc_size_bytes / 1024.0f;
            if (kb_size < 1024.0f) {
                ImGui::TextDisabled("(%.1f KB, %d baris)", kb_size, g_attached_doc_lines);
            } else {
                ImGui::TextDisabled("(%.2f MB, %d baris)", kb_size / 1024.0f, g_attached_doc_lines);
            }
            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();

            if (ImGui::Button("Ganti File", ImVec2(80, 24))) {
                std::string chosen = OpenDocumentFileDialog(g_hMainWnd);
                if (!chosen.empty()) {
                    AttachDocument(chosen.c_str());
                }
            }

            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.60f, 0.15f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.80f, 0.20f, 0.20f, 1.0f));
            if (ImGui::Button("Batal ❌##Doc", ImVec2(75, 24))) {
                ClearAttachedDocument();
            }
            ImGui::PopStyleColor(2);

            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
            ImGui::Spacing();
        }

        // Preview Foto Awal jika sedang mode Edit Foto (Image-to-Image) / ControlNet
        if (g_has_init_image) {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.16f, 0.24f, 0.90f));
            ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.28f, 0.55f, 0.85f, 0.70f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
            ImGui::BeginChild("InitImageCard", ImVec2(right_panel_width - 12.0f, 72.0f), true, ImGuiWindowFlags_NoScrollbar);

            // Baris 1: Thumbnail, Info Foto, Tombol Ganti, Tombol Batal
            if (g_init_image_view) {
                ImGui::Image((void*)g_init_image_view, ImVec2(30, 30));
                ImGui::SameLine();
            }

            std::string fname = g_init_image_path;
            size_t slash = fname.find_last_of("\\/");
            if (slash != std::string::npos) fname = fname.substr(slash + 1);
            if (fname.length() > 24) fname = fname.substr(0, 21) + "...";

            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(ImVec4(0.38f, 0.85f, 0.98f, 1.0f), "FOTO ACUAN: %s", fname.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();

            if (ImGui::Button("Ganti Foto", ImVec2(80, 24))) {
                std::string chosen = OpenImageFileDialog(g_hMainWnd);
                if (!chosen.empty()) {
                    SetInitImage(chosen.c_str());
                }
            }

            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.60f, 0.15f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.80f, 0.20f, 0.20f, 1.0f));
            if (ImGui::Button("Batal ❌", ImVec2(75, 24))) {
                ClearInitImage();
            }
            ImGui::PopStyleColor(2);

            // Baris 2: Mode ControlNet vs Img2Img
            bool cnet_available = (GetFileAttributesA("models\\ControlNet\\control_sd15_canny.safetensors") != INVALID_FILE_ATTRIBUTES);
            if (cnet_available) {
                ImGui::Checkbox("Kunci Pose Presisi (ControlNet Canny)", &g_use_controlnet);
                HelpBadge(
                    "Kunci Pose Presisi (ControlNet Canny)",
                    "Mengekstrak kontur garis tubuh & anatomi secara otomatis (Canny Edge).\nAI dipaksa 100% mengikuti pose acuan tanpa meleset sedikitpun, sementara tekstur tubuh, pakaian, dan pencahayaan dirender baru sesuai prompt Anda.",
                    "Sangat direkomendasikan untuk ganti pakaian, cosplay, pose realistis, atau konten NSFW dengan pose yang tetap sama persis.",
                    "Bekerja optimal pada model SD1.5 & CyberRealistic V5.0."
                );
                ImGui::SameLine();
                ImGui::TextDisabled("|");
                ImGui::SameLine();
            } else {
                g_use_controlnet = false;
            }

            if (g_use_controlnet) {
                ImGui::SetNextItemWidth(140.0f);
                ImGui::SliderFloat("Kekuatan Pose##CNetStrength", &g_controlnet_strength, 0.30f, 1.00f, "%.2f");
                HelpBadgeSimple(
                    "Kekuatan Pose ControlNet",
                    "Menentukan seberapa ketat AI harus mematuhi garis tepi pose foto acuan.",
                    "Nilai 0.90 - 1.00 mengunci pose 100% presisi tanpa meleset."
                );
            } else {
                ImGui::SetNextItemWidth(140.0f);
                ImGui::SliderFloat("Kekuatan Edit##ImgStrength", &g_img2img_strength, 0.10f, 0.95f, "%.2f");
                HelpBadge(
                    "Kekuatan Edit (Denoising Strength)",
                    "Menentukan seberapa drastis AI mengubah foto asal:\n- 0.20 - 0.40 (Halus): Bentuk wajah, pose, dan rincian asli dipertahankan 90%. Hanya memoles warna/pencahayaan.\n- 0.50 - 0.65 (Seimbang): Pakaian, rambut, atau latar belakang berganti rapi sesuai prompt, sementara wajah & pose tetap mirip.\n- 0.75 - 0.95 (Drastis): AI bebas merombak total gambar Anda menjadi karya baru.",
                    "Hanya relevan saat mode Edit Foto (Image-to-Image) aktif.",
                    "Gunakan 0.60 - 0.65 untuk hasil ganti pakaian atau ganti latar belakang terbaik."
                );
            }

            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
            ImGui::Spacing();
        }

        float send_btn_w = 95.0f;
        float smart_btn_w = 145.0f;
        float input_text_w = right_panel_width - send_btn_w - smart_btn_w - 28.0f;
        if (input_text_w < 220.0f) {
            input_text_w = right_panel_width - send_btn_w - 18.0f;
            smart_btn_w = 0.0f;
        }

        ImGui::SetNextItemWidth(input_text_w);
        bool enter_pressed = ImGui::InputTextMultiline(
            "##PromptInput", g_input_prompt, sizeof(g_input_prompt),
            ImVec2(input_text_w, 52),
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CtrlEnterForNewLine
        );

        ImGui::SameLine();

        // Tombol Input Ekstra (✨ Prompt Pintar untuk Foto, atau 🎙️ Bicara untuk Suara AI)
        if (smart_btn_w > 0.0f) {
            if (g_selected_mode == 5) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.08f, 0.55f, 0.45f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.12f, 0.70f, 0.58f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.05f, 0.42f, 0.35f, 1.0f));
                if (ImGui::Button("🎙️ Bicara", ImVec2(smart_btn_w, 52))) {
                    TriggerWindowsVoiceTyping();
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Aktifkan Mikrofon: Bicara dalam Bahasa Indonesia, kata-kata Anda akan otomatis diketik ke kolom pesan!");
                }
                ImGui::PopStyleColor(3);
            } else {
                if (g_is_optimizing_prompt.load()) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.20f, 0.55f, 1.0f));
                    ImGui::Button("✨ Berpikir...", ImVec2(smart_btn_w, 52));
                    ImGui::PopStyleColor();
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.18f, 0.72f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.58f, 0.24f, 0.90f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.12f, 0.60f, 1.0f));
                    if (ImGui::Button("✨ Prompt Pintar", ImVec2(smart_btn_w, 52))) {
                        if (strlen(g_input_prompt) > 0 && !g_is_processing) {
                            g_is_optimizing_prompt.store(true);
                            std::string user_idea = g_input_prompt;
                            int current_framing = g_framing_idx;
                            if (g_optimize_thread.joinable()) g_optimize_thread.join();
                            g_optimize_thread = std::thread([user_idea, current_framing]() {
                                std::string res = GenerateSmartPrompt(user_idea, g_num_threads, current_framing);
                                if (!res.empty()) {
                                    std::lock_guard<std::mutex> lock(g_input_mutex);
                                    strncpy(g_input_prompt, res.c_str(), sizeof(g_input_prompt) - 1);
                                    g_input_prompt[sizeof(g_input_prompt) - 1] = '\0';
                                }
                                g_is_optimizing_prompt.store(false);
                            });
                        }
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Klik untuk mengubah ide singkat Anda menjadi prompt fotorealistis kaya detail dengan AI!");
                    }
                    ImGui::PopStyleColor(3);
                }
            }
            ImGui::SameLine();
        }

        if (g_is_processing) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.30f, 0.40f, 1.0f));
            ImGui::Button("Proses...", ImVec2(send_btn_w, 52));
            ImGui::PopStyleColor();
        } else {
            if ((ImGui::Button("Kirim", ImVec2(send_btn_w, 52)) || enter_pressed) && (strlen(g_input_prompt) > 0 || g_has_attached_doc)) {
                std::string actual_prompt_to_send = g_input_prompt;
                if (actual_prompt_to_send.empty()) {
                    actual_prompt_to_send = "Tolong baca, jelaskan ringkasan, dan analisis poin-poin penting dari isi dokumen ini secara terstruktur.";
                }

                std::string prompt_for_worker = actual_prompt_to_send;
                std::string doc_tag = "";
                if (g_has_attached_doc) {
                    std::string doc_name = g_attached_doc_path;
                    size_t sl = doc_name.find_last_of("\\/");
                    if (sl != std::string::npos) doc_name = doc_name.substr(sl + 1);

                    prompt_for_worker = "[DOKUMEN LAMPIRAN: " + doc_name + "]\n" + g_attached_doc_content + "\n[/DOKUMEN LAMPIRAN]\n\nPertanyaan/Instruksi Pengguna: " + actual_prompt_to_send;
                    doc_tag = "Anda (📄 " + doc_name + ")";
                }

                ChatMsg userMsg;
                userMsg.role = "User";
                userMsg.text = actual_prompt_to_send;
                userMsg.timestamp = GetCurrentTimestamp();
                if (!doc_tag.empty()) {
                    userMsg.tag = doc_tag;
                } else if (g_has_init_image) {
                    userMsg.tag = g_use_controlnet ? "Anda (ControlNet Pose)" : "Anda (Edit Foto)";
                    userMsg.image_path = g_init_image_path;
                    int uw = 0, uh = 0;
                    userMsg.texture_view = LoadTextureFromFileDX11(g_pd3dDevice, g_init_image_path, &uw, &uh);
                    userMsg.img_w = uw;
                    userMsg.img_h = uh;
                } else {
                    userMsg.tag = "Anda";
                }

                if (g_active_session_idx >= 0 && g_active_session_idx < (int)g_sessions.size()) {
                    auto& cur_sess = g_sessions[g_active_session_idx];
                    cur_sess.messages.push_back(userMsg);
                    cur_sess.last_time = userMsg.timestamp;
                    if (cur_sess.title == "Percakapan Baru" || cur_sess.title.rfind("Percakapan #", 0) == 0) {
                        std::string clean_title = actual_prompt_to_send;
                        if (clean_title.length() > 24) clean_title = clean_title.substr(0, 21) + "...";
                        cur_sess.title = clean_title;
                    }
                    SaveSessionToFile(cur_sess);
                    SaveSessionsIndex();
                }

                int target_mode = g_has_attached_doc ? 1 : g_selected_mode;

                g_is_processing = true;
                g_process_progress.store(0.05f);
                g_pending_prompt = prompt_for_worker;
                g_pending_mode = target_mode;
                g_worker_finished = false;
                g_worker_result = "";
                g_process_start_time = std::chrono::steady_clock::now();
                {
                    std::lock_guard<std::mutex> lock(g_stream_mutex);
                    g_live_stream_text = "";
                }
                {
                    std::lock_guard<std::mutex> lock(g_status_mutex);
                    g_process_status = g_has_attached_doc ? "Membaca dan menganalisis dokumen..." : (g_has_init_image ? (g_use_controlnet ? "Mempersiapkan Kunci Pose (ControlNet Canny)..." : "Mempersiapkan Edit Foto (Image-to-Image)...") : ((g_selected_mode == 2 || g_selected_mode == 3) ? "Mempersiapkan model foto C++..." : (g_selected_mode == 5) ? "Mempersiapkan Asisten Suara C++..." : "Mempersiapkan model LLM C++..."));
                }

                if (g_worker_thread.joinable()) {
                    g_worker_thread.join();
                }
                std::string current_init = (g_has_init_image && !g_has_attached_doc) ? g_init_image_path : "";
                float current_str = g_img2img_strength;
                bool current_cnet = g_use_controlnet;
                float current_cnet_str = g_controlnet_strength;
                g_worker_thread = std::thread(
                    RunInferenceWorker,
                    prompt_for_worker,
                    target_mode,
                    g_selected_llm,
                    g_selected_photo,
                    g_num_threads,
                    g_image_steps,
                    g_cfg_scale,
                    current_init,
                    current_str,
                    current_cnet,
                    current_cnet_str
                );

                g_input_prompt[0] = '\0';
            }
        }

        ImGui::Checkbox("Auto Prompt", &g_smart_prompt_auto);
        HelpBadge(
            "Auto Prompt Pintar (AI Expander)",
            "Jika Anda mengetik ide singkat (contoh: 'cewek cantik di pantai'), AI LLM lokal otomatis memperluasnya menjadi master prompt fotografi profesional berstandar industri dengan lensa kamera, sudut pandang, pencahayaan, dan detail 8K.",
            "AI Diffusion langsung menggunakan teks yang Anda ketik apa adanya tanpa penambahan/perluasan dari model LLM.",
            "AKTIFKAN (ON) terutama jika Anda suka mengetik ide singkat dalam Bahasa Indonesia agar hasilnya langsung memukau."
        );

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        ImGui::SetNextItemWidth(175.0f);
        ImGui::Combo("##FramingSelector", &g_framing_idx, g_framing_names, IM_ARRAYSIZE(g_framing_names));
        HelpBadge(
            "Framing / Sudut Pandang Kamera",
            "Pilihan framing sudut foto kamera:\n- Auto: AI menyesuaikan otomatis dari kata-kata prompt Anda.\n- Wajah (85mm): Foto potret fokus jarak dekat khusus wajah dan mata (Close-up).\n- 1/2 Badan (50mm): Foto proporsional dari pinggang ke atas (Medium shot).\n- Full Body (35mm): Foto utuh dari kepala hingga ujung kaki dengan sepatu menyentuh tanah.",
            "AI sepenuhnya mematuhi opsi yang Anda pilih di sini tanpa memotong tubuh secara sembarangan.",
            "Pilih 'Full Body (35mm)' jika ingin foto seluruh badan utuh, dan pasangkan dengan Rasio Vertikal 512x768."
        );

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        ImGui::SetNextItemWidth(170.0f);
        ImGui::Combo("##ResolutionSelector", &g_resolution_idx, g_resolutions, IM_ARRAYSIZE(g_resolutions));
        HelpBadge(
            "Rasio / Resolusi Foto",
            "Pilihan rasio & ukuran output foto:\n- 512 x 512: Persegi Kotak (1:1), proses paling cepat di CPU.\n- 512 x 768: Potret Vertikal (2:3), ruang vertikal luas SANGAT IDEAL untuk Seluruh Badan (Full Body) atau wallpaper HP.\n- 768 x 512: Lanskap Mendatar (3:2), cocok untuk pemandangan & mobil.",
            "Resolusi dikunci sesuai pilihan Anda dan TIDAK DIUBAH otomatis secara diam-diam oleh AI.",
            "Gunakan 512x768 jika mengambil foto Seluruh Badan (Full Body) agar proporsi tubuh manusia tampak sempurna."
        );

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        ImGui::Checkbox("HD Wajah & Anatomi", &g_fullbody_face_fix);
        HelpBadge(
            "HD Wajah & Anatomi (Anti-Muka Kabur & Cacat)",
            "Menajamkan wajah dan memperbaiki anatomi pada foto Seluruh Badan (Full Body):\n- Menambahkan bobot ekstra pada detail wajah (1.25x), mata simetris, dan iris jernih.\n- Mencegah deformasi tubuh, jari berlebih/kurang, dan tangan rusak.\n- Mengoptimalkan steps denoising (25 steps) agar wajah jarak jauh tidak meleleh.",
            "Prompt berjalan apa adanya tanpa koreksi anatomi otomatis (rawan muka buram di foto Full Body).",
            "SANGAT DIREKOMENDASIKAN SELALU AKTIF (ON) terutama untuk pose berdiri / Full Body."
        );

        if (!g_has_attached_doc) {
            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();

            if (ImGui::Button("📄 Lampirkan File", ImVec2(130, 24))) {
                std::string chosen = OpenDocumentFileDialog(g_hMainWnd);
                if (!chosen.empty()) {
                    AttachDocument(chosen.c_str());
                }
            }
            HelpBadge(
                "Lampirkan File / Dokumen",
                "Memilih dokumen teks, file data, atau source code dari laptop Anda untuk dibaca dan dianalisis oleh AI.\nFormat didukung: .txt, .pdf, .md, .csv, .json, .py, .c, .cpp, .log, dll.\nAnda juga bisa langsung Drag & Drop file ke dalam aplikasi!",
                "AI akan membaca isi dokumen, lalu merangkum, mencari informasi, atau menjawab pertanyaan Anda mengenai dokumen tersebut.",
                "Sangat berguna untuk membaca skripsi, menganalisis data laporan, mereview coding, atau mengekstrak isi teks."
            );
        }

        if (!g_has_init_image) {
            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();

            if (ImGui::Button("📷 Masukkan Foto", ImVec2(130, 24))) {
                std::string chosen = OpenImageFileDialog(g_hMainWnd);
                if (!chosen.empty()) {
                    SetInitImage(chosen.c_str());
                }
            }
            HelpBadge(
                "Masukkan Foto (Image-to-Image / Edit Foto)",
                "Memilih foto dari laptop Anda untuk diedit atau diubah gayanya dengan AI.\nAnda juga bisa langsung Drag & Drop (seret file foto) ke dalam aplikasi kapan saja!",
                "Mode biasa (Text-to-Image) membuat foto baru dari nol berdasarkan teks prompt saja.",
                "Gunakan fitur ini jika ingin mengubah pakaian, mengganti latar belakang, atau merombak gaya foto yang sudah ada."
            );
        }

        if (right_panel_width > 920.0f) {
            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();
            ImGui::TextDisabled("Enter: Kirim");
        }

        ImGui::EndChild(); // BottomInput

        ImGui::EndGroup(); // Panel Kanan

        // ================= MODAL JENDELA PENGATURAN (SETTINGS) =================
        if (g_show_settings) {
            ImGui::SetNextWindowSize(ImVec2(600, 520), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos(ImVec2(app_width * 0.5f - 300, app_height * 0.5f - 260), ImGuiCond_FirstUseEver);

            if (ImGui::Begin("Pengaturan Studio & Panduan Menu", &g_show_settings, ImGuiWindowFlags_NoCollapse)) {
                ImGui::TextColored(ImVec4(0.22f, 0.74f, 0.97f, 1.0f), "Konfigurasi Model & Panduan Parameter (Hover tanda (?) untuk info)");
                ImGui::Separator();

                if (ImGui::CollapsingHeader("Pilihan Model Aktif", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Text("Mode Utama:");
                    ImGui::Combo("##SettingMode", &g_selected_mode, g_mode_names, IM_ARRAYSIZE(g_mode_names));

                    ImGui::Text("Model LLM (Percakapan):");
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 30);
                    if (!g_llm_items.empty()) {
                        ImGui::Combo("##SettingLLM", &g_selected_llm, g_llm_items.data(), (int)g_llm_items.size());
                    }
                    HelpBadgeLlmModels(g_selected_llm);

                    ImGui::Text("Model Foto (Diffusion):");
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 30);
                    if (!g_photo_items.empty()) {
                        ImGui::Combo("##SettingPhoto", &g_selected_photo, g_photo_items.data(), (int)g_photo_items.size());
                    }
                    HelpBadgePhotoModels(g_selected_photo);

                    ImGui::Spacing();
                    if (ImGui::Button("📥 Pasang Model .GGUF Baru", ImVec2(210, 28))) {
                        std::string chosen = OpenModelFileDialog(g_hMainWnd);
                        if (!chosen.empty()) {
                            g_current_inspection = InspectModelFile(chosen);
                            g_show_import_modal = true;
                            g_is_importing = false;
                            g_import_progress = 0.0f;
                            g_import_status_text = "";
                        }
                    }
                    HelpBadge(
                        "Pasang Model .GGUF Baru",
                        "Buka file dialog untuk memilih model .gguf atau .safetensors dari laptop Anda. AI Studio akan membaca arsitektur model, mendeteksi kelengkapan VAE/komponen, dan menyalinnya ke folder yang sesuai.",
                        "Menyalin file secara manual melalui Windows Explorer ke folder models\\.",
                        "Gunakan tombol ini atau seret (drag-and-drop) file model langsung ke jendela aplikasi."
                    );

                    ImGui::SameLine();
                    if (ImGui::Button("🔄 Pindai Ulang", ImVec2(120, 28))) {
                        ScanModelsDirectory();
                    }
                    HelpBadge(
                        "Pindai Ulang Model (.GGUF)",
                        "Menjelajahi folder models\\ dan langsung memperbarui daftar model: model yang baru disalin akan muncul, dan model yang dihapus dari disk akan otomatis hilang.",
                        "Menggunakan daftar lama di memori tanpa sinkronisasi disk.",
                        "Klik tombol ini setelah Anda menambah atau menghapus file lewat File Explorer."
                    );

                    ImGui::SameLine();
                    if (ImGui::Button("🗑️ Hapus Model", ImVec2(120, 28))) {
                        if (g_selected_mode == 2 || g_selected_mode == 3) {
                            if (g_selected_photo >= 0 && g_selected_photo < (int)g_detected_photos.size()) {
                                g_delete_target_path = g_detected_photos[g_selected_photo].full_path;
                                g_delete_target_name = g_detected_photos[g_selected_photo].display_name;
                                g_show_delete_confirm_modal = true;
                            }
                        } else {
                            if (g_selected_llm >= 0 && g_selected_llm < (int)g_detected_llms.size()) {
                                g_delete_target_path = g_detected_llms[g_selected_llm].full_path;
                                g_delete_target_name = g_detected_llms[g_selected_llm].display_name;
                                g_show_delete_confirm_modal = true;
                            }
                        }
                    }
                    HelpBadge(
                        "Hapus Model Aktif dari Disk",
                        "Menghapus file model yang sedang dipilih secara aman dari hardisk komputer Anda untuk menghemat ruang penyimpanan laptop. Model akan langsung hilang dari dropdown.",
                        "Membuka File Explorer secara manual dan mencari folder penyimpanan.",
                        "Peringatan: File model yang dihapus tidak bisa dikembalikan kecuali Anda mengunduhnya kembali."
                    );
                }

                if (ImGui::CollapsingHeader("🖥️ Deteksi Hardware & Akselerasi GPU (NVIDIA RTX / CPU)", ImGuiTreeNodeFlags_DefaultOpen)) {
                    // 1. Info CPU & RAM
                    ImGui::TextColored(ImVec4(0.22f, 0.74f, 0.97f, 1.0f), "PROSESOR & MEMORI SISTEM");
                    
                    ImGui::Text("CPU:");
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "%s", g_hw_profile.cpu_brand.c_str());

                    ImGui::Text("Core & Thread:");
                    ImGui::SameLine();
                    ImGui::Text("%d Core Fisik | %d Thread Logikal", g_hw_profile.cpu_cores_physical, g_hw_profile.cpu_threads_logical);

                    ImGui::Text("Instruksi Hardware:");
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.4f, 1.0f), "%s  %s",
                        g_hw_profile.has_avx2 ? "✅ AVX2 (256-bit)" : "❌ Tanpa AVX2",
                        g_hw_profile.has_fma ? "✅ FMA Math" : "❌ Tanpa FMA"
                    );

                    ImGui::Text("RAM Sistem:");
                    ImGui::SameLine();
                    ImGui::Text("%d GB Total (Tersedia: %d GB)", g_hw_profile.total_ram_gb, g_hw_profile.free_ram_gb);

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    // 2. Info GPU Terdeteksi via DXGI
                    ImGui::TextColored(ImVec4(0.22f, 0.74f, 0.97f, 1.0f), "KARTU GRAFIS TERDETEKSI (DXGI DIRECTX 11)");
                    for (size_t i = 0; i < g_hw_profile.gpus.size(); i++) {
                        const auto& gpu = g_hw_profile.gpus[i];
                        ImVec4 color = gpu.is_rtx ? ImVec4(0.2f, 0.95f, 0.4f, 1.0f) : (gpu.is_nvidia ? ImVec4(0.4f, 0.8f, 1.0f, 1.0f) : ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
                        ImGui::Bullet();
                        ImGui::TextColored(color, "%s", gpu.name.c_str());
                        ImGui::SameLine();
                        if (gpu.vram_mb > 0) {
                            ImGui::TextDisabled("(VRAM: %d MB / %d GB | %s)", gpu.vram_mb, gpu.vram_gb, gpu.vendor_name.c_str());
                        } else {
                            ImGui::TextDisabled("(GPU Terintegrasi / VRAM Bersama | %s)", gpu.vendor_name.c_str());
                        }
                    }

                    ImGui::Spacing();

                    // 3. Kartu Banner Profil Kompatibilitas Otomatis
                    bool is_rtx_active = (g_hardware_backend_mode == 2 || (g_hardware_backend_mode == 0 && g_hw_profile.has_nvidia_gpu));
                    if (is_rtx_active) {
                        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.06f, 0.22f, 0.12f, 0.85f));
                        ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.20f, 0.90f, 0.40f, 0.85f));
                    } else {
                        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.06f, 0.14f, 0.24f, 0.85f));
                        ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.22f, 0.74f, 0.97f, 0.85f));
                    }
                    ImGui::BeginChild("HwProfileCard", ImVec2(ImGui::GetContentRegionAvail().x, 95), true);
                    ImGui::TextColored(is_rtx_active ? ImVec4(0.2f, 0.95f, 0.4f, 1.0f) : ImVec4(0.3f, 0.85f, 1.0f, 1.0f), "%s", g_hw_profile.compatibility_status.c_str());
                    ImGui::Spacing();
                    ImGui::TextWrapped("%s", g_hw_profile.profile_summary.c_str());
                    ImGui::EndChild();
                    ImGui::PopStyleColor(2);

                    ImGui::Spacing();

                    // 4. Tombol Pindai Ulang & Kontrol Kompatibilitas
                    if (ImGui::Button("🔍 Pindai Ulang Hardware & Terapkan Otomatis", ImVec2(330, 30))) {
                        g_hw_profile = DetectHardwareAndCompatibility();
                        ApplyHardwareCompatibilitySettings();
                    }
                    HelpBadge(
                        "Pindai Ulang Hardware & Auto-Konfigurasi",
                        "Mendeteksi ulang kartu grafis dan prosesor laptop secara instan. Jika Anda sewaktu-waktu memasang kartu grafis NVIDIA RTX atau eGPU, sistem akan otomatis beralih dari CPU ke CUDA Tensor Cores.",
                        "Menyetel manual lewat baris perintah.",
                        "Klik tombol ini setelah Anda memasang atau menancapkan kartu grafis baru ke laptop/PC."
                    );

                    ImGui::Spacing();
                    ImGui::Text("Mode Backend Komputasi:");
                    ImGui::SetNextItemWidth(280);
                    if (ImGui::Combo("##BackendModeCombo", &g_hardware_backend_mode, g_backend_mode_names, IM_ARRAYSIZE(g_backend_mode_names))) {
                        ApplyHardwareCompatibilitySettings(true);
                    }
                    HelpBadge(
                        "Mode Backend Komputasi",
                        "Memilih mesin komputasi AI:\n- Auto: Otomatis memilih NVIDIA RTX jika ada, atau CPU AVX2 jika laptop tanpa GPU diskrit.\n- CPU AVX2: Memaksa eksekusi di prosesor laptop (hemat baterai/VRAM).\n- NVIDIA RTX: Memaksa akselerasi CUDA & Tensor Cores.",
                        "Mengubah konfigurasi kode sumber aplikasi secara manual.",
                        "Pilih 'Auto' agar aplikasi selalu optimal menyesuaikan hardware yang terpasang."
                    );

                    if (is_rtx_active) {
                        ImGui::SliderInt("Layer Offload GPU (-ngl)", &g_gpu_layers, 0, 99);
                        HelpBadge(
                            "Offload Layer GPU (-ngl)",
                            "Menentukan berapa layer model LLM yang dimasukkan ke VRAM kartu grafis RTX Anda:\n- 99: Full GPU (Paling cepat, 80-120 token/detik).\n- 20-35: Mode Hybrid jika VRAM laptop terbatas (4-6 GB) agar tidak kehabisan memori.",
                            "Seluruh model diproses di RAM sistem biasa.",
                            "Gunakan 99 jika VRAM kartu grafis Anda 8 GB ke atas."
                        );
                    }

                    ImGui::SliderInt("Jumlah Thread CPU", &g_num_threads, 1, 16);
                    HelpBadgeSimple(
                        "Jumlah Thread CPU",
                        "Menentukan berapa banyak core prosesor yang digunakan untuk kalkulasi komputasi AI AVX2.",
                        "Gunakan 4 hingga 8 thread untuk prosesor Intel Core i5 laptop Anda."
                    );
                    ImGui::TextDisabled("Thread diatur sesuai core CPU Intel laptop Anda.");

                    ImGui::Checkbox("Fast VAE Decode (TAESD - Pangkas Waktu 1 Menit)", &g_use_fast_vae);
                    HelpBadge(
                        "Fast VAE (TAESD)",
                        "Decoding VAE di CPU selesai dalam ~2.7 detik tanpa pengurangan mutu kasat mata.",
                        "Decoding VAE butuh waktu ~61 detik di CPU.",
                        "SELALU AKTIFKAN (ON) untuk menghemat waktu."
                    );

                    ImGui::Checkbox("Mode Turbo Kilat (LCM 4-Steps)", &g_turbo_mode);
                    HelpBadge(
                        "Turbo LCM",
                        "Generasi 4 langkah kilat (~3-5 detik via LCM LoRA).",
                        "Generasi 20 langkah DSLR paling tajam, pori-pori kulit nyata, dan fotorealistis.",
                        "NON-AKTIFKAN (OFF) untuk model CyberRealistic V5 agar hasil paling nyata sekelas kamera DSLR."
                    );
                }

                if (ImGui::CollapsingHeader("Parameter Foto (Diffusion)", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::SliderInt("Langkah Inferensi (Steps)", &g_image_steps, 4, 30);
                    HelpBadgeSimple(
                        "Langkah Inferensi (Denoising Steps)",
                        "Jumlah tahapan AI membersihkan noise menjadi foto jernih.\n- 4 steps: Draft kilat khusus Turbo LCM.\n- 20-25 steps: Kualitas foto seimbang, tajam, dan realistis.\n- 30 steps: Detail ekstra padat.",
                        "Pilih 20 steps untuk model CyberRealistic V5."
                    );

                    ImGui::SliderFloat("CFG Guidance Scale", &g_cfg_scale, 1.0f, 15.0f, "%.1f");
                    HelpBadgeSimple(
                        "CFG Guidance Scale",
                        "Tingkat seberapa patuh AI terhadap teks prompt Anda.\n- 1.5 - 2.0: Khusus mode Turbo LCM.\n- 6.5 - 7.5: Ideal untuk foto fotorealistis manusia alami.\n- >10: Terlalu kaku, rawan warna kontras tajam/rusak.",
                        "Gunakan 7.0 untuk 20 steps, atau 1.8 jika Turbo LCM aktif."
                    );

                    ImGui::Combo("Sudut Pandang (Framing)", &g_framing_idx, g_framing_names, IM_ARRAYSIZE(g_framing_names));
                    HelpBadgeSimple(
                        "Sudut Pandang Kamera (Framing)",
                        "Pilihan sudut framing foto:\n- Auto: Menyesuaikan otomatis dari prompt teks Anda.\n- Wajah (85mm): Foto potret fokus dekat pada wajah & mata.\n- 1/2 Badan (50mm): Foto proporsional dari pinggang ke atas.\n- Full Body (35mm): Foto utuh dari kepala hingga ujung kaki dengan sepatu menyentuh tanah.",
                        "Gunakan 'Full Body' jika ingin foto seluruh badan seutuhnya."
                    );

                    ImGui::Combo("Resolusi Output", &g_resolution_idx, g_resolutions, IM_ARRAYSIZE(g_resolutions));
                    HelpBadgeSimple(
                        "Resolusi Citra Foto",
                        "- 512 x 512 px: Persegi Kotak (paling cepat di RAM/CPU laptop).\n- 512 x 768 px: Potret vertikal (SANGAT COCOK untuk Seluruh Badan / Full Body & wallpaper HP).\n- 768 x 512 px: Lanskap mendatar (cocok untuk pemandangan & mobil).",
                        "Gunakan 512x768 jika mengambil foto Seluruh Badan (Full Body) agar proporsi tubuh sempurna."
                    );

                    ImGui::Checkbox("HD Wajah & Anatomi (Anti-Muka Kabur / Cacat)", &g_fullbody_face_fix);
                    HelpBadgeSimple(
                        "HD Wajah & Anatomi (Face & Anatomy Fix)",
                        "Otomatis menyuntikkan bobot penajam wajah (1.25x), mata simetris, filter anti-cacat jari/tangan, dan optimasi 25 langkah sampling untuk foto Full Body.",
                        "SELALU AKTIFKAN (ON) terutama saat memilih sudut Full Body."
                    );

                    ImGui::SliderFloat("Kekuatan Edit Foto (Strength)", &g_img2img_strength, 0.10f, 0.95f, "%.2f");
                    HelpBadgeSimple(
                        "Kekuatan Transformasi Foto (Denoising Strength)",
                        "Seberapa besar AI mengubah foto asal saat foto diunggah:\n- 0.20 - 0.40: Menjaga kemiripan asli 90% (hanya memoles).\n- 0.50 - 0.65: Proporsional untuk ganti pakaian & ganti latar belakang.\n- 0.75 - 0.95: Rombak total sesuai prompt baru.",
                        "Gunakan 0.60 - 0.65 untuk hasil terbaik."
                    );
                }

                if (ImGui::CollapsingHeader("Pengaturan Khusus NSFW & Realistis", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::TextColored(ImVec4(0.98f, 0.35f, 0.55f, 1.0f), "Mode Uncensored 100% C++ Native Engine");
                    ImGui::Checkbox("Otomatis Sisipkan Keyword Realistis (Booster)", &g_nsfw_quality_booster);
                    HelpBadge(
                        "Keyword Realistis (Booster)",
                        "Menambahkan detail pori-pori kulit, mata jernih, dan lensa 85mm f/1.4 ke prompt.",
                        "Menggunakan prompt murni tanpa penambahan.",
                        "SELALU AKTIFKAN (ON)."
                    );

                    ImGui::Checkbox("Acak Seed Setiap Generasi (Random Variation)", &g_random_seed);
                    HelpBadge(
                        "Acak Seed",
                        "Menghasilkan variasi foto baru di setiap generasi.",
                        "Mengunci hasil foto agar identik/konsisten.",
                        "AKTIFKAN (ON)."
                    );
                    if (!g_random_seed) {
                        ImGui::InputInt("Seed Manual", &g_custom_seed);
                    }

                    ImGui::Combo("Sampling Sampler", &g_sampling_method_idx, g_sampling_methods, IM_ARRAYSIZE(g_sampling_methods));
                    HelpBadgeSimple(
                        "Algoritma Sampling Sampler",
                        "- euler_a: Halus, organik, sangat cocok untuk wajah manusia fotorealistis.\n- euler: Standar deterministik.\n- dpm++2m: Sangat tajam untuk arsitektur/objek.\n- lcm: Khusus 4-step kilat.",
                        "Pilih euler_a untuk CyberRealistic V5."
                    );

                    ImGui::Text("Negative Prompt Khusus Realistis / NSFW:");
                    HelpBadgeSimple(
                        "Negative Prompt (Penyaring Cacat)",
                        "Daftar hal yang DILARANG muncul di gambar (mencegah jari lebih dari 5, mata juling, wajah rusak, atau bentuk kartun 3D).",
                        "Biarkan teks default yang sudah disediakan untuk hasil manusia terbaik."
                    );
                    ImGui::InputTextMultiline("##NegativePromptInput", g_negative_prompt, sizeof(g_negative_prompt), ImVec2(-1, 60));
                    ImGui::TextDisabled("Negative prompt menyaring kecacatan anatomi, jari rusak, atau gaya kartun.");
                }

                if (ImGui::CollapsingHeader("Opsi Fitur Tambahan")) {
                    ImGui::Checkbox("Pencarian Web Otomatis", &g_search_internet);
                    HelpBadge(
                        "Pencarian Web",
                        "Mencari data online dari internet sebelum menjawab.",
                        "100% Offline tanpa internet.",
                        "AKTIFKAN hanya jika butuh berita terkini."
                    );

                    ImGui::Checkbox("Output Suara / TTS", &g_voice_enabled);
                    HelpBadge(
                        "Output Suara TTS",
                        "AI dapat membacakan teks respon menggunakan suara Windows SAPI offline native.",
                        "Hanya menampilkan teks tanpa suara audio.",
                        "AKTIFKAN jika ingin mendengar suara AI."
                    );

                    ImGui::Checkbox("Mode Agen Otonom", &g_agent_mode);
                    HelpBadge(
                        "Mode Agen Otonom",
                        "AI mengeksekusi tugas multi-langkah secara mandiri.",
                        "AI hanya menjawab interaksi langkah demi langkah.",
                        "AKTIFKAN untuk tugas cerdas."
                    );
                }

                ImGui::Separator();
                if (ImGui::Button("Simpan & Tutup Pengaturan", ImVec2(-1, 38))) {
                    g_show_settings = false;
                }
            }
            ImGui::End();
        }

        // ================= MODAL DETEKSI & IMPORT MODEL GGUF =================
        RenderModelImportModal();

        // ================= MODAL KONFIRMASI HAPUS MODEL =================
        RenderDeleteConfirmModal();

        ImGui::End(); // RootWindow

        // Rendering DirectX 11
        ImGui::Render();
        const float clear_color_with_alpha[4] = { 0.043f, 0.059f, 0.098f, 1.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Present with VSync (60 FPS)
        HRESULT hr = g_pSwapChain->Present(1, 0);
        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
            bDone = true;
        }
    }

    // 6. Cleanup & Pelepasan Resource
    if (g_worker_thread.joinable()) {
        g_worker_thread.detach();
    }
    for (auto& sess : g_sessions) {
        for (auto& msg : sess.messages) {
            if (msg.texture_view) {
                msg.texture_view->Release();
                msg.texture_view = NULL;
            }
        }
    }
    if (g_init_image_view) {
        g_init_image_view->Release();
        g_init_image_view = NULL;
    }
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    Core_Shutdown();

    if (g_logf) { fclose(g_logf); g_logf = NULL; }

    return 0;
}

// Direct3D 11 Helper Functions
bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(
        NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags,
        featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain,
        &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext
    );
    if (res == DXGI_ERROR_UNSUPPORTED) {
        res = D3D11CreateDeviceAndSwapChain(
            NULL, D3D_DRIVER_TYPE_WARP, NULL, createDeviceFlags,
            featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain,
            &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext
        );
    }
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
}

void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
        case WM_ACTIVATE:
            if (LOWORD(wParam) != WA_INACTIVE) {
                ScanModelsDirectory();
            }
            return 0;
        case WM_DROPFILES:
        {
            HDROP hDrop = (HDROP)wParam;
            char droppedFile[MAX_PATH] = { 0 };
            if (DragQueryFileA(hDrop, 0, droppedFile, sizeof(droppedFile))) {
                std::string path_str = droppedFile;
                std::string lower_path = path_str;
                for (char& c : lower_path) c = (char)tolower(c);
                if (lower_path.find(".gguf") != std::string::npos || lower_path.find(".safetensors") != std::string::npos) {
                    g_current_inspection = InspectModelFile(path_str);
                    g_show_import_modal = true;
                    g_is_importing = false;
                    g_import_progress = 0.0f;
                    g_import_status_text = "";
                } else if (lower_path.find(".png") != std::string::npos ||
                           lower_path.find(".jpg") != std::string::npos ||
                           lower_path.find(".jpeg") != std::string::npos ||
                           lower_path.find(".webp") != std::string::npos ||
                           lower_path.find(".bmp") != std::string::npos) {
                    SetInitImage(droppedFile);
                } else {
                    AttachDocument(droppedFile);
                }
            }
            DragFinish(hDrop);
            return 0;
        }
        case WM_SIZE:
            if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED) {
                CleanupRenderTarget();
                g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
                CreateRenderTarget();
            }
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU)
                return 0;
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

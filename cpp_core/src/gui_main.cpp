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
#include "../include/memory_manager.h"
#include "../include/hardware_telemetry.h"
#include "../imgui/imgui_internal.h"

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
int g_selected_llm = 0;
int g_selected_photo = 0;
static bool g_search_internet = false;
static bool g_voice_enabled = false;
static bool g_auto_speak = false; // Default: OFF (AI diam dan TIDAK ngomong sendiri kecuali disuruh)
static bool g_agent_mode = true;
static CoreTelemetry g_telemetry = {};

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
bool g_use_fast_vae = true; // Gunakan TAESD (~2 detik vs 61 detik di CPU)
static bool g_turbo_mode = false;  // Mode Turbo LCM 4-Step (Off secara default untuk foto DSLR 20-step paling tajam)
static bool g_use_realism_lora = true; // LoRA Detail Kulit & Realisme Alami (add_detail)
static float g_realism_lora_strength = 0.75f;
static int g_prompt_assist_mode = 0; // 0: Prompt Asli (Raw), 1: Koreksi & Rapikan Saja, 2: Auto Kembangkan (AI)
static const char* g_prompt_assist_names[] = {
    "Prompt Asli (Raw)",
    "Koreksi & Rapikan",
    "Auto Kembangkan (AI)"
};
static bool g_smart_prompt_auto = false; // backward-compat flag
static std::atomic<bool> g_is_optimizing_prompt(false);
static std::thread g_optimize_thread;
static std::mutex g_input_mutex;

// Settings Modal & Parameters
static bool g_show_settings = false;
int g_num_threads = 4;
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

// Fullscreen Image Modal Globals
static bool g_show_fullscreen_image_modal = false;
static ID3D11ShaderResourceView* g_fullscreen_image_view = NULL;
static std::string g_fullscreen_image_path = "";
static int g_fullscreen_img_w = 512;
static int g_fullscreen_img_h = 512;

// Window Fullscreen (Borderless F11)
static bool g_is_borderless_fullscreen = false;
static WINDOWPLACEMENT g_wp_prev = { sizeof(g_wp_prev) };

void ToggleBorderlessFullscreen(HWND hwnd) {
    if (!hwnd) return;
    DWORD dwStyle = GetWindowLong(hwnd, GWL_STYLE);
    if (!g_is_borderless_fullscreen) {
        MONITORINFO mi = { sizeof(mi) };
        if (GetWindowPlacement(hwnd, &g_wp_prev) &&
            GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
            SetWindowLong(hwnd, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(hwnd, HWND_TOP,
                         mi.rcMonitor.left, mi.rcMonitor.top,
                         mi.rcMonitor.right - mi.rcMonitor.left,
                         mi.rcMonitor.bottom - mi.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            g_is_borderless_fullscreen = true;
        }
    } else {
        SetWindowLong(hwnd, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(hwnd, &g_wp_prev);
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        g_is_borderless_fullscreen = false;
    }
}

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

static std::string ReadArchiveEntryViaTar(const std::string& archivePath, const std::string& entryName, size_t maxBytes = 150000) {
    char cmd[MAX_PATH + 200];
    snprintf(cmd, sizeof(cmd), "tar.exe -xf \"%s\" -O \"%s\"", archivePath.c_str(), entryName.c_str());
    FILE* pipe = _popen(cmd, "rb");
    if (!pipe) return "";
    std::string out;
    char buf[1024];
    while (size_t n = fread(buf, 1, sizeof(buf), pipe)) {
        out.append(buf, n);
        if (out.size() > maxBytes) break;
    }
    _pclose(pipe);
    return out;
}

static std::string ExtractDocxContent(const std::string& path) {
    std::string xml = ReadArchiveEntryViaTar(path, "word/document.xml", 200000);
    if (xml.empty()) return "";
    std::string text;
    bool in_tag = false;
    std::string tag;
    for (size_t i = 0; i < xml.size(); ++i) {
        char c = xml[i];
        if (c == '<') {
            in_tag = true;
            tag.clear();
        } else if (c == '>') {
            in_tag = false;
            if (tag == "/w:p" || tag == "w:p") {
                if (!text.empty() && text.back() != '\n') text += '\n';
            } else if (tag == "w:tab" || tag == "w:tab/") {
                text += '\t';
            }
        } else {
            if (in_tag) tag += c;
            else text += c;
        }
    }
    return text;
}

static std::string ExtractXlsxContent(const std::string& path) {
    // 1. Ekstrak shared strings tabel Excel
    std::string sst_xml = ReadArchiveEntryViaTar(path, "xl/sharedStrings.xml", 150000);
    std::vector<std::string> sst;
    if (!sst_xml.empty()) {
        bool in_t = false;
        std::string cur_str;
        for (size_t i = 0; i < sst_xml.size(); ++i) {
            char c = sst_xml[i];
            if (c == '<') {
                std::string ttag;
                while (i < sst_xml.size() && sst_xml[i] != '>') {
                    ttag += sst_xml[i++];
                }
                if (ttag == "<t" || ttag.rfind("<t ", 0) == 0) {
                    in_t = true;
                    cur_str.clear();
                } else if (ttag == "</t") {
                    in_t = false;
                    sst.push_back(cur_str);
                }
            } else if (in_t) {
                cur_str += c;
            }
        }
    }

    // 2. Ekstrak baris dan sel sheet1
    std::string sheet_xml = ReadArchiveEntryViaTar(path, "xl/worksheets/sheet1.xml", 200000);
    if (sheet_xml.empty()) return "";

    std::string table = "";
    std::string cur_cell_type;
    std::string cur_val;
    bool in_v = false;
    bool first_in_row = true;

    for (size_t i = 0; i < sheet_xml.size(); ++i) {
        char c = sheet_xml[i];
        if (c == '<') {
            std::string tag;
            while (i < sheet_xml.size() && sheet_xml[i] != '>') {
                tag += sheet_xml[i++];
            }
            if (tag.rfind("<c ", 0) == 0) {
                if (tag.find("t=\"s\"") != std::string::npos) {
                    cur_cell_type = "s";
                } else {
                    cur_cell_type = "";
                }
            } else if (tag == "<v") {
                in_v = true;
                cur_val.clear();
            } else if (tag == "</v") {
                in_v = false;
                std::string cell_display = cur_val;
                if (cur_cell_type == "s") {
                    try {
                        int idx = std::stoi(cur_val);
                        if (idx >= 0 && idx < (int)sst.size()) {
                            cell_display = sst[idx];
                        }
                    } catch (...) {}
                }
                if (!first_in_row) table += " | ";
                table += cell_display;
                first_in_row = false;
            } else if (tag == "</row") {
                table += "\n";
                first_in_row = true;
            }
        } else if (in_v) {
            cur_val += c;
        }
    }
    return table;
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
    if (ext == ".docx") {
        clean_text = ExtractDocxContent(path_str);
        if (clean_text.empty()) {
            clean_text = "[Dokumen Word kosong atau tidak dapat diekstrak]";
        }
    } else if (ext == ".xlsx") {
        clean_text = ExtractXlsxContent(path_str);
        if (clean_text.empty()) {
            clean_text = "[File Excel kosong atau tidak dapat diekstrak]";
        }
    } else if (ext == ".pdf") {
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

    if (clean_text.length() > MAX_READ_BYTES) {
        clean_text = clean_text.substr(0, MAX_READ_BYTES) + "\n\n...[Isi dokumen dipotong karena ukuran melebihi batas 45 KB]...";
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
    ofn.lpstrFilter = "Semua Dokumen (*.docx;*.xlsx;*.csv;*.pdf;*.txt;*.md)\0*.docx;*.xlsx;*.csv;*.pdf;*.txt;*.md;*.json;*.py;*.c;*.cpp;*.log;*.h;*.hpp;*.html;*.js;*.bat;*.ini;*.xml;*.yaml\0Microsoft Word (*.docx)\0*.docx\0Microsoft Excel (*.xlsx;*.csv)\0*.xlsx;*.csv;*.tsv\0PDF (*.pdf)\0*.pdf\0File Teks (*.txt;*.md;*.log)\0*.txt;*.md;*.log\0Kode Pemrograman (*.c;*.cpp;*.py;*.js)\0*.c;*.cpp;*.py;*.js;*.html;*.h;*.hpp\0Semua File (*.*)\0*.*\0";
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

#include "../include/prompt_engine.h"

// ================= HARDWARE & SYSTEM COMPATIBILITY ENGINE (RTX / CPU) =================
#include "../include/hardware_detector.h"

// ================= REGISTRI MODEL AI & AUTO-DETEKSI GGUF / SAFETENSORS =================
#include "../include/model_manager.h"


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

    // Mode Penanganan Prompt:
    // 0: ⚡ Prompt Asli Langsung (Raw) -> Tanpa LLM, tanpa penambahan konsep/kata otomatis
    // 1: ✍️ Koreksi / Rapikan Saja -> LLM menerjemahkan & membersihkan ejaan, 100% setia tanpa halusinasi
    // 2: ✨ Kembangkan Cerdas (AI) -> LLM memperkaya detail fotografi & pencahayaan dengan setia

    if (is_photo && g_prompt_assist_mode == 1) {
        // Mode 1: Koreksi / Rapikan Ejaan
        g_process_progress.store(0.10f);
        {
            std::lock_guard<std::mutex> lock(g_status_mutex);
            g_process_status = "✍️ Mengoreksi & merapikan ejaan prompt...";
        }
        std::string corrected = GenerateCorrectedPrompt(prompt, num_threads);
        if (!corrected.empty()) {
            smart_prompt = corrected;
        }
    } else if (is_photo && g_prompt_assist_mode == 2) {
        // Mode 2: Kembangkan Cerdas
        g_process_progress.store(0.10f);
        {
            std::lock_guard<std::mutex> lock(g_status_mutex);
            g_process_status = "✨ Mengembangkan ide menjadi Prompt Fotografi Cerdas...";
        }
        std::string enriched = GenerateSmartPrompt(prompt, num_threads, g_framing_idx);
        if (!enriched.empty()) {
            smart_prompt = enriched;
        }
    }

    // Enhance prompt dengan keywords realistik HANYA jika Mode 2 (Kembangkan Cerdas) aktif
    std::string final_prompt = smart_prompt;
    if (is_photo && (actual_mode == 3 || actual_mode == 2) && g_prompt_assist_mode == 2 && g_nsfw_quality_booster) {
        std::string lower_p = final_prompt;
        for (char& ch : lower_p) ch = (char)tolower(ch);
        if (lower_p.find("photorealistic") == std::string::npos && 
            lower_p.find("realistic") == std::string::npos) {
            final_prompt += ", ultra realistic, 8k uhd, photorealistic, natural skin texture, realistic detailed face, beautiful expressive eyes, micro skin pores, masterpiece, raw candid photo";
        }
    }

    // Terapkan framing eksplisit jika opsi manual dipilih oleh pengguna (Close-up, Medium, Full Body)
    if (is_photo && (actual_mode == 3 || actual_mode == 2) && g_framing_idx != 0) {
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

        if (is_fullbody_shot && g_fullbody_face_fix && g_prompt_assist_mode != 0) {
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
        float actual_cfg = cfg;
        std::string lora_dir_arg = "";
        if (GetFileAttributesA("models\\LoRA") != INVALID_FILE_ATTRIBUTES) {
            lora_dir_arg = "--lora-model-dir \"models\\LoRA\"";
        }

        std::string final_safe_prompt = safe_prompt;

        // Model LCM atau Mode Turbo: BEBAS ATUR STEPS (tidak lagi dikunci 4 steps!)
        bool is_lcm_model = (model_path.find("LCM") != std::string::npos || model_path.find("lcm") != std::string::npos);
        if (g_turbo_mode || is_lcm_model) {
            // Patuhi 100% nilai steps pilihan pengguna! Jika belum diatur, default ke 4 untuk LCM
            actual_steps = (steps > 0) ? steps : 4;

            // CFG adaptif agar gambar tetap halus dan tidak gosong/over-saturated saat steps dinaikkan
            if (actual_steps <= 8) {
                actual_cfg = 1.8f;
            } else if (actual_steps <= 15) {
                actual_cfg = 2.5f;
            } else {
                actual_cfg = 3.5f;
            }

            // Jika model standar SD 1.5 dijalankan di mode Turbo, lampirkan LoRA LCM agar proses cepat tetap tajam
            if (!is_lcm_model && GetFileAttributesA("models\\LoRA\\lcm_lora_sd15.safetensors") != INVALID_FILE_ATTRIBUTES) {
                if (final_safe_prompt.find("lcm_lora_sd15") == std::string::npos) {
                    final_safe_prompt += " <lora:lcm_lora_sd15:1.0>";
                }
            }
        }

        // LoRA Realisme Kulit & Wajah Alami (add_detail)
        if (g_use_realism_lora && GetFileAttributesA("models\\LoRA\\add_detail.safetensors") != INVALID_FILE_ATTRIBUTES) {
            if (final_safe_prompt.find("add_detail") == std::string::npos) {
                char lbuf[64];
                snprintf(lbuf, sizeof(lbuf), " <lora:add_detail:%.2f>", g_realism_lora_strength);
                final_safe_prompt += lbuf;
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
                final_safe_prompt.c_str(), actual_steps, out_w, out_h, num_threads, actual_cfg, seed_val, out_img_path.c_str()
            );
        } else {
            std::string vae_arg = "";
            if (g_use_fast_vae && GetFileAttributesA("models\\VAE\\taesd.safetensors") != INVALID_FILE_ATTRIBUTES) {
                vae_arg = "--taesd \"models\\VAE\\taesd.safetensors\"";
            } else if (GetFileAttributesA("models\\VAE\\vae-ft-mse-840000-ema-pruned.safetensors") != INVALID_FILE_ATTRIBUTES) {
                vae_arg = "--vae \"models\\VAE\\vae-ft-mse-840000-ema-pruned.safetensors\"";
            }

            snprintf(cmd, sizeof(cmd),
                "sd_bin\\sd-cli.exe -m \"%s\" %s %s %s %s %s -p \"%s\" --negative-prompt \"%s\" --steps %d -W %d -H %d -t %d --cfg-scale %.1f -s %d --sampling-method %s -o \"%s\"",
                model_path.c_str(), backend_arg.c_str(), vae_arg.c_str(), lora_dir_arg.c_str(), controlnet_arg.c_str(), img2img_arg.c_str(), final_safe_prompt.c_str(), safe_neg.c_str(), actual_steps, out_w, out_h, num_threads, actual_cfg, seed_val, sampler_flag, out_img_path.c_str()
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
            std::string clean_resp = CleanLlamaStreamOutput(full_output);
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
    style.ChildRounding     = 8.0f;
    style.FrameRounding     = 6.0f;
    style.PopupRounding     = 8.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabRounding      = 6.0f;
    style.TabRounding       = 6.0f;
    style.FramePadding      = ImVec2(10, 6);
    style.ItemSpacing       = ImVec2(8, 6);
    style.ItemInnerSpacing  = ImVec2(6, 4);
    style.TouchExtraPadding = ImVec2(0, 0);  // 0 Padding tambahan agar perhitungan grid presisi 100% anti-tabrak!
    style.WindowPadding     = ImVec2(12, 10);
    style.WindowBorderSize  = 0.0f;
    style.ChildBorderSize   = 1.0f;
    style.FrameBorderSize   = 1.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg]             = ImVec4(0.035f, 0.050f, 0.080f, 1.00f); // #090D14
    colors[ImGuiCol_ChildBg]              = ImVec4(0.055f, 0.080f, 0.130f, 0.95f); // #0E1421
    colors[ImGuiCol_PopupBg]              = ImVec4(0.070f, 0.100f, 0.160f, 0.98f); // #121A29
    colors[ImGuiCol_Border]               = ImVec4(0.140f, 0.190f, 0.280f, 0.70f); // #243047
    colors[ImGuiCol_BorderShadow]         = ImVec4(0.000f, 0.000f, 0.000f, 0.00f);
    colors[ImGuiCol_FrameBg]              = ImVec4(0.080f, 0.120f, 0.190f, 1.00f); // #141F30
    colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.120f, 0.180f, 0.280f, 1.00f);
    colors[ImGuiCol_FrameBgActive]        = ImVec4(0.150f, 0.240f, 0.380f, 1.00f);
    colors[ImGuiCol_TitleBg]              = ImVec4(0.055f, 0.080f, 0.130f, 1.00f);
    colors[ImGuiCol_TitleBgActive]        = ImVec4(0.070f, 0.110f, 0.180f, 1.00f);
    colors[ImGuiCol_Button]               = ImVec4(0.130f, 0.350f, 0.750f, 0.90f); // Modern Electric Blue
    colors[ImGuiCol_ButtonHovered]        = ImVec4(0.180f, 0.450f, 0.900f, 1.00f);
    colors[ImGuiCol_ButtonActive]         = ImVec4(0.100f, 0.280f, 0.650f, 1.00f);
    colors[ImGuiCol_Header]               = ImVec4(0.100f, 0.150f, 0.240f, 1.00f);
    colors[ImGuiCol_HeaderHovered]        = ImVec4(0.160f, 0.240f, 0.380f, 1.00f);
    colors[ImGuiCol_HeaderActive]         = ImVec4(0.130f, 0.350f, 0.750f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.035f, 0.050f, 0.080f, 0.50f);
    colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.160f, 0.220f, 0.320f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.240f, 0.340f, 0.500f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.200f, 0.500f, 0.950f, 1.00f);
    colors[ImGuiCol_CheckMark]            = ImVec4(0.250f, 0.750f, 1.000f, 1.00f); // #40C0FF
    colors[ImGuiCol_SliderGrab]           = ImVec4(0.250f, 0.750f, 1.000f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.350f, 0.850f, 1.000f, 1.00f);
    colors[ImGuiCol_Text]                 = ImVec4(0.940f, 0.960f, 0.980f, 1.00f);
    colors[ImGuiCol_TextDisabled]         = ImVec4(0.500f, 0.580f, 0.680f, 1.00f);
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

static ID3D11ShaderResourceView* g_hub_custom_tex_chat = NULL;
static ID3D11ShaderResourceView* g_hub_custom_tex_photo = NULL;
static ID3D11ShaderResourceView* g_hub_custom_tex_nsfw  = NULL;
static ID3D11ShaderResourceView* g_hub_custom_tex_voice = NULL;

static void SetCardCover(int target_mode, const std::string& path) {
    if (!g_pd3dDevice || path.empty()) return;
    int w = 0, h = 0;
    ID3D11ShaderResourceView* tex = LoadTextureFromFileDX11(g_pd3dDevice, path.c_str(), &w, &h);
    if (!tex) return;

    if (target_mode == 1) {
        if (g_hub_custom_tex_chat) g_hub_custom_tex_chat->Release();
        g_hub_custom_tex_chat = tex;
    } else if (target_mode == 2) {
        if (g_hub_custom_tex_photo) g_hub_custom_tex_photo->Release();
        g_hub_custom_tex_photo = tex;
    } else if (target_mode == 3) {
        if (g_hub_custom_tex_nsfw) g_hub_custom_tex_nsfw->Release();
        g_hub_custom_tex_nsfw = tex;
    } else if (target_mode == 5) {
        if (g_hub_custom_tex_voice) g_hub_custom_tex_voice->Release();
        g_hub_custom_tex_voice = tex;
    }
}


// Enum Kategori Warna Aksen Dashboard Card
enum CardCategoryTheme {
    CARD_THEME_CHAT = 0,   // Cyan   (Chat AI & Koding)
    CARD_THEME_PHOTO,      // Green  (Studio Foto Diffusion)
    CARD_THEME_ADULT,      // Red    (Dewasa 18+ Uncensored)
    CARD_THEME_VOICE       // Purple (Suara Manusia & TTS)
};

/**
 * @brief Menggambar kartu dashboard interaktif dengan border glowing dan badge spesifikasi.
 */
static bool DrawDashboardCard(
    const char* card_id,
    const char* title,
    const char* active_model,
    const char* description,
    const char* action_text,
    ID3D11ShaderResourceView* thumbnail_srv,
    CardCategoryTheme theme,
    const char* spec_badge_1,
    const char* spec_badge_2,
    int target_mode,
    float card_w = 0.0f,
    float card_h = 108.0f
) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    ImDrawList* draw_list = window->DrawList;

    if (card_w <= 0.0f) {
        card_w = ImGui::GetContentRegionAvail().x;
        if (card_w < 320.0f) card_w = 320.0f;
    }

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size(card_w, card_h);
    ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
    ImGui::ItemSize(size, style.FramePadding.y);

    const ImGuiID id = window->GetID(card_id);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered = false;
    bool held = false;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

    if (hovered) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }

    ImVec4 accent_color;
    const char* category_label = "MODE";

    switch (theme) {
        case CARD_THEME_CHAT:
            accent_color = ImVec4(0.22f, 0.74f, 0.97f, 1.0f); // Cyan #38bdf8
            category_label = "PERCAKAPAN & KODING";
            break;
        case CARD_THEME_PHOTO:
            accent_color = ImVec4(0.06f, 0.72f, 0.50f, 1.0f); // Emerald #10b981
            category_label = "STUDIO FOTOGRAFI";
            break;
        case CARD_THEME_ADULT:
            accent_color = ImVec4(0.96f, 0.25f, 0.37f, 1.0f); // Rose #f43f5e
            category_label = "DEWASA (18+ UNCENSORED)";
            break;
        case CARD_THEME_VOICE:
        default:
            accent_color = ImVec4(0.66f, 0.33f, 0.97f, 1.0f); // Violet #a855f7
            category_label = "SUARA AI & AUDIO";
            break;
    }

    ImU32 col_bg = hovered ? ImColor(26, 36, 54, 250) : ImColor(15, 23, 42, 240);
    ImU32 col_border = hovered 
        ? ImColor(accent_color.x, accent_color.y, accent_color.z, 0.90f)
        : ImColor(45, 58, 78, 160);

    const float round_r = 10.0f;

    // Glowing Border Pass (2 Lapisan saat Hover)
    if (hovered) {
        draw_list->AddRect(
            ImVec2(bb.Min.x - 2.0f, bb.Min.y - 2.0f),
            ImVec2(bb.Max.x + 2.0f, bb.Max.y + 2.0f),
            ImColor(accent_color.x, accent_color.y, accent_color.z, 0.16f),
            round_r + 2.0f, 0, 3.0f
        );
        draw_list->AddRect(
            ImVec2(bb.Min.x - 1.0f, bb.Min.y - 1.0f),
            ImVec2(bb.Max.x + 1.0f, bb.Max.y + 1.0f),
            ImColor(accent_color.x, accent_color.y, accent_color.z, 0.35f),
            round_r + 1.0f, 0, 1.5f
        );
    }

    draw_list->AddRectFilled(bb.Min, bb.Max, col_bg, round_r);
    draw_list->AddRect(bb.Min, bb.Max, col_border, round_r, 0, hovered ? 1.5f : 1.0f);

    // Garis Aksen Vertikal Kiri
    draw_list->AddRectFilled(
        ImVec2(bb.Min.x, bb.Min.y + 10.0f),
        ImVec2(bb.Min.x + 4.0f, bb.Max.y - 10.0f),
        ImColor(accent_color),
        2.0f
    );

    // Thumbnail Kiri
    const float thumb_size = 80.0f;
    ImVec2 thumb_min(bb.Min.x + 14.0f, bb.Min.y + (card_h - thumb_size) * 0.5f);
    ImVec2 thumb_max(thumb_min.x + thumb_size, thumb_min.y + thumb_size);

    if (thumbnail_srv) {
        draw_list->AddImageRounded(
            (ImTextureID)thumbnail_srv,
            thumb_min, thumb_max,
            ImVec2(0, 0), ImVec2(1, 1),
            IM_COL32_WHITE, 8.0f
        );
        draw_list->AddRect(thumb_min, thumb_max, ImColor(accent_color.x, accent_color.y, accent_color.z, 0.40f), 8.0f);
    } else {
        draw_list->AddRectFilled(thumb_min, thumb_max, ImColor(20, 29, 46, 255), 8.0f);
        draw_list->AddRect(thumb_min, thumb_max, ImColor(accent_color.x * 0.5f, accent_color.y * 0.5f, accent_color.z * 0.5f, 0.6f), 8.0f);

        ImVec2 center(thumb_min.x + thumb_size * 0.5f, thumb_min.y + thumb_size * 0.5f);
        draw_list->AddCircleFilled(center, 14.0f, ImColor(accent_color.x, accent_color.y, accent_color.z, 0.25f));
        draw_list->AddCircle(center, 14.0f, ImColor(accent_color), 0, 1.5f);

        const char* hint_txt = "[+ Pilih]";
        ImVec2 hint_sz = ImGui::CalcTextSize(hint_txt);
        draw_list->AddText(ImVec2(center.x - hint_sz.x * 0.5f, center.y + 18.0f), ImColor(accent_color), hint_txt);
    }

    // Tombol Thumbnail Ganti Sampul
    if (ImGui::IsMouseHoveringRect(thumb_min, thumb_max) && ImGui::IsMouseClicked(0)) {
        std::string chosen = OpenImageFileDialog(g_hMainWnd);
        if (!chosen.empty()) {
            SetCardCover(target_mode, chosen);
        }
        return false; // Jangan navigasi mode saat hanya ingin ganti cover
    }

    // Badge / Pil Spesifikasi di Pojok Kanan Atas
    float cur_badge_x = bb.Max.x - 14.0f;
    const float badge_y = bb.Min.y + 12.0f;
    const float badge_h = 20.0f;

    auto DrawSpecPill = [&](const char* text, ImU32 text_col) {
        if (!text || text[0] == 0) return;
        ImVec2 txt_sz = ImGui::CalcTextSize(text);
        float pill_w = txt_sz.x + 14.0f;
        cur_badge_x -= pill_w;

        ImVec2 pill_min(cur_badge_x, badge_y);
        ImVec2 pill_max(cur_badge_x + pill_w, badge_y + badge_h);

        draw_list->AddRectFilled(pill_min, pill_max, ImColor(30, 41, 59, 230), 10.0f);
        draw_list->AddRect(pill_min, pill_max, ImColor(71, 85, 105, 180), 10.0f, 0, 1.0f);
        draw_list->AddText(
            ImVec2(pill_min.x + 7.0f, pill_min.y + (badge_h - txt_sz.y) * 0.5f),
            text_col, text
        );
        cur_badge_x -= 8.0f;
    };

    if (spec_badge_2 && spec_badge_2[0] != 0) {
        DrawSpecPill(spec_badge_2, ImColor(148, 163, 184));
    }
    if (spec_badge_1 && spec_badge_1[0] != 0) {
        DrawSpecPill(spec_badge_1, ImColor(accent_color));
    }

    // Konten Teks Tengah
    float content_x = thumb_max.x + 16.0f;
    draw_list->AddText(ImVec2(content_x, bb.Min.y + 12.0f), ImColor(accent_color), category_label);
    draw_list->AddText(ImVec2(content_x, bb.Min.y + 28.0f), ImColor(241, 245, 249), title);
    draw_list->AddText(ImVec2(content_x, bb.Min.y + 49.0f), ImColor(148, 163, 184), active_model);
    draw_list->AddText(ImVec2(content_x, bb.Min.y + 68.0f), ImColor(100, 116, 139), description);

    // Tombol Aksi Kanan Bawah
    ImVec2 act_sz = ImGui::CalcTextSize(action_text);
    float btn_w = act_sz.x + 22.0f;
    float btn_h = 28.0f;
    ImVec2 btn_min(bb.Max.x - btn_w - 14.0f, bb.Max.y - btn_h - 12.0f);
    ImVec2 btn_max(bb.Max.x - 14.0f, bb.Max.y - 12.0f);

    ImU32 col_btn_bg = hovered
        ? ImColor(accent_color.x, accent_color.y, accent_color.z, 0.28f)
        : ImColor(30, 41, 59, 180);
    ImU32 col_btn_border = hovered ? ImColor(accent_color) : ImColor(51, 65, 85, 180);

    draw_list->AddRectFilled(btn_min, btn_max, col_btn_bg, 6.0f);
    draw_list->AddRect(btn_min, btn_max, col_btn_border, 6.0f, 0, 1.0f);
    draw_list->AddText(
        ImVec2(btn_min.x + 11.0f, btn_min.y + (btn_h - act_sz.y) * 0.5f),
        hovered ? ImColor(255, 255, 255) : ImColor(accent_color),
        action_text
    );

    return pressed;
}

// Render Dashboard Card Hub (Modern Direct3D 11 Vector Cards)
static void RenderDashboardHub(float panel_w, float panel_h) {
    ImGui::BeginChild("DashboardHub", ImVec2(panel_w, panel_h), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

    ImGui::Dummy(ImVec2(0, 6));

    // Header Title
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 10));
    ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "KATALOG STUDIO & MODE AI");
    ImGui::TextDisabled("Pilih mode di bawah untuk langsung membuka studio. Pure C++ Native Engine (100%% Offline).");
    ImGui::PopStyleVar();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    float card_w = panel_w - 24.0f;

    // Model Teks Aktif
    std::string active_llm = (!g_llm_items.empty() && g_selected_llm < (int)g_llm_items.size()) ? g_llm_items[g_selected_llm] : "Qwen2.5-Coder 1.5B Instruct";
    std::string chat_sub = active_llm + " - Penalaran Logika & Dokumen";

    // Model Foto Aktif
    std::string active_photo = (!g_photo_items.empty() && g_selected_photo < (int)g_photo_items.size()) ? g_photo_items[g_selected_photo] : "CyberRealistic V5.0";
    std::string photo_sub = active_photo + " - Potret Manusia DSLR 85mm";

    // Kartu 1: Percakapan Bebas & Koding (Cyan)
    if (DrawDashboardCard(
        "Card_Chat",
        "Percakapan Bebas & Koding",
        chat_sub.c_str(),
        "Asisten koding responsif, bedah bug algoritma C++, dan tanya jawab dokumen.",
        "Buka Chat ->",
        g_hub_custom_tex_chat,
        CARD_THEME_CHAT,
        "Q4_K_M",
        "RAM: ~1.2 GB",
        1,
        card_w
    )) {
        g_selected_mode = 1;
    }

    ImGui::Spacing();

    // Kartu 2: Generator Foto Realistis (Green)
    if (DrawDashboardCard(
        "Card_Photo",
        "Generator Foto Realistis",
        photo_sub.c_str(),
        "Generasi potret fotorealistis DSLR 85mm dengan tekstur kulit mikro berpori.",
        "Buka Studio ->",
        g_hub_custom_tex_photo,
        CARD_THEME_PHOTO,
        "FP16",
        "Steps: 20-25",
        2,
        card_w
    )) {
        g_selected_mode = 2;
    }

    ImGui::Spacing();

    // Kartu 3: Studio Dewasa & Sensual (Red)
    if (DrawDashboardCard(
        "Card_NSFW",
        "Studio Dewasa & Sensual (18+)",
        "CyberRealistic V5.0 + Qwen Uncensored",
        "Generasi potret dan cerita bebas tanpa sensor langsung di CPU/GPU laptop.",
        "Buka 18+ ->",
        g_hub_custom_tex_nsfw,
        CARD_THEME_ADULT,
        "Uncensored",
        "100% Offline",
        3,
        card_w
    )) {
        g_selected_mode = 3;
    }

    ImGui::Spacing();

    // Kartu 4: Asisten Suara AI (Purple)
    if (DrawDashboardCard(
        "Card_Voice",
        "Asisten Suara AI",
        "Indonesian Natural Voice (Gadis & Ardi)",
        "Dikte suara dan sintesis pembaca teks bahasa Indonesia ramah & sopan.",
        "Buka Suara ->",
        g_hub_custom_tex_voice,
        CARD_THEME_VOICE,
        "SAPI / Piper",
        "Latensi 0ms",
        5,
        card_w
    )) {
        g_selected_mode = 5;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextDisabled("Status Hardware: RAM %.1f / %.1f GB | %d Thread CPU | AVX2 + TAESD Fast VAE Active",
        (double)(g_telemetry.total_ram_bytes - g_telemetry.free_ram_bytes)/(1024.0*1024.0*1024.0),
        (double)g_telemetry.total_ram_bytes/(1024.0*1024.0*1024.0),
        g_num_threads
    );

    ImGui::EndChild(); // DashboardHub
}


// Render WhatsApp-Style Chat Bubble
void RenderWhatsAppBubble(const ChatMsg& msg, float chat_w, int idx) {
    bool is_user = (msg.role == "User");
    float max_bubble_w = chat_w * 0.78f;
    if (max_bubble_w < 320.0f) max_bubble_w = chat_w - 36.0f;

    std::string display_text = is_user ? msg.text : CleanLlamaStreamOutput(msg.text);
    if (display_text.empty()) display_text = msg.text;

    float wrap_w = max_bubble_w - 32.0f;
    ImVec2 text_size = ImGui::CalcTextSize(display_text.c_str(), NULL, false, wrap_w);
    
    float bubble_w = text_size.x + 38.0f;
    if (bubble_w < 260.0f) bubble_w = 260.0f;
    if (bubble_w > max_bubble_w) bubble_w = max_bubble_w;

    float actual_wrap_w = bubble_w - 32.0f;
    text_size = ImGui::CalcTextSize(display_text.c_str(), NULL, false, actual_wrap_w);

    float img_h_extra = 0.0f;
    float preview_w = bubble_w - 28.0f;
    if (preview_w > 400.0f) preview_w = 400.0f;
    if (preview_w < 200.0f) preview_w = 200.0f;
    float aspect = (msg.img_w > 0 && msg.img_h > 0) ? ((float)msg.img_h / (float)msg.img_w) : 1.0f;
    if (msg.texture_view != NULL) {
        img_h_extra = preview_w * aspect + 18.0f;
    }
    float action_btn_extra = !msg.image_path.empty() ? 52.0f : 0.0f;
    float voice_btn_extra = (!is_user && msg.image_path.empty()) ? 46.0f : 0.0f;
    float bubble_h = text_size.y + 78.0f + img_h_extra + action_btn_extra + voice_btn_extra;

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
    ImGui::BeginChild(child_id, ImVec2(bubble_w, bubble_h), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

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
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + actual_wrap_w);
    ImGui::TextUnformatted(display_text.c_str());
    ImGui::PopTextWrapPos();
    ImGui::Spacing();

    // 1. Render Gambar Langsung di Layar Chat Direct3D 11
    if (msg.texture_view != NULL) {
        ImGui::Spacing();
        // Klik gambar langsung membuka Layar Penuh!
        char img_btn_id[32];
        snprintf(img_btn_id, sizeof(img_btn_id), "img_btn##%d", idx);
        if (ImGui::ImageButton(img_btn_id, (ImTextureID)msg.texture_view, ImVec2(preview_w, preview_w * aspect))) {
            g_fullscreen_image_view = msg.texture_view;
            g_fullscreen_img_w = msg.img_w > 0 ? msg.img_w : 512;
            g_fullscreen_img_h = msg.img_h > 0 ? msg.img_h : 512;
            char full_p[MAX_PATH] = { 0 };
            GetFullPathNameA(msg.image_path.c_str(), MAX_PATH, full_p, NULL);
            g_fullscreen_image_path = full_p;
            g_show_fullscreen_image_modal = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Klik untuk Buka Full Screen!");
        }
        ImGui::Spacing();
    }

    // 2. Tombol Aksi Gambar (Layar Penuh, Folder, Salin Path)
    if (!msg.image_path.empty()) {
        ImGui::Separator();
        ImGui::Spacing();

        char btn_view[64], btn_folder[64], btn_copy[64];
        snprintf(btn_view, sizeof(btn_view), "🔍 Layar Penuh##%d", idx);
        snprintf(btn_folder, sizeof(btn_folder), "📁 Folder Output##%d", idx);
        snprintf(btn_copy, sizeof(btn_copy), "📋 Salin Path##%d", idx);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.50f, 0.35f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.65f, 0.45f, 1.0f));
        if (ImGui::Button(btn_view, ImVec2(120, 30))) {
            g_fullscreen_image_view = msg.texture_view;
            g_fullscreen_img_w = msg.img_w > 0 ? msg.img_w : 512;
            g_fullscreen_img_h = msg.img_h > 0 ? msg.img_h : 512;
            char full_p[MAX_PATH] = { 0 };
            GetFullPathNameA(msg.image_path.c_str(), MAX_PATH, full_p, NULL);
            g_fullscreen_image_path = full_p;
            g_show_fullscreen_image_modal = true;
        }
        ImGui::PopStyleColor(2);

        ImGui::SameLine();
        if (ImGui::Button(btn_folder, ImVec2(120, 30))) {
            char full_dir[MAX_PATH] = { 0 };
            GetFullPathNameA("outputs\\foto", MAX_PATH, full_dir, NULL);
            ShellExecuteA(NULL, "open", full_dir, NULL, NULL, SW_SHOWNORMAL);
        }

        ImGui::SameLine();
        if (ImGui::Button(btn_copy, ImVec2(90, 30))) {
            char full_p[MAX_PATH] = { 0 };
            GetFullPathNameA(msg.image_path.c_str(), MAX_PATH, full_p, NULL);
            CopyTextToClipboard(full_p);
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
            SpeakNaturalVoiceAsync(display_text, 0);
        }
        ImGui::PopStyleColor(2);

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.35f, 0.55f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.48f, 0.72f, 1.0f));
        if (ImGui::Button(btn_ardi, ImVec2(120, 26))) {
            SpeakNaturalVoiceAsync(display_text, 1);
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


// ================= PENAMPIL GAMBAR LAYAR PENUH (FULLSCREEN IMAGE MODAL) =================
void RenderFullscreenImageModal(float app_width, float app_height) {
    if (!g_show_fullscreen_image_modal || !g_fullscreen_image_view) return;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(app_width, app_height));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.02f, 0.03f, 0.05f, 0.96f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    if (ImGui::Begin("##FullscreenImageViewer", &g_show_fullscreen_image_modal,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus)) {

        // Top bar
        ImGui::TextColored(ImVec4(0.38f, 0.85f, 0.98f, 1.0f), "🖼️ Penampil Gambar Layar Penuh Direct3D 11");
        ImGui::SameLine();
        ImGui::TextDisabled("| %d x %d px", g_fullscreen_img_w, g_fullscreen_img_h);

        ImGui::SetCursorPos(ImVec2(app_width - 150.0f, 12.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.70f, 0.15f, 0.15f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.20f, 0.20f, 1.0f));
        if (ImGui::Button("❌ Tutup (ESC)", ImVec2(130, 32))) {
            g_show_fullscreen_image_modal = false;
        }
        ImGui::PopStyleColor(2);

        // Calculate maximum aspect-preserved drawing area
        float max_w = app_width - 32.0f;
        float max_h = app_height - 110.0f;
        float scale_w = max_w / (float)g_fullscreen_img_w;
        float scale_h = max_h / (float)g_fullscreen_img_h;
        float best_scale = (scale_w < scale_h) ? scale_w : scale_h;
        if (best_scale > 2.5f) best_scale = 2.5f;

        float draw_w = (float)g_fullscreen_img_w * best_scale;
        float draw_h = (float)g_fullscreen_img_h * best_scale;

        float center_x = (app_width - draw_w) * 0.5f;
        float center_y = 52.0f + (max_h - draw_h) * 0.5f;

        ImGui::SetCursorPos(ImVec2(center_x, center_y));
        ImGui::Image((void*)g_fullscreen_image_view, ImVec2(draw_w, draw_h));

        // Bottom action bar
        ImGui::SetCursorPos(ImVec2(24.0f, app_height - 48.0f));
        if (ImGui::Button("📁 Buka di File Explorer", ImVec2(190, 32))) {
            char cmd[MAX_PATH + 50];
            snprintf(cmd, sizeof(cmd), "explorer.exe /select,\"%s\"", g_fullscreen_image_path.c_str());
            system(cmd);
        }
        ImGui::SameLine();
        if (ImGui::Button("🖼️ Buka di Aplikasi Windows", ImVec2(210, 32))) {
            ShellExecuteA(NULL, "open", g_fullscreen_image_path.c_str(), NULL, NULL, SW_SHOWNORMAL);
        }
        ImGui::SameLine();
        if (ImGui::Button("📋 Salin Path Gambar", ImVec2(160, 32))) {
            CopyTextToClipboard(g_fullscreen_image_path);
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            g_show_fullscreen_image_modal = false;
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

// ================= STATUS BAR TELEMETRI HARDWARE DIRECT3D 11 =================
void RenderD3D11StatusBar(float app_width, float app_height) {
    const float bar_height = 26.0f;
    ImGui::SetCursorPos(ImVec2(0, app_height - bar_height));

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.035f, 0.050f, 0.075f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.140f, 0.180f, 0.260f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);

    ImGui::BeginChild("D3D11StatusBar", ImVec2(app_width, bar_height), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    HardwareTelemetrySnapshot snap = HardwareTelemetry::Get().GetSnapshot();

        // 1. RAM Usage Meter
        ImVec4 ram_col = ImVec4(0.10f, 0.85f, 0.55f, 1.0f);
        if (snap.ram_percent >= 85.0f) {
            ram_col = ImVec4(0.95f, 0.25f, 0.30f, 1.0f);
        } else if (snap.ram_percent >= 75.0f) {
            ram_col = ImVec4(0.95f, 0.70f, 0.20f, 1.0f);
        }

        ImGui::TextColored(ram_col, "RAM: %.1f%%", snap.ram_percent);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Memori Fisik Terpakai: %llu MB / %llu MB (Sisa Bebas: %llu MB)",
                (unsigned long long)snap.ram_used_mb,
                (unsigned long long)snap.ram_total_mb,
                (unsigned long long)snap.ram_free_mb);
        }

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        // 2. CPU Usage & Logical Threads
        ImVec4 cpu_col = (snap.cpu_usage_percent > 80.0f) ? ImVec4(0.95f, 0.40f, 0.20f, 1.0f) : ImVec4(0.40f, 0.75f, 0.95f, 1.0f);
        ImGui::TextColored(cpu_col, "CPU: %.1f%% (%u Cores, %u Threads)",
            snap.cpu_usage_percent, (unsigned int)snap.total_logical_cores, (unsigned int)snap.active_engine_threads);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Beban Kerja CPU Total dan Thread Komputasi AI AVX2 Aktif");
        }

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        // 3. Instruction Set Capabilities (AVX2 / FMA)
        if (snap.has_avx2) {
            ImGui::TextColored(ImVec4(0.20f, 0.90f, 0.70f, 1.0f), "AVX2+FMA");
        } else {
            ImGui::TextColored(ImVec4(0.80f, 0.80f, 0.40f, 1.0f), "SSE4.2");
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Ekstensi Vektor SIMD CPU: %s", snap.has_avx2 ? "AVX2 dan FMA Aktif (Auto-Vectorization -O3)" : "Standar");
        }

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        // 4. TAESD Fast VAE Decode
        if (snap.is_taesd_active) {
            ImGui::TextColored(ImVec4(0.20f, 0.90f, 0.45f, 1.0f), "TAESD: AKTIF");
        } else {
            ImGui::TextDisabled("TAESD: OFF");
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Fast Tiny AutoEncoder (TAESD): Pangkas waktu decode foto dari ~61 detik ke ~2.7 detik");
        }

        // 5. FPS & Graphics API Display
        float right_info_w = 210.0f;
        if (app_width > 720.0f) {
            ImGui::SameLine(app_width - right_info_w);
            float fps = ImGui::GetIO().Framerate;
            ImGui::TextDisabled("D3D11 | %.1f FPS (%.1f ms)", fps, 1000.0f / (fps > 1.0f ? fps : 1.0f));
        }

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

int WINAPI WinMain
(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    g_logf = fopen("cpp_core/native_gui.log", "w");
    if (g_logf) { fprintf(g_logf, "[LOG] AI Studio Native Starting...\n"); fflush(g_logf); }

    // 1. Inisialisasi C++ Native Core & Auto-Deteksi Hardware (CPU / RAM / GPU RTX)
    Core_Init(0);
    ScanModelsDirectory(); // Auto-deteksi semua model .gguf & .safetensors di folder models
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
        if (app_width < 200.0f) app_width = 200.0f;
        if (app_height < 200.0f) app_height = 200.0f;

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(app_width, app_height));
        ImGuiWindowFlags root_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                      ImGuiWindowFlags_NoBringToFrontOnFocus;

        ImGui::Begin("RootWindow", NULL, root_flags);

        // Sinkronisasi status engine ke modul Hardware Telemetry
        HardwareTelemetry::Get().SetActiveEngineThreads((uint32_t)g_num_threads);
        HardwareTelemetry::Get().SetTAESDActive(g_use_fast_vae);

        const float status_bar_h = 26.0f;
        const float main_content_h = app_height - status_bar_h - 10.0f; // Ruang pasti di atas status bar

        float sidebar_width = 280.0f;
        if (app_width < 800.0f) sidebar_width = 230.0f;
        float right_panel_width = app_width - sidebar_width - 24.0f;
        if (right_panel_width < 280.0f) right_panel_width = 280.0f;

        // ================= 1. SIDEBAR KIRI (FULL HEIGHT DENGAN BATAS STATUS BAR) =================
        ImGui::BeginChild("SidebarPanel", ImVec2(sidebar_width, main_content_h), true);

        // Logo & Brand Header Card
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.040f, 0.060f, 0.095f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.120f, 0.170f, 0.260f, 0.80f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 7.0f));
        ImGui::BeginChild("BrandHeaderCard", ImVec2(-1, 54), true, ImGuiWindowFlags_NoScrollbar);
        
        ImGui::TextColored(ImVec4(0.25f, 0.75f, 1.0f, 1.0f), "⚡ AI STUDIO");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.15f, 0.85f, 0.55f, 1.0f), "NATIVE");
        ImGui::TextColored(ImVec4(0.40f, 0.85f, 0.60f, 1.0f), "🟢 100%% C++ Direct3D 11");

        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
        ImGui::Spacing();

        // Tombol Percakapan Baru
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.38f, 0.65f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.50f, 0.82f, 1.0f));
        if (ImGui::Button("+ Percakapan Baru", ImVec2(-1, 34))) {
            CreateNewSession();
        }
        ImGui::PopStyleColor(2);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Hitung tinggi daftar percakapan secara dinamis agar Telemetri & Tombol Pengaturan selalu tampak sempurna!
        const float sidebar_header_h = 96.0f;
        const float sidebar_footer_h = 118.0f;
        float topics_h = main_content_h - sidebar_header_h - sidebar_footer_h;
        if (topics_h < 75.0f) topics_h = 75.0f;

        // Daftar Percakapan / Topics (Scrollable)
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
            ImGui::BeginChild(card_name.c_str(), ImVec2(-1, 52), true, ImGuiWindowFlags_NoScrollbar);

            if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) {
                g_active_session_idx = (int)t;
            }

            float card_w = ImGui::GetContentRegionAvail().x;
            std::string title_disp = g_sessions[t].title;
            if (title_disp.length() > 18) title_disp = title_disp.substr(0, 16) + "..";
            ImGui::TextColored(selected ? ImVec4(0.45f, 0.85f, 1.0f, 1.0f) : ImVec4(0.90f, 0.92f, 0.96f, 1.0f), "# %s", title_disp.c_str());

            if (g_sessions.size() > 1) {
                ImGui::SameLine(card_w - 18.0f);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.15f, 0.15f, 0.8f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.60f, 0.65f, 0.75f, 0.8f));
                if (ImGui::SmallButton("x")) {
                    DeleteSession((int)t);
                    ImGui::PopStyleColor(3);
                    ImGui::EndChild();
                    ImGui::PopStyleVar(2);
                    ImGui::PopStyleColor(2);
                    ImGui::PopID();
                    break;
                }
                ImGui::PopStyleColor(3);
            }

            int msg_count = (int)g_sessions[t].messages.size();
            ImGui::TextDisabled("%d pesan • %s", msg_count, g_sessions[t].last_time.c_str());

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
            ImGui::TextDisabled("Akselerasi: CUDA RTX Active");
        } else {
            ImGui::TextDisabled("CPU: %d Thread | AVX2 + TAESD Fast", g_num_threads);
        }
        ImGui::Spacing();

        // Tombol Pengaturan & Outputs di Footer Sidebar
        float footer_btn_w = (sidebar_width - 24.0f) * 0.48f;
        if (ImGui::Button("Pengaturan", ImVec2(footer_btn_w, 28))) {
            g_show_settings = !g_show_settings;
        }
        ImGui::SameLine();
        if (ImGui::Button("Output", ImVec2(footer_btn_w, 28))) {
            ShellExecuteA(NULL, "open", "outputs", NULL, NULL, SW_SHOWNORMAL);
        }

        ImGui::EndChild(); // SidebarPanel

        ImGui::SameLine();

        // ================= 2. PANEL UTAMA CHAT (KANAN) =================
        ImGui::BeginGroup();

        // A. TOP NAVBAR (Tinggi 42px)
        ImGui::BeginChild("TopNavbar", ImVec2(right_panel_width, 42), true, ImGuiWindowFlags_NoScrollbar);
        
        static const char* nav_mode_labels[] = { "Home", "Chat AI", "Studio Foto", "Dewasa (18+)", "Video AI", "Suara AI" };

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
        for (int m = 0; m < 6; m++) {
            if (m > 0) ImGui::SameLine();
            bool is_active = (g_selected_mode == m);
            if (m == 0) {
                // Tombol Home / Dashboard
                if (is_active) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.90f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.55f, 0.98f, 1.0f));
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.16f, 0.24f, 0.85f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.24f, 0.35f, 1.0f));
                }
            } else if (m == 3) {
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
            if (ImGui::Button(nav_mode_labels[m], ImVec2(0, 26))) {
                g_selected_mode = m;
            }
            ImGui::PopStyleColor(2);
        }
        ImGui::PopStyleVar();

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        float remaining_nav_w = ImGui::GetContentRegionAvail().x;
        float combo_w = remaining_nav_w - 185.0f;
        if (combo_w > 240.0f) combo_w = 240.0f;
        if (combo_w < 130.0f) combo_w = 130.0f;

        if (g_selected_mode == 2 || g_selected_mode == 3) {
            ImGui::SetNextItemWidth(combo_w);
            if (!g_photo_items.empty()) {
                ImGui::Combo("##PhotoCombo", &g_selected_photo, g_photo_items.data(), (int)g_photo_items.size());
            }
            if (ImGui::IsItemHovered() && !g_photo_items.empty() && g_selected_photo < (int)g_photo_items.size()) {
                ImGui::SetTooltip("Model Difusi Aktif: %s", g_photo_items[g_selected_photo]);
            }
        } else if (g_selected_mode == 0) {
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 0.95f), "Dashboard Hub");
        } else {
            ImGui::SetNextItemWidth(combo_w);
            if (!g_llm_items.empty()) {
                ImGui::Combo("##LLMCombo", &g_selected_llm, g_llm_items.data(), (int)g_llm_items.size());
            }
            if (ImGui::IsItemHovered() && !g_llm_items.empty() && g_selected_llm < (int)g_llm_items.size()) {
                ImGui::SetTooltip("Model Chat Aktif: %s", g_llm_items[g_selected_llm]);
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("+ Model", ImVec2(62, 26))) {
            std::string chosen = OpenModelFileDialog(g_hMainWnd);
            if (!chosen.empty()) {
                g_current_inspection = InspectModelFile(chosen);
                g_show_import_modal = true;
                g_is_importing = false;
                g_import_progress = 0.0f;
                g_import_status_text = "";
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(g_is_borderless_fullscreen ? "🗗 Normal" : "⛶ Layar Penuh", ImVec2(100, 26))) {
            ToggleBorderlessFullscreen(g_hMainWnd);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Buka Layar Penuh (Borderless Fullscreen). Shortcut: Tekan F11");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Pasang Model .GGUF / .safetensors baru dari laptop Anda");
        }

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
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Booster Realistis: Otomatis menyuntikkan parameter fotografi 8K, pori-pori kulit alami, dan pencahayaan studio DSLR.");

            ImGui::SameLine();
            ImGui::Checkbox("HD Wajah", &g_fullbody_face_fix);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("HD Wajah: Penajam mata & muka anti-buram pada foto Full Body.");

            if (right_panel_width > 750.0f) {
                ImGui::SameLine();
                ImGui::Checkbox("Fast VAE", &g_use_fast_vae);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Fast VAE (TAESD): Pangkas waktu decode foto menjadi hanya ~2.7 detik.");

                ImGui::SameLine();
                ImGui::Checkbox("Turbo LCM", &g_turbo_mode);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Mode Turbo: 4 langkah kilat (~3-5 detik). Matikan untuk 20 langkah DSLR paling tajam.");
            }

            ImGui::EndChild(); // NsfwSubNavbar
            ImGui::PopStyleColor(2);
        } else if (g_selected_mode == 2) {
            subbar_h = 42.0f;
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.12f, 0.18f, 0.90f));
            ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.20f, 0.40f, 0.65f, 0.70f));
            ImGui::BeginChild("PhotoSubNavbar", ImVec2(right_panel_width, 38), true, ImGuiWindowFlags_NoScrollbar);

            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(ImVec4(0.38f, 0.75f, 0.98f, 1.0f), "PRESET:");
            ImGui::SameLine();

            if (ImGui::Button("Gadis Cantik", ImVec2(0, 24))) {
                strcpy(g_input_prompt, "Close-up portrait of a gorgeous Indonesian woman smiling outdoors in nature, golden hour warm lighting, natural skin pores, Canon EOS R5 85mm f/1.4, 8k");
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Potret jarak dekat wanita Indonesia di alam terbuka (Golden Hour)");
            ImGui::SameLine();
            if (ImGui::Button("Pemandangan", ImVec2(0, 24))) {
                strcpy(g_input_prompt, "Breathtaking landscape photography of Mount Bromo sunrise with sea of clouds, epic morning sun rays, ultra detailed, award winning nature photo, 8k uhd");
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pemandangan alam Gunung Bromo dengan lautan awan");
            ImGui::SameLine();
            if (ImGui::Button("Cyberpunk", ImVec2(0, 24))) {
                strcpy(g_input_prompt, "Futuristic neon cyberpunk city street at rainy night, wet asphalt reflections, vibrant blue and purple holographic billboards, cinematic 8k");
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Kota neon futuristik cyberpunk saat hujan malam");
            ImGui::SameLine();
            if (ImGui::Button("Mobil Sport", ImVec2(0, 24))) {
                strcpy(g_input_prompt, "Hypercar luxury sports car parked on coastal cliff highway at sunset, sleek carbon fiber body reflections, automotive photography, 8k");
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Mobil sport mewah di tepi tebing laut");
            ImGui::SameLine();
            if (ImGui::Button("Full Body", ImVec2(0, 24))) {
                g_framing_idx = 3;
                g_resolution_idx = 1;
                strcpy(g_input_prompt, "Full length body shot of a beautiful model standing gracefully outdoors, head to toe view, fashionable elegant dress, visible shoes on ground, wide angle 35mm lens, full figure framing, (detailed beautiful face:1.2), sharp detailed eyes, natural facial expression, accurate anatomy, natural proportions, 5 fingers, natural lighting, 8k uhd, photorealistic, masterpiece");
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Foto seluruh badan dari kepala hingga ujung kaki (Vertikal)");

            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();

            ImGui::Checkbox("Booster Realistis", &g_nsfw_quality_booster);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Menyuntikkan token fotografi 8K: tekstur kulit alami, pori-pori mikro, mata jernih.");

            ImGui::SameLine();
            ImGui::Checkbox("HD Wajah", &g_fullbody_face_fix);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Penajam wajah berbobot tinggi & filter negative prompt anti-cacat anatomi.");

            ImGui::SameLine();
            ImGui::Checkbox("Acak Seed", &g_random_seed);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Setiap generate menghasilkan variasi baru yang segar. Matikan untuk mengunci seed.");

            ImGui::SameLine();
            ImGui::Checkbox("Fast VAE", &g_use_fast_vae);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Akselerasi TAESD: Decode gambar di CPU 2.7 detik (22x lebih cepat).");

            ImGui::SameLine();
            ImGui::Checkbox("Turbo LCM", &g_turbo_mode);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Mode kilat 4 langkah (~3-5 detik). Matikan untuk 20-30 langkah DSLR paling tajam.");

            ImGui::SameLine();
            ImGui::Checkbox("LoRA Realism", &g_use_realism_lora);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Menyertakan LoRA add_detail (bobot 0.75) untuk tekstur pori-pori kulit alami.");

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

        if (g_selected_mode == 0) {
            // TAMPILKAN DASHBOARD CARD HUB (MODERN & RINGAN)
            float hub_h = main_content_h - 44.0f;
            RenderDashboardHub(right_panel_width, hub_h);
        } else {
            // B. CHAT CANVAS (Scrollable Area)
            float input_card_h = 0.0f;
            if (g_has_init_image) input_card_h += 76.0f;
            if (g_has_attached_doc) input_card_h += 48.0f;
            bool two_rows_options = (right_panel_width < 720.0f);
            float options_bar_h = (g_selected_mode == 2 || g_selected_mode == 3) ? (two_rows_options ? 62.0f : 34.0f) : 34.0f;
            float input_area_h = 52.0f + options_bar_h + input_card_h + 8.0f;
            float chat_canvas_h = main_content_h - 40.0f - subbar_h - input_area_h - 8.0f;
            if (chat_canvas_h < 100.0f) chat_canvas_h = 100.0f;

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

        float send_btn_w = 80.0f;
        float smart_btn_w = 0.0f;
        if (g_selected_mode == 2 || g_selected_mode == 3) {
            smart_btn_w = 125.0f;
        } else {
            smart_btn_w = 90.0f;
        }

        float input_text_w = right_panel_width - send_btn_w - smart_btn_w - 20.0f;
        if (input_text_w < 180.0f) {
            input_text_w = right_panel_width - send_btn_w - 14.0f;
            smart_btn_w = 0.0f;
        }

        ImGui::SetNextItemWidth(input_text_w);
        bool enter_pressed = ImGui::InputTextMultiline(
            "##PromptInput", g_input_prompt, sizeof(g_input_prompt),
            ImVec2(input_text_w, 46),
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CtrlEnterForNewLine
        );

        ImGui::SameLine();

        // Tombol Input Tambahan (✨ Prompt Pintar untuk Foto, atau 🎙️ Bicara untuk Chat & Suara)
        if (smart_btn_w > 0.0f) {
            if (g_selected_mode == 2 || g_selected_mode == 3) {
                if (g_is_optimizing_prompt.load()) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.20f, 0.55f, 1.0f));
                    ImGui::Button("✨ Berpikir...", ImVec2(smart_btn_w, 46));
                    ImGui::PopStyleColor();
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.18f, 0.72f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.58f, 0.24f, 0.90f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.12f, 0.60f, 1.0f));
                    if (ImGui::Button("✨ Prompt Pintar", ImVec2(smart_btn_w, 46))) {
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
                        ImGui::SetTooltip("AI akan mengembangkan ide singkat Anda menjadi prompt fotografi realistis kaya detail!");
                    }
                    ImGui::PopStyleColor(3);
                }
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.08f, 0.55f, 0.45f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.12f, 0.70f, 0.58f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.05f, 0.42f, 0.35f, 1.0f));
                if (ImGui::Button("🎙️ Bicara", ImVec2(smart_btn_w, 46))) {
                    TriggerWindowsVoiceTyping();
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Aktifkan Mikrofon: Bicara dalam Bahasa Indonesia, suara Anda otomatis diketik ke kolom pesan!");
                }
                ImGui::PopStyleColor(3);
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

        // ------------------ OPSI KONTEKSTUAL BAWAH (ENCAPSULATED RIBBON) ------------------
        if (g_selected_mode == 2 || g_selected_mode == 3) {
            // MODE DIFUSI FOTO / NSFW (Responsif & Anti-Overlap)
            bool two_rows = (right_panel_width < 720.0f);
            float ribbon_h = two_rows ? 58.0f : 32.0f;

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.045f, 0.070f, 0.115f, 0.95f));
            ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.140f, 0.190f, 0.280f, 0.85f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 5.0f));
            ImGui::BeginChild("PhotoOptionsRibbon", ImVec2(right_panel_width - 4.0f, ribbon_h), true, ImGuiWindowFlags_NoScrollbar);

            ImGui::SetNextItemWidth(two_rows ? 115.0f : 120.0f);
            ImGui::Combo("##PromptAssistMode", &g_prompt_assist_mode, g_prompt_assist_names, IM_ARRAYSIZE(g_prompt_assist_names));
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(
                    "Mode Penanganan Prompt AI:\n"
                    "- Prompt Asli (Raw): 100% persis dari apa yang Anda ketik.\n"
                    "- Koreksi & Rapikan: Menerjemahkan ke Inggris secara setia tanpa objek liar.\n"
                    "- Auto Kembangkan: Memperkaya ide dengan parameter fotografi 8K."
                );
            }

            ImGui::SameLine();
            ImGui::SetNextItemWidth(two_rows ? 90.0f : 95.0f);
            ImGui::Combo("##FramingSelector", &g_framing_idx, g_framing_names, IM_ARRAYSIZE(g_framing_names));
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Sudut Pandang Kamera: Auto, Wajah, 1/2 Badan, Full Body");

            ImGui::SameLine();
            ImGui::SetNextItemWidth(two_rows ? 88.0f : 90.0f);
            ImGui::Combo("##ResolutionSelector", &g_resolution_idx, g_resolutions, IM_ARRAYSIZE(g_resolutions));
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Rasio Foto: 512x512 (Kotak), 512x768 (Vertikal), 768x512 (Lanskap)");

            ImGui::SameLine();
            ImGui::SetNextItemWidth(two_rows ? 82.0f : 85.0f);
            ImGui::SliderInt("##StepsSlider", &g_image_steps, 1, 50, "%d Stp");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Langkah Sampling (1 - 50 Steps): 4=Kilat LCM, 20=DSLR Seimbang, 30-50=Ultra Mulus");

            if (two_rows) {
                ImGui::Spacing();
            } else {
                ImGui::SameLine();
            }

            ImGui::Checkbox("HD Wajah", &g_fullbody_face_fix);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("HD Wajah & Anatomi: Penajam mata & muka anti-buram pada foto Full Body");

            if (!g_has_init_image) {
                ImGui::SameLine();
                if (ImGui::Button("Foto Acuan", ImVec2(80, 22))) {
                    std::string chosen = OpenImageFileDialog(g_hMainWnd);
                    if (!chosen.empty()) {
                        SetInitImage(chosen.c_str());
                    }
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Masukkan Foto Acuan untuk Edit Foto (Image-to-Image) atau ControlNet");
            }

            ImGui::SameLine();
            ImGui::TextDisabled("| Enter: Generate");

            ImGui::EndChild(); // PhotoOptionsRibbon
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);
        } else {
            // MODE TEKS (CHAT / AUTO / VIDEO / SUARA)
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.045f, 0.070f, 0.115f, 0.95f));
            ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.140f, 0.190f, 0.280f, 0.85f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 5.0f));
            ImGui::BeginChild("TextOptionsRibbon", ImVec2(right_panel_width - 4.0f, 32.0f), true, ImGuiWindowFlags_NoScrollbar);

            if (!g_has_attached_doc) {
                if (ImGui::Button("📄 Lampirkan File", ImVec2(125, 22))) {
                    std::string chosen = OpenDocumentFileDialog(g_hMainWnd);
                    if (!chosen.empty()) {
                        AttachDocument(chosen.c_str());
                    }
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(
                        "Pilih file dokumen teks, data, atau skrip koding (.txt, .pdf, .md, .csv, .py, .cpp, dll)\n"
                        "AI akan membaca isi dokumen, lalu merangkum, menganalisis, atau menjawab pertanyaan Anda."
                    );
                }
                ImGui::SameLine();
                ImGui::TextDisabled("|");
                ImGui::SameLine();
            }

            ImGui::Checkbox("🌐 Cari di Web", &g_search_internet);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(
                    "Pencarian Web Otomatis:\n"
                    "AI akan mencari informasi online terkini dari internet sebelum menjawab.\n"
                    "Matikan jika ingin AI menjawab 100%% offline dari data lokal model."
                );
            }

            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();
            ImGui::TextDisabled("💡 Shift+Enter: baris baru • Enter: kirim");

            ImGui::EndChild(); // TextOptionsRibbon
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);
        }

        ImGui::EndChild(); // BottomInput
        } // Akhir if (g_selected_mode == 0) ... else

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
                    ImGui::SliderInt("Langkah Inferensi (Steps)", &g_image_steps, 1, 50);
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
                    ImGui::TextColored(ImVec4(0.98f, 0.35f, 0.55f, 1.0f), "%s", "Mode Uncensored 100% C++ Native Engine");
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

                                if (ImGui::CollapsingHeader("🧹 Pengelolaan & Optimasi Memori (RAM / VRAM)", ImGuiTreeNodeFlags_DefaultOpen)) {
                    float currentRAM_MB = (float)MemoryManager::Get().GetCurrentWorkingSetBytes() / (1024.0f * 1024.0f);
                    float peakRAM_MB = (float)MemoryManager::Get().GetPeakWorkingSetBytes() / (1024.0f * 1024.0f);

                    ImGui::Text("Penggunaan RAM Fisik:");
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.4f, 1.0f), "%.1f MB (Puncak: %.1f MB)", currentRAM_MB, peakRAM_MB);

                    ImGui::Text("Status Buffer:");
                    ImGui::SameLine();
                    ImGui::TextDisabled("%s", MemoryManager::Get().GetLastStatusMessage().c_str());

                    ImGui::Spacing();
                    if (MemoryManager::Get().IsPurging()) {
                        ImGui::BeginDisabled();
                        ImGui::Button("⏳ Sedang Membersihkan Memori...", ImVec2(-1, 34));
                        ImGui::EndDisabled();
                    } else {
                        if (ImGui::Button("🧹 Bersihkan RAM & VRAM Sekarang (Purge All)", ImVec2(-1, 34))) {
                            MemoryManager::Get().PurgeAllAsync();
                        }
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Melepas seluruh buffer tekstur GPU D3D11 yang tidak aktif, memadatkan working set proses, dan mengembalikan RAM fisik ke Windows secara instan.");
                        }
                    }
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

        // Shortcut F11 untuk Layar Penuh
        if (ImGui::IsKeyPressed(ImGuiKey_F11)) {
            ToggleBorderlessFullscreen(g_hMainWnd);
        }

        // Penampil Gambar Layar Penuh Sinematik
        RenderFullscreenImageModal(app_width, app_height);

        // Render Status Bar Telemetri D3D11 di bagian paling bawah
        RenderD3D11StatusBar(app_width, app_height);

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
    MemoryManager::Get().Initialize(g_pd3dDevice, g_pd3dDeviceContext);
    HardwareTelemetry::Get().Start(500);
    return true;
}

void CleanupDeviceD3D() {
    HardwareTelemetry::Get().Stop();
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

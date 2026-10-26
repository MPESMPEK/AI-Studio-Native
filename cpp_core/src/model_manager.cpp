#include "../include/model_manager.h"
#include <algorithm>

// Global model containers
std::vector<DetectedModel> g_detected_llms;
std::vector<DetectedModel> g_detected_photos;
std::vector<std::string> g_detected_llm_labels;
std::vector<std::string> g_detected_photo_labels;
std::vector<const char*> g_llm_items;
std::vector<const char*> g_photo_items;

// Container Global LoRA, VAE, ControlNet (Studio Node Native)
std::vector<DetectedModel> g_detected_loras;
std::vector<DetectedModel> g_detected_vaes;
std::vector<DetectedModel> g_detected_cnets;
std::vector<std::string> g_detected_lora_labels;
std::vector<std::string> g_detected_vae_labels;
std::vector<std::string> g_detected_cnet_labels;
std::vector<const char*> g_lora_items;
std::vector<const char*> g_vae_items;
std::vector<const char*> g_cnet_items;

int g_selected_lora_idx = 0;
float g_selected_lora_strength = 0.75f;
int g_selected_vae_idx = 0;
int g_selected_cnet_idx = 0;
float g_selected_cnet_strength = 0.90f;

// State import & delete
bool g_show_import_modal = false;
bool g_is_importing = false;
float g_import_progress = 0.0f;
std::string g_import_status_text = "";
ModelInspectionResult g_current_inspection;
std::thread g_import_thread;

bool g_show_delete_confirm_modal = false;
std::string g_delete_target_path = "";
std::string g_delete_target_name = "";

// External UI state
extern int g_selected_llm;
extern int g_selected_photo;

void ScanModelsDirectory() {
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
                    m.display_name = "[GGUF] " + fname + " (" + std::to_string(mb) + " MB)";
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
                    m.display_name = "[GGUF] " + fname + " (" + std::to_string(mb) + " MB)";
                    m.desc_plus = "Model GGUF terdeteksi dari root folder models\\.";
                    m.desc_minus = "Kecepatan & penalaran bergantung pada spesifikasi model.";
                    m.rec = "Dapat langsung dipilih untuk percakapan teks.";
                    g_detected_llms.push_back(m);
                }
            }
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }

    // 2. Pindai folder models\Gambar_Diffusion\*.safetensors dan *.gguf
    const char* diff_patterns[] = {
        "models\\Gambar_Diffusion\\*.safetensors",
        "models\\Gambar_Diffusion\\*.gguf",
        "models\\*.safetensors"
    };

    for (int p = 0; p < 3; p++) {
        hFind = FindFirstFileA(diff_patterns[p], &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    std::string fname = fd.cFileName;
                    bool already = false;
                    for (const auto& ex : g_detected_photos) {
                        if (ex.filename == fname) { already = true; break; }
                    }
                    if (already) continue;

                    std::string prefix = (p == 2) ? "models\\" : "models\\Gambar_Diffusion\\";
                    std::string path = prefix + fname;
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
                    } else if (lower_f.find("realistic_vision") != std::string::npos || lower_f.find("realistic-vision") != std::string::npos) {
                        m.display_name = "Realistic Vision V5.1 (Serbaguna & Stabil)";
                        m.desc_plus = "Sangat seimbang dan stabil: unggul untuk foto manusia, pemandangan alam, arsitektur, dan mobil sport.";
                        m.desc_minus = "Tekstur pori-pori kulit wajah sedikit lebih halus/soft dibandingkan CyberRealistic V5.";
                        m.rec = "Sangat direkomendasikan jika ingin membuat pemandangan alam, mobil balap, interior, atau foto campuran manusia & lingkungan.";
                    } else if (lower_f.find("dreamshaper") != std::string::npos) {
                        m.display_name = "DreamShaper 8 LCM (Ultra Fast 4-Step)";
                        m.desc_plus = "Akselerasi Kilat: Menghasilkan gambar hanya dalam 4 steps denoising (~3-5 detik di CPU).";
                        m.desc_minus = "Gaya visual memiliki sentuhan semi-ilustratif/artistik 2.5D, tidak semurni DSLR CyberRealistic.";
                        m.rec = "Gunakan saat butuh pembuatan gambar instan, thumbnail ide cepat, atau seni konsep artistik.";
                    } else if (lower_f.find("ghostmix") != std::string::npos) {
                        m.display_name = "GhostMix V2.0 (Anime-Realistis 2.5D Uncensored)";
                        m.desc_plus = "Spesialis Anime-Realistis 2.5D & NSFW Uncensored: Memadukan estetika karakter anime cantik dengan tekstur kulit nyata, anatomi tubuh proporsional, dan pencahayaan 3D mendalam. Sudah termasuk Baked VAE.";
                        m.desc_minus = "Bukan foto kamera jurnalis murni, melainkan perpaduan anime x fotorealistis.";
                        m.rec = "PILIHAN UTAMA untuk NSFW anime yang tampak hidup, sensual, dan beranatomi nyata.";
                    } else if (lower_f.find("chilloutmix") != std::string::npos) {
                        m.display_name = "ChilloutMix Ni (Foto Realistis Asia & Ringan)";
                        m.desc_plus = "Model legendaris untuk foto wanita Asia fotorealistis dan pose NSFW intim. Sangat tajam dan toleran pada resolusi sedang/rendah (384x512).";
                        m.desc_minus = "Karakter wajah konsisten bergaya kecantikan Asia Timur / idol.";
                        m.rec = "Sangat direkomendasikan untuk potret sensual wanita Asia dengan kulit mulus dan pencahayaan studio.";
                    } else {
                        // Model difusi kustom (.safetensors / .gguf)
                        m.is_custom = true;
                        std::string tag = (lower_f.find(".safetensors") != std::string::npos) ? "[Safetensors]" : "[GGUF]";
                        m.display_name = "" + tag + " " + fname + " (" + std::to_string(mb) + " MB)";
                        m.desc_plus = "Model difusi kustom otomatis terdeteksi. Kompatibel 100% dengan mesin sd-cli C++.";
                        m.desc_minus = "Karakter artistik dan ketajaman bergantung pada checkpoint yang Anda pasang.";
                        m.rec = "Dapat langsung dipilih untuk generasi foto atau mode NSFW.";
                    }
                    g_detected_photos.push_back(m);
                }
            } while (FindNextFileA(hFind, &fd));
            FindClose(hFind);
        }
    }

    // Default Fallback jika folder models kosong
    if (g_detected_llms.empty()) {
        DetectedModel def;
        def.display_name = "Qwen2.5-Coder 1.5B (Default)";
        def.filename = "qwen2.5-coder-1.5b-instruct-q4_k_m.gguf";
        def.full_path = "models\\Teks_LLM\\qwen2.5-coder-1.5b-instruct-q4_k_m.gguf";
        def.desc_plus = "Model default.";
        def.desc_minus = "Tidak ada model lain ditemukan.";
        def.rec = "Salin file .gguf ke folder models\\Teks_LLM.";
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
        g_detected_photos.push_back(def);
    }

    // Bangun label dropdown UI
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

    // 3. Pindai folder models\LoRA\*.safetensors dan *.gguf
    g_detected_loras.clear();
    g_detected_lora_labels.clear();
    g_lora_items.clear();

    {
        DetectedModel none_lora;
        none_lora.display_name = "(Tanpa LoRA / Disabled)";
        none_lora.filename = "";
        none_lora.full_path = "";
        none_lora.desc_plus = "Generasi gambar tanpa modifikasi adaptor LoRA.";
        none_lora.desc_minus = "-";
        none_lora.rec = "Standar bawaan model.";
        none_lora.size_mb = 0;
        none_lora.is_custom = false;
        g_detected_loras.push_back(none_lora);
    }

    const char* lora_patterns[] = { "models\\LoRA\\*.safetensors", "models\\LoRA\\*.gguf" };
    for (int lp = 0; lp < 2; lp++) {
        hFind = FindFirstFileA(lora_patterns[lp], &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    std::string fname = fd.cFileName;
                    std::string lower_f = fname;
                    for (char& c : lower_f) c = (char)tolower(c);
                    if (lower_f.find("readme") != std::string::npos) continue;

                    bool already = false;
                    for (const auto& ex : g_detected_loras) {
                        if (ex.filename == fname) { already = true; break; }
                    }
                    if (already) continue;

                    ULARGE_INTEGER fileSize;
                    fileSize.LowPart = fd.nFileSizeLow;
                    fileSize.HighPart = fd.nFileSizeHigh;
                    int mb = (int)(fileSize.QuadPart / (1024 * 1024));

                    DetectedModel m;
                    m.filename = fname;
                    m.full_path = "models\\LoRA\\" + fname;
                    m.size_mb = mb;
                    m.is_custom = false;

                    if (lower_f.find("add_detail") != std::string::npos) {
                        m.display_name = "add_detail (Penajam Pori Kulit & Wajah)";
                        m.desc_plus = "Meningkatkan ketajaman tekstur mikro kulit, pori-pori, dan iris mata manusia DSLR.";
                        m.desc_minus = "Jika bobot terlalu tinggi (>1.2), gambar bisa sedikit noise.";
                        m.rec = "Sangat direkomendasikan untuk potret realistis (bobot 0.6 - 0.85).";
                    } else if (lower_f.find("lcm") != std::string::npos) {
                        m.display_name = "lcm_lora_sd15 (LCM Turbo 4-Step Denoise)";
                        m.desc_plus = "Mengubah model difusi SD 1.5 menjadi turbo 4 langkah kilat (~3-5 detik di CPU).";
                        m.desc_minus = "Sedikit penurunan ketajaman dibanding 25 steps standar.";
                        m.rec = "Gunakan saat butuh kecepatan generasi instan.";
                    } else {
                        m.is_custom = true;
                        m.display_name = "[LoRA] " + fname + " (" + std::to_string(mb) + " MB)";
                        m.desc_plus = "LoRA kustom otomatis terdeteksi.";
                        m.desc_minus = "-";
                        m.rec = "Bisa disetel bobot pengaruhnya pada alur pipa node.";
                    }
                    g_detected_loras.push_back(m);
                }
            } while (FindNextFileA(hFind, &fd));
            FindClose(hFind);
        }
    }

    g_detected_lora_labels.resize(g_detected_loras.size());
    g_lora_items.resize(g_detected_loras.size());
    for (size_t i = 0; i < g_detected_loras.size(); i++) {
        g_detected_lora_labels[i] = g_detected_loras[i].display_name;
        g_lora_items[i] = g_detected_lora_labels[i].c_str();
    }
    if (g_selected_lora_idx >= (int)g_detected_loras.size()) g_selected_lora_idx = 0;

    // 4. Pindai folder models\VAE\*.safetensors
    g_detected_vaes.clear();
    g_detected_vae_labels.clear();
    g_vae_items.clear();

    {
        DetectedModel fast_vae;
        fast_vae.display_name = "Fast VAE (TAESD ~2.7s CPU)";
        fast_vae.filename = "taesd.safetensors";
        fast_vae.full_path = "models\\VAE\\taesd.safetensors";
        fast_vae.desc_plus = "Decode latensi sangat rendah: 2.7 detik di CPU (22x lebih kencang dari VAE biasa).";
        fast_vae.desc_minus = "Warna sedikit lebih kontras.";
        fast_vae.rec = "Default terbaik untuk kecepatan CPU.";
        fast_vae.size_mb = 9;
        fast_vae.is_custom = false;
        g_detected_vaes.push_back(fast_vae);

        DetectedModel def_vae;
        def_vae.display_name = "(VAE Bawaan Checkpoint)";
        def_vae.filename = "";
        def_vae.full_path = "";
        def_vae.desc_plus = "Menggunakan VAE bawaan yang tertanam dalam file checkpoint.";
        def_vae.desc_minus = "Decoding membutuhkan waktu standar (~30-60 detik di CPU jika tanpa TAESD).";
        def_vae.rec = "Akurasi warna 100% sesuai perancang checkpoint.";
        def_vae.size_mb = 0;
        def_vae.is_custom = false;
        g_detected_vaes.push_back(def_vae);
    }

    hFind = FindFirstFileA("models\\VAE\\*.safetensors", &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                std::string fname = fd.cFileName;
                std::string lower_f = fname;
                for (char& c : lower_f) c = (char)tolower(c);
                if (lower_f.find("readme") != std::string::npos || lower_f == "taesd.safetensors") continue;

                ULARGE_INTEGER fileSize;
                fileSize.LowPart = fd.nFileSizeLow;
                fileSize.HighPart = fd.nFileSizeHigh;
                int mb = (int)(fileSize.QuadPart / (1024 * 1024));

                DetectedModel m;
                m.filename = fname;
                m.full_path = "models\\VAE\\" + fname;
                m.size_mb = mb;
                m.is_custom = false;

                if (lower_f.find("mse") != std::string::npos) {
                    m.display_name = "vae-ft-mse (SD1.5 High Quality MSE)";
                    m.desc_plus = "Warna lembut, akurat, dan gradasi kulit mulus tanpa banding artefak.";
                    m.desc_minus = "Ukuran 334 MB, waktu decode ~15-25 detik di CPU.";
                    m.rec = "Pilihan nomor 1 untuk rendering kualitas tertinggi majalah/DSLR.";
                } else if (lower_f.find("qwen") != std::string::npos) {
                    m.display_name = "qwen_image_2.1_vae (Qwen DiT BF16)";
                    m.desc_plus = "Decoder khusus untuk arsitektur difusi generasi baru DiT.";
                    m.desc_minus = "Hanya untuk model berbasis DiT/Qwen.";
                    m.rec = "Gunakan bersama checkpoint DiT.";
                } else {
                    m.is_custom = true;
                    m.display_name = "[VAE] " + fname + " (" + std::to_string(mb) + " MB)";
                    m.desc_plus = "VAE kustom otomatis terdeteksi.";
                    m.desc_minus = "-";
                    m.rec = "Dapat langsung dipilih pada Node VAE Decoder.";
                }
                g_detected_vaes.push_back(m);
            }
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }

    g_detected_vae_labels.resize(g_detected_vaes.size());
    g_vae_items.resize(g_detected_vaes.size());
    for (size_t i = 0; i < g_detected_vaes.size(); i++) {
        g_detected_vae_labels[i] = g_detected_vaes[i].display_name;
        g_vae_items[i] = g_detected_vae_labels[i].c_str();
    }
    if (g_selected_vae_idx >= (int)g_detected_vaes.size()) g_selected_vae_idx = 0;

    // 5. Pindai folder models\ControlNet\*.safetensors
    g_detected_cnets.clear();
    g_detected_cnet_labels.clear();
    g_cnet_items.clear();

    {
        DetectedModel none_cnet;
        none_cnet.display_name = "(Tanpa ControlNet / Disabled)";
        none_cnet.filename = "";
        none_cnet.full_path = "";
        none_cnet.desc_plus = "Generasi gambar murni dari prompt teks (Text-to-Image).";
        none_cnet.desc_minus = "-";
        none_cnet.rec = "Standar T2I.";
        none_cnet.size_mb = 0;
        none_cnet.is_custom = false;
        g_detected_cnets.push_back(none_cnet);
    }

    hFind = FindFirstFileA("models\\ControlNet\\*.safetensors", &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                std::string fname = fd.cFileName;
                std::string lower_f = fname;
                for (char& c : lower_f) c = (char)tolower(c);
                if (lower_f.find("readme") != std::string::npos) continue;

                ULARGE_INTEGER fileSize;
                fileSize.LowPart = fd.nFileSizeLow;
                fileSize.HighPart = fd.nFileSizeHigh;
                int mb = (int)(fileSize.QuadPart / (1024 * 1024));

                DetectedModel m;
                m.filename = fname;
                m.full_path = "models\\ControlNet\\" + fname;
                m.size_mb = mb;
                m.is_custom = false;

                if (lower_f.find("canny") != std::string::npos) {
                    m.display_name = "control_sd15_canny (Edge Detection Canny)";
                    m.desc_plus = "Mengunci garis tepi, postur pose manusia, dan siluet bentuk dari foto acuan.";
                    m.desc_minus = "Memerlukan input foto acuan yang jelas.";
                    m.rec = "Sangat ampuh untuk meniru pose persis dari foto referensi.";
                } else {
                    m.is_custom = true;
                    m.display_name = "[ControlNet] " + fname + " (" + std::to_string(mb) + " MB)";
                    m.desc_plus = "ControlNet kustom otomatis terdeteksi.";
                    m.desc_minus = "-";
                    m.rec = "Dapat langsung dipilih pada Node ControlNet.";
                }
                g_detected_cnets.push_back(m);
            }
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }

    g_detected_cnet_labels.resize(g_detected_cnets.size());
    g_cnet_items.resize(g_detected_cnets.size());
    for (size_t i = 0; i < g_detected_cnets.size(); i++) {
        g_detected_cnet_labels[i] = g_detected_cnets[i].display_name;
        g_cnet_items[i] = g_detected_cnet_labels[i].c_str();
    }
    if (g_selected_cnet_idx >= (int)g_detected_cnets.size()) g_selected_cnet_idx = 0;
}

ModelInspectionResult InspectModelFile(const std::string& filepath) {
    ModelInspectionResult res;
    res.filepath = filepath;

    size_t last_slash = filepath.find_last_of("\\/");
    res.filename = (last_slash != std::string::npos) ? filepath.substr(last_slash + 1) : filepath;

    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExA(filepath.c_str(), GetFileExInfoStandard, &fad)) {
        res.is_valid_file = false;
        res.is_critical_error = true;
        res.critical_reason = "File tidak dapat dibaca atau tidak ditemukan pada sistem file Windows.";
        return res;
    }

    res.is_valid_file = true;
    ULARGE_INTEGER sz;
    sz.LowPart = fad.nFileSizeLow;
    sz.HighPart = fad.nFileSizeHigh;
    res.file_size_mb = (int)(sz.QuadPart / (1024ULL * 1024ULL));

    std::string lower_fn = res.filename;
    for (char& c : lower_fn) c = (char)tolower(c);

    res.is_gguf = (lower_fn.rfind(".gguf") != std::string::npos);
    res.is_safetensors = (lower_fn.rfind(".safetensors") != std::string::npos);

    if (!res.is_gguf && !res.is_safetensors) {
        res.is_critical_error = true;
        res.critical_reason = "Format file tidak didukung! Aplikasi Pure C++ ini khusus mengeksekusi model berformat .gguf (llama.cpp) atau .safetensors (sd.cpp). Format seperti .ckpt, .bin, atau .pt tidak aman dan tidak didukung.";
        res.consequences.push_back("File tidak akan terbaca oleh mesin inferensi C++ native.");
        res.consequences.push_back("Dapat menyebabkan runtime crash jika dipaksakan masuk ke engine.");
        return res;
    }

    // Klasifikasi Kategori Model
    if (lower_fn.find("lora") != std::string::npos) {
        res.category = MODEL_CAT_LORA;
        res.category_name = "LoRA (Adaptor Bobot Ringan)";
        res.target_folder = "models\\LoRA";
    } else if (lower_fn.find("control") != std::string::npos || lower_fn.find("cnet") != std::string::npos) {
        res.category = MODEL_CAT_CONTROLNET;
        res.category_name = "ControlNet (Panduan Struktur & Pose)";
        res.target_folder = "models\\ControlNet";
    } else if (lower_fn.find("vae") != std::string::npos || lower_fn.find("taesd") != std::string::npos) {
        res.category = MODEL_CAT_VAE;
        res.category_name = "VAE (Decoder Gambar Cepat)";
        res.target_folder = "models\\VAE";
    } else if (lower_fn.find("mmproj") != std::string::npos || lower_fn.find("projector") != std::string::npos) {
        res.category = MODEL_CAT_VISION_MULTIMODAL;
        res.category_name = "Vision Projector (Mata Multimodal LLM)";
        res.target_folder = "models\\Multimodal_Vision";
    } else if (lower_fn.find("diffusion") != std::string::npos ||
               lower_fn.find("sd15") != std::string::npos ||
               lower_fn.find("sdxl") != std::string::npos ||
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
        if (GetFileAttributesA("models\\VAE\\taesd.safetensors") != INVALID_FILE_ATTRIBUTES) {
            res.dependencies_status.push_back("[OK] Fast VAE (TAESD): Terpasang di models\\VAE\\taesd.safetensors.");
        } else {
            res.dependencies_status.push_back("[!] [Non-Kritis] Fast VAE (taesd.safetensors) belum ada di folder models\\VAE.");
            res.consequences.push_back("KONSEKUENSI VAE: Foto tetap bisa digenerasi normal menggunakan VAE standar bawaan model. Namun, tahap decoding akhir gambar di CPU memakan waktu ~1 menit per foto (jika ada TAESD, hanya ~2.7 detik).");
        }

        if (lower_fn.find("lcm") != std::string::npos) {
            if (GetFileAttributesA("models\\LoRA\\lcm_lora_sd15.safetensors") != INVALID_FILE_ATTRIBUTES) {
                res.dependencies_status.push_back("[OK] LoRA LCM: Terpasang untuk akselerasi 4-step kilat.");
            } else {
                res.dependencies_status.push_back("[i] [Info] LoRA LCM belum terpasang.");
                res.consequences.push_back("KONSEKUENSI LCM: Model akan berjalan dengan jumlah langkah standar 20 steps (~20 detik di CPU), bukan 4 langkah.");
            }
        }

        if (res.file_size_mb > 4000) {
            res.dependencies_status.push_back("[!] [Hardware] Ukuran model di atas 4 GB.");
            res.consequences.push_back("KONSEKUENSI RAM: Membutuhkan RAM laptop sekitar 8-12 GB saat proses komputasi difusi. Disarankan menutup aplikasi berat lain saat rendering.");
        } else {
            res.dependencies_status.push_back("[OK] Ukuran model sangat ideal untuk RAM prosesor laptop.");
        }

        res.recommendation = "Model difusi siap dipasang ke folder models\\Gambar_Diffusion. Otomatis muncul di menu Mode Foto & NSFW.";
    }
    else if (res.category == MODEL_CAT_TEXT_LLM) {
        if (lower_fn.find("vl") != std::string::npos || lower_fn.find("vision") != std::string::npos) {
            bool has_mmproj = false;
            WIN32_FIND_DATAA fd_mm;
            HANDLE hMM = FindFirstFileA("models\\Multimodal_Vision\\*.gguf", &fd_mm);
            if (hMM != INVALID_HANDLE_VALUE) {
                has_mmproj = true;
                FindClose(hMM);
            }
            if (has_mmproj) {
                res.dependencies_status.push_back("[OK] Vision Projector (mmproj) terdeteksi di models\\Multimodal_Vision.");
            } else {
                res.dependencies_status.push_back("[!] [Perhatian] Model ini memiliki kapabilitas Vision (mata), tetapi file pendamping mmproj-*.gguf belum ditemukan.");
                res.consequences.push_back("KONSEKUENSI VISION: Model tetap bisa diajak mengobrol teks dan koding dengan normal, namun fitur analisis gambar/foto belum bisa aktif hingga file mmproj dipasang.");
            }
        }

        if (res.file_size_mb > 5000) {
            res.dependencies_status.push_back("[!] [Hardware] Model LLM besar (> 5 GB). Kecepatan token di CPU berkisar ~5-10 t/s.");
        } else {
            res.dependencies_status.push_back("[OK] Model sangat ringan dan responsif untuk komputasi CPU laptop.");
        }
        res.recommendation = "Model bahasa siap dipasang ke folder models\\Teks_LLM. Otomatis muncul di selector model Chat AI.";
    }
    else if (res.category == MODEL_CAT_LORA) {
        res.dependencies_status.push_back("[OK] LoRA siap dipasang untuk penyesuaian gaya gambar difusi.");
        res.recommendation = "File akan disimpan di models\\LoRA dan dapat diaktifkan pada menu opsi difusi.";
    }
    else if (res.category == MODEL_CAT_VAE) {
        res.dependencies_status.push_back("[OK] VAE siap dipasang untuk mempercepat decoding gambar.");
        res.recommendation = "File akan disimpan di models\\VAE.";
    }

    return res;
}

void StartImportModelWorker(const ModelInspectionResult& insp) {
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

void DeleteModelFile(const std::string& path_to_del) {
    if (GetFileAttributesA(path_to_del.c_str()) != INVALID_FILE_ATTRIBUTES) {
        DeleteFileA(path_to_del.c_str());
        ScanModelsDirectory();
    }
}

# 📂 PANDUAN STRUKTUR FOLDER & BERKAS AI STUDIO

Dokumen ini menjelaskan seluruh isi folder project, mana yang **KEPAKAI (AKTIF)**, mana yang **PENDUKUNG**, dan mana yang **ARSIP LAMA (TIDAK KEPAKAI)**.

---

## 🟢 1. BERKAS UTAMA NATIVE C++ (SANGAT KEPAKAI)
Ini adalah inti dari aplikasi modern 100% C++ Direct3D 11 yang super cepat, ringan (~51 MB RAM), dan mendukung touchscreen:

| Berkas / Folder | Status | Fungsi & Penjelasan |
| :--- | :---: | :--- |
| **`AI-Studio-Native.exe`** | 🟢 **UTAMA** | Aplikasi desktop mandiri 100% C++ murni. Direct3D 11, Touchscreen API, 60 FPS, chat bubble WA, dan tanpa Python. |
| **`engine_core.dll`** | 🟢 **UTAMA** | Library komputasi C++ AVX2 SIMD Core (Dot Product, telemetri RAM/CPU, interpolasi video). Digunakan oleh `AI-Studio-Native.exe`. |
| **`Jalankan-AI-Native-CPP.bat`** | 🟢 **UTAMA** | Shortcut batch untuk membuka langsung `AI-Studio-Native.exe`. |
| **`Buka-AI-Desktop.bat`** | 🟢 **UTAMA** | Launcher utama project, otomatis membuka versi C++ Native. |
| **`Kompilasi-Ulang-CPP.bat`** | 🟢 **UTAMA** | Script praktis untuk mengompilasi ulang DLL, benchmark, dan GUI C++ hanya dengan sekali klik. |
| **`Jalankan-Benchmark-CPP.bat`**| 🟢 **UTAMA** | Menjalankan pengujian kecepatan SIMD AVX2 vs CPU biasa. |
| **`cpp_core/`** | 🟢 **UTAMA** | Seluruh kode sumber C++: `src/` (kode GUI & Core), `include/` (header), `imgui/` (library UI), dan `bin/` (hasil kompilasi). |
| **`w64devkit/`** | 🟢 **UTAMA** | Toolchain compiler GCC 16 C++ mandiri offline (tidak perlu install MinGW/Visual Studio lagi). |

---

## 🟡 2. BERKAS MODEL & HASIL GENERASI (SANGAT KEPAKAI)
Semua bobot model AI dan hasil output gambar/video/audio:

| Folder | Status | Fungsi & Penjelasan |
| :--- | :---: | :--- |
| **`models/`** | 🟢 **KEPAKAI** | Tempat seluruh bobot AI tersimpan rapi: |
| ├── `Gambar_Diffusion/` | 🟢 | Model render gambar: *Realistic Vision V5.1*, *DreamShaper 8 LCM*, *Sakura-Iris 3B DiT*, *sd_model.gguf*. |
| ├── `Teks_LLM/` | 🟢 | Model Chat/Koding: *DeepSeek-R1 Distill*, *Qwen2.5-Coder 1.5B & 7B*, *Qwen3.8-2B Uncensored*. |
| ├── `Multimodal_Vision/` | 🟢 | Model pemroses gambar (Vision/VLM): *Janus-Pro-1B*, *mmproj-Qwen-Image*, *Sakura-TextEncoder*. |
| ├── `Gambar_OpenVINO/` | 🟢 | Model SD 1.5 format INT8 teroptimasi khusus Intel UHD Graphics iGPU. |
| ├── `LoRA/` | 🟢 | LoRA akselerasi LCM dan sudut kamera Qwen2.1. |
| ├── `VAE/` | 🟢 | VAE decoder bfloat16 untuk decoding gambar beresolusi tinggi. |
| └── `Audio_Suara/` | 🟢 | Model sintesis suara bahasa Indonesia (Piper TTS ONNX). |
| **`outputs/`** | 🟢 **KEPAKAI** | Seluruh hasil ekspor rapi: |
| ├── `foto/` | 🟢 | Subfolder otomatis tiap model: `realistic_vision/`, `dreamshaper_lcm/`, `sakura_iris/`, `qwen21_image/`, `openvino_sd15/`, `sd15_gguf/`. |
| ├── `video/` | 🟢 | Berkas MP4 dan GIF hasil interpolasi gerak C++ AVX2. |
| ├── `audio/` | 🟢 | Berkas suara MP3 dan WAV hasil pembacaan asisten. |
| └── `exports/` | 🟢 | Berkas ringkasan percakapan dalam format Markdown. |
| **`sd_bin/`** | 🟢 **KEPAKAI** | Binary `stable-diffusion.cpp` (`sd-cli.exe`, `stable-diffusion.dll`) untuk inferensi Diffusion cepat di CPU tanpa PyTorch. |

---

## 🔵 3. BERKAS PYTHON BACKEND (KEPAKAI SEBAGAI OPSIONAL / CADANGAN)
File ini digunakan jika Anda ingin menjalankan versi Python Tkinter atau sebagai pembanding:

| Berkas | Status | Fungsi & Penjelasan |
| :--- | :---: | :--- |
| **`native_app.py`** | 🔵 **CADANGAN** | GUI Desktop versi Python (CustomTkinter). Memiliki fitur side-by-side comparison dan touch scroll. |
| **`engine.py`** | 🔵 **PENDUKUNG** | Backend inferensi Python yang menghubungkan LLM Llama-cpp, Diffusion, VLM, dan TTS. |
| **`cpp_bridge.py`** | 🔵 **PENDUKUNG** | Modul ctypes Python untuk memanggil fungsi SIMD AVX2 di `engine_core.dll`. |
| **`sd_directml.py`** | 🔵 **PENDUKUNG** | Pipeline Stable Diffusion Python dengan fallback CPU anti-crash. |
| **`Buka-AI-Python-Legacy.bat`** | 🔵 **OPSIONAL** | Launcher untuk membuka GUI versi Python jika sewaktu-waktu ingin melihat versi lama. |
| **`AI-Studio.exe`** | 🔵 **OPSIONAL** | Launcher launcher C++ lama yang memanggil `native_app.py`. |
| **`chat_sessions.json`** | 🔵 **DATA** | Menyimpan riwayat percakapan dari versi Python. |
| **`negative_presets.json`**| 🔵 **DATA** | Menyimpan preset negative prompt gambar. |

---

## 🔴 4. BERKAS YANG TELAH DIHAPUS (BERSIH TOTAL)
Berkas usang berikut telah berhasil dieksekusi dan dihapus secara permanen dari sistem:

| Berkas / Folder | Keterangan Penghapusan |
| :--- | :--- |
| ~~`app.py`~~ | **DIHAPUS PERMANEN** (Gradio WebUI lama sebelum era aplikasi desktop). |
| ~~`__pycache__/`~~ | **DIHAPUS PERMANEN** (Seluruh berkas cache `.pyc` lama). |
| ~~`cpp_core/obj/`~~ | **DIHAPUS** (Direktori sementara kosong). |
| ~~`models/ControlNet/`~~ | **DIHAPUS** (Direktori kosong). |

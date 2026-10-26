# ⚡ AI Studio — 100% Pure C++ Native Workstation

![Platform](https://img.shields.io/badge/Platform-Windows%2010%20%2F%2011-blue)
![Language](https://img.shields.io/badge/Language-C%2B%2B17%20Native-00599C)
![DirectX](https://img.shields.io/badge/Renderer-Direct3D%2011%20(60%20FPS)-purple)
![AVX2](https://img.shields.io/badge/Acceleration-AVX2%20%2B%20FMA%20%2B%20OpenMP-orange)
![Version](https://img.shields.io/badge/Version-v1.3.1-brightgreen)
![License](https://img.shields.io/badge/License-MIT-green)

Aplikasi studio AI desktop generasi baru yang dibangun **100% murni menggunakan C++ native dan Direct3D 11**. Menggabungkan **Large Language Model (LLM)** dan **Diffusion Photo Generator** dalam satu biner mandiri berukuran **2.39 MB** tanpa runtime Python, PyTorch, Electron, maupun virtual environment.

---

## 🌟 Fitur Unggulan

- **0% Python / Electron Dependency**: Berjalan langsung tanpa instalasi Python, PyTorch, CUDA Toolkit eksternal, atau Node.js.
- **Arsitektur Modular C++ (-O3 SIMD)**: Dibangun dengan subsistem terisolasi (`hardware_detector`, `model_manager`, `prompt_engine`, `memory_manager`, `hardware_telemetry`).
- **MemoryManager (RAM & VRAM Purge)**: Pembersihan memori asinkron yang melepas D3D11 Texture Shader Resource Views dan memadatkan RAM fisik proses via Win32 `EmptyWorkingSet`.
- **Hardware Telemetry Concurrency (Zero UI Stuttering)**: Background worker thread (interval 500ms) memantau pemakaian RAM fisik dan CPU delta tanpa memicu frame drop pada loop render D3D11 60 FPS.
- **Dashboard Card Hub DirectX 11**: Tampilan katalog mode modern berbasis vektor `ImDrawList` dengan efek *2-layer border glow* saat kursor hover, aksen warna per kategori, dan spec pills.
- **Fast VAE Acceleration (TAESD)**: Memangkas waktu decode VAE di CPU dari **61 detik** menjadi **2.7 detik** (~22x lebih cepat).
- **LLM Reasoning & Coding**: Mendukung model GGUF (DeepSeek-R1, Qwen2.5-Coder, Llama 3) dengan multi-turn chat memory dan pembersih tag `<think>` otomatis.
- **Fotorealistis & Uncensored Diffusion**: Model CyberRealistic V5 dan DreamShaper dengan kebebasan kustomisasi 1 hingga 50 langkah sampling (*denoising steps*).
- **RAG & Analisis Dokumen Native**: Pembacaan dokumen Word (`.docx`), Excel (`.xlsx`, `.csv`), PDF, dan skrip koding native.
- **Suara Alami Offline (TTS)**: Pembacaan respon AI dengan profil suara manusia (Gadis & Ardi) via ONNX Piper dan Windows SAPI native.

---

## 📁 Struktur Arsitektur Modular

```text
├── cpp_core/                  # Kode sumber C++ & arsitektur sistem
│   ├── include/               # Header modular subsistem
│   │   ├── hardware_detector.h   # Deteksi CPU, RAM, & GPU NVIDIA RTX
│   │   ├── model_manager.h       # Scanner GGUF/safetensors & metadata
│   │   ├── prompt_engine.h       # Prompt expander, grammar, DeepSeek cleaner
│   │   ├── memory_manager.h      # Controller RAM fisik & VRAM D3D11
│   │   ├── hardware_telemetry.h  # Concurrency telemetry background worker
│   │   ├── engine_core.h         # Vector math SIMD AVX2
│   │   └── simd_math.h           # Intrinsics AVX2/FMA dot-product & cosine
│   ├── src/                   # Implementasi C++
│   │   ├── gui_main.cpp          # Main window & Direct3D 11 render loop
│   │   ├── hardware_detector.cpp
│   │   ├── model_manager.cpp
│   │   ├── prompt_engine.cpp
│   │   ├── memory_manager.cpp
│   │   ├── hardware_telemetry.cpp
│   │   └── engine_core.cpp
│   └── imgui/                 # Backend Dear ImGui DirectX 11 & Win32
├── models/                    # Direktori penyimpanan model AI
│   ├── Teks_LLM/              # Model LLM format .gguf
│   ├── Gambar_Diffusion/      # Model difusi format .safetensors
│   ├── VAE/                   # Fast VAE TAESD
│   ├── LoRA/                  # Detailer mikro & Turbo LCM
│   ├── ControlNet/            # Pose lock model Canny
│   └── Audio_Suara/           # Model ONNX TTS Piper
├── outputs/                   # Folder hasil output gambar, audio, & video
├── tools/                     # Skrip otomasi batch (Kompilasi & Benchmark)
├── compile_step_by_step.bat   # Skrip kompilasi inkremental (-O3 AVX2 SIMD)
└── CHANGELOG.md               # Riwayat perubahan terstruktur dari v0.0.1 ke v1.1.3
```

---

## 🛠️ Kompilasi & Build

Kompilasi dilakukan menggunakan compiler GCC 16 (w64devkit) dengan optimasi SIMD AVX2 penuh:

```bat
compile_step_by_step.bat
```

Flag kompilasi:
```text
-O3 -mavx2 -mfma -fopenmp -DSTATIC_ENGINE_CORE -s
```

Hasil biner executable: **`AI-Studio.exe`** (~2.39 MB).

---

## 📋 Riwayat Versi (Changelog)

Catatan rilis lengkap dari versi awal `v0.0.1` hingga build terbaru `v1.1.3` dapat dilihat pada dokumen:
👉 **[CHANGELOG.md](CHANGELOG.md)**

---

## 🧠 Dokumentasi Obsidian Vault

Dokumentasi arsitektur mendalam, catatan ilmu teknik C++, dan laporan pengujian tersimpan rapi pada Obsidian vault:
- **`Knowledge-Base-Teknik-C++.md`**: Catatan rekayasa sistem, flag kompiler, concurrency, dan pencegahan bug C++.
- **`Panduan-Penggunaan-dan-Spesifikasi.md`**: Panduan pengoperasian dan peta konfigurasi model AI.
- **`AI-Studio-Native-Report.md`**: Laporan teknis inkremental dan riwayat build biner.

---

## 📄 Lisensi
Didistribusikan di bawah lisensi MIT. Bebas digunakan, dimodifikasi, dan didistribusikan secara mandiri.

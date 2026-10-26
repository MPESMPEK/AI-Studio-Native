# 📋 Changelog — AI Studio Native

Semua catatan perubahan, perbaikan bug, penambahan fitur, dan perombakan arsitektur proyek **AI Studio** dari era prototipe Python pertama hingga arsitektur native C++ Direct3D 11 terbaru dicatat secara urut dan terstruktur di bawah ini.

---

## 🏷️ Skema Penomoran Versi (Semantic Versioning Rules)
- **`v0.0.1 - v0.0.9`**: Prototipe awal & perbaikan bug (*Bug Fixes / Patch*).
- **`v0.1.x - v0.9.x`**: Pembaruan fitur menengah (*Minor Features & Enhancements*).
- **`v1.x.x`**: Perubahan drastis arsitektur & sistem inti (*Major System Overhaul*).

---

## [v0.0.1] - 2026-10-23 (Era Prototipe Python)
### ✨ Fitur Baru
- **Rilis Prototipe Perdana AI Studio (Python/PyTorch)**:
  - Pembuatan skrip awal berbasis Python menggunakan HuggingFace `diffusers`, `transformers`, dan `torch`.
  - Integrasi uji coba model Large Language Model (LLM) untuk percakapan teks dan Stable Diffusion untuk pembuatan gambar teks-ke-foto.

---

## [v0.0.2] - 2026-10-23
### 🐛 Bug Fix
- **Penanganan Out-Of-Memory (OOM) Python**:
  - Mengatasi crash kehabisan memori RAM/VRAM di PyTorch dengan mengaktifkan presisi setengah `torch.float16` dan `enable_attention_slicing()`.

---

## [v0.0.3] - 2026-10-23
### 🐛 Bug Fix
- **Fallback Otomatis CPU Backend**:
  - Memperbaiki kegagalan inisialisasi pada laptop tanpa GPU diskrit NVIDIA (`CUDA not available`) dengan membuat pipeline fallback otomatis ke komputasi CPU.

---

## [v0.0.4] - 2026-10-23
### 🐛 Bug Fix
- **Konflik Dependensi Library Pip**:
  - Menyelesaikan konflik versi antar paket Python (`safetensors`, `accelerate`, dan `huggingface-hub`) yang menyebabkan script generator gagal mengimpor model checkpoint.

---

## [v0.0.5] - 2026-10-24
### 🔄 Pembaruan / Update
- **Optimalisasi Model & Reduksi Bobot Python**:
  - Mengganti checkpoint berat dengan format `CyberRealistic_V5` dan model teks quantisasi 4-bit (GGUF), memotong penggunaan RAM dari 6 GB ke ~3.5 GB.

---

## [v0.1.0] - 2026-10-24
### ✨ Fitur Baru
- **Integrasi ControlNet Canny Pose-Locking**:
  - Menambahkan modul deteksi kontur tepi gambar (Canny edge detection) untuk mengunci pose tubuh dan gestur foto referensi secara presisi.
### 🐛 Bug Fix
- **Kompatibilitas Tensor ControlNet**:
  - Memperbaiki model Canny yang tidak cocok (format lama Diffusers) dengan menggantinya ke model standar FP16 (`control_sd15_canny.safetensors`, 722 MB).

---

## [v0.2.0] - 2026-10-24
### ✨ Fitur Baru
- **Analisis Dokumen & Pembacaan Kode Native (RAG Lokal)**:
  - Menambahkan kemampuan membaca file dokumen Word (`.docx`), Excel (`.xlsx`, `.csv`), PDF, dan skrip koding (`.py`, `.cpp`, `.json`).
### 🐛 Bug Fix
- **Batasan Baris Perintah Windows 8 KB**:
  - Mengatasi batas arsitektur `CreateProcessA` (maksimal 8.191 karakter) saat pengguna memasukkan dokumen panjang dengan mengalirkan teks prompt ke file sementara `llama_prompt.txt` via argumen `-f`.

---

## [v0.3.0] - 2026-10-25
### ✨ Fitur Baru
- **3 Mode Penanganan Prompt Dinamis**:
  - *Mode Raw (Asli)*: AI mengeksekusi 100% prompt teks apa adanya tanpa modifikasi.
  - *Mode Koreksi Grammar*: Menerjemahkan dan merapikan ejaan secara setia tanpa menambah objek baru.
  - *Mode Auto AI*: Ekspansi prompt cerdas dengan pencahayaan kamera DSLR anti-halusinasi.
### 🔄 Pembaruan / Update
- **Pembersihan Komponen Obesitas (DiT Model Removal)**:
  - Menghapus model Qwen-Image DiT (5.6 GB) dan Qwen3-VL (4.1 GB) yang membutuhkan RAM > 10.4 GB (melebihi kapasitas RAM laptop 8 GB), menghemat 10 GB ruang penyimpanan SSD.
  - Auto-load VAE Realistik MSE dan LoRA Realism untuk foto kulit manusia fotorealistis.

---

## [v0.4.0] - 2026-10-25
### ✨ Fitur Baru
- **Integrasi Neural Voice TTS Offline**:
  - Menambahkan mode pemrosesan suara AI dengan profil suara manusia (Gadis & Ardi) via model ONNX Piper dan Windows SAPI.
### 🐛 Bug Fix
- **Crash Assertion ImGui**:
  - Memperbaiki assertion error `BeginChild` saat rendering streaming gelembung live typing.
- **Logika Suara Default**:
  - Menonaktifkan auto-speak default agar asisten AI hanya bersuara saat diminta (*on-demand*).

---

## [v0.5.0] - 2026-10-25
### 🔄 Pembaruan / Update
- **Desain Katalog Modern Hub (Uizard Template)**:
  - Menghadirkan portal beranda kartu visual modern horizontal di layar Home dengan sampul kustom pilihan pengguna.
- **Kebebasan Kustomisasi Langkah Sampling (Steps 1-50)**:
  - Membuka kebebasan slider steps dari 1 hingga 50 langkah (fleksibel untuk Turbo LCM 4-step maupun DSLR 20-35 steps).
- **Penegasan Label Mode Prompt**:
  - Menata label kontrol prompt yang jelas dan terarah: *Raw (Asli)*, *Koreksi*, dan *Auto AI*.

---

## [v1.0.0] - 2026-10-25
### 🚀 Perubahan Drastis: Transisi Total ke Pure C++ DirectX 11 Native
- **Eliminasi Total Runtime Python**:
  - Menghapus 100% dependensi ekosistem Python, PyTorch, pip, dan virtual environment. Menghemat ~12 GB disk dan memangkas penggunaan RAM idle dari 3.5 GB menjadi **< 45 MB**.
- **Arsitektur Desktop Hardware-Accelerated Single Binary**:
  - Membangun aplikasi desktop murni menggunakan **C++17**, **Win32 API**, dan **Direct3D 11 Hardware SwapChain** (terkunci di VSync 60 FPS bebas lag).
  - Menghasilkan biner mandiri super ringkas (~2.39 MB) tanpa instalasi dependensi luar.
- **Fast VAE Acceleration (TAESD)**:
  - Memangkas waktu decode gambar di CPU dari **61 detik** menjadi **2.7 detik** (~22x lebih cepat).

---

## [v1.1.0] - 2026-10-26
### 🚀 Perubahan Drastis: Arsitektur Modular C++ & Kompilasi -O3 AVX2 SIMD
- **Modularisasi Kode Sumber**:
  - Memecah monolith `gui_main.cpp` (~4,800 baris) menjadi subsistem-subsistem C++ mandiri dengan header dan translation unit terpisah:
    - `cpp_core/include/hardware_detector.h` & `hardware_detector.cpp`: Deteksi CPU cores, AVX2 SIMD, dan GPU NVIDIA RTX.
    - `cpp_core/include/model_manager.h` & `model_manager.cpp`: Pemindaian model GGUF/safetensors, inspeksi metadata, dan impor asinkron.
    - `cpp_core/include/prompt_engine.h` & `prompt_engine.cpp`: Logika prompt enrichment, translator, dan pembersih output DeepSeek-R1.
- **Optimasi Kompilasi SIMD Tertinggi**:
  - Mengonfigurasi `compile_step_by_step.bat` dengan flag `-O3 -mavx2 -mfma -fopenmp -s`.
  - Mengaktifkan vektorisasi AVX2/FMA paralel pada CPU, menghasilkan kecepatan kalkulasi matematika 4096-dimensi hingga **1,027.7 juta operasi/detik** pada latensi **0.973 mikrodetik**.

---

## [v1.2.0] - 2026-10-26
### ✨ Fitur Baru
- **Modul Manajemen Memori Terintegrasi (`MemoryManager`)**:
  - Fitur pembersihan memori asinkron (`PurgeAllAsync`) tanpa freezing / stutter pada loop render D3D11 60 FPS.
  - Pelepasan D3D11 Texture Shader Resource Views (SRV) tidak aktif dan pemanggilan pipeline `ClearState()` / `Flush()`.
  - Pemadatan physical RAM seketika ke OS via Win32 `EmptyWorkingSet()` dan `SetProcessWorkingSetSize()`.
- **Komponen UI Vektor `DrawDashboardCard()` D3D11**:
  - Komponen rendering kartu berbasis vektor langsung menggunakan `ImDrawList`.
  - Aksen warna per kategori: Cyan (Chat AI), Hijau Emerald (Studio Foto), Merah Neon Crimson (Dewasa 18+), dan Ungu Indigo (Suara AI).
  - Efek visual *Smooth Hover & Glowing Border* 2-layer alpha dan corner spec pills (`Q4_K_M`, `FP16`, `RAM: ~1.2 GB`).

---

## [v1.3.0] - 2026-10-26
### ✨ Fitur Baru
- **Modul Hardware Telemetry Concurrency & Status Bar D3D11 (Zero UI Stuttering)**:
  - Background worker thread Win32 dengan interval 500ms via `std::condition_variable` untuk mengambil pemakaian RAM (`GlobalMemoryStatusEx`) dan beban CPU delta (`GetSystemTimes`).
  - Getter thread-safe ultra-cepat (< 10 nanodetik) untuk menyalin struct snapshot data tanpa membebani render thread utama.
  - Status Bar sleek di bagian bawah jendela aplikasi:
    - Indikator RAM % dinamis (Hijau, Kuning, Merah).
    - Indikator CPU % beserta core logis dan thread AI aktif.
    - Status flag instruksi SIMD (`AVX2+FMA` / `SSE4.2`).
    - Status Fast VAE (`TAESD: AKTIF` / `OFF`).
    - Real-time VSync FPS counter & frame time Direct3D 11.

---

## [v1.3.1] - 2026-10-26
### 🐛 Bug Fix
- **Perbaikan Skrip Otomasi Batch (`tools/`)**:
  - Memperbaiki error parser batch Windows CMD akibat karakter emoji Unicode pada `title` dan simbol redirection `>` yang tidak sengaja membuat file sampah.
  - Sinkronisasi `tools/Kompilasi-Ulang-CPP.bat` agar memanggil alur kompilasi modular `compile_step_by_step.bat`.
- **Eliminasi Peringatan Kompiler Ketat (`-Wall -Wextra`)**:
  - Memperbaiki format string `ImGui::TextColored` pada string `"Mode Uncensored 100% C++"` yang sebelumnya memicu warning compiler `% C` (format specifier tidak sengaja terbaca) menjadi `"%s"`.
  - Menghilangkan karakter backslash `\` di akhir komentar baris `ScanModelsDirectory()` yang berisiko memicu *multi-line comment line splicing* pada standar C++.
  - Memperbaiki inisialisasi struct `CoreTelemetry` menjadi standar C++ `{}`.
  - Menghapus variabel `total_steps` yang tidak digunakan (*unused variable*).
  - Seluruh codebase kini terverifikasi **0 Error, 0 Warning**.

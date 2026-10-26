# 📋 Changelog — AI Studio Native C++

Semua catatan perubahan, perbaikan bug, penambahan fitur, dan perombakan arsitektur proyek **AI Studio Native (Direct3D 11 & C++ SIMD)** dicatat secara terstruktur di bawah ini.

---

## 🏷️ Skema Penomoran Versi (Semantic Versioning Rules)
- **`v0.0.1 - v0.0.9`**: Rilis awal & perbaikan bug (*Bug Fixes / Patch*).
- **`v0.1.x - v0.9.x`**: Pembaruan fitur menengah (*Minor Features & Enhancements*).
- **`v1.x.x`**: Perubahan drastis arsitektur & modul inti (*Major System Overhaul*).

---

## [v0.0.1] - 2026-10-24
### ✨ Fitur Baru
- **Fondasi Rilis Perdana AI Studio Native**:
  - Implementasi 100% Pure C++ Standalone Workstation tanpa runtime Python, Electron, ataupun PyTorch.
  - Integrasi rendering windowing native Win32 & pipeline Direct3D 11 (VSync 60 FPS).
  - Integrasi mesin inferensi lokal: LLaMA (LLM) dan Stable Diffusion (Generasi Gambar) berbasis CLI backend native.

---

## [v0.0.2] - 2026-10-25
### ✨ Fitur Baru
- **Integrasi Suara Alami (Neural Voice TTS)**:
  - Menambahkan mode pemrosesan suara AI dengan profil suara manusia (Gadis & Ardi) via model ONNX Piper dan Windows SAPI offline.

---

## [v0.0.3] - 2026-10-25
### 🐛 Bug Fix
- **Perbaikan Crash Assertion ImGui**:
  - Memperbaiki error `BeginChild` assertion saat rendering gelembung chat teks live typing streaming.

---

## [v0.0.4] - 2026-10-25
### 🐛 Bug Fix
- **Auto-Detection Model Diffusion DiT**:
  - Memperbaiki parsing otomatis arsitektur model DiT dan pemisahan perutean Text Encoder dan VAE terpisah.
- **Logika Suara Default**:
  - Menonaktifkan fitur auto-speak secara default agar asisten AI hanya bersuara jika diminta eksplisit (*on-demand*).

---

## [v0.1.0] - 2026-10-25
### ✨ Fitur Baru
- **Integrasi ControlNet Canny Pose-Locking**:
  - Menambahkan modul deteksi tepi gambar (Canny edge detection) untuk mengunci pose tubuh referensi pada foto studio.
  - Menambahkan saklar pintar (smart toggles) di antarmuka input gambar.

---

## [v0.2.0] - 2026-10-25
### ✨ Fitur Baru
- **Analisis Dokumen & Pembacaan Kode Native (RAG Lokal)**:
  - Dukungan drag-and-drop file dokumen ke dalam percakapan AI.
  - Parser dokumen native untuk Microsoft Word (`.docx`), Microsoft Excel (`.xlsx`, `.csv`), dokumen teks (`.pdf`, `.txt`, `.md`), serta file skrip koding (`.cpp`, `.py`, `.json`, `.bat`, dll).

---

## [v0.3.0] - 2026-10-25
### 🔄 Pembaruan / Update
- **Optimalisasi Pipeline Realistik**:
  - Auto-load VAE Realistik MSE dan LoRA Realism otomatis untuk model foto manusia.
  - Pembersihan komponen runtime yang tidak digunakan untuk menghemat memori.
### ✨ Fitur Baru
- **3 Mode Penanganan Prompt Dinamis**:
  - Mode 1: *Prompt Asli (Raw Direct)* — prompt diteruskan tanpa modifikasi.
  - Mode 2: *Koreksi Grammar Saja* — merapikan ejaan dan tata bahasa prompt tanpa menambah konten.
  - Mode 3: *Auto AI (Kembangkan Cerdas)* — ekspansi prompt fotorealistis cerdas anti-halusinasi.

---

## [v0.4.0] - 2026-10-25
### 🔄 Pembaruan / Update
- **Rombak Total Tata Letak UI/UX**:
  - Pembersihan layout antarmuka agar bebas tabrakan (anti-overlap).
  - Kontrol kontekstual yang menyesuaikan secara dinamis tergantung mode yang aktif (Teks, Foto, Dewasa, Suara).
### ✨ Fitur Baru
- **Dashboard Card Hub DirectX 11**:
  - Menghadirkan portal beranda kartu visual modern (terinspirasi dari template Uizard) yang dirender native di Direct3D 11.

---

## [v0.5.0] - 2026-10-26
### 🔄 Pembaruan / Update
- **Penyempurnaan Dashboard Hub**:
  - Kartu horizontal dengan rasio visual seimbang dan integrasi thumbnail real-time.
  - Pengguna bebas memilih gambar sampul (cover) kustom untuk masing-masing kartu mode AI.
- **Bebas Kustomisasi Steps**:
  - Membuka kebebasan slider langkah sampling (1-50 steps) untuk model Turbo LCM (4-step) maupun CyberRealistic DSLR (20-30 steps).
- **Penegasan Label Mode Prompt**:
  - Penataan label combo box yang jelas dan deskriptif: *Raw (Asli)*, *Koreksi*, dan *Auto AI*.

---

## [v1.0.0] - 2026-10-26
### 🚀 Perubahan Drastis (Major Architecture Overhaul)
- **Modularisasi Arsitektur C++**:
  - Memecah kode monolitik `gui_main.cpp` (~4,800 baris) menjadi modul-modul subsistem C++ mandiri yang terisolasi dan mudah dikelola:
    - `cpp_core/include/hardware_detector.h` & `hardware_detector.cpp`: Deteksi hardware, CPU cores, AVX2 SIMD, dan GPU NVIDIA RTX.
    - `cpp_core/include/model_manager.h` & `model_manager.cpp`: Pemindaian model GGUF/safetensors, inspeksi metadata, dan impor asinkron.
    - `cpp_core/include/prompt_engine.h` & `prompt_engine.cpp`: Logika prompt enrichment, translator, dan pembersih output DeepSeek-R1.
- **Peningkatan Kompiler Maksimal (-O3 SIMD)**:
  - Mengonfigurasi `compile_step_by_step.bat` dengan flag `-O3 -mavx2 -mfma -fopenmp -s`.
  - Auto-vectorization SIMD AVX2 aktif di seluruh modul komputasi inti.
  - Menghasilkan biner super ramping (2.38 MB) dengan waktu linking inkremental jauh lebih cepat.

---

## [v1.1.0] - 2026-10-26
### ✨ Fitur Baru
- **Modul Manajemen Memori Terintegrasi (`MemoryManager`)**:
  - Dibuat `cpp_core/include/memory_manager.h` dan `cpp_core/src/memory_manager.cpp`.
  - Fitur pembersihan memori asinkron (`PurgeAllAsync`) tanpa freezing / stutter pada loop render D3D11 60 FPS.
  - Pelepasan Direct3D 11 Shader Resource Views (SRV) tekstur tidak terpakai dan pemanggilan pipeline `ClearState()` / `Flush()`.
  - Pemadatan physical RAM seketika ke OS via Win32 `EmptyWorkingSet()` dan `SetProcessWorkingSetSize()`.
  - Menambahkan panel kontrol pemantauan RAM & tombol *Purge All* di menu Pengaturan Studio.

---

## [v1.1.1] - 2026-10-26
### ✨ Fitur Baru
- **Komponen UI Vektor `DrawDashboardCard()` Direct3D 11**:
  - Komponen rendering kartu berbasis vektor langsung menggunakan `ImDrawList`.
  - Aksen warna per kategori: Cyan (Chat AI), Hijau Emerald (Studio Foto), Merah Neon Crimson (Dewasa 18+), dan Ungu Indigo (Suara AI).
  - Efek visual *Smooth Hover & Glowing Border*: 2-layer alpha glow halus saat kursor melintas.
  - Badges / spec pills di sudut kartu yang menampilkan spesifikasi model (seperti `Q4_K_M`, `FP16`, `RAM: ~1.2 GB`, `Latensi 0ms`).

---

## [v1.1.2] - 2026-10-26
### ✨ Fitur Baru
- **Modul Hardware Telemetry Concurrency & Status Bar D3D11 (Zero UI Stuttering)**:
  - Dibuat `cpp_core/include/hardware_telemetry.h` dan `cpp_core/src/hardware_telemetry.cpp`.
  - Background worker thread Win32 dengan interval 500ms via `std::condition_variable` untuk mengambil pemakaian RAM (`GlobalMemoryStatusEx`) dan beban CPU delta (`GetSystemTimes`).
  - Getter thread-safe ultra-cepat (< 10 nanodetik) untuk menyalin struct snapshot data tanpa membebani render thread utama.
  - Status Bar sleek di bagian bawah jendela aplikasi:
    - Indikator RAM % dengan pewarnaan dinamis (Hijau, Kuning, Merah).
    - Indikator CPU % beserta core logis dan thread AI aktif.
    - Status flag instruksi SIMD (`AVX2+FMA` / `SSE4.2`).
    - Status Fast VAE (`TAESD: AKTIF` / `OFF`).
    - Real-time VSync FPS counter & frame time Direct3D 11.

---

## [v1.1.3] - 2026-10-26
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

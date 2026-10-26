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

## [v1.5.0] - 2026-10-26
### 🎨 Perombakan Desain UI/UX Total (All-New Aesthetic & Zero-Collision Architecture)
- **Eliminasi Total Tabrakan & Tumpang Tindih Komponen (Zero-Collision Geometry)**:
  - Mengisolasi tata letak layar menjadi zona terpartisi matematis murni. Seluruh komponen (Sidebar, Top Navbar, Sub Navbar, Chat Canvas, dan Bottom Input Dock) dihitung secara adaptif terhadap sisa ruang di atas Status Bar.
  - Menghilangkan `TouchExtraPadding = (6, 6)` liar pada Dear ImGui (`TouchExtraPadding = (0, 0)`), mengembalikan komputasi batas hit-test dan bounding box ke tingkat presisi piksel 1:1.
- **Encapsulated Control Ribbon (PhotoOptionsRibbon & TextOptionsRibbon)**:
  - Seluruh tombol kontrol studio foto (`Prompt Asli/Auto`, `Framing Kamera`, `Rasio Aspek`, `Steps`, `HD Wajah`, dan `Foto Acuan`) kini berada di dalam ribbon berbingkai kartu modern (*encapsulated child container*) yang adaptif:
    - Layar Lebar (>720px): Ditata dalam 1 baris ramping.
    - Layar Sedang/Kecil (<720px): Otomatis ditata menjadi 2 tingkat rapi tanpa pernah memotong teks atau menembus batas status bar.
  - Area teks obrolan juga memiliki ribbon khusus untuk lampiran file, pencarian web, dan panduan tombol kirim.
- **Tema Visual Obsidian Studio (Palet Gelap Mewah & Estetik Tinggi)**:
  - Latar belakang gradasi obsidian pekat (`#090D14`), kartu midnight slate (`#0E1421`), bingkai pemisah halus (`#243047`), dan aksen Electric Blue (`#3182CE`), Vivid Violet (`#8B5CF6`), Rose Crimson (`#F43F5E`), serta Emerald Mint (`#10B981`).
  - Brand Header Card baru di sidebar kiri dengan logo `⚡ AI STUDIO NATIVE` dan indikator status aktif `🟢 100% C++ Direct3D 11`.
- **Sidebar Dinamis Anti-Tergusur**:
  - Tinggi daftar topik percakapan (`TopicsList`) kini dihitung secara fleksibel dari sisa ruang (`main_content_h - header_h - footer_h`), menjamin panel telemetri hardware dan tombol `[⚙️ Pengaturan]` serta `[📁 Output]` selalu terpancang kokoh dan tampak utuh di bagian bawah.
- **Penyatuan Sistem Desain Resmi ke Obsidian Vault**:
  - Diterbitkannya dokumen standar sistem desain lengkap di `docs/UI_UX_DESIGN_SYSTEM.md` yang merinci filosofi, diagram grid, token warna, spesifikasi komponen, dan pintasan keyboard global.

---

## [v1.4.2] - 2026-10-26
### ✨ Fitur Baru (Penampil Gambar Layar Penuh Sinematik & F11 Borderless Window)
- **Penampil Gambar Layar Penuh Sinematik Direct3D 11 (`RenderFullscreenImageModal`)**:
  - Pengguna kini dapat langsung mengklik gambar hasil render di dalam bubble chat atau menekan tombol `🔍 Layar Penuh` untuk membuka penampil gambar fullscreen sinematik di dalam aplikasi.
  - Gambar ditampilkan terpusat (*centered*) dengan menjaga rasio aspek asli (*aspect ratio preservation*) pada latar belakang gelap mewah.
  - Menampilkan informasi resolusi gambar (contoh: `512 x 768 px`).
  - Dilengkapi tombol navigasi cepat: `❌ Tutup (ESC)`, `📁 Buka di File Explorer`, `🖼️ Buka di Aplikasi Windows`, dan `📋 Salin Path Gambar`.
  - Dapat ditutup seketika dengan menekan tombol keyboard `ESC`.
- **Mode Layar Penuh Borderless Jendela Aplikasi (`ToggleBorderlessFullscreen`)**:
  - Menambahkan tombol `⛶ Layar Penuh` / `🗗 Normal` di navbar atas aplikasi.
  - Mendukung shortcut keyboard global `F11` untuk beralih ke mode layar penuh tanpa border (*borderless fullscreen*), menutupi taskbar untuk pengalaman kerja AI maksimal.
- **Perbaikan Pemanggilan Path Absolut Windows (`GetFullPathNameA`)**:
  - Mengatasi masalah tombol "Layar Penuh" sebelumnya yang tidak merespons di Windows 10/11 saat membuka aplikasi penampil foto bawaan Windows (UWP Photos) akibat penggunaan path relatif. Kini seluruh pemanggilan berkas dipastikan menggunakan path absolut kanonikal.

### 🐛 Bug Fix
- **Pembersihan Scrollbar Ganda di Bubble Chat**:
  - Mengeliminasi scrollbar vertikal di dalam bubble WhatsApp dengan menyinkronkan kalkulasi lebar wrap teks dan menerapkan `ImGuiWindowFlags_NoScrollbar`.
- **Perapian Tata Letak Opsi Generator Gambar**:
  - Memperbaiki tumpang tindih (*overlap*) antara deretan dropdown opsi (`Framing`, `Rasio`, `Steps`, `HD Wajah`) dengan Status Bar bawah melalui penyesuaian tinggi area canvas chat dan lebar dropdown.
- **Pembersihan Banner llama-cli & Tag Berpikir Reasoning**:
  - Menyaring teks banner interaktif llama-cli `/glob <pattern>` dan merapikan tag reasoning `<think>` / `[Start thinking]` menjadi ikon collapsible elegan `💭 [Pemikiran Logika DeepSeek]` dan `💡 [Jawaban]`.

---

## [v1.4.1] - 2026-10-26
### ✨ Fitur Baru (Inisialisasi Otomatis Folder Model AI & Impor File di Android)
- **Auto-Initialization Folder Model AI di Penyimpanan HP (initAIStudioFolders)**:
  - Saat aplikasi Android pertama kali diinstal & dibuka (onCreate), aplikasi secara otomatis mendeteksi dan membuat folder Documents/AIStudio/ beserta seluruh subfolder model:
    - models/Teks_LLM/ (untuk model teks LLM quantisasi .gguf)
    - models/VAE/ (untuk VAE .safetensors atau TAESD fast VAE)
    - models/Gambar_Diffusion/ (untuk model checkpoint gambar)
    - models/LoRA/ (untuk file adapter LoRA)
    - models/ControlNet/ (untuk adapter pose/garis ControlNet)
    - models/Audio_Suara/ (untuk suara ONNX TTS Piper)
    - outputs/ (untuk hasil simpan gambar & teks yang digenerate)
  - Otomatis membuat file PETUNJUK_IMPORT_MODEL.txt di dalam folder HP agar pengguna tahu lokasi meletakkan file model secara manual via aplikasi file manager (ZArchiver, Files by Google, My Files).
- **Fitur File Picker Impor Model Android Langsung dari UI**:
  - Tombol bawaan antarmuka 📥 Impor Model Baru (.gguf / .safetensors) yang membuka pemilih file native Android.
  - Background worker thread yang otomatis menyalin file model dari penyimpanan ponsel dan mengarahkannya ke subfolder yang sesuai secara cerdas berdasarkan format nama file (.gguf ke Teks_LLM, keyword ae ke VAE, lora ke LoRA, dsb.).
  - Banner dinamis di UI Home dan Settings yang menampilkan lokasi path penyimpanan aktif di HP secara real-time.

---

## [v1.4.0] - 2026-10-26
### ✨ Fitur Baru (Ekspansi Mobile Android & CI/CD GitHub Actions)
- **Proyek Mobile Android Mandiri (ndroid/)**:
  - Membangun struktur proyek Android Gradle lengkap (com.mpesmpek.aistudio) berbasis Android SDK 34.
  - Mengintegrasikan antarmuka visual mobile responsif (berbasis desain modern Uizard) di ssets/index.html dengan navigasi tab: *Home*, *Chat LLM*, *Studio Foto*, dan *Pengaturan LAN*.
  - Native Activity MainActivity.java dengan hardware-accelerated WebView, permission internet, dan bridge JavaScript native AndroidNative untuk feedback getar (*haptic vibration*).
- **Otomasi Build Cloud GitHub Actions (ndroid/build-android-workflow.yml)**:
  - Konfigurasi alur kerja CI/CD cloud Ubuntu Runner dengan JDK 17, Android SDK, dan Gradle.
  - Otomatis mengompilasi release APK dan debug APK saat push ke GitHub, serta mengunggah file .apk siap pasang ke GitHub Releases dan Actions Artifacts.

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

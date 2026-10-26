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

## [v1.8.0] - 2026-10-26
### 💎 Perbaikan Font 100% Anti-Mojibake & Kanvas Studio Node Interaktif (Zoom In/Out + Panning Bebas)
- **Eliminasi Total Font Mojibake (100% Clean ASCII Modern Badge Typography)**:
  - Menyelesaikan akar masalah karakter aneh/rusak (`ðŸ§`, `âš¡`, `dYs?`, `dY"`, dll.) yang muncul akibat pemetaan Latin-1 bawaan Dear ImGui terhadap emoji Unicode UTF-8 multi-byte.
  - Menghapus seluruh karakter emoji mentah dari seluruh file sumber C++ (`gui_main.cpp`, `model_manager.cpp`, `hardware_detector.cpp`, `prompt_engine.cpp`) dan memvalidasi `0 non-ascii bytes` di seluruh codebase `cpp_core/src/`.
  - Mengganti teks dengan label badge beraksen modern yang bersih, konsisten, dan elegan: `[MODEL] Checkpoint Loader`, `[LoRA] Adaptor & Bobot`, `[PROMPT] Positif & Negatif`, `[CONTROL] ControlNet Canny`, `[SAMPLER] K-Sampler Engine`, `[VAE] Decoder Engine`, `[PREVIEW] Hasil Render Gambar`, `[>] Jalankan Alur Node`, `[MAX]`, `[OK]`, `[!]`, dll.
  - Mengkalibrasi ukuran baku Segoe UI dari 19.0f menjadi **17.0f** agar tata huruf di seluruh jendela aplikasi tampil tajam, rapi, dan proporsional.
- **Fitur Zoom In / Zoom Out Kanvas Alur Node Studio (ComfyUI Visual Pipeline)**:
  - **Skala Zoom Bawaan 85% (`0.85f`)**: Menyusutkan dimensi 3 kolom node secara proporsional sehingga seluruh 7 node muat sempurna di layar tanpa sesak, tanpa bertabrakan, dan tanpa kartu pratinjau yang terpotong.
  - **Rentang Zoom Luas**: Fleksibel mulai dari 50% (`0.50f`) untuk melihat keseluruhan peta alur kerja, hingga 160% (`1.60f`) untuk melihat teks prompt mikro.
  - **Zoom Roda Mouse (*Cursor-Centric Mouse Wheel*)**: Memusatkan pembesaran dan pengecilan tepat di titik kursor pengguna.
  - **Toolbar Zoom Responsif**:
    - `[-]`: Zoom Out bertahap per 10%.
    - `[ 85% ]`: Label persentase zoom real-time, klik untuk reset ke 100%.
    - `[+]`: Zoom In bertahap per 10%.
    - `[80% Pas]`: Tombol satu-klik untuk melihat seluruh alur node secara ringkas dan pas.
    - `[Reset Posisi]`: Mengembalikan seluruh 7 node ke tata letak koordinat grid semula.
- **Fitur Panning Kanvas Tak Terbatas (*Infinite Canvas Pan*)**:
  - Menggeser (*drag/pan*) seluruh kanvas ke segala arah menggunakan **Klik Tengah Mouse (*Middle Mouse Drag*)** atau **Klik Kanan Mouse (*Right Click Drag*)**.
  - Latar belakang grid titik/garis berskala dinamis mengikuti translasi pan dan faktor zoom (*infinite parallax grid*).
  - Skalabilitas presisi pada seluruh pin kabel kurva Bézier kubik menyala, header jendela node, dan teks internal (`SetWindowFontScale`) tanpa pergeseran koordinat (*zero drifting*).

---

## [v1.6.0] - 2026-10-26
### 💎 Penguncian Mutlak Scrollbar Sidebar Kiri & Estetika iOS Liquid Glassmorphism
- **Penguncian Mutlak Scrollbar Sidebar Kiri (`NoScrollWithMouse` & Zero-Drift Dynamic Sizing)**:
  - Mengeliminasi total bug di mana panel samping kiri masih dapat digulung ke atas/bawah menggunakan roda mouse. Kini diaktifkan flag proteksi ganda `ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse` pada container utama `SidebarPanel`, `BrandHeaderCard`, dan `SessionCard`.
  - Mengganti estimasi tinggi statis dengan kalkulasi dinamis presisi `ImGui::GetContentRegionAvail().y - 168.0f`.
  - Panel samping kiri kini **100% terkunci mati (Fixed Unscrollable Container)**. Hanya daftar percakapan di bagian tengah (`TopicsList`) yang dapat menggulung secara mandiri saat sesi bertambah banyak.
- **Implementasi Estetika Visual iOS Liquid Glassmorphism (VisionOS / Apple Dribbble Style)**:
  - **Pembiasan Cahaya Kaca (*Specular Top Rim Reflection*)**: Penambahan garis pantulan 1px di tepi atas kartu dan panel (*glass thickness highlight*).
  - **Kelengkungan Kapsul Halus (*Fluid Pill Rounding*)**: Peningkatan kurvatur sudut (`WindowRounding = 14px`, `ChildRounding = 12px`, `BubbleRounding = 14px`, `ButtonRounding = 10px`).
  - **Palet Kaca Tembus Pandang (*Frosted Glass Alpha*)**: Latar panel kaca tembus pandang (`rgba(17, 24, 41, 0.80)`) dengan border pembiasan cahaya specular di atas latar belakang deep cosmic navy (`#0A0D17`).
  - **Gelembung Obrolan Kristal**:
    - Balon pesan pengguna: Liquid Electric Azure Glass (`rgba(26, 107, 220, 0.88)` dengan border highlight cyan).
    - Balon pesan AI: Frosted Deep Sapphire Glass (`rgba(19, 28, 47, 0.80)` dengan border pembiasan lembut).
    - Balon pesan NSFW: Frosted Ruby Rose Glass (`rgba(61, 20, 41, 0.85)`).

---

## [v1.5.3] - 2026-10-26
### 📐 Optimasi Sizing Presisi 1:1 & Eliminasi Scrollbar Liar (Perfect Fit, Zero Screen-Shift)
- **Eliminasi Total Scrollbar Root Window (Zero Screen Jitter)**:
  - Mengunci `RootWindow` dengan `WindowPadding = (0, 0)` dan mengaktifkan `ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse`.
  - Meniadakan bug pergeseran layar global saat scroll mouse digunakan, memastikan seluruh antarmuka terkunci stabil dan kokoh pada posisinya.
- **Partisi Geometri Matematika Murni (Zero Margin Leakage)**:
  - Tinggi konten utama dihitung secara presisi: `main_content_h = app_height - 38px` dengan margin atas 6px, celah bawah 6px, dan Status Bar 26px duduk rata (*flush*) di dasar jendela.
  - Perhitungan tinggi canvas chat dan topics list disinkronkan tepat hingga piksel terakhir, mencegah luapan konten (*overflow*).
- **Auto-Scrollbar Cerdas pada ChatCanvas, TopicsList, & DashboardHub**:
  - Menghapus flag paksa `AlwaysVerticalScrollbar`. Batang gulir kini hanya muncul secara halus dan dinamis ketika pesan obrolan atau daftar topik benar-benar melebihi area pandang. Saat item sedikit, area tampak bersih dan estetik tanpa garis scrollbar abu-abu yang mengganggu.
- **Penyelarasan Tinggi Kontrol Input Bar**:
  - Menyamakan tinggi tombol `[Kirim]` (46px) dengan kotak input prompt teks multiline dan tombol `[✨ Prompt Pintar]` / `[🎙️ Bicara]` (46px), menciptakan deretan kontrol yang rata dan proporsional.

---

## [v1.5.2] - 2026-10-26
### 🎨 Peningkatan UI/UX (Aksesibilitas Tombol Pengaturan Tanpa Perlu Scroll)
- **Tombol Pintas ⚙️ Pengaturan di Top Navbar (Pojok Kanan Atas)**:
  - Tombol `[⚙️ Pengaturan]` kini disematkan langsung pada Top Navbar di samping tombol `[⛶ Layar Penuh]`. Pengguna dapat membuka menu pengaturan, memilih batas RAM, atau konfigurasi hardware dalam 1 klik dari mana saja tanpa perlu scroll.
  - Bersifat adaptif: Menampilkan teks `⚙️ Pengaturan` pada layar normal/lebar dan ikon ringkas `⚙️` pada resolusi sempit (< 780px).
- **Tombol Cepat Gear ⚙️ di Header Sidebar**:
  - Menyandingkan tombol `[+ Percakapan]` dengan tombol gear `[⚙️]` di bagian paling atas sidebar tepat di bawah logo brand, memberikan akses langsung ke pengaturan saat mengelola percakapan.
- **Pembersihan Scrollbar Liar & Penyelarasan Footer Sidebar (Zero Footer Clipping)**:
  - Menetapkan flag `ImGuiWindowFlags_NoScrollbar` pada container utama `SidebarPanel`, memastikan panel samping tidak pernah memunculkan scrollbar ganda.
  - Memadatkan kartu telemetri hardware dan RAM badge sehingga tombol `[⚙️ Pengaturan]` dan `[📁 Output]` di bagian bawah sidebar selalu terpampang utuh 100% dan tidak pernah terdorong keluar layar.

---

## [v1.5.1] - 2026-10-26
### ⚡ Fitur Baru (Menu Batas RAM & Mode Los RAM / Instant Purge)
- **Menu Pilihan Profil Alokasi RAM (Batas RAM vs Los RAM)**:
  - Penambahan pengaturan kuota RAM fisik Windows (`SetProcessWorkingSetSize`):
    - **🚀 Mode Los RAM (Unlimited / Bebas)**: Membebaskan pembatasan sistem untuk performa AI dan kecepatan komputasi CPU/GPU maksimal tanpa hambatan alokasi.
    - **🛡️ Batas 4 GB (Hemat - Laptop 8 GB)**: Membatasi working set memori hingga maksimal 4 GB agar laptop dengan RAM pas-pasan tetap lancar dan tidak memicu disk swapping saat membuka browser/tugas lain.
    - **⚖️ Batas 6 GB (Seimbang - Standar)**: Batasan seimbang untuk penggunaan harian dan multitasking standar.
    - **⚡ Batas 8 GB (Maksimal Multitasking)**: Memberikan keleluasaan working set tinggi untuk laptop/PC dengan RAM 16 GB+.
- **Penyesuaian Dinamis Ukuran Konteks LLM (Llama Engine Context)**:
  - Ukuran jendela konteks percakapan LLM disinkronkan secara otomatis sesuai mode RAM yang aktif: 2048 token (Batas 4 GB), 3072 token (Batas 6 GB), dan 4096 token (Batas 8 GB & Mode Los RAM).
- **Tombol Pintas "🧹 Los RAM Sekarang" di Sidebar & Pengaturan**:
  - Disediakan tombol aksi cepat di Sidebar Kiri (di bawah status hardware) dan di Modal Pengaturan.
  - Memanggil `MemoryManager::Get().PurgeAllAsync()` di latar belakang untuk seketika membuang cache Direct3D 11 VRAM dan memanggil Windows API `EmptyWorkingSet(GetCurrentProcess())`, memangkas penggunaan RAM yang membengkak secara instan.
- **Auto-Los RAM Setelah Selesai Render**:
  - Opsi otomatisasi pembersihan RAM pasca-render foto atau respon teks obrolan, mengembalikan memori proses ke kondisi efisien seketika pekerjaan komputasi AI selesai.
- **Indikator Lencana Mode RAM di Sidebar**:
  - Menampilkan lencana status mode aktif (`⚡ Mode: Los RAM` / `🛡️ Batas 4 GB` / dll.) secara langsung pada kartu status perangkat keras sidebar.

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

## [v1.5.0] - 2026-10-26
### ✨ Fitur Baru (Studio Node Alur Visual ComfyUI Native C++, Dynamic Multi-Model Scanning & AI Real Covers)
- **Mode Khusus Studio Node (ComfyUI-Style Visual Pipeline Pure C++ Native)**:
  - Menu tab baru `"Studio Node"` di Top Navbar dan Kartu ke-5 di Dashboard Hub.
  - Alur visual pipa (Node Graph) dengan 7 Node Interaktif:
    1. **🧠 Model Checkpoint Loader**: Pindai dan pilih Checkpoint difusi format `.safetensors` atau `.gguf` (CyberRealistic V5, Realistic Vision V5.1, DreamShaper LCM) lengkap dengan status arsitektur & kapasitas memori.
    2. **🧩 LoRA Loader & Bobot**: Pindai dinamis direktori `models/LoRA/`, pilih adaptor LoRA (`add_detail.safetensors`, `lcm_lora_sd15.safetensors`, atau LoRA kustom), serta slider bobot (*strength*) dari 0.10 hingga 1.50.
    3. **📝 Prompt Conditioning Engine**: Editor prompt positif bertingkat multiline, input negative prompt anti-cacat anatomi, quick style injection chips (`[+ 8K DSLR]`, `[+ Pori Kulit]`, `[+ Golden Hour]`), serta tombol pembersih prompt.
    4. **🎯 ControlNet Canny (Pose Lock)**: Pindai dinamis direktori `models/ControlNet/`, pemilihan foto acuan pose manusia via file picker native, serta slider kekuatan kendali (*control strength*) dari 0.10 hingga 1.00.
    5. **🎛️ K-Sampler Engine**: Pemilihan algoritma sampling (*Euler Ancestral*, *DPM++ 2M Karras*, *LCM Fast 4-Step*, *Euler Standar*), denoising steps (4 hingga 50 langkah), CFG Scale kepatuhan prompt (1.0 hingga 15.0), seed acak/terkunci, serta rasio resolusi dinamis (512x512, 512x768, 768x512, 1024x1024).
    6. **⚡ VAE Decoder Engine**: Pindai dinamis direktori `models/VAE/`, pilihan Fast VAE TAESD (~2.7 detik CPU) atau MSE High Quality VAE (~25 detik CPU) atau VAE bawaan checkpoint.
    7. **🖼️ Image Output Preview & Export**: Tampilan hasil render gambar resolusi tinggi langsung di dalam node, indikator live progress dan estimasi waktu, tombol perbesar layar penuh (*fullscreen viewer*), buka berkas, dan buka folder `outputs/`.
  - **Kabel Bézier Menyala (Glowing Bézier Cables)**: Render dinamis kabel kurva kubik Bézier (`DrawGlowingBezier`) menghubungkan pin input/output dengan aksen warna neon per kategori (Ungu untuk Model, Amber untuk LoRA, Biru Langit untuk Prompt, Rose untuk ControlNet, Zamrud untuk Latent, dan Cyan untuk Gambar Akhir).
  - **Draggable Node Header**: Setiap kartu node dapat digeser dan ditata bebas oleh pengguna menggunakan kursor mouse pada canvas grid, dengan tombol `[🔄 Reset Posisi]` untuk mengembalikan tata letak standar dalam satu klik.
  - **Preset Alur Cepat**:
    - `📸 DSLR 8K Potret` (CyberRealistic + add_detail + MSE VAE)
    - `⚡ Ultra Fast 4-Step` (DreamShaper LCM + TAESD ~3.5 detik)
    - `🎯 ControlNet Canny Pose-Lock` (CyberRealistic + Canny Edge)
    - `🔮 Kustom Mandiri` (Bebas rangkai node)
- **Scanning Dinamis Seluruh Tipe Model AI**:
  - `models/LoRA/*.safetensors` & `*.gguf`: Otomatis terdeteksi dan masuk ke dropdown LoRA.
  - `models/VAE/*.safetensors`: Otomatis terdeteksi dan masuk ke dropdown VAE.
  - `models/ControlNet/*.safetensors`: Otomatis terdeteksi dan masuk ke dropdown ControlNet.
  - Terintegrasi langsung dengan mesin eksekusi `sd-cli.exe` C++ native tanpa dependensi Python runtime.
- **Cover Visual Gemini AI Resmi untuk Katalog Mode**:
  - 4 file gambar cover fotorealistis & holografis beresolusi tinggi otomatis termuat saat aplikasi dijalankan:
    - `assets/covers/cover_chat_llm.jpg` (Holographic Neural Interface - Chat LLM & Koding)
    - `assets/covers/cover_photo_studio.jpg` (DSLR 85mm Golden Hour Portrait - Studio Foto)
    - `assets/covers/cover_nsfw_studio.jpg` (Glamour Sensual Model Portrait - Dewasa 18+ Uncensored)
    - `assets/covers/cover_studio_node.jpg` (Cyber Glass Nodes & Neon Cables - Studio Node)

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

# ⚡ AI Studio — 100% Pure C++ Native Workstation

![Platform](https://img.shields.io/badge/Platform-Windows%2010%20%2F%2011-blue)
![Language](https://img.shields.io/badge/Language-C%2B%2B17%20Native-00599C)
![DirectX](https://img.shields.io/badge/Renderer-Direct3D%2011-purple)
![AVX2](https://img.shields.io/badge/Acceleration-AVX2%20%2B%20FMA%20%2B%20OpenMP-orange)
![License](https://img.shields.io/badge/License-MIT-green)

Aplikasi studio AI desktop generasi baru yang dibangun **100% murni menggunakan C++ native dan Direct3D 11**. Menggabungkan **Large Language Model (LLM)** dan **Diffusion Photo Generator** dalam satu biner mandiri berukuran $\approx 1.4\text{ MB}$ tanpa runtime Python, PyTorch, maupun virtual environment.

---

## 🌟 Fitur Utama

- **Zero Python Dependency**: Berjalan langsung tanpa instalasi Python, PyTorch, atau Conda.
- **LLM Reasoning & Coding**: Mendukung arsitektur GGUF (Qwen2.5-Coder, DeepSeek-R1, Llama 3) dengan multi-turn chat memory.
- **Fotorealistis & Uncensored Diffusion**: Checkpoint SD 1.5, SDXL, dan CyberRealistic V5 dengan detail tekstur kulit dan mata alami.
- **Fast VAE Acceleration (TAESD)**: Memangkas waktu decode VAE di CPU dari **61 detik** menjadi **2.7 detik** ($22\times$ lebih cepat).
- **AI Smart Prompt Expander**: Otomatis mengubah ide singkat menjadi prompt fotografi master dengan lensa 85mm f/1.4, pencahayaan sinematik, dan framing.
- **Anti-Face-Blur & Anatomy Fix**: Injeksi bobot penajam wajah ($1.25\times$) dan rasio vertikal khusus untuk foto *Full Body*.
- **Image-to-Image (Img2Img)**: Drag & drop foto referensi dengan slider kekuatan transformasi visual.
- **Auto-Deteksi Hardware & RTX**: Otomatis mendeteksi prosesor CPU AVX2 dan GPU NVIDIA RTX (CUDA Tensor Cores) secara dinamis via DXGI.
- **GGUF Binary Inspector**: Drag & drop file `.gguf` / `.safetensors`, sistem otomatis membaca arsitektur dan mengarahkan ke folder yang sesuai.

---

## 📁 Struktur Direktori

```text
├── cpp_core/                  # Kode sumber C++ & antarmuka Direct3D 11
│   ├── src/gui_main.cpp       # Aplikasi utama & render loop ImGui
│   ├── src/engine_core.cpp    # Vector math SIMD AVX2 & telemetri
│   ├── include/               # Header C++ (engine_core.h, stb_image.h)
│   └── imgui/                 # Backend Dear ImGui DirectX 11
├── llama_bin/                 # Runner llama-cli C++ native
├── sd_bin/                    # Runner sd-cli C++ native
├── models/                    # Folder penyimpanan model AI (.gguf / .safetensors)
│   ├── Teks_LLM/              # Model LLM percakapan & penalaran
│   ├── Gambar_Diffusion/      # Model difusi foto & checkpoint
│   ├── VAE/                   # Fast VAE TAESD (taesd.safetensors)
│   └── LoRA/                  # LoRA adaptasi gaya & turbo LCM
├── compile_step_by_step.bat   # Skrip kompilasi mandiri
└── AI-Studio.exe              # File biner aplikasi hasil kompilasi
```

---

## 🛠️ Cara Kompilasi dari Source Code

Pastikan Anda memiliki toolkit compiler MinGW GCC (w64devkit). Cukup jalankan:

```bat
compile_step_by_step.bat
```

Skrip akan mengompilasi seluruh modul C++ dengan optimasi hardware `-O2 -mavx2 -mfma -fopenmp` dan menghasilkan file tunggal **`AI-Studio.exe`**.

---

## 📥 Panduan Penempatan Model AI

Unduh model format `.gguf` atau `.safetensors` dari Hugging Face, lalu letakkan di folder berikut:

1. **Model Percakapan (LLM)**: Letakkan file `.gguf` di `models\Teks_LLM\`.
   - *Rekomendasi*: `qwen2.5-coder-1.5b-instruct-q4_k_m.gguf`
2. **Model Foto (Diffusion)**: Letakkan file `.safetensors` / `.gguf` di `models\Gambar_Diffusion\`.
   - *Rekomendasi*: `CyberRealistic_V5_FP16.safetensors`
3. **Fast VAE TAESD**: Letakkan `taesd.safetensors` di `models\VAE\`.

---

## 💻 Kebutuhan Sistem

- **Sistem Operasi**: Windows 10 atau Windows 11 (64-bit).
- **Prosesor (CPU)**: Intel Core i3/i5/i7/i9 atau AMD Ryzen dengan dukungan instruksi **AVX2** (prosesor buatan tahun 2014 ke atas).
- **RAM**: Minimal 8 GB (Direkomendasikan 16 GB).
- **GPU (Opsional)**: Kartu grafis NVIDIA GeForce GTX/RTX untuk akselerasi instan CUDA Tensor Cores (otomatis terdeteksi).

# 📦 Panduan Direktori Model AI — AI Studio Native

Folder ini adalah tempat meletakkan seluruh file bobot model AI yang dijalankan secara offline oleh **AI Studio Native**. Semua model telah dikelompokkan ke dalam subdirektori masing-masing agar rapi, terisolasi, dan mudah dikelola:

---

## 🗺️ Peta Lokasi Subdirektori Model

| Subdirektori | Format File | Fungsi Utama | Contoh File yang Cocok |
|---|---|---|---|
| 🧠 **[`models/Teks_LLM/`](Teks_LLM/)** | `.gguf` | Otak percakapan teks, penalaran logika, koding, dan expander prompt otomatis. | `DeepSeek-R1-Distill-Qwen-1.5B-Q4_K_M.gguf`, `qwen2.5-coder-7b-instruct-q3_k_m.gguf` |
| 🎨 **[`models/VAE/`](VAE/)** | `.safetensors` | Koreksi kontras warna kulit alami dan akselerasi decode CPU kilat. | `taesd.safetensors` (Fast VAE ~2.7 detik), `vae-ft-mse-840000-ema-pruned.safetensors` |
| 📸 **[`models/Gambar_Diffusion/`](Gambar_Diffusion/)** | `.safetensors`, `.gguf` | Checkpoint utama generasi foto realistis, potret manusia, dan seni visual. | `CyberRealistic_V5_FP16.safetensors`, `Realistic_Vision_V5.1.safetensors`, `DreamShaper8_LCM.gguf` |
| 🔬 **[`models/LoRA/`](LoRA/)** | `.safetensors` | Modul detailer pori-pori mikro dan akselerator sampling langkah kilat. | `add_detail.safetensors` (Pori kulit & mata), `lcm_lora_sd15.safetensors` (Turbo 4-step) |
| 📐 **[`models/ControlNet/`](ControlNet/)** | `.safetensors` | Ekstraksi kontur garis (Canny) untuk mengunci pose dan gestur foto referensi 100% presisi. | `control_sd15_canny.safetensors` |
| 🎙️ **[`models/Audio_Suara/`](Audio_Suara/)** | `.onnx`, `.json` | Model suara sintetis manusia (TTS) offline bahasa Indonesia. | `id_ID-news_tts-medium.onnx` (Piper TTS) |

---

## 💡 Cara Menambahkan Model Baru
1. Unduh model berformat `.gguf` atau `.safetensors` dari HuggingFace atau Civitai.
2. Masukkan file ke dalam subfolder yang sesuai di atas.
3. Buka **AI Studio** ➔ Model akan otomatis terdeteksi dan langsung muncul di menu pilihan model tanpa perlu restart aplikasi!

---
title: "Katalog Lengkap Semua Model AI"
tags: [katalog, models, diffusion, llm, lora, vae]
aliases: ["Daftar Model AI", "Model Registry"]
---

# 🤖 Katalog Lengkap Model AI Studio Native

Semua model difusi disimpan di `models/Gambar_Diffusion/` dan model LLM di `models/Teks_LLM/`.

```mermaid
graph TD
    Models["Folder models/"] --> Diff["Gambar_Diffusion/ (SD & DiT)"]
    Models --> LLM["Teks_LLM/ (Chat & Kode)"]
    Models --> LoRA["LoRA/ (Adaptor Gaya)"]
    Models --> VAE["VAE/ (Decoder Cepat)"]

    Diff --> CM["[[ChilloutMix_Ni_Spesialis_Asia|ChilloutMix Ni (2.03 GB)]]"]
    Diff --> GM["[[GhostMix_V2_Anime_Realism_NSFW|GhostMix V2.0 (2.29 GB)]]"]
    Diff --> CR["[[CyberRealistic_V5_DSLR_Murni|CyberRealistic V5.0 (2.03 GB)]]"]
    Diff --> DS["[[DreamShaper8_LCM_Turbo|DreamShaper 8 LCM (1.55 GB)]]"]
    Diff --> RV["[[Realistic_Vision_V51|Realistic Vision V5.1 (2.03 GB)]]"]

    LLM --> Qwen["[[Qwen_Dan_Sakura_LLM|Qwen 2.5 Coder 1.5B]]"]
    LLM --> Sakura["[[Qwen_Dan_Sakura_LLM|Sakura Iris 3B DiT]]"]
```

## 📋 Ringkasan Cepat Model:
| Nama Model | Format | Ukuran | Spesialisasi Utama |
| :--- | :---: | :---: | :--- |
| **[[ChilloutMix_Ni_Spesialis_Asia|ChilloutMix Ni]]** | `.safetensors` | 2.03 GB | Potret wanita Asia & foto intim NSFW fotorealistis |
| **[[GhostMix_V2_Anime_Realism_NSFW|GhostMix V2.0]]** | `.safetensors` | 2.29 GB | Anime 2.5D realism uncensored + Baked VAE |
| **[[CyberRealistic_V5_DSLR_Murni|CyberRealistic V5]]** | `.safetensors` | 2.03 GB | Foto kamera DSLR, lensa 85mm, pori kulit asli |
| **[[DreamShaper8_LCM_Turbo|DreamShaper 8 LCM]]** | `.gguf` | 1.55 GB | Generasi turbo 4-step kilat (~3-5s di CPU) |
| **[[Realistic_Vision_V51|Realistic Vision V5.1]]** | `.safetensors` | 2.03 GB | Pencahayaan studio portret dramatis |

Tautan Terkait:
- [[Node_Model_Checkpoint_Loader|Cara Memilih Model di Studio Node]]
- [[00_DASHBOARD_OTAK_UTAMA|Kembali ke Dashboard Otak]]

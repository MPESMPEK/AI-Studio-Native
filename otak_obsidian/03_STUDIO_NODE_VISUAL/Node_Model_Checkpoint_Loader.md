---
title: "Node Model Checkpoint Loader"
tags: [node, model-loader, checkpoint, rekomendasi]
aliases: ["Node Model", "Checkpoint Node"]
---

# 📦 Node Model: Checkpoint Loader & Tombol Rekomendasi

Node ini bertugas memuat checkpoint difusi (`.safetensors` atau `.gguf`) ke dalam alur kerja visual.

## ⭐ Fitur Unggulan: Tombol Rekomendasi Model Ini
Tepat di bawah dropdown checkpoint, terdapat tombol:
`[*] Terapkan Rekomendasi Model Ini`

Saat diklik, sistem secara cerdas menyinkronkan seluruh parameter:
- **ChilloutMix**: Menyetel Sampler `DPM++ 2M Karras`, Steps `22`, CFG `6.0`, Resolusi `384x512`.
- **GhostMix V2.0**: Menyetel Sampler `DPM++ 2M Karras`, Steps `24`, CFG `6.5`, Resolusi `384x512`.
- **CyberRealistic V5**: Menyetel Sampler `Euler Ancestral`, Steps `24`, CFG `5.5`, Resolusi `384x512`.
- **DreamShaper 8 LCM**: Menyetel Sampler `LCM Fast`, Steps `5`, CFG `1.8`, Resolusi `384x384`.

Tautan Terkait:
- [[Node_Sampler_Dan_Resolusi_250p|Node Sampler Engine]]
- [[Katalog_Semua_Model|Peta Model AI]]
- [[00_DASHBOARD_OTAK_UTAMA|Kembali ke Dashboard Otak]]

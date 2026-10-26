---
title: "Node Sampler dan Resolusi 250p"
tags: [node, sampler, resolusi, 250p, 380p, karras]
aliases: ["Node Sampler", "Resolusi Rendah"]
---

# ⏱️ Node Sampler: Multi-Resolusi 250p/380p & K-Sampling

Node Sampler mengontrol proses denoising laten di CPU laptop:

## 📐 Pilihan 9 Rasio Resolusi (Bebas Pilih):
- **250p Kotak (1:1)**: `256 x 256` (~4x lebih cepat)
- **250p Potret**: `256 x 384` (~3.5x lebih cepat)
- **250p Lanskap**: `384 x 256` (~3.5x lebih cepat)
- **380p Kotak (1:1)**: `384 x 384` (~2x lebih cepat)
- **380p Potret**: `384 x 512` (~1.8x lebih cepat - **Sangat Direkomendasikan**)
- **380p Lanskap**: `512 x 384` (~1.8x lebih cepat)
- **512p Standar**: `512 x 512` (1:1 SD1.5)
- **512p Potret**: `512 x 768`
- **512p Lanskap**: `768 x 512`

Tautan Terkait:
- [[Panduan_Resolusi_Rendah_CPU|Panduan Resolusi Rendah di CPU]]
- [[00_DASHBOARD_OTAK_UTAMA|Kembali ke Dashboard Otak]]

---
title: "Kabel Bezier dan Routing Data"
tags: [node, bezier, routing, math, canvas]
aliases: ["Kabel Bezier", "Kabel Node"]
---

# ➰ Matematika Kabel Bezier Kubik & Canvas Smooth Panning

Kabel penghubung antar node digambar menggunakan kurva Bezier kubik Direct3D 11:
- Titik awal `P0` (Output Pin node sumber)
- Titik kontrol `P1` (`P0.x + offset`, `P0.y`)
- Titik kontrol `P2` (`P3.x - offset`, `P3.y`)
- Titik akhir `P3` (Input Pin node target)

Warna kabel bersinar (*glow*) sesuai tipe data:
- **Ungu**: MODEL
- **Biru Muda**: CONDITIONING (Prompt)
- **Hijau Toska**: LATENT
- **Cyan**: IMAGE
- **Oranye**: LoRA Adaptor

Tautan Terkait:
- [[Konsep_Visual_Pipeline_ComfyUI|Konsep Studio Node]]
- [[00_DASHBOARD_OTAK_UTAMA|Kembali ke Dashboard Otak]]

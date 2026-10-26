---
title: "Panduan Resolusi Rendah di CPU"
tags: [performance, cpu, 250p, 380p, low-res]
aliases: ["Generasi Cepat CPU", "Low-Res Guide"]
---

# 🚀 Panduan Generasi Cepat Resolusi Rendah di CPU

Bagi pengguna laptop tanpa GPU diskrit, menghasilkan gambar di 512p bisa memakan waktu beberapa menit. Solusinya adalah memanfaatkan resolusi adaptif:

| Kebutuhan | Resolusi | Perkiraan Waktu di CPU | Hasil Visual |
| :--- | :---: | :---: | :--- |
| **Draft Kilat** | `256 x 256` | ~15 - 25 detik | Sketsa komposisi kasar |
| **Portrait Cepat** | `256 x 384` | ~25 - 40 detik | Proporsi tubuh terlihat jelas |
| **Sweet Spot Terbaik** | `384 x 512` | ~50 - 80 detik | **Wajah sangat tajam, pori kulit terlihat** |
| **Standar SD1.5** | `512 x 512` | ~90 - 140 detik | Detail standar |

Tautan Terkait:
- [[Node_Sampler_Dan_Resolusi_250p|Node Sampler Resolusi]]
- [[00_DASHBOARD_OTAK_UTAMA|Kembali ke Dashboard Otak]]

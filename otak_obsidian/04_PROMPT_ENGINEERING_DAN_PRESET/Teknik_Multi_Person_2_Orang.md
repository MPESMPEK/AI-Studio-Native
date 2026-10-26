---
title: "Teknik Generasi 2 Orang / Couple"
tags: [prompt, multi-person, 2-people, couple, besties]
aliases: ["Multi Person Prompt", "2 Orang Tanpa Menyatu"]
---

# 👭 Teknik Generasi 2 Orang / Couple Tanpa Wajah Menyatu

## ❓ Mengapa di SD 1.5 Dua Orang Sering Melebur?
Stable Diffusion 1.5 menggunakan self-attention global. Pada resolusi sempit seperti `256x256`, ruang laten tidak cukup untuk membedakan dua kepala, sehingga AI mencampur fitur wajah mereka menjadi 1 orang aneh dengan anggota tubuh berlebih.

## 🔑 3 Kunci Keberhasilan 2 Orang:
1. **Resolusi Minimal**: Gunakan **`384 x 512`** (Portrait) atau **`512 x 384`** (Landscape).
2. **Pemisahan Spasial Jelas (Kiri vs Kanan)**:
   > `two distinct people, 1man standing on the left side with short black hair, 1woman on the right side with long brown hair hugging, smiling, eye contact, depth of field`
3. **Negative Prompt Fused Body**:
   > Masukkan: `fused bodies, extra limbs, extra heads, combined faces, bad anatomy, deformed`

Tautan Terkait:
- [[Node_Prompt_Dan_Saran_Interaktif|Saran 2 Orang di Studio Node]]
- [[00_DASHBOARD_OTAK_UTAMA|Kembali ke Dashboard Otak]]

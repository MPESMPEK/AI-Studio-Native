# 📐 Folder ControlNet (Kunci Pose & Komposisi)

> **Lokasi:** `models/ControlNet/`  
> **Format File yang Didukung:** `.safetensors`

## Fungsi:
Folder ini adalah tempat bobot ControlNet yang bertugas mengekstrak garis tepi (*Canny edge detection*) dari foto acuan/referensi yang diunggah pengguna, sehingga pose tubuh, gestur tangan, dan sudut kamera terkunci 100% presisi.

## Contoh Model yang Direkomendasikan:
1. **`control_sd15_canny.safetensors`** (~722 MB):
   - Model Canny FP16 standar untuk penguncian pose manusia dan bentuk objek presisi tinggi.

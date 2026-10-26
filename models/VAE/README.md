# 🎨 Folder VAE (Variational Autoencoder)

> **Lokasi:** `models/VAE/`  
> **Format File yang Didukung:** `.safetensors`

## Fungsi:
Folder ini adalah tempat untuk meletakkan model VAE yang bertugas menerjemahkan ruang laten (*latent space*) menjadi gambar foto tajam, mengoreksi kontras warna kulit alami, serta mempercepat proses decoding akhir.

## Contoh Model yang Direkomendasikan:
1. **`taesd.safetensors`** (~9.8 MB) — **SANGAT DIREKOMENDASIKAN UNTUK CPU LAPTOP**:
   - Tiny AutoEncoder for Stable Diffusion.
   - **Memangkas waktu decode gambar di CPU dari 61 detik menjadi ~2.7 detik** tanpa penurunan kualitas kasat mata.
2. **`vae-ft-mse-840000-ema-pruned.safetensors`** (~334 MB):
   - VAE resmi Stability AI untuk rentang dinamis warna kulit manusia fotorealistis sekelas kamera DSLR.

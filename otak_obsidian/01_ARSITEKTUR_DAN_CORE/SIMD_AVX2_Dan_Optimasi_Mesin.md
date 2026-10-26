---
title: "SIMD AVX2 dan Optimasi Mesin"
tags: [simd, avx2, fma, optimasi, performance]
aliases: ["Vektorisasi CPU", "AVX2 Performance"]
---

# ⚡ Vektorisasi SIMD AVX2 + FMA & Zero-Python

Aplikasi dikompilasi menggunakan compiler GCC 64-bit MinGW (`w64devkit`) dengan instruksi perangkat keras tingkat lanjut:
- Flag: `-O3 -mavx2 -mfma -fopenmp -DSTATIC_ENGINE_CORE`

## 📊 Keunggulan Vektorisasi CPU:
1. **AVX2 (Advanced Vector Extensions 2)**: Memproses register 256-bit secara paralel (8 float single-precision per siklus CPU).
2. **FMA (Fused Multiply-Add)**: Melakukan perkalian dan penjumlahan matriks bobot neural network dalam 1 instruksi mesin tunggal, mengurangi error pembulatan dan mempercepat inferensi.
3. **OpenMP Multithreading**: Membagi beban kerja ke seluruh core logis prosesor Intel Core i5 pengguna secara seimbang.

> [!important] Dampak di Laptop Intel Core i5
> Dengan optimasi ini, komputasi difusi 250p/380p dapat diselesaikan dalam hitungan detik tanpa membutuhkan GPU diskrit bernilai mahal.

Tautan Terkait:
- [[Arsitektur_Pure_CPP_Direct3D11|Arsitektur Pure C++]]
- [[Panduan_Resolusi_Rendah_CPU|Panduan Resolusi Rendah di CPU]]
- [[00_DASHBOARD_OTAK_UTAMA|Kembali ke Dashboard Otak]]

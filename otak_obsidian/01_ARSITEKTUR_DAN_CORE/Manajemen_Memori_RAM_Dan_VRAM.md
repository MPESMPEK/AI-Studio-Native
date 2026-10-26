---
title: "Manajemen Memori RAM dan VRAM"
tags: [memori, ram, vram, direct3d11, directx]
aliases: ["RAM Management", "VRAM Purge"]
---

# 💾 Manajemen RAM Laptop & Auto Purge VRAM

Untuk memastikan laptop pengguna tidak kehabisan memori (*Out Of Memory* / OOM), sistem memiliki kontrol alokasi cerdas:

## 🎚️ Mode Batas RAM Pengguna:
- **Mode Los RAM (Unlimited)**: Membebaskan model menggunakan RAM sistem penuh untuk performa maksimal.
- **Batas 4 GB / 6 GB / 8 GB**: Membatasi ukuran konteks LLM dan ukuran tensor difusi agar laptop tetap dingin dan multitasking lancar.

## 🧹 Pencegahan Kebocoran Memori (Resource Safety):
- **Tekstur Direct3D 11**: Setiap `ID3D11ShaderResourceView` yang dimuat ke GPU memiliki siklus rilis pasti (`->Release()`) saat gambar diganti atau sesi dihapus.
- **Auto Purge Cache**: Memanggil `MemoryManager::Get().PurgeAllAsync()` setelah inferensi selesai untuk mengembalikan halaman memori (*working set*) ke OS Windows.

Tautan Terkait:
- [[Audit_Celah_Keamanan_Total|Audit Keamanan & Integritas Memori]]
- [[00_DASHBOARD_OTAK_UTAMA|Kembali ke Dashboard Otak]]

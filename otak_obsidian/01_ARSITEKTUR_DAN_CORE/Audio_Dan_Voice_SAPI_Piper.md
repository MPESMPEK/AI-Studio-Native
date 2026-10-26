---
title: "Audio dan Voice TTS Offline"
tags: [audio, voice, tts, sapi, piper]
aliases: ["Sistem Suara AI", "TTS Engine"]
---

# 🎙️ Sistem Suara AI Offline (Windows SAPI & Piper TTS)

AI Studio Native mendukung output suara alami tanpa memerlukan koneksi internet:

1. **Windows SAPI Native (Offline)**: Mengakses suara sistem Microsoft Windows secara langsung dengan latensi 0 ms.
2. **Piper Neural TTS (ONNX)**: Model suara neural offline `id_ID-news_tts-medium.onnx` untuk intonasi bahasa Indonesia yang luwes dan natural.
3. **MCI Player Win32**: Pemutar audio native melalui `mciSendStringA`, menghindari dependensi library eksternal yang berat.

Tautan Terkait:
- [[Arsitektur_Pure_CPP_Direct3D11|Arsitektur Pure C++]]
- [[00_DASHBOARD_OTAK_UTAMA|Kembali ke Dashboard Otak]]

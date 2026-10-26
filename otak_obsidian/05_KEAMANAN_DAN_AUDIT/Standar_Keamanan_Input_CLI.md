---
title: "Standar Keamanan Input Argumen CLI"
tags: [keamanan, cli, escaping, best-practices]
aliases: ["Standar Keamanan CLI", "Escape Rules"]
---

# 🔒 Standar Keamanan Input Argumen CLI & Escape Rule

## Aturan Standar Eksekusi Windows di AI Studio Native:
1. **Hindari `system()` dan `_popen()`** jika tidak ada kebutuhan shell piping. Utamakan `CreateProcessA` atau `ShellExecuteA`.
2. **Aturan Quote dan Backslash (MSVCRT Argv Parsing)**:
   - Setiap tanda kutip ganda `"` wajib diescape menjadi `"`.
   - Di akhir kalimat berpagar kutip, pastikan tidak ada tanda backslash ganjil `\` agar penutup kutip tidak diescape menjadi literal quote.
3. **LLM File-Based Passing**:
   - Seluruh prompt panjang untuk `llama-cli` ditulis ke berkas perantara `llama_prompt.txt` dan dibaca menggunakan argumen `-f`, menjamin 100% imun terhadap batasan panjang CLI Windows (8191 byte) dan injeksi argumen.

Tautan Terkait:
- [[Audit_Celah_Keamanan_Total|Audit Keamanan Total]]
- [[00_DASHBOARD_OTAK_UTAMA|Kembali ke Dashboard Otak]]

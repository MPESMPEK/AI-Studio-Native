# 📦 Direktori Model AI - AI Studio Offline Workstation

Folder ini menyimpan seluruh model AI yang digunakan oleh sistem secara offline. Semua model telah dikelompokkan berdasarkan fungsinya agar rapi, mudah ditemukan, dan tidak tercampur:

```
models/
├── Teks_LLM/              --> Model Percakapan Teks & Pemrograman (GGUF)
│   ├── DeepSeek-R1-Distill-Qwen-1.5B-Q4_K_M.gguf  (Rantai Pikir / Reasoning Cerdas)
│   ├── qwen2.5-coder-1.5b-instruct-q4_k_m.gguf    (Koding Cepat & Ringan)
│   ├── qwen2.5-coder-7b-instruct-q3_k_m.gguf      (Koding Mahir & Arsitektur)
│   └── Qwen3.8-2B-Uncensored-Q4_K_M.gguf          (Chat Santai Bebas Filter)
│
├── Gambar_Diffusion/      --> Model Generator Gambar (GGUF & Safetensors)
│   ├── Realistic_Vision_V5.1_fp16-no-ema.safetensors (Foto Ultra-Realistis Manusia/Objek)
│   ├── DreamShaper8_LCM_q4_0.gguf                    (Render Kilat 4 Steps)
│   ├── Sakura-Iris-3B-Q4_K_M-3.14GiB.gguf           (Anime / Ilustrasi Estetik)
│   └── qwen-image-2.1-UC-Q6_K.gguf                  (Difusi Presisi Tinggi)
│
├── Gambar_OpenVINO/       --> Pipeline Intel GPU / OpenVINO (Akselerasi Grafis Intel UHD)
│   └── (Unet, VAE, Tokenizer, Text Encoder terkompilasi OpenVINO IR)
│
├── Multimodal_Vision/     --> Text Encoders & Vision Language Models (VLMs)
│   ├── Janus-Pro-1B.Q2_K.gguf
│   ├── mmproj-Qwen-Image-2.1-Turbo-Uncensored-F16.gguf
│   └── Sakura-TextEncoder-Qwen3VL-4B-Uncensored-Q4_K_M-2.33GiB.gguf
│
├── LoRA/                  --> Bobot Tambahan Gaya / Akselerator Foto (Safetensors)
│   ├── lcm_lora_sd15.safetensors                     (Turbo Speed Booster 4-8 steps)
│   └── qwen21_multiple_angles_v2.safetensors         (Kamera Multi-Sudut Pandang)
│
├── ControlNet/            --> Model Pengontrol Pose / Komposisi
│   └── (Tempat bobot ControlNet OpenPose, Canny, Depth)
│
├── VAE/                   --> Variational Autoencoder (Pewarnaan & Dekoder Gambar)
│   └── qwen_image_2.1_vae_bf16.safetensors
│
└── Audio_Suara/           --> Model Suara Suara Manusia (TTS Offline)
    ├── id_ID-news_tts-medium.onnx
    └── id_ID-news_tts-medium.onnx.json
```

---
*Catatan: Semua ekspor hasil pembuatan AI tidak disimpan di sini, melainkan otomatis masuk ke folder `outputs/` sesuai jenisnya.*

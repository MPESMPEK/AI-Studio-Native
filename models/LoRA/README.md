# 🔬 Folder LoRA (Low-Rank Adaptation)

> **Lokasi:** `models/LoRA/`  
> **Format File yang Didukung:** `.safetensors`

## Fungsi:
Folder ini adalah tempat bobot modifikasi tambahan (*LoRA*) untuk menyuntikkan detail mikro ekstra (seperti pori-pori kulit halus) atau mengakselerasi sampling steps.

## Contoh Model yang Direkomendasikan:
1. **`add_detail.safetensors`** (~37.8 MB):
   - Menajamkan detail mikro pada pori-pori kulit, helai rambut, dan pantulan cahaya iris mata.
2. **`lcm_lora_sd15.safetensors`** (~134 MB):
   - Akselerator sampling kilat yang memungkinkan model SD 1.5 biasa selesai dalam 4 hingga 8 langkah saja.

#pragma once
#ifndef HARDWARE_DETECTOR_H
#define HARDWARE_DETECTOR_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <string>
#include <vector>

struct GpuInfo {
    std::string name;
    std::string vendor_name; // "NVIDIA", "Intel", "AMD", dll.
    UINT vendor_id = 0;
    int vram_mb = 0;
    int vram_gb = 0;
    bool is_nvidia = false;
    bool is_rtx = false;
    bool is_discrete = false;
};

struct HardwareDetectionResult {
    std::string cpu_brand = "";
    int cpu_cores_physical = 1;
    int cpu_threads_logical = 1;
    bool has_avx2 = false;
    bool has_fma = false;

    int total_ram_gb = 0;
    int free_ram_gb = 0;

    std::vector<GpuInfo> gpus;
    int selected_gpu_idx = 0;
    bool has_nvidia_gpu = false;
    bool has_rtx_gpu = false;

    int recommended_backend = 0; // 0=CPU AVX2, 1=NVIDIA RTX / CUDA
    int recommended_threads = 4;
    int recommended_gpu_layers = 0;
    bool recommended_fast_vae = true;
    int recommended_steps = 20;
    std::string profile_summary = "";
    std::string compatibility_status = "";
};

// Global Hardware Profile & Backend Settings
extern HardwareDetectionResult g_hw_profile;
extern bool g_hw_initialized;
extern int  g_hardware_backend_mode; // 0=Otomatis (Rekomendasi Cerdas), 1=Paksa CPU AVX2, 2=Paksa NVIDIA CUDA/RTX
extern const char* g_backend_mode_names[3];
extern int  g_gpu_layers; // Layers offload ke GPU (0 - 99)
extern std::string g_hw_notification_msg;

HardwareDetectionResult DetectHardwareAndCompatibility();
void ApplyHardwareCompatibilitySettings(bool user_override_backend = false);

#endif // HARDWARE_DETECTOR_H

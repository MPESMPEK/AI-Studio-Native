#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstdint>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

// Snapshot data telemetri yang siap dikonsumsi langsung oleh render loop D3D11/ImGui
struct HardwareTelemetrySnapshot {
    // RAM Metrics
    float ram_percent;            // Persentase pemakaian RAM (0.0f - 100.0f)
    uint64_t ram_used_mb;         // RAM terpakai (MB)
    uint64_t ram_total_mb;        // Total RAM fisik (MB)
    uint64_t ram_free_mb;         // RAM tersedia (MB)

    // CPU Metrics
    float cpu_usage_percent;      // Pemakaian CPU global sistem (0.0f - 100.0f)
    uint32_t total_logical_cores; // Jumlah core/thread CPU logis
    uint32_t active_engine_threads; // Thread aktif yang dialokasikan engine AI

    // Hardware & Instruction Set Capabilities
    bool has_avx2;                // AVX2 SIMD Instruction Set
    bool has_fma;                 // Fused Multiply-Add
    bool has_avx512;              // AVX-512 Support
    bool is_taesd_active;         // Flag status Fast VAE (TAESD)
};

class HardwareTelemetry {
public:
    static HardwareTelemetry& Get();

    // Memulai thread telemetri di background (interval sampling default: 500ms)
    void Start(uint32_t sample_interval_ms = 500);

    // Menghentikan thread secara graceful saat aplikasi ditutup
    void Stop();

    // Pengambilan data thread-safe instan (hanya copy struct POD di memori, latensi < 10 nanodetik)
    HardwareTelemetrySnapshot GetSnapshot() const;

    // Setter untuk status internal engine (thread-safe)
    void SetActiveEngineThreads(uint32_t count);
    void SetTAESDActive(bool active);

private:
    HardwareTelemetry();
    ~HardwareTelemetry();

    HardwareTelemetry(const HardwareTelemetry&) = delete;
    HardwareTelemetry& operator=(const HardwareTelemetry&) = delete;

    // Loop pekerja di background thread
    void WorkerLoop();

    // Utilitas perhitungan CPU usage Win32
    void SampleCPUUsage(float& out_cpu_percent);
    static uint64_t FileTimeToUint64(const FILETIME& ft);

    // Deteksi fitur CPU via __cpuid Win32 / MSVC / GCC intrinsics
    void DetectInstructionSets();

private:
    std::atomic<bool> m_is_running{false};
    uint32_t m_interval_ms{500};
    std::thread m_worker_thread;
    std::condition_variable m_cv_sleep;
    std::mutex m_cv_mutex;

    // Double buffering / mutex minimal untuk render snapshot
    mutable std::mutex m_snapshot_mutex;
    HardwareTelemetrySnapshot m_current_snapshot{};

    // State internal perhitungan CPU
    uint64_t m_prev_idle_time{0};
    uint64_t m_prev_kernel_time{0};
    uint64_t m_prev_user_time{0};

    // Flag instruksi tetap (dihitung sekali saat inisialisasi)
    bool m_avx2_supported{false};
    bool m_fma_supported{false};
    bool m_avx512_supported{false};

    std::atomic<uint32_t> m_active_threads{4};
    std::atomic<bool> m_taesd_active{false};
};

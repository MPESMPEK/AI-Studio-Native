#include "../include/hardware_telemetry.h"
#include <chrono>

#if defined(_MSC_VER)
    #include <intrin.h>
#elif defined(__GNUC__) || defined(__clang__)
    #include <cpuid.h>
#endif

HardwareTelemetry& HardwareTelemetry::Get() {
    static HardwareTelemetry instance;
    return instance;
}

HardwareTelemetry::HardwareTelemetry() {
    DetectInstructionSets();
    
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    m_current_snapshot.total_logical_cores = sys_info.dwNumberOfProcessors;
    m_current_snapshot.has_avx2 = m_avx2_supported;
    m_current_snapshot.has_fma = m_fma_supported;
    m_current_snapshot.has_avx512 = m_avx512_supported;
    m_current_snapshot.active_engine_threads = m_active_threads.load();
    m_current_snapshot.is_taesd_active = m_taesd_active.load();
}

HardwareTelemetry::~HardwareTelemetry() {
    Stop();
}

void HardwareTelemetry::DetectInstructionSets() {
#if defined(_MSC_VER)
    int cpu_info[4] = {0};
    __cpuid(cpu_info, 1);
    m_fma_supported = (cpu_info[2] & (1 << 12)) != 0;

    __cpuidex(cpu_info, 7, 0);
    m_avx2_supported = (cpu_info[1] & (1 << 5)) != 0;
    m_avx512_supported = (cpu_info[1] & (1 << 16)) != 0;
#elif defined(__GNUC__) || defined(__clang__)
    unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
        m_fma_supported = (ecx & (1 << 12)) != 0;
    }
    if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
        m_avx2_supported = (ebx & (1 << 5)) != 0;
        m_avx512_supported = (ebx & (1 << 16)) != 0;
    }
#else
    m_avx2_supported = true;
    m_fma_supported = true;
    m_avx512_supported = false;
#endif
}

uint64_t HardwareTelemetry::FileTimeToUint64(const FILETIME& ft) {
    return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

void HardwareTelemetry::SampleCPUUsage(float& out_cpu_percent) {
    FILETIME idle_time, kernel_time, user_time;
    if (!GetSystemTimes(&idle_time, &kernel_time, &user_time)) {
        out_cpu_percent = 0.0f;
        return;
    }

    uint64_t cur_idle = FileTimeToUint64(idle_time);
    uint64_t cur_kernel = FileTimeToUint64(kernel_time);
    uint64_t cur_user = FileTimeToUint64(user_time);

    if (m_prev_idle_time != 0) {
        uint64_t delta_idle = cur_idle - m_prev_idle_time;
        uint64_t delta_kernel = cur_kernel - m_prev_kernel_time;
        uint64_t delta_user = cur_user - m_prev_user_time;

        uint64_t total_sys = delta_kernel + delta_user;
        if (total_sys > 0) {
            uint64_t active_time = (total_sys > delta_idle) ? (total_sys - delta_idle) : 0;
            out_cpu_percent = (static_cast<float>(active_time) * 100.0f) / static_cast<float>(total_sys);
            if (out_cpu_percent > 100.0f) out_cpu_percent = 100.0f;
            if (out_cpu_percent < 0.0f) out_cpu_percent = 0.0f;
        }
    }

    m_prev_idle_time = cur_idle;
    m_prev_kernel_time = cur_kernel;
    m_prev_user_time = cur_user;
}

void HardwareTelemetry::Start(uint32_t sample_interval_ms) {
    if (m_is_running.exchange(true)) {
        return;
    }
    m_interval_ms = sample_interval_ms;
    m_worker_thread = std::thread(&HardwareTelemetry::WorkerLoop, this);
}

void HardwareTelemetry::Stop() {
    if (!m_is_running.exchange(false)) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_cv_mutex);
        m_cv_sleep.notify_all();
    }
    if (m_worker_thread.joinable()) {
        m_worker_thread.join();
    }
}

void HardwareTelemetry::WorkerLoop() {
    float initial_cpu = 0.0f;
    SampleCPUUsage(initial_cpu);

    while (m_is_running.load(std::memory_order_relaxed)) {
        // 1. Ambil status RAM menggunakan Win32 GlobalMemoryStatusEx
        MEMORYSTATUSEX mem_status;
        mem_status.dwLength = sizeof(MEMORYSTATUSEX);

        HardwareTelemetrySnapshot new_data{};
        if (GlobalMemoryStatusEx(&mem_status)) {
            new_data.ram_percent = static_cast<float>(mem_status.dwMemoryLoad);
            new_data.ram_total_mb = mem_status.ullTotalPhys / (1024 * 1024);
            new_data.ram_free_mb = mem_status.ullAvailPhys / (1024 * 1024);
            new_data.ram_used_mb = new_data.ram_total_mb - new_data.ram_free_mb;
        }

        // 2. Ambil status CPU
        float cpu_pct = 0.0f;
        SampleCPUUsage(cpu_pct);
        new_data.cpu_usage_percent = cpu_pct;

        // 3. Masukkan parameter arsitektur & status engine
        SYSTEM_INFO sys_info;
        GetSystemInfo(&sys_info);
        new_data.total_logical_cores = sys_info.dwNumberOfProcessors;
        new_data.active_engine_threads = m_active_threads.load(std::memory_order_relaxed);
        new_data.has_avx2 = m_avx2_supported;
        new_data.has_fma = m_fma_supported;
        new_data.has_avx512 = m_avx512_supported;
        new_data.is_taesd_active = m_taesd_active.load(std::memory_order_relaxed);

        // 4. Update snapshot secara thread-safe
        {
            std::lock_guard<std::mutex> lock(m_snapshot_mutex);
            m_current_snapshot = new_data;
        }

        // 5. Sleep interupsi menggunakan condition variable
        std::unique_lock<std::mutex> sleep_lock(m_cv_mutex);
        m_cv_sleep.wait_for(sleep_lock, std::chrono::milliseconds(m_interval_ms), [this] {
            return !m_is_running.load(std::memory_order_relaxed);
        });
    }
}

HardwareTelemetrySnapshot HardwareTelemetry::GetSnapshot() const {
    std::lock_guard<std::mutex> lock(m_snapshot_mutex);
    return m_current_snapshot;
}

void HardwareTelemetry::SetActiveEngineThreads(uint32_t count) {
    m_active_threads.store(count, std::memory_order_relaxed);
}

void HardwareTelemetry::SetTAESDActive(bool active) {
    m_taesd_active.store(active, std::memory_order_relaxed);
}

#ifndef BUILDING_ENGINE_CORE_DLL
#define BUILDING_ENGINE_CORE_DLL
#endif
#include "../include/engine_core.h"
#include "../include/simd_math.h"

#include <windows.h>
#include <omp.h>
#include <stdlib.h>
#include <stdio.h>
#include <intrin.h>

static int g_initialized = 0;
static int g_num_threads = 4;
static int g_has_avx2 = 0;
static int g_has_fma  = 0;
static int g_has_f16c = 0;

static void detect_cpu_features() {
    int cpuInfo[4];
    __cpuid(cpuInfo, 1);
    g_has_f16c = (cpuInfo[2] & (1 << 29)) != 0;
    g_has_fma  = (cpuInfo[2] & (1 << 12)) != 0;

    __cpuidex(cpuInfo, 7, 0);
    g_has_avx2 = (cpuInfo[1] & (1 << 5)) != 0;
}

CORE_API int Core_Init(int num_threads) {
    if (num_threads <= 0) {
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        g_num_threads = sysinfo.dwNumberOfProcessors;
    } else {
        g_num_threads = num_threads;
    }

    omp_set_num_threads(g_num_threads);
    detect_cpu_features();

    g_initialized = 1;
    return 0;
}

CORE_API void Core_GetTelemetry(CoreTelemetry* out) {
    if (!out) return;

    if (!g_initialized) {
        Core_Init(0);
    }

    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);

    out->total_ram_bytes = memInfo.ullTotalPhys;
    out->free_ram_bytes  = memInfo.ullAvailPhys;

    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    out->cpu_threads_logical = sysinfo.dwNumberOfProcessors;
    out->cpu_cores_physical  = sysinfo.dwNumberOfProcessors / 2; // Intel Hyperthreading

    out->has_avx2 = g_has_avx2;
    out->has_fma  = g_has_fma;
    out->has_f16c = g_has_f16c;

    out->arena_allocated_mb = 0.0;
    out->arena_peak_mb = 0.0;
}

CORE_API float Core_DotProductAVX2(const float* a, const float* b, int dimension) {
    if (!a || !b || dimension <= 0) return 0.0f;
    return simd_dot_product_avx2(a, b, dimension);
}

CORE_API float Core_CosineSimilarityAVX2(const float* a, const float* b, int dimension) {
    if (!a || !b || dimension <= 0) return 0.0f;
    return simd_cosine_similarity_avx2(a, b, dimension);
}

CORE_API void Core_BatchCosineAVX2(const float* query, const float* matrix, int num_vectors, int dimension, float* out_scores) {
    if (!query || !matrix || !out_scores || num_vectors <= 0 || dimension <= 0) return;

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < num_vectors; ++i) {
        const float* vec_b = matrix + ((size_t)i * dimension);
        out_scores[i] = simd_cosine_similarity_avx2(query, vec_b, dimension);
    }
}

CORE_API void Core_Shutdown(void) {
    g_initialized = 0;
}

#ifndef ENGINE_CORE_H
#define ENGINE_CORE_H

#include <stdint.h>
#include <stddef.h>

#ifdef _WIN32
    #ifdef BUILDING_ENGINE_CORE_DLL
        #define CORE_API __declspec(dllexport)
    #elif defined(STATIC_ENGINE_CORE)
        #define CORE_API
    #else
        #define CORE_API __declspec(dllimport)
    #endif
#else
    #define CORE_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Telemetri Hardware & Memori Real-Time
typedef struct {
    uint64_t total_ram_bytes;
    uint64_t free_ram_bytes;
    int cpu_cores_physical;
    int cpu_threads_logical;
    int has_avx2;
    int has_fma;
    int has_f16c;
    double arena_allocated_mb;
    double arena_peak_mb;
} CoreTelemetry;

// Inisialisasi Engine C++ & Konfigurasi Threading
CORE_API int Core_Init(int num_threads);

// Mendapatkan Telemetri Akurat Tanpa Overhead
CORE_API void Core_GetTelemetry(CoreTelemetry* out_telemetry);

// Vector Math SIMD AVX2 (Dot Product & Cosine Similarity Super Cepat)
CORE_API float Core_DotProductAVX2(const float* a, const float* b, int dimension);
CORE_API float Core_CosineSimilarityAVX2(const float* a, const float* b, int dimension);
CORE_API void Core_BatchCosineAVX2(const float* query, const float* matrix, int num_vectors, int dimension, float* out_scores);

// Motion Synthesis C++ (Multi-Threaded Video Frame Interpolator)
// motion_type: 0=ZoomIn, 1=ZoomOut, 2=PanLeft, 3=PanRight, 4=Orbit3D
CORE_API int Core_SynthesizeMotionFrames(
    const uint8_t* src_rgb,
    int width,
    int height,
    uint8_t* dst_frames,
    int num_frames,
    int motion_type,
    float ease_factor
);

// Filter Pemrosesan Citra Cepat (Contrast, Vignette & Sharpening AVX2)
CORE_API void Core_ApplyCinematicGrading(uint8_t* rgb_data, int width, int height, float contrast, float vignette_str);

// Membersihkan Memory Arena & Menutup Engine
CORE_API void Core_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif // ENGINE_CORE_H

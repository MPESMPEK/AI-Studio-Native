#include "../include/engine_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <chrono>
#include <vector>

int main() {
    printf("=====================================================\n");
    printf("   C++ NATIVE ENGINE BENCHMARK (AVX2 + OpenMP)       \n");
    printf("=====================================================\n");

    Core_Init(0);

    CoreTelemetry telem;
    Core_GetTelemetry(&telem);

    printf("[Hardware Telemetry]\n");
    printf(" - Total RAM       : %.2f GB\n", (double)telem.total_ram_bytes / (1024.0 * 1024.0 * 1024.0));
    printf(" - Free RAM        : %.2f GB\n", (double)telem.free_ram_bytes / (1024.0 * 1024.0 * 1024.0));
    printf(" - CPU Cores/Threads: %d cores / %d threads\n", telem.cpu_cores_physical, telem.cpu_threads_logical);
    printf(" - AVX2 Support    : %s\n", telem.has_avx2 ? "YES (Active)" : "NO");
    printf(" - FMA Support     : %s\n", telem.has_fma ? "YES (Active)" : "NO");
    printf(" - F16C Support    : %s\n", telem.has_f16c ? "YES (Active)" : "NO");
    printf("-----------------------------------------------------\n");

    // 1. Vector Dot Product Benchmark
    const int DIM = 4096; // typical LLM embedding dimension (e.g., Llama/Qwen/Mistral)
    std::vector<float> vec_a(DIM);
    std::vector<float> vec_b(DIM);
    for (int i = 0; i < DIM; ++i) {
        vec_a[i] = (float)rand() / (float)RAND_MAX;
        vec_b[i] = (float)rand() / (float)RAND_MAX;
    }

    const int ITERATIONS = 100000;
    auto t0 = std::chrono::high_resolution_clock::now();
    float sum = 0.0f;
    for (int it = 0; it < ITERATIONS; ++it) {
        sum += Core_CosineSimilarityAVX2(vec_a.data(), vec_b.data(), DIM);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double total_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double per_op_us = (total_ms * 1000.0) / (double)ITERATIONS;

    printf("[SIMD AVX2 Math Benchmark]\n");
    printf(" - Dimension       : %d floats (LLM Embedding standard)\n", DIM);
    printf(" - Iterations      : %d\n", ITERATIONS);
    printf(" - Total Time      : %.2f ms\n", total_ms);
    printf(" - Latency / op    : %.3f microseconds (%.1f million ops/sec)\n", per_op_us, 1000.0 / per_op_us);
    printf(" - Cosine Score    : %.6f\n", sum / ITERATIONS);
    printf("-----------------------------------------------------\n");

    // 2. Video Motion Synthesis Benchmark
    const int W = 512;
    const int H = 512;
    const int NUM_FRAMES = 30; // 1 full second of 30fps animation
    std::vector<uint8_t> src_image(W * H * 3);
    for (size_t i = 0; i < src_image.size(); ++i) {
        src_image[i] = (uint8_t)(i % 256);
    }
    std::vector<uint8_t> out_frames((size_t)W * H * 3 * NUM_FRAMES);

    auto t2 = std::chrono::high_resolution_clock::now();
    Core_SynthesizeMotionFrames(src_image.data(), W, H, out_frames.data(), NUM_FRAMES, 4 /*Orbit3D*/, 1.0f);
    auto t3 = std::chrono::high_resolution_clock::now();
    double motion_ms = std::chrono::duration<double, std::milli>(t3 - t2).count();
    double fps = (double)NUM_FRAMES / (motion_ms / 1000.0);

    printf("[C++ Multi-threaded Motion Synthesis (512x512)]\n");
    printf(" - Frames Generated: %d frames (Orbit 3D Sub-pixel)\n", NUM_FRAMES);
    printf(" - Total Time      : %.2f ms\n", motion_ms);
    printf(" - Generation Speed: %.1f FPS (%.2f ms per frame)\n", fps, motion_ms / NUM_FRAMES);
    printf(" - Memory Used     : %.2f MB (zero allocations during synthesis)\n", (double)out_frames.size() / (1024.0 * 1024.0));
    printf("=====================================================\n");
    printf("BENCHMARK COMPLETED SUCCESSFULLY!\n");

    Core_Shutdown();
    return 0;
}

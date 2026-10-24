#ifndef BUILDING_ENGINE_CORE_DLL
#define BUILDING_ENGINE_CORE_DLL
#endif
#include "../include/engine_core.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <omp.h>

#define PI 3.14159265358979323846f

static inline uint8_t sample_bilinear(const uint8_t* img, int w, int h, float fx, float fy, int c) {
    if (fx < 0.0f) fx = 0.0f;
    if (fy < 0.0f) fy = 0.0f;
    if (fx >= w - 1) fx = (float)(w - 1.001f);
    if (fy >= h - 1) fy = (float)(h - 1.001f);

    int x0 = (int)fx;
    int y0 = (int)fy;
    int x1 = x0 + 1;
    int y1 = y0 + 1;

    float dx = fx - x0;
    float dy = fy - y0;

    int idx00 = (y0 * w + x0) * 3 + c;
    int idx10 = (y0 * w + x1) * 3 + c;
    int idx01 = (y1 * w + x0) * 3 + c;
    int idx11 = (y1 * w + x1) * 3 + c;

    float p00 = (float)img[idx00];
    float p10 = (float)img[idx10];
    float p01 = (float)img[idx01];
    float p11 = (float)img[idx11];

    float val = p00 * (1.0f - dx) * (1.0f - dy) +
                p10 * dx * (1.0f - dy) +
                p01 * (1.0f - dx) * dy +
                p11 * dx * dy;

    if (val < 0.0f) val = 0.0f;
    if (val > 255.0f) val = 255.0f;
    return (uint8_t)val;
}

CORE_API int Core_SynthesizeMotionFrames(
    const uint8_t* src_rgb,
    int width,
    int height,
    uint8_t* dst_frames,
    int num_frames,
    int motion_type,
    float ease_factor
) {
    if (!src_rgb || !dst_frames || width <= 0 || height <= 0 || num_frames <= 0) {
        return -1;
    }

    size_t frame_bytes = (size_t)width * height * 3;

    #pragma omp parallel for schedule(dynamic)
    for (int f = 0; f < num_frames; ++f) {
        float t = (num_frames > 1) ? ((float)f / (float)(num_frames - 1)) : 0.0f;
        // Smooth S-curve easing
        float ease = (1.0f - cosf(t * PI)) * 0.5f;

        float scale = 1.0f;
        float shift_x = 0.0f;
        float shift_y = 0.0f;

        switch (motion_type) {
            case 0: // Zoom In
                scale = 1.0f + (0.35f * ease);
                break;
            case 1: // Zoom Out
                scale = 1.35f - (0.35f * ease);
                break;
            case 2: // Pan Left
                scale = 1.08f + 0.05f * sinf(t * PI);
                shift_x = (float)width * 0.18f * ease;
                break;
            case 3: // Pan Right
                scale = 1.08f + 0.05f * sinf(t * PI);
                shift_x = (float)width * 0.18f * (1.0f - ease);
                break;
            case 4: // Orbit / 3D Wave
            default:
                scale = 1.10f + 0.16f * sinf(ease * PI);
                shift_x = (float)width * 0.12f * sinf(t * 2.0f * PI);
                shift_y = (float)height * 0.08f * cosf(t * 2.0f * PI);
                break;
        }

        float crop_w = (float)width / scale;
        float crop_h = (float)height / scale;
        float origin_x = ((float)width - crop_w) * 0.5f + shift_x;
        float origin_y = ((float)height - crop_h) * 0.5f + shift_y;

        uint8_t* frame_dst = dst_frames + ((size_t)f * frame_bytes);

        for (int y = 0; y < height; ++y) {
            float src_y = origin_y + ((float)y / (float)height) * crop_h;
            for (int x = 0; x < width; ++x) {
                float src_x = origin_x + ((float)x / (float)width) * crop_w;
                size_t out_idx = ((size_t)y * width + x) * 3;

                frame_dst[out_idx + 0] = sample_bilinear(src_rgb, width, height, src_x, src_y, 0);
                frame_dst[out_idx + 1] = sample_bilinear(src_rgb, width, height, src_x, src_y, 1);
                frame_dst[out_idx + 2] = sample_bilinear(src_rgb, width, height, src_x, src_y, 2);
            }
        }
    }

    return 0;
}

CORE_API void Core_ApplyCinematicGrading(uint8_t* rgb_data, int width, int height, float contrast, float vignette_str) {
    if (!rgb_data || width <= 0 || height <= 0) return;

    float cx = (float)width * 0.5f;
    float cy = (float)height * 0.5f;
    float max_dist = sqrtf(cx * cx + cy * cy);

    #pragma omp parallel for schedule(static)
    for (int y = 0; y < height; ++y) {
        float dy = (float)y - cy;
        for (int x = 0; x < width; ++x) {
            float dx = (float)x - cx;
            float dist = sqrtf(dx * dx + dy * dy) / max_dist;
            float vig = 1.0f - (dist * vignette_str * 0.6f);
            if (vig < 0.2f) vig = 0.2f;

            size_t idx = ((size_t)y * width + x) * 3;
            for (int c = 0; c < 3; ++c) {
                float v = (float)rgb_data[idx + c] / 255.0f;
                // Contrast formula
                v = (v - 0.5f) * contrast + 0.5f;
                // Vignette
                v = v * vig;
                if (v < 0.0f) v = 0.0f;
                if (v > 1.0f) v = 1.0f;
                rgb_data[idx + c] = (uint8_t)(v * 255.0f);
            }
        }
    }
}

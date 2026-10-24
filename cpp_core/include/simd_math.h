#ifndef SIMD_MATH_H
#define SIMD_MATH_H

#include <immintrin.h>
#include <math.h>
#include <stdint.h>

#if defined(__AVX2__) || defined(_M_AMD64)
static inline float simd_dot_product_avx2(const float* a, const float* b, int n) {
    __m256 sum = _mm256_setzero_ps();
    int i = 0;
    for (; i <= n - 8; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
#ifdef __FMA__
        sum = _mm256_fmadd_ps(va, vb, sum);
#else
        sum = _mm256_add_ps(sum, _mm256_mul_ps(va, vb));
#endif
    }
    
    // Horizontal reduction of 8 floats in sum
    float buffer[8];
    _mm256_storeu_ps(buffer, sum);
    float result = buffer[0] + buffer[1] + buffer[2] + buffer[3] +
                   buffer[4] + buffer[5] + buffer[6] + buffer[7];
                   
    // Remaining elements
    for (; i < n; ++i) {
        result += a[i] * b[i];
    }
    return result;
}

static inline float simd_cosine_similarity_avx2(const float* a, const float* b, int n) {
    __m256 sum_dot = _mm256_setzero_ps();
    __m256 sum_a2  = _mm256_setzero_ps();
    __m256 sum_b2  = _mm256_setzero_ps();
    
    int i = 0;
    for (; i <= n - 8; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
#ifdef __FMA__
        sum_dot = _mm256_fmadd_ps(va, vb, sum_dot);
        sum_a2  = _mm256_fmadd_ps(va, va, sum_a2);
        sum_b2  = _mm256_fmadd_ps(vb, vb, sum_b2);
#else
        sum_dot = _mm256_add_ps(sum_dot, _mm256_mul_ps(va, vb));
        sum_a2  = _mm256_add_ps(sum_a2,  _mm256_mul_ps(va, va));
        sum_b2  = _mm256_add_ps(sum_b2,  _mm256_mul_ps(vb, vb));
#endif
    }
    
    float buf_dot[8], buf_a2[8], buf_b2[8];
    _mm256_storeu_ps(buf_dot, sum_dot);
    _mm256_storeu_ps(buf_a2, sum_a2);
    _mm256_storeu_ps(buf_b2, sum_b2);
    
    float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
    for (int k = 0; k < 8; ++k) {
        dot    += buf_dot[k];
        norm_a += buf_a2[k];
        norm_b += buf_b2[k];
    }
    
    for (; i < n; ++i) {
        dot    += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    
    float denom = sqrtf(norm_a) * sqrtf(norm_b);
    if (denom <= 1e-8f) return 0.0f;
    return dot / denom;
}
#endif

#endif // SIMD_MATH_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
// On ARM64, we need these headers to query the auxiliary vector for HWCAPS
#if defined(__aarch64__)
#include <sys/auxv.h>
#include <asm/hwcap.h>
#ifndef HWCAP_SVE
#define HWCAP_SVE (1 << 22) // Fallback definition if header is old
#endif
#endif
#include "../include/types.h"
#include "../include/workloads.h"

// A Power Hog: Generates massive heat and power draw to force throttling
power_hog_func_t power_hog = NULL;

#if defined(__x86_64__)
// Use vfmadd231pd (Fused Multiply-Add for Packed Double-Precision).
// This lights up the heavy vector ALUs on x86 CPU.
__attribute__((target("avx512f")))
void power_hog_avx512(void) {
    // Ensure this thread can be forcefully stopped by the main thread
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    // Allocate 32 MB of memory per thread.
    // Across 192 threads, this requires ~6 GB of RAM and thoroughly thrashes
    // the EPYC L3 cache and memory controllers.
    size_t buf_size = 32 * 1024 * 1024;

    // We MUST align the memory to 64 bytes for AVX-512 (zmm) instructions to work safely.
    double *buffer = (double *)aligned_alloc(64, buf_size);
    if (!buffer) {
        // Fallback if allocation fails
        while(1) {}
    }

    // Initialize the buffer to prevent multiplying by NaN/0
    for (size_t i = 0; i < buf_size / sizeof(double); i++) {
        buffer[i] = 1.00001;
    }

    // Pointers for our sliding window
    double *ptr = buffer;
    double *end = buffer + (buf_size / sizeof(double));

    __asm__ volatile (
        // Initialize accumulator registers with dummy data
        "vmovapd 0(%[start]), %%zmm4\n\t"
        "vmovapd 64(%[start]), %%zmm5\n\t"
        "vmovapd 128(%[start]), %%zmm6\n\t"
        "vmovapd 192(%[start]), %%zmm7\n\t"

        ".align 16\n"
        "1:\n\t"
        // 1. LOAD 4 cache lines (256 bytes) from Memory into zmm0-zmm3
        "vmovapd 0(%[ptr]), %%zmm0\n\t"
        "vmovapd 64(%[ptr]), %%zmm1\n\t"
        "vmovapd 128(%[ptr]), %%zmm2\n\t"
        "vmovapd 192(%[ptr]), %%zmm3\n\t"

        // 2. COMPUTE: Massive 512-bit Fused Multiply-Add
        "vfmadd231pd %%zmm4, %%zmm4, %%zmm0\n\t"
        "vfmadd231pd %%zmm5, %%zmm5, %%zmm1\n\t"
        "vfmadd231pd %%zmm6, %%zmm6, %%zmm2\n\t"
        "vfmadd231pd %%zmm7, %%zmm7, %%zmm3\n\t"

        // 3. STORE the computed results back to Memory
        "vmovapd %%zmm0, 0(%[ptr])\n\t"
        "vmovapd %%zmm1, 64(%[ptr])\n\t"
        "vmovapd %%zmm2, 128(%[ptr])\n\t"
        "vmovapd %%zmm3, 192(%[ptr])\n\t"

        // 4. Advance the pointer by 256 bytes (32 doubles)
        "add $256, %[ptr]\n\t"

        // 5. Check if we reached the end of the 32MB buffer
        "cmp %[end], %[ptr]\n\t"
        "jb 1b\n\t" // Jump back to '1' if pointer is below end

        // 6. Reset pointer to start and loop endlessly
        "mov %[start], %[ptr]\n\t"
        "jmp 1b\n\t"

        // Operand Bindings
        : [ptr] "+r" (ptr)
        : [end] "r" (end), [start] "r" (buffer)
        : "zmm0", "zmm1", "zmm2", "zmm3", "zmm4", "zmm5", "zmm6", "zmm7", "cc", "memory"
    );
}

__attribute__((target("avx2")))
void power_hog_avx2(void) {
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    __asm__ volatile (
        ".align 16\n"
        "1:\n\t"
        "vfmadd231pd %%ymm0, %%ymm0, %%ymm0\n\t"
        "vfmadd231pd %%ymm1, %%ymm1, %%ymm1\n\t"
        "vfmadd231pd %%ymm2, %%ymm2, %%ymm2\n\t"
        "vfmadd231pd %%ymm3, %%ymm3, %%ymm3\n\t"
        "vfmadd231pd %%ymm4, %%ymm4, %%ymm4\n\t"
        "vfmadd231pd %%ymm5, %%ymm5, %%ymm5\n\t"
        "jmp 1b\n\t"
        : // No outputs
        : // No inputs
        : "ymm0", "ymm1", "ymm2", "ymm3", "ymm4", "ymm5" // Clobbered registers
    );
}

// Fallback scalar loop for non-AVX x86 machines
void power_hog_scalar_x86(void) {
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    volatile double a = 1.1, b = 2.2;
    while (1) {
        a = a * b + a;
    }
}
#endif

#if defined(__aarch64__)
// Use the fmla instruction (Fused Multiply-Add) on 128-bit 'q' registers.
// 'fmla v0.2d, v0.2d, v0.2d' multiplies two double-precision floats in v0
// and adds the result to v0, looping endlessly.
__attribute__((target("+sve")))
void power_hog_sve(void) {
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    __asm__ volatile (
        "ptrue p0.d\n\t"
        ".p2align 4\n"
        "1:\n\t"
        "fmla z0.d, p0/m, z0.d, z0.d\n\t"
        "fmla z1.d, p0/m, z1.d, z1.d\n\t"
        "fmla z2.d, p0/m, z2.d, z2.d\n\t"
        "fmla z3.d, p0/m, z3.d, z3.d\n\t"
        "b 1b\n\t"
        : // No outputs
        : // No inputs
        : "p0", "z0", "z1", "z2", "z3" // Clobber the SVE registers used
    );
}

void power_hog_neon(void) {
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    __asm__ volatile (
        ".p2align 4\n"
        "1:\n\t"
        "fmla v0.2d, v0.2d, v0.2d\n\t"
        "fmla v1.2d, v1.2d, v1.2d\n\t"
        "fmla v2.2d, v2.2d, v2.2d\n\t"
        "fmla v3.2d, v3.2d, v3.2d\n\t"
        "fmla v4.2d, v4.2d, v4.2d\n\t"
        "fmla v5.2d, v5.2d, v5.2d\n\t"
        "b 1b\n\t"
        : // No outputs
        : // No inputs
        : "v0", "v1", "v2", "v3", "v4", "v5" // Clobber the NEON registers used
    );
    __asm__ volatile (
        ".p2align 4\n"
        "1:\n\t"
        "fmla v0.2d, v0.2d, v0.2d\n\t"
        "fmla v1.2d, v1.2d, v1.2d\n\t"
        "fmla v2.2d, v2.2d, v2.2d\n\t"
        "fmla v3.2d, v3.2d, v3.2d\n\t"
        "b 1b\n\t"
        : : : "v0", "v1", "v2", "v3"
    );
}
#endif

void power_hog_ansi_c(void) {
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    // 8 independent double-precision floats.
    // Use 8 to ensure we flood all available FPU pipelines on the core.
    double a0 = 0.1, a1 = 0.2, a2 = 0.3, a3 = 0.4;
    double a4 = 0.5, a5 = 0.6, a6 = 0.7, a7 = 0.8;

    // A volatile sink. Writing to a volatile variable forces the compiler
    // to actually execute the math, preventing Dead-Code Elimination (DCE).
    volatile double power_sink = 0.0;

    while (1) {
        // Use "x = x * 0.999 + 0.001"
        // This guarantees the numbers never reach Infinity or NaN, ensuring
        // the FPU runs at maximum electrical throughput without microcode stalls.
        a0 = a0 * 0.999 + 0.001;
        a1 = a1 * 0.999 + 0.001;
        a2 = a2 * 0.999 + 0.001;
        a3 = a3 * 0.999 + 0.001;
        a4 = a4 * 0.999 + 0.001;
        a5 = a5 * 0.999 + 0.001;
        a6 = a6 * 0.999 + 0.001;
        a7 = a7 * 0.999 + 0.001;

        // Dump the sum into the volatile sink.
        power_sink = a0 + a1 + a2 + a3 + a4 + a5 + a6 + a7;
    }
    (void)power_sink;
}

void resolve_power_hog(void) {
#if defined(__x86_64__)
    // __builtin_cpu_supports queries CPUID at startup efficiently
    if (__builtin_cpu_supports("avx512f")) {
        if (verbose) { printf("[+] Detected AVX-512. Engaging high-power AVX-512 stress.\n"); }
        power_hog = power_hog_avx512;
    } else if (__builtin_cpu_supports("avx2")) {
        if (verbose) { printf("[+] Detected AVX2. Engaging AVX2 stress.\n"); }
        power_hog = power_hog_avx2;
    } else {
        if (verbose) { printf("[+] Detected standard x86. Engaging scalar math fallback stress.\n"); }
        power_hog = power_hog_scalar_x86;
    }

#elif defined(__aarch64__)
    // getauxval queries HWCAPS on Linux ELF architectures for SVE instruction availability
    unsigned long hwcap = getauxval(AT_HWCAP);
    if (hwcap & HWCAP_SVE) {
        if (verbose) { printf("[+] Detected ARM SVE. Engaging scalable vector SVE stress.\n"); }
        power_hog = power_hog_sve;
    } else {
        if (verbose) { printf("[+] Detected ARM NEON. Engaging 128-bit NEON stress.\n"); }
        power_hog = power_hog_neon;
    }

#else
    if (verbose) { printf("[+] Detected no specific vector ISA. Resolving to ANSI C stress.\n"); }
    power_hog = power_hog_ansi_c;
#endif
}

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
// On ARM64, headers to query the auxiliary vector for HWCAPS
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
volatile bool stop_hog = false;

#if defined(__x86_64__)
static double *global_power_buffer = NULL;
static size_t global_buf_size = 4096;
static volatile int buffer_lock = 0; // Simple spinlock for thread safety
// Use vfmadd231pd (Fused Multiply-Add for Packed Double-Precision).
// This lights up the heavy vector ALUs on x86 CPU.
__attribute__((target("avx512f")))
void power_hog_avx512(void) {
    // Thread-safe global memory allocation
    if (global_power_buffer == NULL) {
        if (__sync_bool_compare_and_swap(&buffer_lock, 0, 1)) {
            if (global_power_buffer == NULL) {
                global_power_buffer = (double *)aligned_alloc(64, global_buf_size);
                if (global_power_buffer) {
                    memset(global_power_buffer, 0x3F, global_buf_size);
                }
            }
        } else {
            while (global_power_buffer == NULL) {
                __asm__ volatile ("pause" ::: "memory");
            }
        }
    }

    if (!global_power_buffer) while(1) {}

    uint32_t loops = 0;
    while (1) {
        if (stop_hog) {
            break;
        }
        // Inner assembly block running 32 back-to-back FMAs per iteration.
        // Only use scratch registers zmm0-zmm7.
        __asm__ volatile (
            "vmovupd 0(%[start]), %%zmm0\n\t"
            "vmovupd 0(%[start]), %%zmm1\n\t"
            "vmovupd 0(%[start]), %%zmm2\n\t"
            "vmovupd 0(%[start]), %%zmm3\n\t"
            "vmovupd 0(%[start]), %%zmm4\n\t"
            "vmovupd 0(%[start]), %%zmm5\n\t"
            "vmovupd 0(%[start]), %%zmm6\n\t"
            "vmovupd 0(%[start]), %%zmm7\n\t"

            "mov $1000, %[loops]\n\t" // Run 32,000 FMAs before checking stop flag again
            ".align 16\n"
            "1:\n\t"
            // We alternate operations on zmm0-zmm7.
            // This saturates all execution ports with absolutely zero register dependencies or pipeline bubbles!
            "vfmadd231pd %%zmm0, %%zmm0, %%zmm0\n\t"
            "vfmadd231pd %%zmm1, %%zmm1, %%zmm1\n\t"
            "vfmadd231pd %%zmm2, %%zmm2, %%zmm2\n\t"
            "vfmadd231pd %%zmm3, %%zmm3, %%zmm3\n\t"
            "vfmadd231pd %%zmm4, %%zmm4, %%zmm4\n\t"
            "vfmadd231pd %%zmm5, %%zmm5, %%zmm5\n\t"
            "vfmadd231pd %%zmm6, %%zmm6, %%zmm6\n\t"
            "vfmadd231pd %%zmm7, %%zmm7, %%zmm7\n\t"

            "vfmadd231pd %%zmm0, %%zmm0, %%zmm0\n\t"
            "vfmadd231pd %%zmm1, %%zmm1, %%zmm1\n\t"
            "vfmadd231pd %%zmm2, %%zmm2, %%zmm2\n\t"
            "vfmadd231pd %%zmm3, %%zmm3, %%zmm3\n\t"
            "vfmadd231pd %%zmm4, %%zmm4, %%zmm4\n\t"
            "vfmadd231pd %%zmm5, %%zmm5, %%zmm5\n\t"
            "vfmadd231pd %%zmm6, %%zmm6, %%zmm6\n\t"
            "vfmadd231pd %%zmm7, %%zmm7, %%zmm7\n\t"

            "vfmadd231pd %%zmm0, %%zmm0, %%zmm0\n\t"
            "vfmadd231pd %%zmm1, %%zmm1, %%zmm1\n\t"
            "vfmadd231pd %%zmm2, %%zmm2, %%zmm2\n\t"
            "vfmadd231pd %%zmm3, %%zmm3, %%zmm3\n\t"
            "vfmadd231pd %%zmm4, %%zmm4, %%zmm4\n\t"
            "vfmadd231pd %%zmm5, %%zmm5, %%zmm5\n\t"
            "vfmadd231pd %%zmm6, %%zmm6, %%zmm6\n\t"
            "vfmadd231pd %%zmm7, %%zmm7, %%zmm7\n\t"

            "vfmadd231pd %%zmm0, %%zmm0, %%zmm0\n\t"
            "vfmadd231pd %%zmm1, %%zmm1, %%zmm1\n\t"
            "vfmadd231pd %%zmm2, %%zmm2, %%zmm2\n\t"
            "vfmadd231pd %%zmm3, %%zmm3, %%zmm3\n\t"
            "vfmadd231pd %%zmm4, %%zmm4, %%zmm4\n\t"
            "vfmadd231pd %%zmm5, %%zmm5, %%zmm5\n\t"
            "vfmadd231pd %%zmm6, %%zmm6, %%zmm6\n\t"
            "vfmadd231pd %%zmm7, %%zmm7, %%zmm7\n\t"

            "dec %[loops]\n\t"
            "jnz 1b\n\t"

            : [loops] "=r" (loops)
            : [start] "r" (global_power_buffer), "[loops]" (0)
            : "zmm0", "zmm1", "zmm2", "zmm3", "zmm4", "zmm5", "zmm6", "zmm7", "cc", "memory"
        );
    }
}

__attribute__((target("avx2")))
void power_hog_avx2(void) {
    uint32_t loops = 0;
    while (1) {
        if (stop_hog) {
            break;
        }
        __asm__ volatile (
            "mov $1000, %[loops]\n\t" 
            ".align 16\n"
            "1:\n\t"

            // Load data from L1 Cache
            "vmovupd 0(%[start]), %%ymm0\n\t"
            "vmovupd 32(%[start]), %%ymm1\n\t"
            "vmovupd 64(%[start]), %%ymm2\n\t"
            "vmovupd 96(%[start]), %%ymm3\n\t"

            // Smash with Fused Multiply-Add
            "vfmadd231pd %%ymm0, %%ymm0, %%ymm0\n\t"
            "vfmadd231pd %%ymm1, %%ymm1, %%ymm1\n\t"
            "vfmadd231pd %%ymm2, %%ymm2, %%ymm2\n\t"
            "vfmadd231pd %%ymm3, %%ymm3, %%ymm3\n\t"

            // Write data back to L1 Cache (Forces the Load/Store Units to pull max current)
            "vmovupd %%ymm0, 0(%[start])\n\t"
            "vmovupd %%ymm1, 32(%[start])\n\t"
            "vmovupd %%ymm2, 64(%[start])\n\t"
            "vmovupd %%ymm3, 96(%[start])\n\t"

            // Repeat to keep instruction density high
            "vmovupd 128(%[start]), %%ymm4\n\t"
            "vmovupd 160(%[start]), %%ymm5\n\t"
            "vmovupd 192(%[start]), %%ymm6\n\t"
            "vmovupd 224(%[start]), %%ymm7\n\t"

            "vfmadd231pd %%ymm4, %%ymm4, %%ymm4\n\t"
            "vfmadd231pd %%ymm5, %%ymm5, %%ymm5\n\t"
            "vfmadd231pd %%ymm6, %%ymm6, %%ymm6\n\t"
            "vfmadd231pd %%ymm7, %%ymm7, %%ymm7\n\t"

            "vmovupd %%ymm4, 128(%[start])\n\t"
            "vmovupd %%ymm5, 160(%[start])\n\t"
            "vmovupd %%ymm6, 192(%[start])\n\t"
            "vmovupd %%ymm7, 224(%[start])\n\t"

            "dec %[loops]\n\t"
            "jnz 1b\n\t"

            : [loops] "=r" (loops)
            : [start] "r" (global_power_buffer), "[loops]" (0)
            : "ymm0", "ymm1", "ymm2", "ymm3", "ymm4", "ymm5", "ymm6", "ymm7", "cc", "memory"
        );
    }
}

// Fallback scalar loop for non-AVX x86 machines
void power_hog_scalar_x86(void) {
    volatile double a = 1.1, b = 2.2;
    while (1) {
        if (stop_hog) {
            break;
        }
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
    uint32_t loops = 0;
    while (1) {
        if (stop_hog) {
            break;
        }
        __asm__ volatile (
            "mov %[loops], #1000\n\t"
            ".p2align 4\n"
            "1:\n\t"
            "fmla z0.d, p0/m, z0.d, z0.d\n\t"
            "fmla z1.d, p0/m, z1.d, z1.d\n\t"
            "fmla z2.d, p0/m, z2.d, z2.d\n\t"
            "fmla z3.d, p0/m, z3.d, z3.d\n\t"
            "subs %[loops], %[loops], #1\n\t"
            "b.ne 1b\n\t"                    // Branch back to '1' if loops != 0

            : [loops] "=r" (loops)
            : "[loops]" (0)
            : "p0", "z0", "z1", "z2", "z3", "cc", "memory" // Clobber the SVE registers used
        );
    }
}

void power_hog_neon(void) {
    uint32_t loops = 0;
    while (1) {
        if (stop_hog) {
            break;
        }
        __asm__ volatile (
            "mov %[loops], #1000\n\t"
            ".p2align 4\n"
            "1:\n\t"
            "fmla v0.2d, v0.2d, v0.2d\n\t"
            "fmla v1.2d, v1.2d, v1.2d\n\t"
            "fmla v2.2d, v2.2d, v2.2d\n\t"
            "fmla v3.2d, v3.2d, v3.2d\n\t"
            "fmla v4.2d, v4.2d, v4.2d\n\t"
            "fmla v5.2d, v5.2d, v5.2d\n\t"
            "fmla v0.2d, v0.2d, v0.2d\n\t"
            "fmla v1.2d, v1.2d, v1.2d\n\t"
            "fmla v2.2d, v2.2d, v2.2d\n\t"
            "fmla v3.2d, v3.2d, v3.2d\n\t"
            "subs %[loops], %[loops], #1\n\t"
            "b.ne 1b\n\t"                    // Branch back to '1' if loops != 0

            : [loops] "=r" (loops)
            : "[loops]" (0)
            : "v0", "v1", "v2", "v3", "v4", "v5", "cc", "memory" // Clobber the NEON registers used
        );
    }
}
#endif

void power_hog_ansi_c(void) {
    // 8 independent double-precision floats.
    // Use 8 to ensure we flood all available FPU pipelines on the core.
    double a0 = 0.1, a1 = 0.2, a2 = 0.3, a3 = 0.4;
    double a4 = 0.5, a5 = 0.6, a6 = 0.7, a7 = 0.8;
    // A volatile sink. Writing to a volatile variable forces the compiler
    // to actually execute the math, preventing Dead-Code Elimination (DCE).
    volatile double power_sink = 0.0;
    while (1) {
        if (stop_hog) {
            break;
        }
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

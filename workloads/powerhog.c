#include <stdint.h>
#include <pthread.h>
#include "../include/workloads.h"

// A Power Hog: Generates massive heat and power draw to force throttling
void power_hog() {
#if defined(__x86_64__)
    // We use vfmadd231pd (Fused Multiply-Add for Packed Double-Precision).
    // This lights up the heavy vector ALUs on x86 CPU.
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
#elif defined(__aarch64__)
    // We use the fmla instruction (Fused Multiply-Add) on 128-bit 'q' registers.
    // 'fmla v0.2d, v0.2d, v0.2d' multiplies two double-precision floats in v0 
    // and adds the result to v0, looping endlessly.
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
#endif
}

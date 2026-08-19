#include <stdint.h>
#include "../include/workloads.h"

// Heavy workload to force CPU core to hit SISD max turbo boost
void sisd_workload(uint64_t iterations) {
#if defined(__x86_64__)
    // Implementation for x86_64 Architecture
    uint64_t d1 = 0, d2 = 1, d3 = 2, d4 = 3;
    for (uint64_t i = 0; i < iterations / 32; ++i) {
        __asm__ __volatile__ (
            ".rept 32\n\t"
            "add %1, %0; add %3, %2\n\t"
            "add %0, %1; add %2, %3\n\t"
            "xor %1, %0; xor %3, %2\n\t"
            ".endr\n\t"
            : "+r" (d1), "+r" (d2), "+r" (d3), "+r" (d4)
        );
    }

#elif defined(__aarch64__)
    // Implementation for ARM64 (AArch64) Architecture
    // Uses standard 64-bit general-purpose registers (x0-x3) to prevent pipeline stalls
    uint64_t d1 = 0, d2 = 1, d3 = 2, d4 = 3;
    for (uint64_t i = 0; i < iterations / 32; ++i) {
        __asm__ __volatile__ (
            ".rept 32\n\t"
            "add %0, %0, %1\n\t"
            "add %2, %2, %3\n\t"
            "eor %1, %1, %0\n\t"
            "eor %3, %3, %2\n\t"
            ".endr\n\t"
            : "+r" (d1), "+r" (d2), "+r" (d3), "+r" (d4)
        );
    }

#else
    // Fallback for any other architecture (Standard optimized C if assembly is unknown)
    volatile uint64_t d1 = 0, d2 = 1, d3 = 2, d4 = 3;
    for (uint64_t i = 0; i < iterations; ++i) {
        d1 += d2;
        d3 += d4;
        d2 ^= d1;
        d4 ^= d3;
    }
#endif
}

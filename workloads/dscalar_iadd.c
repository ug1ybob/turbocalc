#include <stdint.h>
#include "../include/workloads.h"

#if !defined(__x86_64__) && !defined(__aarch64__)
static volatile uint64_t iadd_sink;
#endif

void deterministic_scalar_iadd_workload(uint64_t total_instructions) {
    total_instructions -= total_instructions % 100;
    if (total_instructions == 0) return;

#if defined(__x86_64__)

    __asm__ volatile (
        // 1. INIT: {xor eax, eax}
        "xor %%eax, %%eax\n\t"
        
        ".align 16\n"
        "1:\n\t"
        
        // 2. BODY: {add rax, rax} unrolled exactly 100 times
        "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t"
        "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t"
        "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t"
        "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t"
        "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t"
        "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t"
        "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t"
        "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t"
        "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t"
        "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t"
        
        "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t"
        "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t"
        "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t"
        "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t"
        "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t"
        "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t"
        "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t"
        "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t"
        "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t"
        "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t" "add %%rax, %%rax\n\t"

        "sub $100, %[instructions]\n\t"
        "jnz 1b\n\t"

        : [instructions] "+r" (total_instructions)
        : 
        : "rax", "cc"
    );

#elif defined(__aarch64__)
    // Exact ARM64 equivalent of xor eax,eax and add rax,rax
    __asm__ volatile (
        "mov x0, #0\n\t"
        
        ".p2align 4\n"
        "1:\n\t"
        
        "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t"
        "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t"
        "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t"
        "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t"
        "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t"
        "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t"
        "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t"
        "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t"
        "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t"
        "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t"

        "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t"
        "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t"
        "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t"
        "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t"
        "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t"
        "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t"
        "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t"
        "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t"
        "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t"
        "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t" "add x0, x0, x0\n\t"

        "subs %[instructions], %[instructions], #100\n\t"
        "b.ne 1b\n\t"

        : [instructions] "+r" (total_instructions)
        : 
        : "x0", "cc"
    );

#else
    // ANSI C cannot guarantee the generated instruction sequence. Keep the
    // result observable, but do not treat this fallback as cycle-accurate.
    uint64_t value = total_instructions | 1;
    uint64_t loops = total_instructions / 100;

#define IADD() value += value
#define IADD10() \
    IADD(); IADD(); IADD(); IADD(); IADD(); \
    IADD(); IADD(); IADD(); IADD(); IADD()

    while (loops-- != 0) {
        IADD10(); IADD10(); IADD10(); IADD10(); IADD10();
        IADD10(); IADD10(); IADD10(); IADD10(); IADD10();
    }

    iadd_sink = value;

#undef IADD10
#undef IADD
#endif
}

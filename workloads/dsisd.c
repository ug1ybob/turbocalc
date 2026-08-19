#include <stdint.h>
#include "../include/workloads.h"

// A cycle-deterministic SISD workload
void deterministic_sisd_workload(uint64_t total_instructions) {
    // We unroll the loop by 10 to hide the branch overhead (dec + jnz).
    // The OoO engine will execute the loop control in parallel,
    // leaving the add/xor chain as the sole timing bottleneck.
    uint64_t loops = total_instructions / 10;

    // val carries the dependency chain.
    // toggle_val is a constant used to mutate val.
    uint64_t val = 0;
    uint64_t toggle_val = 1;

#if defined(__x86_64__)
    __asm__ volatile (
        ".align 16\n"                   // Align loop target to 16-byte boundary for fetch efficiency
        "1:\n\t"

        // --- Dependency Chain Begins ---
        "add %[toggle], %[val]\n\t"     // val = 0 + 1 = 1      (1 cycle latency)
        "xor %[toggle], %[val]\n\t"     // val = 1 ^ 1 = 0      (1 cycle latency)
        "add %[toggle], %[val]\n\t"     // val = 0 + 1 = 1      (1 cycle latency)
        "xor %[toggle], %[val]\n\t"     // val = 1 ^ 1 = 0      (1 cycle latency)
        "add %[toggle], %[val]\n\t"     // val = 0 + 1 = 1      (1 cycle latency)
        "xor %[toggle], %[val]\n\t"     // val = 1 ^ 1 = 0      (1 cycle latency)
        "add %[toggle], %[val]\n\t"     // val = 0 + 1 = 1      (1 cycle latency)
        "xor %[toggle], %[val]\n\t"     // val = 1 ^ 1 = 0      (1 cycle latency)
        "add %[toggle], %[val]\n\t"     // val = 0 + 1 = 1      (1 cycle latency)
        "xor %[toggle], %[val]\n\t"     // val = 1 ^ 1 = 0      (1 cycle latency)
        // --- Dependency Chain Ends ---

        "dec %[loops]\n\t"              // Decrement loop counter
        "jnz 1b\n\t"                    // Jump back to '1' if loops != 0

        // Output operands: val and loops are read/write (+r)
        : [val] "+r" (val), [loops] "+r" (loops)

        // Input operands: toggle is read-only (r)
        : [toggle] "r" (toggle_val)

        // Clobbers: "cc" informs the compiler that the condition codes (flags) are modified
        : "cc"
    );
#elif defined(__aarch64__)
    __asm__ volatile (
        ".p2align 4\n"                  // Align loop to 16-byte boundary for optimal fetch pipeline
        "1:\n\t"

        // --- Dependency Chain Begins ---
        "add %[val], %[val], %[toggle]\n\t"  // val = val + toggle   (1 cycle latency)
        "eor %[val], %[val], %[toggle]\n\t"  // val = val ^ toggle   (1 cycle latency)
        "add %[val], %[val], %[toggle]\n\t"  // val = val + toggle   (1 cycle latency)
        "eor %[val], %[val], %[toggle]\n\t"  // val = val ^ toggle   (1 cycle latency)
        "add %[val], %[val], %[toggle]\n\t"  // val = val + toggle   (1 cycle latency)
        "eor %[val], %[val], %[toggle]\n\t"  // val = val ^ toggle   (1 cycle latency)
        "add %[val], %[val], %[toggle]\n\t"  // val = val + toggle   (1 cycle latency)
        "eor %[val], %[val], %[toggle]\n\t"  // val = val ^ toggle   (1 cycle latency)
        "add %[val], %[val], %[toggle]\n\t"  // val = val + toggle   (1 cycle latency)
        "eor %[val], %[val], %[toggle]\n\t"  // val = val ^ toggle   (1 cycle latency)
        // --- Dependency Chain Ends ---

        // Loop control on ARM64:
        // 'subs' decrements the loop counter and updates condition flags
        "subs %[loops], %[loops], #1\n\t"
        // 'b.ne' branches back to local label '1' if the result is not equal to 0
        "b.ne 1b\n\t"

        // Output operands: val and loops are read/write (+r)
        : [val] "+r" (val), [loops] "+r" (loops)

        // Input operands: toggle is read-only (r)
        : [toggle] "r" (toggle_val)

        // Clobbers: "cc" (Condition Codes flag register is modified by 'subs')
        : "cc"
    );
#endif
}

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/perf_event.h>
#include <sys/syscall.h>
#include "../include/types.h"
#include "../include/os.h"
#include "../include/workloads.h"

// System call wrapper for perf_event_open
long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
                            int cpu, int group_fd, unsigned long flags) {
    return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}

// function to read x86 TSC or Arm64 CNTPCT_EL0
uint64_t get_cycles(void) {
#if defined(__x86_64__)
    uint32_t lo, hi;
    // rdtscp forces serialization, ensuring all previous instructions have executed
    __asm__ __volatile__ ("rdtscp" : "=a" (lo), "=d" (hi) :: "%rcx");
    return ((uint64_t)hi << 32) | lo;
#elif defined(__aarch64__)
    uint64_t val;
    // "isb" (Instruction Synchronization Barrier) forces instruction-serialization, 
    //  matching the out-of-order execution prevention behavior of Intel's "rdtscp".
    // "mrs" reads the physical counter system register into our 64-bit variable.
    __asm__ __volatile__ (
        "isb\n\t"
        "mrs %[val], cntvct_el0\n\t"
        : [val] "=r" (val)
        :
        : "memory"
    );
    return val;
#endif
}

// Verifies system permissions before running the main logic
int check_perf_permissions() {
    int paranoid_level = 100;
    FILE *fp = fopen("/proc/sys/kernel/perf_event_paranoid", "r");
    if (!fp) {
        fprintf(stderr, "[!] Could not read perf_event_paranoid level. Proceeding with caution...\n");
        return 0;
    }

    if (fscanf(fp, "%d", &paranoid_level) == 1) {
        fclose(fp);
        if (verbose) { printf("[+] System perf_event_paranoid level is: %d\n", paranoid_level); }

        if (paranoid_level > 1 && getuid() != 0) {
            fprintf(stderr, "[X] ERROR: Your current settings will block this program.\n"
                            "    To fix this, run: sudo sysctl -w kernel.perf_event_paranoid=0\n"
                            "    Or run this specific program with sudo.\n\n");
            return -1;
        }
    } else {
        fclose(fp);
    }
    return 0;
}

// Helper to pull the reference frequency from the kernel filesystem
double get_sysfs_cpu_freq_ghz(int cpu_id) {
    char path[128];
    snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", cpu_id);

    FILE *fp = fopen(path, "r");
    if (!fp) return -1.0;

    unsigned long long freq_khz = 0;
    if (fscanf(fp, "%llu", &freq_khz) != 1) {
        fclose(fp);
        return -1.0;
    }
    fclose(fp);
    return (double)freq_khz / 1e6; // Convert kHz to GHz
}

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <sched.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>

#ifndef __bool_true_false_are_defined
    #define bool int
    #define true 1
    #define false 0
    #define __bool_true_false_are_defined 1
#endif

bool verbose = false;

// Workload function pointer type
typedef void (*WorkloadPtr)(uint64_t iterations);

// Structure to map a string to a function pointer
typedef struct {
    const char *name;
    WorkloadPtr func;
    bool deterministic;
} Workload;

// 
void deterministic_sisd_workload(uint64_t total_instructions) {
    // We unroll the loop by 10 to hide the branch overhead (dec + jnz).
    // The OoO engine will execute the loop control in parallel, 
    // leaving the add/xor chain as the sole timing bottleneck.
    uint64_t loops = total_instructions / 10;
    
    // val carries the dependency chain. 
    // toggle_val is a constant used to mutate val.
    uint64_t val = 0;
    uint64_t toggle_val = 1;

    // Inline assembly block
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
}

// Heavy workload to force CPU core to hit SISD max turbo boost
static inline void sisd_workload(uint64_t iterations) {
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

// Workload function lookup table
Workload workloads[] = {
    {"sisd", sisd_workload, false},
    {"d_sisd", deterministic_sisd_workload, true},
    {NULL, NULL, false}
};

static const char *formats[] = { "csv", "txt", NULL };

static char *csv_fmt = "#MAX_SINGLE_GHZ,#MAX_MULTI_GHZ,#AVG_MULTI_GHZ,#MULTI_DROP_GHZ,#MAX_SCT_CORES\n"
                       "%s,%s,%s,%s,%s\n";

static char *txt_fmt = "Peak single-core turbo (CPU 0)  : %s GHz\n"
                       "Peak multi-core turbo (highest) : %s GHz\n"
                       "Average multi-core frequency    : %s GHz\n"
                       "Multi-core thermal/power drop   : %s GHz\n"
                       "Max parallel s-c turbo cores    : %s\n";

// Structure passed to each worker thread
typedef struct {
    int      cpu_id;
    uint64_t total_iterations;
    uint64_t warmup_iterations;
    double   calculated_ghz;
    double   sysfs_ghz;
//    void     (*load_function)(uint64_t);
    char     *load_function;
} thread_data_t;

// Check if provided output format is allowed
bool is_valid_format(const char *format) {
    int i = 0;
    while (formats[i] != NULL) {
        if (strcmp(formats[i], format) == 0) {
            return true;
        }
        i++;
    }
    return false;
}

// List workloads
void list_workloads() {
    int i = 0;
    while (workloads[i].name != NULL) {
        printf("%s\n", workloads[i].name);
        i++;
    }
    return;
}

// Check if provided workload name is allowed
bool is_valid_workload(const char *fname) {
    int i = 0;
    while (workloads[i].name != NULL) {
        if (strcmp(workloads[i].name, fname) == 0) {
            return true;
        }
        i++;
    }
    return false;
}

// Check if provided workload is deterministic
bool is_deterministic_workload(const char *fname) {
    int i = 0;
    while (workloads[i].name != NULL) {
        if (strcmp(workloads[i].name, fname) == 0) {
            return workloads[i].deterministic;
        }
        i++;
    }
    return false;
}

// Call workload function by name string
void run_workload(const char *fname, uint64_t iterations) {
    int i = 0;
    while (workloads[i].name != NULL) {
        if (strcmp(workloads[i].name, fname) == 0) {
            workloads[i].func(iterations);
            return;
        }
        i++;
    }
    printf("Workload '%s' not found.\n", fname);
}

// System call wrapper for perf_event_open
static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
                            int cpu, int group_fd, unsigned long flags) {
    return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}

// Static inline function to read the x86 Time-Stamp Counter (TSC)
static inline uint64_t get_cycles(void) {
    uint32_t lo, hi;
    // rdtscp forces serialization, ensuring all previous instructions have executed
    __asm__ __volatile__ ("rdtscp" : "=a" (lo), "=d" (hi) :: "%rcx");
    return ((uint64_t)hi << 32) | lo;
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

// Compensate for the physical BCLK spread-spectrum drop
// Most modern server motherboards under stress fluctuate around 99.58 MHz instead of 100.00 MHz
double apply_bclk_compensation(thread_data_t data) {
    if (data.sysfs_ghz <= 0.0) {
        return data.calculated_ghz;
    }

    double bclk_variance = data.sysfs_ghz / data.calculated_ghz;

    // If the difference is minor (within a realistic 2% margin), calibrate it out
    if (bclk_variance >= 1.002 && bclk_variance <= 1.005) {
        constexpr double bclk_correction_factor = 100.0 / 99.58;
        return data.calculated_ghz * bclk_correction_factor;
    }

    return data.calculated_ghz;
}

void print_usage() {
    printf("Usage: turbocalc [Options]\n"
           "Options:\n"
           "  -c, --compensate       Compensate result for the physical BCLK spread-spectrum drop\n"
           "  -f, --format <csv|txt> Output format (default: txt)\n"
           "  -i, --iterations <M>   Millions of iterations per thread (default: 200)\n"
           "  -l, --list_workloads   List available workload types\n"
           "  -r, --runs <count>     Number of test runs (default: 5)\n"
           "  -m, --max_tcores       Find the max count of single-core turbo capable cores (basic)\n"
           "  -M, --max_tcores_full  Find the max count of single-core turbo capable cores (thorough)\n"
           "  -v, --verbose          Display more details (only works for txt format)\n"
           "  -w, --workload         Workload type (default: sisd)\n"
           "  -h, --help             Display this help message\n"
	   );
}

// Thread function executed on each CPU core simultaineously
void* thread_benchmark(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;

    // Hard pin the thread to its designated CPU core
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(data->cpu_id, &cpuset);
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0) {
        pthread_exit(NULL);
    }

    // Configure perf_event for this specific thread and core
    struct perf_event_attr pe;
    memset(&pe, 0, sizeof(struct perf_event_attr));
    pe.type           = PERF_TYPE_HARDWARE;
    pe.size           = sizeof(struct perf_event_attr);
    pe.config         = PERF_COUNT_HW_CPU_CYCLES;
    pe.disabled       = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv     = 1;

#if defined(__x86_64__)
    bool is_x86 = true;
#else
    bool is_x86 = false;
#endif

    bool has_pmu = true;
    int fd = perf_event_open(&pe, 0, data->cpu_id, -1, 0);
    if (fd == -1) {
        if (is_x86) {
            has_pmu = false;
        } else {
            pthread_exit(NULL);
        }
    }

    run_workload(data->load_function, data->warmup_iterations);

    struct   timespec start_time, end_time;
    uint64_t start_cycles, end_cycles, total_cycles;

    if (has_pmu) {
        ioctl(fd, PERF_EVENT_IOC_RESET,   0);
        ioctl(fd, PERF_EVENT_IOC_ENABLE,  0);
    } else {
        start_cycles = get_cycles();
    }

    clock_gettime(CLOCK_MONOTONIC, &start_time);
    run_workload(data->load_function, data->total_iterations);
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    double elapsed_seconds = (end_time.tv_sec - start_time.tv_sec) +
                             (end_time.tv_nsec - start_time.tv_nsec) / 1e9;

    if (has_pmu) {
        ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
        if (read(fd, &total_cycles, sizeof(uint64_t)) == -1) {
            close(fd);
            pthread_exit(NULL);
        }
        close(fd);
    } else {
        end_cycles = get_cycles();
        total_cycles = end_cycles - start_cycles;
    }

    if (is_deterministic_workload(data->load_function)) {
        data->calculated_ghz = ((double)data->total_iterations / elapsed_seconds) / 1e9;
    } else {
        data->calculated_ghz = ((double)total_cycles / elapsed_seconds) / 1e9;
    }
    data->sysfs_ghz = get_sysfs_cpu_freq_ghz(data->cpu_id);

    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    uint64_t iterations_input       = 200;     // Default: 200M iterations per thread
    int      num_runs               = 5;       // Default: 5 runs
    char    *out_format             = txt_fmt; // Default: text format
    bool     compensate             = false;   // Default: do not apply BCLK spread-spectrum drop compensation
    bool     max_tcores             = false;   // Default: skip basic test for max parallel sc turbo cores
    bool     max_tcores_full        = false;   // Default: skip thorough test for max parallel sc turbo cores
    char    *workload               = "sisd";

    double   max_multi_ghz          = 0.0;
    double   max_single_ghz         = 0.0;
    int      max_simultaneous_cores = 1;

    static struct option const long_options[] = {
        {"compensate",     no_argument,       NULL, 'c'},
        {"format",         required_argument, NULL, 'f'},
        {"iterations",     required_argument, NULL, 'i'},
        {"list_workloads", no_argument,       NULL, 'l'},
        {"max_tcores",     no_argument,       NULL, 'm'},
        {"max_tcores_f",   no_argument,       NULL, 'M'},
        {"runs",           required_argument, NULL, 'r'},
        {"verbose",        no_argument,       NULL, 'v'},
        {"workload",       required_argument, NULL, 'w'},
        {"help",           no_argument,       NULL, 'h'},
        {NULL,             0,                 NULL, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "f:i:r:w:chlmMv", long_options, NULL)) != -1) {
        switch (opt) {
            case 'c': {
                compensate = true;
                break;
            }
            case 'f': {
                if (!is_valid_format(optarg)) {
                    fprintf(stderr, "Error: Invalid value '%s' for format.\n"
                                    "Allowed values are: csv, txt\n", optarg);
                    return 1;
                }
                if (strcmp(optarg, "csv") == 0) {
                    out_format = csv_fmt;
		    verbose = false;
                }
                break;
            }
            case 'i': {
                long long val = strtoll(optarg, NULL, 10);
                if (val <= 0) {
                    fprintf(stderr, "[X] ERROR: Iterations must be a positive integer.\n");
                    return 1;
                }
                iterations_input = (uint64_t)val;
                break;
            }
            case 'l': {
                list_workloads();
                return 0;
            }
            case 'r': {
                int val = atoi(optarg);
                if (val <= 0) {
                    fprintf(stderr, "[X] ERROR: Runs must be a positive integer.\n");
                    return 1;
                }
                num_runs = val;
                break;
            }
            case 'm': {
                max_tcores = true;
                break;
            }
            case 'M': {
                max_tcores = true;
                max_tcores_full = true;
                break;
            }
            case 'v': {
                if (strcmp(out_format, txt_fmt) == 0) {
                    verbose = true;
                }
                break;
            }
            case 'w': {
                if (!is_valid_workload(optarg)) {
                    fprintf(stderr, "Error: Invalid value '%s' for workload.\n", optarg);
                    return 1;
                }
                workload = optarg;
                break;
            }
            case 'h':
            default:
                print_usage();
                return 0;
        }
    }

    uint64_t total_iterations  = iterations_input * 1000000ULL;
    uint64_t warmup_iterations = total_iterations / 5;

    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (verbose) { printf("[+] Detected logical cores: %d\n", num_cores); }

    if (check_perf_permissions() < 0) {
        return 1;
    }

    thread_data_t  single_data;
    thread_data_t* t_data  = malloc(num_cores * sizeof(thread_data_t));
    pthread_t*     threads = malloc(num_cores * sizeof(pthread_t));

    // Isolated Single-Core Turbo Test
    // For now assume CPU 0 is always a P-core
    if (verbose) { printf("\n=== Testing single-core turbo (isolated CPU 0) (%d runs) ===\n", num_runs); }

    for (int r = 1; r <= num_runs; r++) {
        single_data.cpu_id = 0;
        single_data.total_iterations = total_iterations;
	single_data.warmup_iterations = warmup_iterations;
       	single_data.calculated_ghz = 0.0;
       	single_data.sysfs_ghz = 0.0;
        single_data.load_function = workload;
        pthread_t single_thread;
        if (pthread_create(&single_thread, NULL, thread_benchmark, &single_data) != 0) {
            fprintf(stderr, "[X] Single-core thread creation failed\n");
            return 1;
        }
        pthread_join(single_thread, NULL);
        if (compensate) { single_data.calculated_ghz = apply_bclk_compensation(single_data); }
        if (verbose) { printf("  -> Single-Core run result: %.3f GHz\n", single_data.calculated_ghz); }
        if (single_data.calculated_ghz > max_single_ghz) {
            max_single_ghz = single_data.calculated_ghz;
        }
    }

    // CPU cool down before next stage
    if (verbose) { printf("[+] Cooling down for 1 second...\n"); }
    sleep(1);

    // Max single-core turbo simultaneous cores test loop
    if (max_tcores) {
        if (verbose) { printf("\n=== Testing max number of simultaneous single-core turbo capable cores ===\n"); }
        for (int active_count = 1; active_count <= num_cores; active_count++) {
            for (int i = 0; i < active_count; i++) {
                t_data[i].cpu_id = i;
                t_data[i].total_iterations = total_iterations;
                t_data[i].warmup_iterations = warmup_iterations;
                t_data[i].calculated_ghz = 0.0;
                t_data[i].sysfs_ghz = 0.0;
                t_data[i].load_function = workload;
                pthread_create(&threads[i], NULL, thread_benchmark, &t_data[i]);
            }

            double sum_active = 0.0;
            for (int i = 0; i < active_count; i++) {
                pthread_join(threads[i], NULL);
                if (compensate) { t_data[i].calculated_ghz = apply_bclk_compensation(t_data[i]); }
                sum_active += t_data[i].calculated_ghz;
            }
            double avg_active = sum_active / active_count;

            // Frequency drop tolerance to count as single-core turbo - 0.5%
            int matches_single = (avg_active >= (max_single_ghz * 0.995));
            if (matches_single) {
                max_simultaneous_cores++;
            } else if (!max_tcores_full) {
                break;
            }

            if (verbose) {
                printf("  -> CPU %2d      |       %.3f GHz    | %s\n", 
                    active_count, avg_active, matches_single ? "REACHES MAX TURBO" : "THROTTLED / DROPPED");
            }

            usleep(200000); // CPU cool down between steps
        }
    }

    // Parallel multi-core turbo test loop
    if (verbose) {
        printf("\n=== Stage 2: Testing multi-core turbo (%d runs) ===\n"
               "[+] Launching parallel workload (%luM iterations per core)...\n", num_runs, iterations_input);
    }

    for (int r = 1; r <= num_runs; r++) {
        for (int i = 0; i < num_cores; i++) {
            t_data[i].cpu_id = i;
            t_data[i].total_iterations = total_iterations;
            t_data[i].warmup_iterations = warmup_iterations;
            t_data[i].calculated_ghz = 0.0;
            t_data[i].sysfs_ghz = 0.0;
            t_data[i].load_function = workload;
            if (pthread_create(&threads[i], NULL, thread_benchmark, &t_data[i]) != 0) {
                fprintf(stderr, "[X] Thread creation failed for core %d\n", i);
                free(threads);
                free(t_data);
                return 1;
            }
        }

        double run_sum = 0.0;
        for (int i = 0; i < num_cores; i++) {
            pthread_join(threads[i], NULL);
            if (compensate) { t_data[i].calculated_ghz = apply_bclk_compensation(t_data[i]); }
            run_sum += t_data[i].calculated_ghz;
            if (t_data[i].calculated_ghz > max_multi_ghz) {
                max_multi_ghz = t_data[i].calculated_ghz;
            }
        }
        double run_avg = run_sum / num_cores;
        if (verbose) { printf("  -> Multi-Core run #%d average: %.3f GHz\n", r, run_avg); }
    }

    // Print out the report
    double sum_ghz = 0.0;
    int    successful_measures = 0;
    char   max_single[6]       = "[N/A]";
    char   max_multi[6]        = "[N/A]";
    char   avg_multi[6]        = "[N/A]";
    char   drop_multi[6]       = "[N/A]";
    char   max_cores[11]       = "[N/A]";

    if (verbose) { printf("\n=== Final benchmark summary ===\n"
                          "\nParallel execution breakdown:\n"); }
    for (int i = 0; i < num_cores; i++) {
        if (t_data[i].calculated_ghz > 0.0) {
            if (verbose) { printf("  -> CPU %d: %.3f GHz\n", i, t_data[i].calculated_ghz); }
            sum_ghz += t_data[i].calculated_ghz;
            if (t_data[i].calculated_ghz > max_multi_ghz) {
                max_multi_ghz = t_data[i].calculated_ghz;
            }
            successful_measures++;
        } else {
            if (verbose) { printf("  -> CPU %d: [ERROR]\n", i); }
        }
    }


    if (single_data.calculated_ghz > 0.0) {
        snprintf(max_single, sizeof(max_single), "%.3f", max_single_ghz);
    }

    if (max_tcores) {
        snprintf(max_cores, sizeof(max_cores), "%d", max_simultaneous_cores);
    }

    if (successful_measures > 0) {
        snprintf(max_multi, sizeof(max_multi), "%.3f", max_multi_ghz);
        snprintf(avg_multi, sizeof(avg_multi), "%.3f", sum_ghz / successful_measures);

        if (single_data.calculated_ghz > 0.0) {
            double drop = single_data.calculated_ghz - (sum_ghz / successful_measures);
            snprintf(drop_multi, sizeof(drop_multi), "%.3f", drop);
        }
    } else {
        fprintf(stderr, "[X] Multi-core calculation failed due to active constraints.\n");
    }

    printf(out_format, max_single, max_multi, avg_multi, drop_multi, max_cores);

    free(threads);
    free(t_data);
    return 0;
}

#define _GNU_SOURCE
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include "../include/types.h"
#include "../include/benchmark.h"
#include "../include/os.h"
#include "../include/workloads.h"

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

// Thread function executed on each CPU core simultaneously
void* thread_benchmark(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;

    // Hard pin the thread to its designated CPU core
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(data->cpu_id, &cpuset);
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0) {
        pthread_exit(NULL);
    }
    pthread_barrier_wait(&start_barrier);

    // Configure perf_event for this specific thread and core
    struct perf_event_attr pe;
    memset(&pe, 0, sizeof(struct perf_event_attr));
    pe.type           = PERF_TYPE_HARDWARE;
    pe.size           = sizeof(struct perf_event_attr);
    pe.config         = PERF_COUNT_HW_CPU_CYCLES;
    pe.disabled       = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv     = 1;

    if (data->is_probe) {
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
    } else {
        power_hog();
    }

    pthread_exit(NULL);
}

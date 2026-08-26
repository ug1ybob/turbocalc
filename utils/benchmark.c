#define _GNU_SOURCE
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
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
    int affinity_error = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (affinity_error != 0) {
        fprintf(stderr, "[X] Could not pin worker to CPU %d: %s\n",
                data->cpu_id, strerror(affinity_error));
        atomic_store(&benchmark_start_failed, true);
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

    if (data->is_probe) {
        struct   timespec start_time, end_time;
        uint64_t start_cycles=0, end_cycles=0, total_cycles;
        bool has_pmu = true;
        int fd = -1;

        if (!is_deterministic_workload(data->load_function)) {
            fd = perf_event_open(&pe, 0, data->cpu_id, -1, 0);
            if (fd == -1) {
                has_pmu = false;
            }
        }

        if (!affinity_error) {
            run_workload(data->load_function, data->warmup_iterations);
        }

        // Start only after every worker has finished setup and workload-specific warmup.
        atomic_fetch_add(&ready_workers, 1);
        while (atomic_load(&ready_workers) < data->worker_count &&
               !atomic_load(&benchmark_start_failed)) {
        }
        if (atomic_load(&benchmark_start_failed)) {
            if (fd != -1) close(fd);
            pthread_exit(NULL);
        }

        if (!is_deterministic_workload(data->load_function)) {
            if (has_pmu) {
                ioctl(fd, PERF_EVENT_IOC_RESET,   0);
                ioctl(fd, PERF_EVENT_IOC_ENABLE,  0);
            } else {
                start_cycles = get_cycles();
            }
        }

        clock_gettime(CLOCK_MONOTONIC, &start_time);
        run_workload(data->load_function, data->total_iterations);
        clock_gettime(CLOCK_MONOTONIC, &end_time);

        double elapsed_seconds = (end_time.tv_sec - start_time.tv_sec) +
                                 (end_time.tv_nsec - start_time.tv_nsec) / 1e9;

        if (!is_deterministic_workload(data->load_function)) {
            if (has_pmu) {
                ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
                if (read(fd, &total_cycles, sizeof(uint64_t)) == -1) {
                    total_cycles = 0;
                }
                if (fd != -1) close(fd);
            } else {
                end_cycles = get_cycles();
                total_cycles = end_cycles - start_cycles;
            }
        }

        if (is_deterministic_workload(data->load_function)) {
            data->calculated_ghz = ((double)data->total_iterations / elapsed_seconds) / 1e9;
        } else {
            data->calculated_ghz = ((double)total_cycles / elapsed_seconds) / 1e9;
        }
        data->sysfs_ghz = get_sysfs_cpu_freq_ghz(data->cpu_id);

        atomic_fetch_add(&completed_probes, 1);
        while (atomic_load(&completed_probes) < data->probe_count) {
            run_workload(data->load_function, 100000);
        }
    } else if (power_hog != NULL) {
        atomic_fetch_add(&ready_workers, 1);
        while (atomic_load(&ready_workers) < data->worker_count &&
               !atomic_load(&benchmark_start_failed)) {
        }
        if (atomic_load(&benchmark_start_failed)) {
            pthread_exit(NULL);
        }
        power_hog();
    } else {
        atomic_fetch_add(&ready_workers, 1);
        while (atomic_load(&ready_workers) < data->worker_count &&
               !atomic_load(&benchmark_start_failed)) {
        }
    }

    pthread_exit(NULL);
}

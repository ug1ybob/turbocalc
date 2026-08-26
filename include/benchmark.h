#ifndef _BENCHMARK_H
#define _BENCHMARK_H

extern _Atomic int ready_workers;
extern _Atomic int completed_probes;
extern _Atomic bool benchmark_start_failed;

// Structure passed to each worker thread
typedef struct {
    int       cpu_id;
    uint64_t  total_iterations;
    uint64_t  warmup_iterations;
    double    calculated_ghz;
    double    sysfs_ghz;
    char      *load_function;
    bool      is_probe;
    int       worker_count;
    int       probe_count;
    pthread_t thread_handle;
} thread_data_t;

double apply_bclk_compensation(thread_data_t data);

void* thread_benchmark(void* arg);

#endif /* _BENCHMARK_H */

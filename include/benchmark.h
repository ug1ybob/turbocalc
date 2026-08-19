#ifndef _BENCHMARK_H
#define _BENCHMARK_H

extern pthread_barrier_t start_barrier;

// Structure passed to each worker thread
typedef struct {
    int       cpu_id;
    uint64_t  total_iterations;
    uint64_t  warmup_iterations;
    double    calculated_ghz;
    double    sysfs_ghz;
    char      *load_function;
    bool      is_probe;
    pthread_t thread_handle;
} thread_data_t;

double apply_bclk_compensation(thread_data_t data);

void* thread_benchmark(void* arg);

#endif /* _BENCHMARK_H */

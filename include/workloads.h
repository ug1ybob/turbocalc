#ifndef _WORKLOADS_H
#define _WORKLOADS_H

// Workload function pointer type
typedef void (*WorkloadPtr)(uint64_t iterations);

typedef struct {
    const char *name;
    WorkloadPtr func;
    const char *description;
    bool deterministic;
} Workload;

// Power stress function type
typedef void (*power_hog_func_t)(void);

extern power_hog_func_t power_hog;

// Various stress functions
void power_hog_avx512(void);

void power_hog_avx2(void);

void power_hog_scalar_x86(void);

void power_hog_sve(void);

void power_hog_neon(void);

void power_hog_ansi_c(void);

void resolve_power_hog(void);

// Workload-related functions
void list_workloads();

bool is_valid_workload(const char *wname);

bool is_deterministic_workload(const char *wname);

void run_workload(const char *wname, uint64_t iterations);

void deterministic_sisd_workload(uint64_t total_instructions);

void sisd_workload(uint64_t iterations);

#endif /* _WORKLOADS_H */

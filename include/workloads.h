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

void list_workloads();

bool is_valid_workload(const char *wname);

bool is_deterministic_workload(const char *wname);

void run_workload(const char *wname, uint64_t iterations);

void deterministic_sisd_workload(uint64_t total_instructions);

void sisd_workload(uint64_t iterations);

#endif /* _WORKLOADS_H */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../include/types.h"
#include "../include/workloads.h"

// Workload function lookup table
const Workload workloads[] = {
    {
        .name = "dscalar_iadd",
        .func = deterministic_scalar_iadd_workload,
        .description = "Deterministic scalar_iadd workload",
        .deterministic = true
    },
    {
        .name = "d_sisd",
        .func = deterministic_sisd_workload,
        .description = "Deterministic SISD workload",
        .deterministic = true
    },
    {
        .name = "sisd",
        .func = sisd_workload,
        .description = "SISD workload",
        .deterministic = false
    },
    {NULL, NULL, NULL, false}
};

// List workloads
void list_workloads() {
    int i = 0;
    while (workloads[i].name != NULL) {
        printf("%s - %s\n", workloads[i].name, workloads[i].description);
        i++;
    }
    return;
}

// Check if provided workload name is allowed
bool is_valid_workload(const char *wname) {
    int i = 0;
    while (workloads[i].name != NULL) {
        if (strcmp(workloads[i].name, wname) == 0) {
            return true;
        }
        i++;
    }
    return false;
}

// Check if provided workload is deterministic
bool is_deterministic_workload(const char *wname) {
    int i = 0;
    while (workloads[i].name != NULL) {
        if (strcmp(workloads[i].name, wname) == 0) {
            return workloads[i].deterministic;
        }
        i++;
    }
    return false;
}

// Call workload function by name string
void run_workload(const char *wname, uint64_t iterations) {
    int i = 0;
    while (workloads[i].name != NULL) {
        if (strcmp(workloads[i].name, wname) == 0) {
            workloads[i].func(iterations);
            return;
        }
        i++;
    }
    printf("Workload '%s' not found.\n", wname);
}

#ifndef _TYPES_H
#define _TYPES_H

#ifndef __bool_true_false_are_defined
    #define bool int
    #define true 1
    #define false 0
    #define __bool_true_false_are_defined 1
#endif

extern bool verbose;

// Structure passed to each worker thread
typedef struct {
    int      cpu_id;
    uint64_t total_iterations;
    uint64_t warmup_iterations;
    double   calculated_ghz;
    double   sysfs_ghz;
    char     *load_function;
} thread_data_t;

#endif /* _TYPES_H */

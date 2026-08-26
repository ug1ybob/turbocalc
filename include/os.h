#ifndef _OS_H
#define _OS_H

long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
                            int cpu, int group_fd, unsigned long flags);

uint64_t get_cycles(void);

int check_perf_permissions();

double get_sysfs_cpu_freq_ghz(int cpu_id);

int get_cpu_ids(int **cpu_ids, bool allow_smt);

#endif /* _OS_H */

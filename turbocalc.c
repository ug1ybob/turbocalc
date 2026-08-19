#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <linux/perf_event.h>
#include "include/types.h"
#include "include/benchmark.h"
#include "include/os.h"
#include "include/output.h"
#include "include/workloads.h"

bool verbose = false;
pthread_barrier_t start_barrier;

int main(int argc, char *argv[]) {

    uint64_t iterations_input       = 200;    // Default: 200M iterations per thread
    int      num_runs               = 5;      // Default: 5 runs
    char    *format                 = "txt";  // Default: text format
    bool     compensate             = false;  // Default: do not apply BCLK spread-spectrum drop compensation
    bool     max_tcores             = false;  // Default: skip basic test for max parallel sc turbo cores
    bool     max_tcores_full        = false;  // Default: skip thorough test for max parallel sc turbo cores
    bool     stress_mcore           = false;  // Default: do not attempt to reach throttling via SIMD stress
    char    *workload               = "sisd"; // Default: use SISD workload

    double   max_multi_ghz          = 0.0;
    double   max_single_ghz         = 0.0;
    int      max_simultaneous_cores = 1;

    static const char *usage_msg    = "Usage: turbocalc [Options]\n"
                                      "Options:\n"
                                      "  -c, --compensate       Compensate result for the physical BCLK spread-spectrum drop\n"
                                      "  -f, --format <csv|txt> Output format (default: txt)\n"
                                      "  -i, --iterations <M>   Millions of iterations per thread (default: 200)\n"
                                      "  -l, --list_workloads   List available workload types\n"
                                      "  -r, --runs <count>     Number of test runs (default: 5)\n"
                                      "  -m, --max_tcores       Find the max count of single-core turbo capable cores (basic)\n"
                                      "  -M, --max_tcores_full  Find the max count of single-core turbo capable cores (thorough)\n"
                                      "  -s, --stress_mcore     Apply a SIMD stress to certain multi-core scenarios to induce throttling\n"
                                      "  -v, --verbose          Display more details (only works for txt format)\n"
                                      "  -w, --workload         Workload type (default: sisd)\n"
                                      "  -h, --help             Display this help message\n";

    static struct option const long_options[] = {
        {"compensate",     no_argument,       NULL, 'c'},
        {"format",         required_argument, NULL, 'f'},
        {"iterations",     required_argument, NULL, 'i'},
        {"list_workloads", no_argument,       NULL, 'l'},
        {"max_tcores",     no_argument,       NULL, 'm'},
        {"max_tcores_f",   no_argument,       NULL, 'M'},
        {"runs",           required_argument, NULL, 'r'},
        {"stress_mcore",   no_argument,       NULL, 's'},
        {"verbose",        no_argument,       NULL, 'v'},
        {"workload",       required_argument, NULL, 'w'},
        {"help",           no_argument,       NULL, 'h'},
        {NULL,             0,                 NULL, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "f:i:r:w:chlmMsv", long_options, NULL)) != -1) {
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
		    verbose = false;
                }
                format = optarg;
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
            case 's': {
                stress_mcore = true;
                break;
            }
            case 'v': {
                if (strcmp(format, "txt") == 0) {
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
                printf("%s", usage_msg);
                return 0;
        }
    }

    double sum_ghz = 0.0;
    int    successful_measures = 0;
    char   max_single[6]       = "[N/A]";
    char   max_multi[6]        = "[N/A]";
    char   avg_multi[6]        = "[N/A]";
    char   drop_multi[6]       = "[N/A]";
    char   max_cores[11]       = "[N/A]";

    char *out_format = get_format(format);

    uint64_t total_iterations  = iterations_input * 1000000ULL;
    uint64_t warmup_iterations = total_iterations / 5;

    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);

    if (stress_mcore) { resolve_power_hog(); }
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
        single_data = (thread_data_t){
            .cpu_id = 0,
            .total_iterations = total_iterations,
	    .warmup_iterations = warmup_iterations,
       	    .calculated_ghz = 0.0,
       	    .sysfs_ghz = 0.0,
            .load_function = workload,
	    .is_probe = true
        };
        pthread_t single_thread;
        pthread_barrier_init(&start_barrier, NULL, 1);
        if (pthread_create(&single_thread, NULL, thread_benchmark, &single_data) != 0) {
            fprintf(stderr, "[X] Single-core thread creation failed\n");
            return 1;
        }
        pthread_join(single_thread, NULL);
        pthread_barrier_destroy(&start_barrier);
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
            pthread_barrier_init(&start_barrier, NULL, active_count);
            for (int i = 0; i < active_count; i++) {
                t_data[i] = (thread_data_t){
                    .cpu_id = i,
                    .total_iterations = total_iterations,
                    .warmup_iterations = warmup_iterations,
                    .calculated_ghz = 0.0,
                    .sysfs_ghz = 0.0,
                    .load_function = workload,
	            .is_probe = true
                };
                pthread_create(&threads[i], NULL, thread_benchmark, &t_data[i]);
            }

            double sum_active = 0.0;
            for (int i = 0; i < active_count; i++) {
                pthread_join(threads[i], NULL);
                if (compensate) { t_data[i].calculated_ghz = apply_bclk_compensation(t_data[i]); }
                sum_active += t_data[i].calculated_ghz;
            }
            pthread_barrier_destroy(&start_barrier);

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
        pthread_barrier_init(&start_barrier, NULL, num_cores);
        for (int i = 0; i < num_cores; i++) {
            t_data[i] = (thread_data_t){
                .cpu_id = i,
                .total_iterations = total_iterations,
                .warmup_iterations = warmup_iterations,
                .calculated_ghz = 0.0,
                .sysfs_ghz = 0.0,
                .load_function = workload,
		// only measure the first core, apply a heavy load to the rest
	        .is_probe = (i != 0 && stress_mcore) ? false : true
            };
            if (pthread_create(&threads[i], NULL, thread_benchmark, &t_data[i]) != 0) {
                fprintf(stderr, "[X] Thread creation failed for core %d\n", i);
                free(threads);
                free(t_data);
                return 1;
            }
            t_data[i].thread_handle = threads[i];
        }

        double run_sum = 0.0;
	int run_successful_measures = 0;
        for (int i = 0; i < num_cores; i++) {
            if (!t_data[i].is_probe) {
                pthread_cancel(threads[i]);
            }
            pthread_join(threads[i], NULL);
            if (t_data[i].calculated_ghz > 0.0 && t_data[i].is_probe) {
                if (compensate) { t_data[i].calculated_ghz = apply_bclk_compensation(t_data[i]); }
                run_sum += t_data[i].calculated_ghz;
                if (t_data[i].calculated_ghz > max_multi_ghz) {
                    max_multi_ghz = t_data[i].calculated_ghz;
                }
                run_successful_measures++;
            }
        }
        pthread_barrier_destroy(&start_barrier);
        double run_avg = run_sum / run_successful_measures;
	sum_ghz += run_sum;
	successful_measures += run_successful_measures;
        if (verbose) { printf("  -> Multi-Core run #%d average: %.3f GHz\n", r, run_avg); }
    }

    // Print out the report
    if (verbose) { printf("\n=== Final benchmark summary ===\n"); }

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

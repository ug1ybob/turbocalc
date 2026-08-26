#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../include/types.h"
#include "../include/output.h"
#include "../include/workloads.h"

const char *const CSV_FMT = "#MAX_SINGLE_GHZ,#MAX_MULTI_GHZ,#AVG_MULTI_GHZ,#MULTI_DROP_GHZ,#MAX_SCT_CORES\n"
                            "%s,%s,%s,%s,%s\n";

const char *const TXT_FMT = "Peak single-core turbo          : %s GHz\n"
                            "Peak multi-core turbo (highest) : %s GHz\n"
                            "Average multi-core frequency    : %s GHz\n"
                            "Multi-core thermal/power drop   : %s GHz\n"
                            "Max parallel s-c turbo cores    : %s\n";

// Output format lookup table
Format valid_formats[] = {
    {
        .name = "csv",
        .template = CSV_FMT
    },
    {
        .name = "txt",
        .template = TXT_FMT
    },
    {NULL, NULL}
};

// Check if provided output format is allowed
bool is_valid_format(const char *fname) {
    int i = 0;
    while (valid_formats[i].name != NULL) {
        if (strcmp(valid_formats[i].name, fname) == 0) {
            return true;
        }
        i++;
    }
    return false;
}

char* get_format(const char *fname) {
    int i = 0;
    while (valid_formats[i].name != NULL) {
        if (strcmp(valid_formats[i].name, fname) == 0) {
            return (char *)valid_formats[i].template;
        }
        i++;
    }
    fprintf(stderr, "Format '%s' not found.\n", fname);
    return "";
}

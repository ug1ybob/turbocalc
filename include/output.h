#ifndef _OUTPUT_H
#define _OUTPUT_H

typedef struct {
    const char *name;
    const char *template;
} Format;

bool is_valid_format(const char *fname);

char* get_format(const char *fname);

#endif /* _OUTPUT_H */

#include "text_io.h"

#include <inttypes.h>
#include <stdio.h>

bool text_file_read_int(const char *path, int *value)
{
    FILE *file = fopen(path, "r");
    int parsed;

    if (file == NULL) {
        return false;
    }

    parsed = fscanf(file, "%d", value);
    fclose(file);
    return parsed == 1;
}

bool text_file_read_u64(const char *path, uint64_t *value)
{
    FILE *file = fopen(path, "r");
    int parsed;

    if (file == NULL) {
        return false;
    }

    parsed = fscanf(file, "%" SCNu64, value);
    fclose(file);
    return parsed == 1;
}

bool text_file_read_float(const char *path, float *value)
{
    FILE *file = fopen(path, "r");
    int parsed;

    if (file == NULL) {
        return false;
    }

    parsed = fscanf(file, "%f", value);
    fclose(file);
    return parsed == 1;
}

bool text_file_read_first_int(const char *const *paths, size_t path_count,
                              int *value)
{
    size_t index;

    for (index = 0; index < path_count; ++index) {
        if (text_file_read_int(paths[index], value)) {
            return true;
        }
    }

    return false;
}

bool text_file_read_first_u64(const char *const *paths, size_t path_count,
                              uint64_t *value)
{
    size_t index;

    for (index = 0; index < path_count; ++index) {
        if (text_file_read_u64(paths[index], value)) {
            return true;
        }
    }

    return false;
}

bool text_file_read_first_float(const char *const *paths, size_t path_count,
                                float *value)
{
    size_t index;

    for (index = 0; index < path_count; ++index) {
        if (text_file_read_float(paths[index], value)) {
            return true;
        }
    }

    return false;
}

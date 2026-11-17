#ifndef TEGRASTATS_TEXT_IO_H
#define TEGRASTATS_TEXT_IO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool text_file_read_int(const char *path, int *value);
bool text_file_read_u64(const char *path, uint64_t *value);
bool text_file_read_float(const char *path, float *value);

bool text_file_read_first_int(const char *const *paths, size_t path_count,
                              int *value);
bool text_file_read_first_u64(const char *const *paths, size_t path_count,
                              uint64_t *value);
bool text_file_read_first_float(const char *const *paths, size_t path_count,
                                float *value);

#endif

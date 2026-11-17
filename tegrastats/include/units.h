#ifndef TEGRASTATS_UNITS_H
#define TEGRASTATS_UNITS_H

#include <stddef.h>
#include <stdint.h>

uint64_t units_bytes_to_mib(uint64_t bytes);
uint64_t units_bytes_to_kib(uint64_t bytes);
uint64_t units_hz_to_mhz(uint64_t hz);
uint64_t units_khz_to_mhz(uint64_t khz);
int units_format_bytes(uint64_t bytes, char *buffer, size_t buffer_size);

#endif

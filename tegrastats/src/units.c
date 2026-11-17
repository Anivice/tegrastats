#include "units.h"

#include <inttypes.h>
#include <stdio.h>

enum {
    BYTES_PER_KIB = 1024,
    BYTES_PER_MIB = 1024 * 1024,
    HZ_PER_MHZ = 1000000,
    KHZ_PER_MHZ = 1000
};

uint64_t units_bytes_to_mib(uint64_t bytes)
{
    return (bytes + BYTES_PER_MIB - 1U) / BYTES_PER_MIB;
}

uint64_t units_bytes_to_kib(uint64_t bytes)
{
    return (bytes + BYTES_PER_KIB - 1U) / BYTES_PER_KIB;
}

uint64_t units_hz_to_mhz(uint64_t hz)
{
    return hz / HZ_PER_MHZ;
}

uint64_t units_khz_to_mhz(uint64_t khz)
{
    return khz / KHZ_PER_MHZ;
}

int units_format_bytes(uint64_t bytes, char *buffer, size_t buffer_size)
{
    if (bytes < BYTES_PER_KIB) {
        return snprintf(buffer, buffer_size, "%" PRIu64 "B", bytes);
    }

    if (bytes < BYTES_PER_MIB) {
        return snprintf(buffer, buffer_size, "%" PRIu64 "kB",
                        units_bytes_to_kib(bytes));
    }

    return snprintf(buffer, buffer_size, "%" PRIu64 "MB",
                    units_bytes_to_mib(bytes));
}

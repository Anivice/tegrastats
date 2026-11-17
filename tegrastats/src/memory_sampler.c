#include "memory_sampler.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logger.h"
#include "text_io.h"

enum {
    BUDDY_ORDER_COUNT = 11,
    BUDDY_PAGE_SIZE = 4096
};

static const char *const CARVEOUT_TOTAL_PATH =
    "/sys/devices/platform/tegra-nvmap/misc/nvmap/"
    "heap-generic-0/total_size";
static const char *const CARVEOUT_FREE_PATH =
    "/sys/devices/platform/tegra-nvmap/misc/nvmap/"
    "heap-generic-0/free_size";
static const char *const CARVEOUT_LARGEST_PATH =
    "/sys/devices/platform/tegra-nvmap/misc/nvmap/"
    "heap-generic-0/free_max";
static const char *const IRAM_TOTAL_PATH =
    "/sys/devices/platform/tegra-nvmap/misc/nvmap/"
    "heap-iram/total_size";
static const char *const IRAM_FREE_PATH =
    "/sys/devices/platform/tegra-nvmap/misc/nvmap/"
    "heap-iram/free_size";
static const char *const IRAM_LARGEST_PATH =
    "/sys/devices/platform/tegra-nvmap/misc/nvmap/"
    "heap-iram/free_max";

typedef struct {
    bool debug;
} MemorySampler;

static uint64_t kib_to_bytes(uint64_t kib)
{
    return kib * 1024U;
}

static uint64_t subtract_saturated(uint64_t left, uint64_t right)
{
    return left > right ? left - right : 0;
}

static void read_meminfo(MemoryAreaStats *system_memory)
{
    static const char *const path = "/proc/meminfo";
    FILE *file = fopen(path, "r");
    char line[256];
    uint64_t total_kib = 0;
    uint64_t free_kib = 0;
    uint64_t buffers_kib = 0;
    uint64_t cached_kib = 0;
    bool has_total = false;
    bool has_free = false;
    bool has_buffers = false;
    bool has_cached = false;

    if (file == NULL) {
        return;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char key[64];
        uint64_t value_kib;

        if (sscanf(line, " %63[^:]: %" SCNu64, key, &value_kib) != 2) {
            continue;
        }

        if (strcmp(key, "MemTotal") == 0) {
            total_kib = value_kib;
            has_total = true;
        } else if (strcmp(key, "MemFree") == 0) {
            free_kib = value_kib;
            has_free = true;
        } else if (strcmp(key, "Buffers") == 0) {
            buffers_kib = value_kib;
            has_buffers = true;
        } else if (strcmp(key, "Cached") == 0) {
            cached_kib = value_kib;
            has_cached = true;
        }
    }
    fclose(file);

    if (!has_total || !has_free || !has_buffers || !has_cached) {
        return;
    }

    system_memory->total_bytes = kib_to_bytes(total_kib);
    system_memory->free_bytes = kib_to_bytes(free_kib);
    system_memory->used_bytes =
        subtract_saturated(system_memory->total_bytes,
                           system_memory->free_bytes +
                               kib_to_bytes(buffers_kib) +
                               kib_to_bytes(cached_kib));
    system_memory->available = true;
}

static void read_buddyinfo(MemoryAreaStats *system_memory)
{
    static const char *const path = "/proc/buddyinfo";
    FILE *file = fopen(path, "r");
    char line[256];
    uint64_t block_counts[BUDDY_ORDER_COUNT] = {0};
    bool found_zone = false;

    if (file == NULL) {
        return;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char *zone = strstr(line, "zone");
        const char *cursor;
        int offset = 0;
        int order;

        if (zone == NULL ||
            sscanf(zone, "zone%*s%n", &offset) != 0 ||
            offset == 0) {
            continue;
        }

        cursor = zone + offset;
        for (order = 0; order < BUDDY_ORDER_COUNT; ++order) {
            uint64_t count;
            int consumed = 0;

            if (sscanf(cursor, "%" SCNu64 "%n", &count, &consumed) != 1) {
                break;
            }
            cursor += consumed;
            block_counts[order] += count;
        }
        found_zone = true;
    }
    fclose(file);

    if (found_zone) {
        int order = BUDDY_ORDER_COUNT - 1;

        while (order > 0 && block_counts[order] == 0) {
            --order;
        }
        system_memory->largest_block_count =
            block_counts[order] > UINT32_MAX
                ? UINT32_MAX
                : (uint32_t)block_counts[order];
        system_memory->largest_free_bytes =
            (UINT64_C(1) << order) * BUDDY_PAGE_SIZE;
        system_memory->largest_block_available = true;
    }
}

static bool read_memory_pool(const char *const total_path,
                             const char *const free_path,
                             const char *const largest_path,
                             MemoryAreaStats *area)
{
    if (!text_file_read_u64(total_path, &area->total_bytes) ||
        !text_file_read_u64(free_path, &area->free_bytes) ||
        !text_file_read_u64(largest_path, &area->largest_free_bytes)) {
        return false;
    }

    area->used_bytes =
        subtract_saturated(area->total_bytes, area->free_bytes);
    area->largest_block_available = true;
    area->available = true;
    return true;
}

static void read_gart_memory(MemoryAreaStats *area)
{
    static const char *const path = "/proc/iovmminfo";
    FILE *file = fopen(path, "r");
    char line[256];
    bool found_device = false;

    if (file == NULL) {
        return;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        uint64_t total_kib;
        uint64_t free_kib;
        uint64_t largest_kib;

        if (strstr(line, "iovmm-gart") != NULL) {
            found_device = true;
            continue;
        }

        if (found_device &&
            sscanf(line, " size: %" SCNu64 "KiB free: %" SCNu64
                         "KiB largest: %" SCNu64 "KiB",
                   &total_kib, &free_kib, &largest_kib) == 3) {
            area->total_bytes = kib_to_bytes(total_kib);
            area->free_bytes = kib_to_bytes(free_kib);
            area->used_bytes =
                subtract_saturated(area->total_bytes, area->free_bytes);
            area->largest_free_bytes = kib_to_bytes(largest_kib);
            area->largest_block_available = true;
            area->available = true;
            break;
        }
    }
    fclose(file);
}

static void memory_sampler_sample(void *instance, TelemetrySnapshot *snapshot)
{
    MemorySampler *sampler = instance;
    MemoryStats *memory = &snapshot->memory;

    memset(memory, 0, sizeof(*memory));
    read_meminfo(&memory->system);
    read_buddyinfo(&memory->system);
    read_memory_pool(CARVEOUT_TOTAL_PATH, CARVEOUT_FREE_PATH,
                     CARVEOUT_LARGEST_PATH, &memory->carveout);
    read_gart_memory(&memory->gart);
    read_memory_pool(IRAM_TOTAL_PATH, IRAM_FREE_PATH, IRAM_LARGEST_PATH,
                     &memory->iram);

    if (sampler->debug && !memory->system.available) {
        log_error("memory: failed to read /proc/meminfo");
    }
}

static void memory_sampler_destroy(void *instance)
{
    free(instance);
}

bool memory_sampler_create(TelemetrySource *source, bool debug)
{
    MemorySampler *sampler = calloc(1, sizeof(*sampler));

    if (sampler == NULL) {
        return false;
    }

    sampler->debug = debug;
    source->name = "memory";
    source->instance = sampler;
    source->sample = memory_sampler_sample;
    source->destroy = memory_sampler_destroy;
    return true;
}

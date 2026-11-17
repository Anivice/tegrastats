#ifndef TEGRASTATS_TELEMETRY_H
#define TEGRASTATS_TELEMETRY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    TEGRASTATS_CPU_COUNT = 2,
    TEGRASTATS_MAX_TELEMETRY_SOURCES = 8
};

typedef struct {
    bool available;
    uint64_t total_bytes;
    uint64_t free_bytes;
    uint64_t used_bytes;
    bool largest_block_available;
    uint64_t largest_free_bytes;
    uint32_t largest_block_count;
} MemoryAreaStats;

typedef struct {
    MemoryAreaStats system;
    MemoryAreaStats carveout;
    MemoryAreaStats gart;
    MemoryAreaStats iram;
} MemoryStats;

typedef struct {
    bool load_available;
    bool online[TEGRASTATS_CPU_COUNT];
    unsigned int load_percent[TEGRASTATS_CPU_COUNT];
    bool frequency_available;
    uint64_t frequency_khz;
    bool temperature_available;
    double temperature_celsius;
} CpuStats;

typedef struct {
    bool available;
    uint64_t rate_hz;
} ClockRateStats;

typedef struct {
    ClockRateStats emc;
    ClockRateStats avp;
    ClockRateStats vdec;
} ClockStats;

typedef struct {
    bool frequency_available;
    uint64_t frequency_hz;
    bool load_available;
    uint32_t load_permille;
} GpuStats;

typedef struct {
    MemoryStats memory;
    CpuStats cpu;
    ClockStats clocks;
    GpuStats gpu;
} TelemetrySnapshot;

typedef void (*TelemetrySampleFn)(void *instance, TelemetrySnapshot *snapshot);
typedef void (*TelemetryDestroyFn)(void *instance);

typedef struct {
    const char *name;
    void *instance;
    TelemetrySampleFn sample;
    TelemetryDestroyFn destroy;
} TelemetrySource;

typedef struct {
    TelemetrySource sources[TEGRASTATS_MAX_TELEMETRY_SOURCES];
    size_t count;
} TelemetryCollector;

void telemetry_snapshot_init(TelemetrySnapshot *snapshot);
void telemetry_collector_init(TelemetryCollector *collector);
bool telemetry_collector_add(TelemetryCollector *collector,
                             TelemetrySource source);
void telemetry_collector_sample(const TelemetryCollector *collector,
                                TelemetrySnapshot *snapshot);
void telemetry_collector_dispose(TelemetryCollector *collector);

#endif

#include "cpu_sampler.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logger.h"
#include "text_io.h"

enum {
    CPU_TIME_FIELD_COUNT = 10,
    CPU_IDLE_FIELD = 3
};

static const char *const CPU0_ONLINE_PATH =
    "/sys/devices/system/cpu/cpu0/online";
static const char *const CPU1_ONLINE_PATH =
    "/sys/devices/system/cpu/cpu1/online";
static const char *const STAT_PATH = "/proc/stat";
static const char *const TEMPERATURE_PATHS[] = {
    "/sys/class/hwmon/hwmon0/device/ext_temperature",
    "/sys/class/thermal/thermal_zone0/temp",
    "/sys/class/thermal/thermal_zone1/temp"
};
static const char *const FREQUENCY_PATHS[] = {
    "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_cur_freq",
    "/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq"
};

typedef struct {
    uint64_t fields[CPU_TIME_FIELD_COUNT];
} CpuTimes;

typedef struct {
    bool debug;
    CpuTimes previous_cpu[TEGRASTATS_CPU_COUNT];
} CpuSampler;

static void parse_cpu_line(const char *line, const char *expected_name,
                           CpuTimes *times, bool *found)
{
    char name[32];
    uint64_t *fields = times->fields;
    int count;

    memset(times, 0, sizeof(*times));
    count = sscanf(line,
                   "%31s %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64
                   " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64
                   " %" SCNu64 " %" SCNu64,
                   name, &fields[0], &fields[1], &fields[2], &fields[3],
                   &fields[4], &fields[5], &fields[6], &fields[7],
                   &fields[8], &fields[9]);

    *found = count >= 5 && strcmp(name, expected_name) == 0;
}

static void read_cpu_times(CpuTimes cpu_times[TEGRASTATS_CPU_COUNT],
                           bool has_cpu[TEGRASTATS_CPU_COUNT])
{
    static const char *const names[TEGRASTATS_CPU_COUNT] = {
        "cpu0",
        "cpu1"
    };
    FILE *file = fopen(STAT_PATH, "r");
    char line[512];
    int index;

    memset(cpu_times, 0,
           sizeof(*cpu_times) * TEGRASTATS_CPU_COUNT);
    memset(has_cpu, 0, sizeof(*has_cpu) * TEGRASTATS_CPU_COUNT);

    if (file == NULL) {
        return;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        for (index = 0; index < TEGRASTATS_CPU_COUNT; ++index) {
            if (!has_cpu[index]) {
                parse_cpu_line(line, names[index], &cpu_times[index],
                               &has_cpu[index]);
            }
        }
    }
    fclose(file);
}

static bool read_online_state(const char *path, bool fallback)
{
    int online;

    if (!text_file_read_int(path, &online)) {
        return fallback;
    }
    return online != 0;
}

static uint64_t sum_cpu_times(const CpuTimes *times)
{
    uint64_t total = 0;
    int index;

    for (index = 0; index < CPU_TIME_FIELD_COUNT; ++index) {
        if (UINT64_MAX - total < times->fields[index]) {
            return UINT64_MAX;
        }
        total += times->fields[index];
    }
    return total;
}

static unsigned int calculate_load(const CpuTimes *current,
                                   const CpuTimes *previous)
{
    CpuTimes delta;
    uint64_t total;
    uint64_t idle;
    uint64_t active;
    int index;

    for (index = 0; index < CPU_TIME_FIELD_COUNT; ++index) {
        delta.fields[index] =
            current->fields[index] >= previous->fields[index]
                ? current->fields[index] - previous->fields[index]
                : 0;
    }

    total = sum_cpu_times(&delta);
    idle = delta.fields[CPU_IDLE_FIELD];
    if (total == 0) {
        return 0;
    }

    active = total >= idle ? total - idle : 0;
    return (unsigned int)((long double)active * 100.0L /
                          (long double)total);
}

static void read_temperature(CpuStats *cpu, bool debug)
{
    float temperature;

    if (!text_file_read_first_float(
            TEMPERATURE_PATHS,
            sizeof(TEMPERATURE_PATHS) / sizeof(TEMPERATURE_PATHS[0]),
            &temperature)) {
        if (debug) {
            log_error("cpu: temperature is unavailable");
        }
        return;
    }

    if (temperature > 1000.0F) {
        temperature /= 1000.0F;
    }
    cpu->temperature_celsius = temperature;
    cpu->temperature_available = true;
}

static void cpu_sampler_sample(void *instance, TelemetrySnapshot *snapshot)
{
    CpuSampler *sampler = instance;
    CpuStats *cpu = &snapshot->cpu;
    CpuTimes cpu_times[TEGRASTATS_CPU_COUNT];
    bool has_cpu[TEGRASTATS_CPU_COUNT];
    uint64_t frequency_khz;
    int index;

    memset(cpu, 0, sizeof(*cpu));
    cpu->online[0] = read_online_state(CPU0_ONLINE_PATH, true);
    cpu->online[1] = read_online_state(CPU1_ONLINE_PATH, false);

    read_cpu_times(cpu_times, has_cpu);

    cpu->load_available = has_cpu[0] || has_cpu[1];
    for (index = 0; index < TEGRASTATS_CPU_COUNT; ++index) {
        if (has_cpu[index]) {
            cpu->load_percent[index] =
                calculate_load(&cpu_times[index],
                               &sampler->previous_cpu[index]);
            sampler->previous_cpu[index] = cpu_times[index];
        }
    }

    if (text_file_read_first_u64(
            FREQUENCY_PATHS,
            sizeof(FREQUENCY_PATHS) / sizeof(FREQUENCY_PATHS[0]),
            &frequency_khz)) {
        cpu->frequency_khz = frequency_khz;
        cpu->frequency_available = true;
    } else if (sampler->debug) {
        log_error("cpu: frequency is unavailable");
    }

    read_temperature(cpu, sampler->debug);

    if (sampler->debug && !cpu->load_available) {
        log_error("cpu: failed to read /proc/stat");
    }
}

static void cpu_sampler_destroy(void *instance)
{
    free(instance);
}

bool cpu_sampler_create(TelemetrySource *source, bool debug)
{
    CpuSampler *sampler = calloc(1, sizeof(*sampler));

    if (sampler == NULL) {
        return false;
    }

    sampler->debug = debug;
    source->name = "cpu";
    source->instance = sampler;
    source->sample = cpu_sampler_sample;
    source->destroy = cpu_sampler_destroy;
    return true;
}

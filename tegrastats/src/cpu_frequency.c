#include "cpu_frequency.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logger.h"

static const char *const AVAILABLE_FREQUENCIES_PATH =
    "/sys/devices/system/cpu/cpu0/cpufreq/"
    "scaling_available_frequencies";
static const char *const SCALING_MIN_FREQUENCY_PATH =
    "/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq";

struct CpuFrequencyController {
    bool debug;
    bool frequencies_loaded;
    uint64_t minimum_khz;
    uint64_t maximum_khz;
};

static bool load_frequency_range(CpuFrequencyController *controller)
{
    FILE *file;
    uint64_t frequency_khz;
    bool found = false;

    if (controller->frequencies_loaded) {
        return true;
    }

    file = fopen(AVAILABLE_FREQUENCIES_PATH, "r");
    if (file == NULL) {
        if (controller->debug) {
            log_error("Error opening %s", AVAILABLE_FREQUENCIES_PATH);
        }
        return false;
    }

    while (fscanf(file, "%" SCNu64, &frequency_khz) == 1) {
        if (!found) {
            controller->minimum_khz = frequency_khz;
            controller->maximum_khz = frequency_khz;
            found = true;
        } else if (frequency_khz > controller->maximum_khz) {
            controller->maximum_khz = frequency_khz;
        }
    }
    fclose(file);

    controller->frequencies_loaded = found;
    return found;
}

CpuFrequencyController *cpu_frequency_controller_create(bool debug)
{
    CpuFrequencyController *controller = calloc(1, sizeof(*controller));

    if (controller != NULL) {
        controller->debug = debug;
    }
    return controller;
}

bool cpu_frequency_controller_apply(CpuFrequencyController *controller,
                                    CpuFrequencyMode mode)
{
    uint64_t target_khz;
    FILE *file;
    int write_result;
    int close_result;

    if (!load_frequency_range(controller)) {
        log_error("Error reading %s", AVAILABLE_FREQUENCIES_PATH);
        return false;
    }

    target_khz = mode == CPU_FREQUENCY_MAXIMUM
                     ? controller->maximum_khz
                     : controller->minimum_khz;

    file = fopen(SCALING_MIN_FREQUENCY_PATH, "w");
    if (file == NULL) {
        log_error("Error opening %s: %s", SCALING_MIN_FREQUENCY_PATH,
                  errno != 0 ? strerror(errno) : "unknown error");
        return false;
    }

    write_result = fprintf(file, "%" PRIu64, target_khz);
    close_result = fclose(file);
    if (write_result < 0 || close_result != 0) {
        log_error("Error writing %s", SCALING_MIN_FREQUENCY_PATH);
        return false;
    }

    if (controller->debug) {
        log_info("CPU minimum frequency set to %" PRIu64 "kHz", target_khz);
    }
    return true;
}

void cpu_frequency_controller_destroy(CpuFrequencyController *controller)
{
    free(controller);
}

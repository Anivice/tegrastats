#include "application.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include "clock_sampler.h"
#include "cpu_frequency.h"
#include "cpu_sampler.h"
#include "gpu_sampler.h"
#include "logger.h"
#include "memory_sampler.h"
#include "reporter.h"

static bool sleep_for_milliseconds(uint32_t milliseconds)
{
    struct timespec remaining;

    remaining.tv_sec = (time_t)(milliseconds / 1000U);
    remaining.tv_nsec =
        (long)(milliseconds % 1000U) * 1000000L;

    while (nanosleep(&remaining, &remaining) != 0) {
        if (errno != EINTR) {
            return false;
        }
    }
    return true;
}

static bool add_source(TelemetryCollector *collector, TelemetrySource source)
{
    if (telemetry_collector_add(collector, source)) {
        return true;
    }

    if (source.destroy != NULL) {
        source.destroy(source.instance);
    }
    return false;
}

static int configure_cpu_frequencies(const CliOptions *options)
{
    CpuFrequencyController *controller =
        cpu_frequency_controller_create(options->debug);
    CpuFrequencyMode mode =
        options->action == CLI_ACTION_SET_MAXIMUM_FREQUENCIES
            ? CPU_FREQUENCY_MAXIMUM
            : CPU_FREQUENCY_DEFAULT;
    bool applied;

    if (controller == NULL) {
        log_error("Error: unable to create CPU frequency controller");
        return EXIT_FAILURE;
    }

    applied = cpu_frequency_controller_apply(controller, mode);
    cpu_frequency_controller_destroy(controller);
    if (!applied) {
        return EXIT_FAILURE;
    }

    if (mode == CPU_FREQUENCY_MAXIMUM) {
        log_info("Set CPU frequency to maximum");
    }
    return EXIT_SUCCESS;
}

int application_run(const CliOptions *options)
{
    TelemetryCollector collector;
    Reporter *reporter = NULL;
    TelemetrySource source;
    TelemetrySnapshot snapshot;
    uint64_t sample_count = 0;
    uint64_t sample_index;
    int exit_code = EXIT_FAILURE;

    if (options->action == CLI_ACTION_SET_DEFAULT_FREQUENCIES ||
        options->action == CLI_ACTION_SET_MAXIMUM_FREQUENCIES) {
        return configure_cpu_frequencies(options);
    }

    telemetry_collector_init(&collector);

    if (!memory_sampler_create(&source, options->debug) ||
        !add_source(&collector, source)) {
        log_error("Error: unable to create memory sampler");
        goto cleanup;
    }
    if (!cpu_sampler_create(&source, options->debug) ||
        !add_source(&collector, source)) {
        log_error("Error: unable to create CPU sampler");
        goto cleanup;
    }
    if (!clock_sampler_create(&source, options->debug) ||
        !add_source(&collector, source)) {
        log_error("Error: unable to create clock sampler");
        goto cleanup;
    }
    if (!gpu_sampler_create(&source, options->debug) ||
        !add_source(&collector, source)) {
        log_error("Error: unable to create GPU sampler");
        goto cleanup;
    }

    reporter = reporter_create(stdout, true);
    if (reporter == NULL) {
        log_error("Error: unable to create reporter");
        goto cleanup;
    }

    if (options->total_seconds != 0) {
        sample_count =
            ((uint64_t)options->total_seconds * 1000U) /
            options->interval_ms;
    }

    for (sample_index = 0;; ++sample_index) {
        telemetry_snapshot_init(&snapshot);
        telemetry_collector_sample(&collector, &snapshot);
        reporter_report(reporter, &snapshot);

        if (sample_count != 0 && sample_index + 1 >= sample_count) {
            break;
        }
        if (!sleep_for_milliseconds(options->interval_ms)) {
            log_error("Error: failed to wait for the next sample");
            goto cleanup;
        }
    }

    exit_code = EXIT_SUCCESS;

cleanup:
    reporter_destroy(reporter);
    telemetry_collector_dispose(&collector);
    return exit_code;
}

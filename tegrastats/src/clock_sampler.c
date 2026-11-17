#include "clock_sampler.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logger.h"
#include "text_io.h"

static const char *const CLOCK_SUMMARY_PATH =
    "/sys/kernel/debug/clk/clk_summary";
static const char *const EMC_RATE_PATHS[] = {
    "/sys/kernel/debug/tegra_bwmgr/emc_rate",
    "/sys/kernel/debug/bpmp/debug/clk/emc/rate",
    "/sys/kernel/debug/clock/emc/rate",
    "/sys/class/devfreq/emc/cur_freq",
    "/sys/class/devfreq/tegra_emc/cur_freq"
};

typedef struct {
    bool debug;
} ClockSampler;

static void update_clock_rate(ClockRateStats *clock, unsigned int enable_count,
                              uint64_t rate_hz)
{
    clock->available = true;
    clock->rate_hz = enable_count == 0 ? 0 : rate_hz;
}

static void read_summary_clocks(ClockStats *clocks)
{
    FILE *file = fopen(CLOCK_SUMMARY_PATH, "r");
    char line[512];

    if (file == NULL) {
        return;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char name[128];
        unsigned int enable_count;
        unsigned int prepare_count;
        uint64_t rate_hz;
        int count;

        if (strstr(line, "*[") != NULL) {
            continue;
        }

        count = sscanf(line, " %127s %u %u %" SCNu64,
                       name, &enable_count, &prepare_count, &rate_hz);
        if (count < 4) {
            continue;
        }

        if (strcmp(name, "avp.sclk") == 0) {
            update_clock_rate(&clocks->avp, enable_count, rate_hz);
        } else if (strcmp(name, "nvdec") == 0) {
            update_clock_rate(&clocks->vdec, enable_count, rate_hz);
        }
    }
    fclose(file);
}

static void clock_sampler_sample(void *instance, TelemetrySnapshot *snapshot)
{
    ClockSampler *sampler = instance;
    ClockStats *clocks = &snapshot->clocks;

    memset(clocks, 0, sizeof(*clocks));
    if (text_file_read_first_u64(
            EMC_RATE_PATHS,
            sizeof(EMC_RATE_PATHS) / sizeof(EMC_RATE_PATHS[0]),
            &clocks->emc.rate_hz)) {
        clocks->emc.available = true;
    } else if (sampler->debug) {
        log_error("clock: EMC rate is unavailable");
    }

    read_summary_clocks(clocks);
    if (sampler->debug) {
        if (!clocks->avp.available) {
            log_error("clock: avp.sclk is unavailable");
        }
        if (!clocks->vdec.available) {
            log_error("clock: nvdec is unavailable");
        }
    }
}

static void clock_sampler_destroy(void *instance)
{
    free(instance);
}

bool clock_sampler_create(TelemetrySource *source, bool debug)
{
    ClockSampler *sampler = calloc(1, sizeof(*sampler));

    if (sampler == NULL) {
        return false;
    }

    sampler->debug = debug;
    source->name = "clocks";
    source->instance = sampler;
    source->sample = clock_sampler_sample;
    source->destroy = clock_sampler_destroy;
    return true;
}

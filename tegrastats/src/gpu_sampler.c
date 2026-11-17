#include "gpu_sampler.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "logger.h"
#include "text_io.h"

static const char *const GPU_FREQUENCY_PATHS[] = {
    "/sys/class/devfreq/57000000.gpu/cur_freq",
    "/sys/devices/57000000.gpu/devfreq/57000000.gpu/cur_freq",
    ("/sys/devices/platform/host1x/57000000.gpu/devfreq/"
     "57000000.gpu/cur_freq")
};
static const char *const GPU_LOAD_PATHS[] = {
    "/sys/devices/57000000.gpu/load",
    "/sys/devices/gpu.0/load",
    "/sys/devices/platform/host1x/57000000.gpu/load"
};

typedef struct {
    bool debug;
} GpuSampler;

static void gpu_sampler_sample(void *instance, TelemetrySnapshot *snapshot)
{
    GpuSampler *sampler = instance;
    GpuStats *gpu = &snapshot->gpu;
    uint64_t load_permille;

    memset(gpu, 0, sizeof(*gpu));
    if (text_file_read_first_u64(
            GPU_FREQUENCY_PATHS,
            sizeof(GPU_FREQUENCY_PATHS) / sizeof(GPU_FREQUENCY_PATHS[0]),
            &gpu->frequency_hz)) {
        gpu->frequency_available = true;
    }

    if (text_file_read_first_u64(
            GPU_LOAD_PATHS,
            sizeof(GPU_LOAD_PATHS) / sizeof(GPU_LOAD_PATHS[0]),
            &load_permille)) {
        gpu->load_permille =
            load_permille > UINT32_MAX ? UINT32_MAX : (uint32_t)load_permille;
        gpu->load_available = true;
    }

    if (sampler->debug) {
        if (!gpu->frequency_available) {
            log_error("gpu: frequency is unavailable");
        }
        if (!gpu->load_available) {
            log_error("gpu: load is unavailable");
        }
    }
}

static void gpu_sampler_destroy(void *instance)
{
    free(instance);
}

bool gpu_sampler_create(TelemetrySource *source, bool debug)
{
    GpuSampler *sampler = calloc(1, sizeof(*sampler));

    if (sampler == NULL) {
        return false;
    }

    sampler->debug = debug;
    source->name = "gpu";
    source->instance = sampler;
    source->sample = gpu_sampler_sample;
    source->destroy = gpu_sampler_destroy;
    return true;
}

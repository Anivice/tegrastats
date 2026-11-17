#ifndef TEGRASTATS_GPU_SAMPLER_H
#define TEGRASTATS_GPU_SAMPLER_H

#include <stdbool.h>

#include "telemetry.h"

bool gpu_sampler_create(TelemetrySource *source, bool debug);

#endif

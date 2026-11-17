#ifndef TEGRASTATS_MEMORY_SAMPLER_H
#define TEGRASTATS_MEMORY_SAMPLER_H

#include <stdbool.h>

#include "telemetry.h"

bool memory_sampler_create(TelemetrySource *source, bool debug);

#endif

#ifndef TEGRASTATS_CPU_SAMPLER_H
#define TEGRASTATS_CPU_SAMPLER_H

#include <stdbool.h>

#include "telemetry.h"

bool cpu_sampler_create(TelemetrySource *source, bool debug);

#endif

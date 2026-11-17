#ifndef TEGRASTATS_CLOCK_SAMPLER_H
#define TEGRASTATS_CLOCK_SAMPLER_H

#include <stdbool.h>

#include "telemetry.h"

bool clock_sampler_create(TelemetrySource *source, bool debug);

#endif

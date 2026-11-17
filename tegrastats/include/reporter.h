#ifndef TEGRASTATS_REPORTER_H
#define TEGRASTATS_REPORTER_H

#include <stdbool.h>
#include <stdio.h>

#include "telemetry.h"

typedef struct Reporter Reporter;

Reporter *reporter_create(FILE *stream, bool flush_after_report);
void reporter_report(Reporter *reporter, const TelemetrySnapshot *snapshot);
void reporter_destroy(Reporter *reporter);

#endif

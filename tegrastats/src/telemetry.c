#include "telemetry.h"

#include <string.h>

void telemetry_snapshot_init(TelemetrySnapshot *snapshot)
{
    memset(snapshot, 0, sizeof(*snapshot));
}

void telemetry_collector_init(TelemetryCollector *collector)
{
    memset(collector, 0, sizeof(*collector));
}

bool telemetry_collector_add(TelemetryCollector *collector,
                             TelemetrySource source)
{
    if (collector->count >= TEGRASTATS_MAX_TELEMETRY_SOURCES ||
        source.sample == NULL) {
        return false;
    }

    collector->sources[collector->count++] = source;
    return true;
}

void telemetry_collector_sample(const TelemetryCollector *collector,
                                TelemetrySnapshot *snapshot)
{
    size_t index;

    for (index = 0; index < collector->count; ++index) {
        const TelemetrySource *source = &collector->sources[index];

        source->sample(source->instance, snapshot);
    }
}

void telemetry_collector_dispose(TelemetryCollector *collector)
{
    size_t index;

    for (index = 0; index < collector->count; ++index) {
        TelemetrySource *source = &collector->sources[index];

        if (source->destroy != NULL) {
            source->destroy(source->instance);
        }
    }

    collector->count = 0;
}

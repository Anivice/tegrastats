#include "reporter.h"

#include <inttypes.h>
#include <stdlib.h>

#include "units.h"

struct Reporter {
    FILE *stream;
    bool flush_after_report;
};

static void report_pool(FILE *stream, const char *label,
                        const MemoryAreaStats *area, bool use_kib)
{
    char largest[32];

    if (!area->available) {
        return;
    }

    if (!area->largest_block_available) {
        return;
    }

    units_format_bytes(area->largest_free_bytes, largest, sizeof(largest));
    if (use_kib) {
        fprintf(stream, " %s %" PRIu64 "/%" PRIu64 "kB (lfb %s)",
                label, units_bytes_to_kib(area->used_bytes),
                units_bytes_to_kib(area->total_bytes), largest);
    } else {
        fprintf(stream, " %s %" PRIu64 "/%" PRIu64 "MB (lfb %s)",
                label, units_bytes_to_mib(area->used_bytes),
                units_bytes_to_mib(area->total_bytes), largest);
    }
}

static void report_memory(FILE *stream, const MemoryStats *memory)
{
    if (memory->system.available) {
        fprintf(stream, "RAM %" PRIu64 "/%" PRIu64 "MB",
                units_bytes_to_mib(memory->system.used_bytes),
                units_bytes_to_mib(memory->system.total_bytes));
        if (memory->system.largest_block_available) {
            char largest[32];

            units_format_bytes(memory->system.largest_free_bytes,
                               largest, sizeof(largest));
            fprintf(stream, " (lfb %ux%s)",
                    (unsigned int)memory->system.largest_block_count,
                    largest);
        }
    } else {
        fprintf(stream, "RAM n/a");
    }

    report_pool(stream, "Carveout", &memory->carveout, false);
    report_pool(stream, "GART", &memory->gart, false);
    report_pool(stream, "IRAM", &memory->iram, true);
}

static void report_cpu(FILE *stream, const CpuStats *cpu)
{
    int index;

    fprintf(stream, " cpu [");
    for (index = 0; index < TEGRASTATS_CPU_COUNT; ++index) {
        if (index != 0) {
            fputc(',', stream);
        }
        if (!cpu->online[index]) {
            fputs("off", stream);
        } else if (cpu->load_available) {
            fprintf(stream, "%u%%", cpu->load_percent[index]);
        } else {
            fputs("n/a", stream);
        }
    }
    fputc(']', stream);

    if (cpu->frequency_available) {
        fprintf(stream, "@%" PRIu64 "MHz",
                units_khz_to_mhz(cpu->frequency_khz));
    }
    if (cpu->temperature_available) {
        fprintf(stream, " (%.2fC)", cpu->temperature_celsius);
    }
}

static void report_clock(FILE *stream, const char *label,
                         const ClockRateStats *clock)
{
    fprintf(stream, " %s ", label);
    if (clock->available) {
        fprintf(stream, "%" PRIu64 "MHz", units_hz_to_mhz(clock->rate_hz));
    } else {
        fputs("-1MHz", stream);
    }
}

static void report_gpu(FILE *stream, const GpuStats *gpu)
{
    if (gpu->load_available) {
        fprintf(stream, " GPU %u%%", gpu->load_permille / 10U);
    } else {
        fputs(" GPU -1%", stream);
    }

    if (gpu->frequency_available) {
        fprintf(stream, "@%" PRIu64 "MHz",
                units_hz_to_mhz(gpu->frequency_hz));
    } else {
        fputs("@-1MHz", stream);
    }
}

Reporter *reporter_create(FILE *stream, bool flush_after_report)
{
    Reporter *reporter;

    if (stream == NULL) {
        return NULL;
    }

    reporter = calloc(1, sizeof(*reporter));
    if (reporter != NULL) {
        reporter->stream = stream;
        reporter->flush_after_report = flush_after_report;
    }
    return reporter;
}

void reporter_report(Reporter *reporter, const TelemetrySnapshot *snapshot)
{
    FILE *stream;

    if (reporter == NULL || snapshot == NULL) {
        return;
    }

    stream = reporter->stream;
    report_memory(stream, &snapshot->memory);
    report_cpu(stream, &snapshot->cpu);
    report_clock(stream, "EMC", &snapshot->clocks.emc);
    report_clock(stream, "AVP", &snapshot->clocks.avp);
    report_clock(stream, "NVDEC", &snapshot->clocks.vdec);
    report_gpu(stream, &snapshot->gpu);
    fputc('\n', stream);

    if (reporter->flush_after_report) {
        fflush(stream);
    }
}

void reporter_destroy(Reporter *reporter)
{
    free(reporter);
}

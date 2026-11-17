#include "cli.h"

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include "logger.h"

enum {
    DEFAULT_INTERVAL_MS = 1000,
    MINIMUM_INTERVAL_MS = 100
};

static bool parse_u32(const char *text, uint32_t *value)
{
    char *end = NULL;
    uintmax_t parsed;

    if (text == NULL || text[0] == '\0' || text[0] == '-') {
        return false;
    }

    errno = 0;
    parsed = strtoumax(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0' ||
        parsed > UINT32_MAX) {
        return false;
    }

    *value = (uint32_t)parsed;
    return true;
}

CliParseResult cli_parse(int argc, char *argv[], CliOptions *options)
{
    int index;

    options->action = CLI_ACTION_RUN;
    options->interval_ms = DEFAULT_INTERVAL_MS;
    options->total_seconds = 0;
    options->debug = false;

    for (index = 1; index < argc; ++index) {
        const char *argument = argv[index];

        if (strcmp(argument, "-total_sec") == 0) {
            uint32_t seconds;

            if (index + 1 >= argc ||
                !parse_u32(argv[++index], &seconds) || seconds == 0) {
                log_error("Error: Bad test time parameter");
                return CLI_PARSE_ERROR;
            }
            options->total_seconds = seconds;
            continue;
        }

        if (strcmp(argument, "-default") == 0) {
            options->action = CLI_ACTION_SET_DEFAULT_FREQUENCIES;
            return CLI_PARSE_OK;
        }

        if (strcmp(argument, "-max") == 0) {
            options->action = CLI_ACTION_SET_MAXIMUM_FREQUENCIES;
            return CLI_PARSE_OK;
        }

        if (strcmp(argument, "-help") == 0) {
            options->action = CLI_ACTION_HELP;
            return CLI_PARSE_OK;
        }

        if (strcmp(argument, "-debug") == 0) {
            options->debug = true;
            continue;
        }

        if (argument[0] == '-') {
            log_error("Error: Invalid parameter");
            return CLI_PARSE_ERROR;
        }

        {
            uint32_t interval_ms;

            if (!parse_u32(argument, &interval_ms)) {
                log_error("Bad interval parameter");
                return CLI_PARSE_ERROR;
            }

            options->interval_ms =
                interval_ms < MINIMUM_INTERVAL_MS
                    ? MINIMUM_INTERVAL_MS
                    : interval_ms;
        }
    }

    if (options->total_seconds != 0 &&
        options->interval_ms >
            (uint64_t)options->total_seconds * 1000U) {
        log_error("Error: The interval can't be more than total duration");
        return CLI_PARSE_ERROR;
    }

    return CLI_PARSE_OK;
}

void cli_print_usage(FILE *stream, const char *program_name)
{
    fprintf(stream, "NVIDIA TegraStats Utility\n\n");
    fprintf(stream,
            "Monitor CPU loading, frequency and temperature, Core DVFS data, "
            "Memory information, etc.\n");
    fprintf(stream, "Specify CPU scaling in full range or no scaling.\n\n");
    fprintf(stream,
            "%s [interval] [-total_sec <sec>] [-default] [-max] "
            "[-debug] [-help]\n",
            program_name);
    fprintf(stream,
            "     interval            Specify the polling interval in "
            "milliseconds; the default is 1000ms.\n");
    fprintf(stream,
            "     -total_sec <sec>    Specify the total duration in seconds; "
            "the default is infinity.\n");
    fprintf(stream,
            "     -default            Restore the minimum CPU frequency.\n");
    fprintf(stream,
            "     -max                Raise the minimum CPU frequency to its "
            "maximum available value.\n");
    fprintf(stream,
            "     -debug              Print diagnostics for unavailable "
            "telemetry.\n");
    fprintf(stream, "     -help               Show this usage.\n");
}

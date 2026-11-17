#ifndef TEGRASTATS_CLI_H
#define TEGRASTATS_CLI_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef enum {
    CLI_ACTION_RUN,
    CLI_ACTION_HELP,
    CLI_ACTION_SET_DEFAULT_FREQUENCIES,
    CLI_ACTION_SET_MAXIMUM_FREQUENCIES
} CliAction;

typedef struct {
    CliAction action;
    uint32_t interval_ms;
    uint32_t total_seconds;
    bool debug;
} CliOptions;

typedef enum {
    CLI_PARSE_OK,
    CLI_PARSE_ERROR
} CliParseResult;

CliParseResult cli_parse(int argc, char *argv[], CliOptions *options);
void cli_print_usage(FILE *stream, const char *program_name);

#endif

// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "application.h"
#include "cli.h"

int main(int argc, char *argv[])
{
    CliOptions options;

    if (cli_parse(argc, argv, &options) != CLI_PARSE_OK) {
        return EXIT_FAILURE;
    }

    if (options.action == CLI_ACTION_HELP) {
        cli_print_usage(stdout, argv[0]);
        return EXIT_SUCCESS;
    }

    return application_run(&options);
}

#include "logger.h"

#include <stdarg.h>
#include <stdio.h>

static void vlog(FILE *stream, const char *format, va_list arguments)
{
    vfprintf(stream, format, arguments);
    fputc('\n', stream);
}

void log_error(const char *format, ...)
{
    va_list arguments;

    va_start(arguments, format);
    vlog(stderr, format, arguments);
    va_end(arguments);
}

void log_info(const char *format, ...)
{
    va_list arguments;

    va_start(arguments, format);
    vlog(stdout, format, arguments);
    va_end(arguments);
}

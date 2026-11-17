#ifndef TEGRASTATS_LOGGER_H
#define TEGRASTATS_LOGGER_H

void log_error(const char *format, ...)
    __attribute__((format(printf, 1, 2)));
void log_info(const char *format, ...)
    __attribute__((format(printf, 1, 2)));

#endif

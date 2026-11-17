#ifndef TEGRASTATS_CPU_FREQUENCY_H
#define TEGRASTATS_CPU_FREQUENCY_H

#include <stdbool.h>

typedef enum {
    CPU_FREQUENCY_DEFAULT,
    CPU_FREQUENCY_MAXIMUM
} CpuFrequencyMode;

typedef struct CpuFrequencyController CpuFrequencyController;

CpuFrequencyController *cpu_frequency_controller_create(bool debug);
bool cpu_frequency_controller_apply(CpuFrequencyController *controller,
                                    CpuFrequencyMode mode);
void cpu_frequency_controller_destroy(CpuFrequencyController *controller);

#endif

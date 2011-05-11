// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#if !NV_IS_LDK
#include <utils/Log.h>
#undef LOG_TAG
#define LOG_TAG "dfs_stress"
#else
#include <string.h>
#define LOGE(...) \
do { \
  printf(__VA_ARGS__); \
  printf("\n"); \
} while (0)

#define LOGI(...) \
do { \
  printf(__VA_ARGS__); \
  printf("\n"); \
} while (0)
#endif

#define DFS_STRESS_MINIMUM_CYCLE_MS    50
#define MAX_PATH_LENGTH         256
#define MAX_DVFS_FREQS          16
#define SCALING_ENABLED_MODULES      2
#define DVFS_CLOCKS_BASE_PATH "/sys/kernel/debug/clock/"
#define CPUCLK DVFS_CLOCKS_BASE_PATH "cpu/rate"
#define EMCCLK DVFS_CLOCKS_BASE_PATH "emc/rate"
#define AVPCLK DVFS_CLOCKS_BASE_PATH "avp.sclk/rate"
#define VDECLK DVFS_CLOCKS_BASE_PATH "vde/rate"
#define CPU_INFO_PATH "/sys/devices/system/cpu/cpu0/cpufreq/"
#define SCALING_FREQUENCIES CPU_INFO_PATH "scaling_available_frequencies"
#define CPU_LATENCY CPU_INFO_PATH "cpuinfo_transition_latency"

#define READ_VALUE(path, pvalue) {          \
  f = fopen(path, "r");              \
  if (f) {                    \
    (void) fscanf(f, "%d", pvalue);        \
    fclose(f);                  \
  } else {                    \
    LOGE("Failed to open %s", path);      \
  }                        \
}

/* Prototypes. */
struct freq_set_table {
  long freq_list[MAX_DVFS_FREQS];
  char *name;
  char *path;
  int range[2]; /* Min-Max Khz*/
  int num_freqs;
  int is_max_set;
  int is_min_set;
  int is_on;
};

#define FRQ_TABLE(_clk_name, _path, _frq_min, _frq_max)  \
{                            \
  .name = _clk_name,                  \
  .path = _path,                    \
  .range[0] = _frq_min,                \
  .range[1] = _frq_max,                \
}

static struct freq_set_table freq_table[10] = {
  /* Set cpu frequencies by reading supported values */
  FRQ_TABLE("cpu", CPUCLK, 0, 0),
  /* Set range for all other modules */
  FRQ_TABLE("emc", EMCCLK, 50000, 660000),
};

int main(int argc, char *argv[]);
static int set_frequency(char *file_name, unsigned long long freqs)
{
  FILE *f;

  f = fopen(file_name, "w");
  if (!f)
  {
    LOGE("freq open %s failed \n", file_name);
    return -1;
  }
  if (fprintf(f, "%llu", freqs) <= 0)
    LOGE("freq select failed \n");
  fclose(f);
  return 0;
}

int main (int argc, char *argv[])
{
  FILE* f;
  int i = 0;
  int cycles_ms = DFS_STRESS_MINIMUM_CYCLE_MS;
  int total_sec = 1;
  int is_quiet = 0;
  int temp;
  int temp_table;

  // Parse command line
  for (i = 1; i < argc; i++)
  {
    char* endptr;
    unsigned int tmp;
    if (!strcmp(argv[i], "-cycle_ms"))
    {
      i++;
      if (i < argc)
      {
        tmp = strtoul(argv[i], &endptr, 0);
        if (!strcmp(endptr, ""))
        {
          if (tmp >= DFS_STRESS_MINIMUM_CYCLE_MS)
            cycles_ms = tmp;
          else
            cycles_ms = DFS_STRESS_MINIMUM_CYCLE_MS;
          continue;
        }
      }
      LOGE("Bad cycle time parameter\n");
    }
    else if (!strcmp(argv[i], "-total_sec"))
    {
      i++;
      if (i < argc)
      {
        tmp = strtoul(argv[i], &endptr, 0);
        if (!strcmp(endptr, ""))
        {
          if (tmp)
            total_sec = tmp;
          else
            total_sec = 1;
          continue;
        }
      }
      LOGE("Bad test time parameter\n");
    }
    else if (!strcmp(argv[i], "-quiet"))
    {
      is_quiet = 1;
    }
  }
  LOGI("total_sec %d cycle_ms %d quiet is %s\n",
      total_sec, cycles_ms, is_quiet? "true": "false");

  /* Set CPU scale frequencies */
  f = fopen(SCALING_FREQUENCIES, "r");
  if (f)
  {
    if (!is_quiet)
      LOGI("Read frequencies \n");
    i = 0;
    while (i < MAX_DVFS_FREQS)
    {
      fscanf(f, "%ld",&(freq_table[0].freq_list[i]));
      if (feof(f))
      {
        freq_table[0].freq_list[i] = 0;
        freq_table[0].num_freqs = i;
        break;
      }
      if (!is_quiet)
        LOGI("%ld ", (freq_table[0].freq_list[i]));
      i++;
    }
  }
  fclose(f);
  if (!is_quiet)
    LOGI("Done read frequencies \n");

  srand((unsigned int)time((time_t *)NULL));
  i = (total_sec * 1000)/cycles_ms;
  while (i--)
  {
    long freq_selected;

    temp_table = rand() % SCALING_ENABLED_MODULES;
    if (freq_table[temp_table].num_freqs)
    {
      temp = rand() % freq_table[temp_table].num_freqs;
      if (!freq_table[temp_table].is_min_set)
      {
        temp = 0;
        freq_table[temp_table].is_min_set = 1;
      }
      else if (!freq_table[temp_table].is_max_set)
      {
        temp = freq_table[temp_table].num_freqs - 1;
        freq_table[temp_table].is_max_set = 1;
      }
      freq_selected = freq_table[temp_table].freq_list[temp];
    }
    else
    {
      freq_selected = rand() % freq_table[temp_table].range[1];
      if (freq_selected < freq_table[temp_table].range[0])
        freq_selected = freq_table[temp_table].range[0];

      if (!freq_table[temp_table].is_min_set)
      {
        freq_selected = freq_table[temp_table].range[0];
        freq_table[temp_table].is_min_set = 1;
      }
      else if (!freq_table[temp_table].is_max_set)
      {
        freq_selected = freq_table[temp_table].range[1];
        freq_table[temp_table].is_max_set = 1;
      }
    }
    if (!is_quiet)
      LOGI("freq selected %ld %s \n",
        freq_selected * 1000, freq_table[temp_table].path);
    /* eg: /d/clock/cpu/rate */
    if (set_frequency(freq_table[temp_table].path,
        freq_selected * 1000) < 0)
      LOGI("freq select failed \n");
    usleep(cycles_ms*1000);
  }

  return 0;
}

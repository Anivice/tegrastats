// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#ifndef NV_IS_LDK
#define NV_IS_LDK 1
#endif

#include <errno.h>

#if !NV_IS_LDK
    #include <utils/Log.h>
    #undef LOG_TAG
    #define LOG_TAG "TegraStats"
#else
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

#define MONITOR_MINIMUM_INTERVAL_MS 100

#define NVMAP_BASE_PATH "/sys/devices/platform/tegra-nvmap/misc/nvmap/"
#define CARVEOUT(x) NVMAP_BASE_PATH "heap-generic-0/" # x
#define IRAM(x)     NVMAP_BASE_PATH "heap-iram/" # x
#define CPU_TEMPERATURE_PATH "/sys/class/hwmon/hwmon0/device/ext_temperature"

#include <errno.h>

#define CLK_SUMMARY_PATH "/sys/kernel/debug/clk/clk_summary"

static int read_clk_summary_rate_hz(const char* clk_name,
                                    unsigned int* enable_cnt,
                                    unsigned long long* rate_hz)
{
    FILE* f = fopen(CLK_SUMMARY_PATH, "r");
    if (!f) return -errno;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "*[")) continue;

        char name[128] = {0};
        unsigned int en = 0, prep = 0;
        unsigned long long rate = 0, req = 0;

        int n = sscanf(line, " %127s %u %u %llu %llu", name, &en, &prep, &rate, &req);
        if (n >= 4 && strcmp(name, clk_name) == 0) {
            if (enable_cnt) *enable_cnt = en;
            if (rate_hz) *rate_hz = rate;
            fclose(f);
            return 0;
        }
    }

    fclose(f);
    return -1;
}

static int read_clk_mhz_or_zero_if_disabled(const char* clk_name, int debug)
{
    unsigned int en = 0;
    unsigned long long hz = 0;
    int rc = read_clk_summary_rate_hz(clk_name, &en, &hz);
    if (rc != 0) {
        if (debug) LOGE("clk_summary: missing %s", clk_name);
        return -1;
    }
    if (en == 0) return 0;
    return (int)(hz / 1000000ULL); // MHz
}

static int read_int_file(const char* path, int* out)
{
    FILE* f = fopen(path, "r");
    if (!f) return -errno;
    if (fscanf(f, "%d", out) != 1) { fclose(f); return -1; }
    fclose(f);
    return 0;
}

static int read_float_file(const char* path, float* out)
{
    FILE* f = fopen(path, "r");
    if (!f) return -errno;
    if (fscanf(f, "%f", out) != 1) { fclose(f); return -1; }
    fclose(f);
    return 0;
}

static int read_first_int(const char* const* paths, size_t n, int* out, int debug)
{
    for (size_t i = 0; i < n; i++) {
        int rc = read_int_file(paths[i], out);
        if (rc == 0) return 0;
        if (debug) LOGE("Failed to open %s", paths[i]);
    }
    return -1;
}

static int read_first_float(const char* const* paths, size_t n, float* out, int debug)
{
    for (size_t i = 0; i < n; i++) {
        int rc = read_float_file(paths[i], out);
        if (rc == 0) return 0;
        if (debug) LOGE("Failed to open %s", paths[i]);
    }
    return -1;
}

/* Prototypes. */

int main(int argc, char *argv[]);

static void logFlush(void);

static int B2MB(int bytes);
static int kB2MB(int kiloBytes);

/* Store clk values to restore later */
// Assuming clk frequencies are same for both CPUs
unsigned int cpuclk[2];

/* Functions. */


static void logFlush(void)
{
#if NV_IS_LDK
    //need to fflush on LDK to make output redirectable
    fflush(stdout);
#endif
}

static int B2MB(int bytes)
{
    bytes += (1<<19)-1;        //rounding
    return bytes >> 20;
}

static int kB2MB(int kiloBytes)
{
    kiloBytes += (1<<9)-1;    //rounding
    return kiloBytes >> 10;
}

static int B2kB(int bytes)
{
    bytes += (1<<9)-1;    //rounding
    return bytes >> 10;
}

static int SmartB2Str(char* str, size_t size, int bytes)
{
    if (bytes < 1024)
        return snprintf(str, size, "%dB", bytes);
    else if (bytes < 1024*1024)
        return snprintf(str, size, "%dkB", B2kB(bytes));
    else
        return snprintf(str, size, "%dMB", B2MB(bytes));
}

static void setFreq(int setMax)
{
    FILE* f;
    LOGI("setFreq %d", setMax);

    if(!cpuclk[0]) {
        f = fopen("/sys/devices/system/cpu/cpu0/cpufreq"
                  "/scaling_available_frequencies", "r");
        if(f) {
            fscanf(f, "%u", &cpuclk[0]);
            while(fscanf(f, "%u", &cpuclk[1]) != EOF);

            LOGI("cpuclk: minfreq = %u maxfreq = %u\n", cpuclk[0], cpuclk[1]);
            fclose(f);
        }
        else {
            LOGE("Error opening file scaling_available_frequencies");
        }
    }
    if(setMax) {
        // set CPU frequency to highest value
        f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq", "w");
        if(f) {
            fprintf(f, "%u", cpuclk[1]);
            fclose(f);
        }
        else {
            LOGE("Error opening file scaling_min_freq\n");
        }
    }
    else {
        // set default
        f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq", "w");
        if(f) {
            fprintf(f, "%u", cpuclk[0]);
            fclose(f);
        }
        else {
            LOGE("Error opening file scaling_min_freq\n");
        }
    }
}

void printUsage()
{
    LOGI( "NVIDIA TegraStats Utility" );
    LOGI( "" );
    LOGI( "Monitor CPU loading, frequency and temperature, Core DVFS data, "
          "Memory information, etc." );
    LOGI( "Specify CPU scaling in full range or no scaling." );
    LOGI( "" );
    LOGI( "./tegrastats [interval] [-total_sec <sec>] [-default] [-max] "
          "[-help]" );
    LOGI( "     interval            Specify the decimal value for polling "
          "interval in milliseconds;" );
    LOGI( "                         the default is 1000ms if you don't set the "
          "parameter." );
    LOGI( "     -total_sec <sec>    Specify the decimal value for total "
          "duration in seconds;" );
    LOGI( "                         the default is infinity." );
    LOGI( "     -default            Specify scaling in full range." );
    LOGI( "     -max                Specify no scaling." );
    LOGI( "     -help               Show this usage." );
}

int main(int argc, char *argv[])
{
    int i;
    unsigned int sleepMS = 1000, totalSec = 0, loopCount = 0;
    int isCpu0Active, isCpu1Active, hasCPUTemp = 0;
    int cpuLoadPrev[3*10], cpuLoad = 0, cpu0Load = 0, cpu1Load = 0;
    int debug = 0;

    memset(cpuLoadPrev, 0, 3*10*sizeof(int));

    for ( i=1; i<argc; i++ )
    {
        char *          endptr;
        unsigned int    tmp;

        if ( argv[i][0] == '-' )
        {
            if ( !strcmp( argv[i], "-total_sec" ) )
            {
                i++;
                if ( i < argc )
                {
                    tmp = strtoul( argv[i], &endptr, 0 );
                    if ( !strcmp( endptr, "" ) )
                    {
                        if ( tmp )
                        {
                            totalSec = tmp;
                            continue;
                        }
                    }
                }

                LOGE( "Error: Bad test time parameter" );
                return 0;
            }
            else if ( !strcmp( argv[i], "-default" ) )
            {
                setFreq( 0 );
                return 0;
            }
            else if ( !strcmp( argv[i], "-max" ) )
            {
                setFreq( 1 );
                LOGI( "Set all components to max frequency" );
                return 0;
            }
            else if ( !strcmp( argv[i], "-help" ) )
            {
                printUsage();
                return 0;
            }
            else if ( !strcmp( argv[i], "-debug" ) )
            {
                debug = 1;
            }
            else
            {
                LOGE( "Error: Invalid parameter" );
                return 0;
            }
        }
        else
        {
            tmp = strtoul( argv[i], &endptr, 0 );
            if ( !strcmp( endptr, "" ) )
            {
                if ( tmp >= MONITOR_MINIMUM_INTERVAL_MS )
                    sleepMS = tmp;
                else
                    sleepMS = MONITOR_MINIMUM_INTERVAL_MS;

                continue;
            }

            LOGE( "Bad interval parameter" );
            return 0;
        }
    }

    // If the input sleepMS is more than total_sec, it is invalid parameter
    if ( totalSec && ( sleepMS > ( totalSec * 1000 ) ) )
    {
        LOGE( "Error: The interval can't be more than total duration" );
        return 0;
    }

    loopCount = ( totalSec * 1000 ) / sleepMS;
    while ( 1 )
    {
        int totalRAMkB = -1, freeRAMkB = -1, largestFreeRAMBlockB = -1;
        int numLargestRAMBlock = -1, buffersRAMkB = -1, cachedRAMkB = -1;
        int totalCarveoutB = -1, freeCarveoutB = -1;
        int largestFreeCarveoutBlockB = -1, totalGARTkB = -1, freeGARTkB = -1;
        int largestFreeGARTBlockkB = -1, totalIRAMB = -1, freeIRAMB = -1;
        int largestFreeIRAMBlockB = -1, currCpuFreq = -1;
        int emcClk = -1, avpClk = -1, vdeClk = -1;
        int gpuFreqHz = -1, gpuLoad10 = -1;
        float currCpuTemp = 0.0;

        if ( totalSec )
            loopCount--;

        // RAM
        FILE* f = fopen("/proc/meminfo", "r");
        if(f)
        {
            // add if (blah) {} to get around compiler warning
            if (fscanf(f, "MemTotal: %d kB\n", &totalRAMkB)) {}
            if (fscanf(f, "MemFree: %d kB\n", &freeRAMkB)) {}
            if (fscanf(f, "Buffers: %d kB\n", &buffersRAMkB)) {}
            if (fscanf(f, "Cached: %d kB\n", &cachedRAMkB)) {}
            fclose(f);
        }
        else
            if (debug) LOGE("Failed to open /sys/devices/system/cpu/cpu0/online");
        f = fopen("/proc/buddyinfo", "r");
        if(f)
        {
#define NUM_SLOTS 11
#define PAGE_SIZE 4096

            char line[256];
            int lineNum = 0;
            int slots[NUM_SLOTS];
            int i;

            // Get the number of free blocks for each size.
            // Separation into nodes and zones is not kept.
            while (fgets(line, sizeof(line), f))
            {
                int j = 0;
                int n;
                int tmpSlots[NUM_SLOTS];
                char* buf = line;

                if (sscanf(buf, "Node %*d, zone %*s%n", &n)) {}
                buf += n;

                while (sscanf(buf, "%d%n", &tmpSlots[j], &n) == 1)
                {
                    buf += n;
                    slots[j] = lineNum ? slots[j] + tmpSlots[j] : tmpSlots[j];
                    j++;
                }

                lineNum++;
            }

            fclose(f);

            // Extract info about the largest available blocks
            i = NUM_SLOTS - 1;
            while (slots[i] == 0 && i > 0)
                i--;
            numLargestRAMBlock = slots[i];
            largestFreeRAMBlockB = (1 << i) * PAGE_SIZE;
        }
        else
            if (debug) LOGE("Failed to open /sys/devices/system/cpu/cpu1/online");

        // CPU 0/1 On/Off
        f = fopen("/sys/devices/system/cpu/cpu0/online", "r");
        if(f)
        {
            // add if (blah) {} to get around compiler warning
            if (fscanf(f, "%d", &isCpu0Active)) {}
            fclose(f);
        }
        else
            LOGE("Failed to open /proc/meminfo");

        f = fopen("/sys/devices/system/cpu/cpu1/online", "r");
        if(f)
        {
            // add if (blah) {} to get around compiler warning
            if (fscanf(f, "%d", &isCpu1Active)) {}
            fclose(f);
        }
        else
            LOGE("Failed to open /proc/meminfo");

        // CPU load
        f = fopen("/proc/stat", "r");
        if(f)
        {
            int c[30], l[30],i;

//from http://www.mjmwired.net/kernel/Documentation/filesystems/proc.txt
//Various pieces of information about kernel activity are available in the
// /proc/stat file. All of the numbers reported in this file are aggregates
//since the system first booted.
//The very first "cpu" line aggregates the numbers in all of the other "cpuN"
//lines. These numbers identify the amount of time the CPU has spent performing
//different kinds of work. Time units are in USER_HZ (typically hundredths of a
//second). The meanings of the columns are as follows, from left to right:
//- user: normal processes executing in user mode
//- nice: niced processes executing in user mode
//- system: processes executing in kernel mode
//- idle: twiddling thumbs
//- iowait: waiting for I/O to complete
//- irq: servicing interrupts
//- softirq: servicing softirqs
//- steal: involuntary wait
//- guest: running a normal guest
//- guest_nice: running a niced guest

            // add if (blah) {} to get around compiler warning
            if (fscanf(f, "cpu  %d %d %d %d %d %d %d %d %d %d\n",
                c+0, c+1, c+2, c+3, c+4, c+5, c+6, c+7, c+8, c+9)) {}
            if (fscanf(f, "cpu0 %d %d %d %d %d %d %d %d %d %d\n",
                c+10, c+11, c+12, c+13, c+14, c+15, c+16, c+17, c+18, c+19)) {}
            if(isCpu1Active) {
                if (fscanf(f, "cpu1 %d %d %d %d %d %d %d %d %d %d\n",
                c+20, c+21, c+22, c+23, c+24, c+25, c+26, c+27, c+28, c+29)) {}
            } else
                memset(c+20,0,10*sizeof(int));
            fclose(f);

            // cpu load = (time spent on something else but idle since the
            // last update) / (total time spent since the last update)
            cpuLoad = 0;
            cpu0Load = 0;
            cpu1Load = 0;
            for(i=0;i<30;i++)
            {
                l[i] = c[i] - cpuLoadPrev[i];
                if(i<10)
                    cpuLoad += l[i];
                else if(i<20)
                    cpu0Load += l[i];
                else
                    cpu1Load += l[i];
                cpuLoadPrev[i] = c[i];
            }
            /*if(debug)
                LOGE("total0 %d  idle0 %d | total1 %d  idle1 %d",
                    cpu0Load, l[3+9], cpu1Load, l[3+18]);*/
            if(cpuLoad)
                cpuLoad = 100*(cpuLoad-l[3])/cpuLoad;
            else
                cpuLoad = 0;
            if(cpu0Load)
                cpu0Load = 100*(cpu0Load-l[3+10])/cpu0Load;
            else
                cpu0Load = 0;
            if(cpu1Load)
                cpu1Load = 100*(cpu1Load-l[3+20])/cpu1Load;
            else
                cpu1Load = 0;
        }
        else
            LOGE("Failed to open /proc/stat");
        (void)read_int_file(CARVEOUT(total_size), &totalCarveoutB);
        (void)read_int_file(CARVEOUT(free_size),  &freeCarveoutB);
        (void)read_int_file(CARVEOUT(free_max),   &largestFreeCarveoutBlockB);

        f = fopen("/proc/iovmminfo", "r");
        if (f) {
            if (fscanf(f, "\ngroups\n\t<unnamed> (device: iovmm-gart)"
                          "\n\t\tsize: %dKiB free: %dKiB largest: %dKiB",
                       &totalGARTkB, &freeGARTkB, &largestFreeGARTBlockkB)) {}
            fclose(f);
        }

        (void)read_int_file(IRAM(total_size), &totalIRAMB);
        (void)read_int_file(IRAM(free_size),  &freeIRAMB);
        (void)read_int_file(IRAM(free_max),   &largestFreeIRAMBlockB);

        // CPU Frequency
        f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_cur_freq", "r");
        if(f)
        {
            (void) fscanf(f, "%d", &currCpuFreq);
            fclose(f);
        }

        // CPU Temperature
        {
            const char* const temp_paths[] = {
                CPU_TEMPERATURE_PATH,
                "/sys/class/thermal/thermal_zone0/temp",
                "/sys/class/thermal/thermal_zone1/temp",
            };
            float t = 0.0f;
            if (read_first_float(temp_paths, sizeof(temp_paths)/sizeof(temp_paths[0]), &t, 0) == 0) {
                if (t > 1000.0f) t = t / 1000.0f;
                currCpuTemp = t;
                hasCPUTemp = 1;
            }
        }

        // DFS
        {
            const char* const emc_paths[] = {
                "/sys/kernel/debug/tegra_bwmgr/emc_rate",
                "/sys/kernel/debug/bpmp/debug/clk/emc/rate",
                "/sys/kernel/debug/clock/emc/rate",
                "/sys/class/devfreq/emc/cur_freq",
                "/sys/class/devfreq/tegra_emc/cur_freq",
            };
            const char* const avp_paths[] = {
                "/sys/kernel/debug/bpmp/debug/clk/avp.sclk/rate",
                "/sys/kernel/debug/clock/avp.sclk/rate",
            };
            const char* const vde_paths[] = {
                "/sys/kernel/debug/bpmp/debug/clk/vde/rate",
                "/sys/kernel/debug/clock/vde/rate",
            };
            (void)read_first_int(emc_paths, sizeof(emc_paths)/sizeof(emc_paths[0]), &emcClk, debug);
            avpClk = read_clk_mhz_or_zero_if_disabled("avp.sclk", debug);
            vdeClk = read_clk_mhz_or_zero_if_disabled("nvdec", debug);
            // (void)read_first_int(avp_paths, sizeof(avp_paths)/sizeof(avp_paths[0]), &avpClk, debug);
            // (void)read_first_int(vde_paths, sizeof(vde_paths)/sizeof(vde_paths[0]), &vdeClk, debug);
        }

        {
            const char* const gpu_freq_paths[] = {
                "/sys/class/devfreq/57000000.gpu/cur_freq",
                "/sys/devices/57000000.gpu/devfreq/57000000.gpu/cur_freq",
                "/sys/devices/platform/host1x/57000000.gpu/devfreq/57000000.gpu/cur_freq",
            };
            const char* const gpu_load_paths[] = {
                "/sys/devices/57000000.gpu/load",
                "/sys/devices/gpu.0/load",
                "/sys/devices/platform/host1x/57000000.gpu/load",
            };
            (void)read_first_int(gpu_freq_paths, sizeof(gpu_freq_paths)/sizeof(gpu_freq_paths[0]), &gpuFreqHz, debug);
            (void)read_first_int(gpu_load_paths, sizeof(gpu_load_paths)/sizeof(gpu_load_paths[0]), &gpuLoad10, debug);
        }

        {
            char cpu0String[5], cpu1String[5];
            char lfbRAM[10], lfbCarveout[10], lfbGART[10], lfbIRAM[10];
            int gpuPct = (gpuLoad10 >= 0) ? (gpuLoad10 / 10) : -1;
            int gpuMHz = (gpuFreqHz >= 0) ? (gpuFreqHz / 1000000) : -1;

            if(isCpu0Active)
                snprintf(cpu0String, 5, "%d%%", cpu0Load);
            else
                snprintf(cpu0String, 5, "off");
            if(isCpu1Active)
                snprintf(cpu1String, 5, "%d%%", cpu1Load);
            else
                snprintf(cpu1String, 5, "off");
            SmartB2Str(lfbRAM, 10, largestFreeRAMBlockB);
            if (largestFreeCarveoutBlockB >= 0) SmartB2Str(lfbCarveout, 10, largestFreeCarveoutBlockB);
            if (largestFreeGARTBlockkB >= 0) SmartB2Str(lfbGART, 10, largestFreeGARTBlockkB * 1024);
            if (largestFreeIRAMBlockB >= 0) SmartB2Str(lfbIRAM, 10, largestFreeIRAMBlockB);

            int hasExtraMem = (totalCarveoutB >= 0 && freeCarveoutB >= 0 &&
                               totalGARTkB >= 0 && freeGARTkB >= 0 &&
                               totalIRAMB >= 0 && freeIRAMB >= 0);

            if ( hasCPUTemp && hasExtraMem )
            {
                LOGE( "RAM %d/%dMB (lfb %dx%s) Carveout %d/%dMB (lfb %s) "
                      "GART %d/%dMB (lfb %s) IRAM %d/%dkB (lfb %s) "
                      "cpu [%s,%s]@%d (%.2fC) EMC %d AVP %d VDE %d GPU %d%%@%dMHz",
                    kB2MB( totalRAMkB - freeRAMkB - buffersRAMkB - cachedRAMkB),
                    kB2MB( totalRAMkB ),
                    numLargestRAMBlock,
                    lfbRAM,
                    B2MB( totalCarveoutB - freeCarveoutB ),
                    B2MB( totalCarveoutB ),
                    lfbCarveout,
                    kB2MB( totalGARTkB - freeGARTkB ),
                    kB2MB( totalGARTkB ),
                    lfbGART,
                    B2kB( totalIRAMB - freeIRAMB ),
                    B2kB( totalIRAMB ),
                    lfbIRAM,
                    cpu0String, cpu1String,
                    currCpuFreq, currCpuTemp,
                    emcClk, avpClk, vdeClk, gpuPct, gpuMHz );
            }
            else if ( hasCPUTemp )
            {
                LOGE( "RAM %d/%dMB (lfb %dx%s) cpu %d (%.2fC) EMC %d AVP %d VDE %d GPU %d%%@%dMHz",
                    kB2MB( totalRAMkB - freeRAMkB - buffersRAMkB - cachedRAMkB),
                    kB2MB( totalRAMkB ),
                    numLargestRAMBlock,
                    lfbRAM,
                    currCpuFreq, currCpuTemp,
                    emcClk, avpClk, vdeClk, gpuPct, gpuMHz );
            }
            else
            {
                LOGE( "RAM %d/%dMB (lfb %dx%s) cpu [%s,%s]@%d EMC %d AVP %d VDE %d GPU %d%%@%dMHz",
                    kB2MB( totalRAMkB - freeRAMkB - buffersRAMkB - cachedRAMkB),
                    kB2MB( totalRAMkB ),
                    numLargestRAMBlock,
                    lfbRAM,
                    cpu0String, cpu1String,
                    currCpuFreq,
                    emcClk, avpClk, vdeClk, gpuPct, gpuMHz );
            }
        }

        // fflush stdout (on LDK) to make the output redirectable.
        logFlush();

        // Break if the totalSec is not infinity and loopCount is zero
        if ( !loopCount && totalSec )
            break;

        usleep( sleepMS * 1000 );
    }

    return 0;
}

/* example contents of /proc/meminfo
MemTotal:         450164 kB
MemFree:          269628 kB
Buffers:            2320 kB
Cached:            69008 kB
SwapCached:            0 kB
Active:            89476 kB
Inactive:          63612 kB
Active(anon):      82272 kB
Inactive(anon):        0 kB
Active(file):       7204 kB
Inactive(file):    63612 kB
Unevictable:           0 kB
Mlocked:               0 kB
SwapTotal:             0 kB
SwapFree:              0 kB
Dirty:                 0 kB
Writeback:             0 kB
AnonPages:         81764 kB
Mapped:            35148 kB
Slab:               5204 kB
SReclaimable:       1760 kB
SUnreclaim:         3444 kB
PageTables:         4316 kB
NFS_Unstable:          0 kB
Bounce:                0 kB
WritebackTmp:          0 kB
CommitLimit:      225080 kB
Committed_AS:    1054316 kB
VmallocTotal:     450560 kB
VmallocUsed:       45964 kB
VmallocChunk:     340056 kB

http://www.linuxweblog.com/meminfo
    * MemTotal: Total usable ram (i.e. physical ram minus a few reserved bits
    *   and the kernel binary code)
    * MemFree: Is sum of LowFree+HighFree (overall * stat)
    * MemShared: 0 is here for compat reasons but always zero.
    * Buffers: * Memory in buffer cache.  mostly useless as metric nowadays
    * Cached: Memory in the pagecache (diskcache) minus SwapCache
    * SwapCache: Memory that once was swapped out, is swapped back in but
    *   still also is in the swapfile (if memory is needed it doesn't need to
    *   be swapped out AGAIN because it is already in the swapfile. This saves
    *   I/O)

VM splits the cache pages into "active" and "inactive" memory. The idea is that
if you need memory and some cache needs to be sacrificed for that, you take it
from inactive since that's expected to be not used. The vm checks what is used
on a regular basis and moves stuff around. When you use memory, the CPU sets a
bit in the pagetable and the VM checks that bit occasionally, and based on
that, it can move pages back to active. And within active there's an order of
"longest ago not used" (roughly, it's a little more complex in reality).

    * Active: Memory that has been used more recently and usually not reclaimed
    * unless absolutely necessary.
    * Inact_dirty: Dirty means "might need writing to disk or swap." Takes more
    * work to free. Examples might be files that have not been written to yet.
    * They aren't written to memory too soon in order to keep the I/O down. For
    * instance, if you're writing logs, it might be better to wait until you
    * have a complete log ready before sending it to disk.
    * Inact_clean: Assumed to be easily freeable.  The kernel will try to keep
    * some clean stuff around always to have a bit of breathing room.
    * Inact_target: Just a goal metric the kernel uses for making sure there are
    * enough inactive pages around. When exceeded, the kernel will not do work
    * to move pages from active to inactive. A page can also get inactive in a
    * few other ways, e.g. if you do a long sequential I/O, the kernel assumes
    * you're not going to use that memory and makes it inactive preventively. So
    * you can get more inactive pages than the target because the kernel marks
    * some cache as "more likely to be never used" and lets it cheat in the
    * "last used" order.
    * HighTotal: is the total amount of memory in the high region. Highmem is
    * all memory above (approx) 860MB of physical RAM. Kernel uses indirect
    * tricks to access the high memory region. Data cache can go in this memory
    * region.
    * LowTotal: The total amount of non-highmem memory.
    * LowFree: The amount of free memory of the low memory region. This is the
    * memory the kernel can address directly.  All kernel datastructures need to
    * go into low memory.
    * SwapTotal: Total amount of physical swap memory.
    * SwapFree: Total amount of swap memory free.
    * Committed_AS: An estimate of how much RAM you would need to make a 99.99%
    * guarantee that there never is OOM (out of memory) for this workload.
    * Normally the kernel will overcommit memory. The Committed_AS is a
    * guesstimate of how much RAM/swap you would need worst-case.
*/


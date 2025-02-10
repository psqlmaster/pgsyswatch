/* SPDX-License-Identifier: Apache-2.0
Copyright 2025 Alexander Scheglov */
#include "pgsyswatch_common.h"

/* Function to calculate CPU usage percentage */
float calculate_cpu_usage(unsigned long long utime, unsigned long long stime, unsigned long long starttime, unsigned long long uptime) {
    /* Total CPU time used by the process (in ticks) */
    unsigned long long total_time = utime + stime;
    /* Process uptime in seconds */
    float seconds_since_start = (float)(uptime - starttime) / sysconf(_SC_CLK_TCK);
    // Calculate CPU usage percentage
    if (seconds_since_start > 0) {
        return ((float)total_time / sysconf(_SC_CLK_TCK)) / seconds_since_start * 100.0;
    }
    return 0.0;
}

/* Function to retrieve process information */
ProcessInfo get_process_info(int pid) {
    ProcessInfo process;
    char path[256];
    FILE *file;
    char line[256];
    float rss, vmsize, swap;
    unsigned long long starttime = 0;
    unsigned long long uptime = 0;

    /* Initialize the structure */
    process.pid = pid;
    process.command = NULL;
    process.state = '\0';  
    process.res_mb = 0.0;
    process.virt_mb = 0.0;
    process.swap_mb = 0.0;
    process.utime = 0;
    process.stime = 0;
    process.cpu_usage = 0.0;
    process.read_bytes = 0;
    process.write_bytes = 0;
    process.voluntary_ctxt_switches = 0;
    process.nonvoluntary_ctxt_switches = 0;
    process.threads = 0;

    /* Read CPU information from /proc/[pid]/stat */
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    file = fopen(path, "r");
    if (file != NULL) {
        /* Format: pid, comm, state, ppid, ..., utime, stime, ..., nice, ... */
        if (fscanf(file, "%*d %*s %c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %llu %llu %*d %*d %*d %*d %*d %*d %llu",
                   &process.state, &process.utime, &process.stime, &starttime) == 4) {
            /* Successfully parsed /proc/[pid]/stat */
        }
        fclose(file);
    }

    /* Read uptime from /proc/uptime */
    file = fopen("/proc/uptime", "r");
    if (file != NULL) {
        if (fscanf(file, "%llu", &uptime) == 1) {
            uptime *= sysconf(_SC_CLK_TCK);  // Convert seconds to ticks
        }
        fclose(file);
    }

    /* Calculate CPU usage */
    process.cpu_usage = calculate_cpu_usage(process.utime, process.stime, starttime, uptime);

    /* Read memory information from /proc/[pid]/status */
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    file = fopen(path, "r");
    if (file != NULL) {
        while (fgets(line, sizeof(line), file)) {
            if (strncmp(line, "VmSize", 6) == 0) {
                if (sscanf(line, "VmSize: %f", &vmsize) == 1) {
                    process.virt_mb = vmsize / 1024.0;
                }
            } else if (strncmp(line, "VmRSS", 5) == 0) {
                if (sscanf(line, "VmRSS: %f", &rss) == 1) {
                    process.res_mb = rss / 1024.0;
                }
            } else if (strncmp(line, "VmSwap", 6) == 0) {
                if (sscanf(line, "VmSwap: %f", &swap) == 1) {
                    process.swap_mb = swap / 1024.0;
                }
            } else if (strncmp(line, "voluntary_ctxt_switches", 23) == 0) {
                if (sscanf(line, "voluntary_ctxt_switches: %d", &process.voluntary_ctxt_switches) == 1) {
                    /* Successfully parsed voluntary context switches */
                }
            } else if (strncmp(line, "nonvoluntary_ctxt_switches", 26) == 0) {
                if (sscanf(line, "nonvoluntary_ctxt_switches: %d", &process.nonvoluntary_ctxt_switches) == 1) {
                    /* Successfully parsed non-voluntary context switches */
                }
            } else if (strncmp(line, "Threads", 7) == 0) {
                if (sscanf(line, "Threads: %d", &process.threads) == 1) {
                    /* Successfully parsed thread count */
                }
            }
        }
        fclose(file);
    }

    /* Read disk I/O information from /proc/[pid]/io */
    snprintf(path, sizeof(path), "/proc/%d/io", pid);
    file = fopen(path, "r");
    if (file != NULL) {
        while (fgets(line, sizeof(line), file)) {
            if (strncmp(line, "read_bytes", 10) == 0) {
                if (sscanf(line, "read_bytes: %lu", &process.read_bytes) == 1) {
                    /* Successfully parsed read bytes */
                }
            } else if (strncmp(line, "write_bytes", 11) == 0) {
                if (sscanf(line, "write_bytes: %lu", &process.write_bytes) == 1) {
                    /* Successfully parsed write bytes */
                }
            }
        }
        fclose(file);
    }

    /* Read command from /proc/[pid]/cmdline */
    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    file = fopen(path, "r");
    if (file != NULL) {
        size_t len = 0;
        ssize_t read;
        read = getline(&process.command, &len, file);
        if (read == -1) {
            if (process.command != NULL) {
                free(process.command);
            }
            process.command = strdup("Unknown");
        }
        fclose(file);
    }

    return process;
}

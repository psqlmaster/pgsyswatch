// pgsyswatch_common.h
// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Alexander Scheglov
#ifndef PGSYSWATCH_COMMON_H
#define PGSYSWATCH_COMMON_H

#include "postgres.h"
#include "fmgr.h"
#include "utils/elog.h"
#include "utils/array.h"
#include "access/htup.h"
#include "catalog/pg_type.h"
#include "utils/builtins.h"
#include "funcapi.h"
#include "executor/spi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/time.h>
#include <unistd.h>

// Declare the debug_enabled variable as extern
extern bool debug_enabled;

// Macro for printing debug messages
#define DEBUG_PRINT(...) \
    do { \
        if (debug_enabled) { \
            fprintf(stderr, __VA_ARGS__); \
        } \
    } while (0)

// Define a structure to store process information
typedef struct {
    int pid;                        // Process ID
    char *command;                  // Command that started the process
    char state;                     // Process state (single letter: R, S, D, Z, etc.)
    float res_mb;                   // Resident memory usage in MB
    float virt_mb;                  // Virtual memory usage in MB
    float swap_mb;                  // Swap usage in MB
    unsigned long long utime;       // Time spent in user mode
    unsigned long long stime;       // Time spent in system mode
    float cpu_usage;                // CPU usage percentage
    unsigned long read_bytes;       // Number of bytes read from disk
    unsigned long write_bytes;      // Number of bytes written to disk
    int voluntary_ctxt_switches;    // Number of voluntary context switches
    int nonvoluntary_ctxt_switches; // Number of involuntary context switches
    int threads;                    // Number of threads
} ProcessInfo;

// Declare the function get_process_info
ProcessInfo get_process_info(int pid);

#endif // PGSYSWATCH_COMMON_H

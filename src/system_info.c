// src/system_info.c
// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Alexander Scheglov
#include "postgres.h"
#include "fmgr.h"
#include "utils/elog.h"
#include "utils/builtins.h"
#include "funcapi.h"
#include "storage/bufmgr.h"
#include "miscadmin.h"
#include "access/htup_details.h"
#include "utils/tuplestore.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> // For isdigit
#include "pgsyswatch_common.h"

// Structure to store swap information
typedef struct SystemSwapInfo
{
    float total_swap;  // In kilobytes
    float used_swap;   // In kilobytes
    float free_swap;   // In kilobytes
} SystemSwapInfo;

// Structure to store CPU frequency information
typedef struct CpuFrequencyInfo {
    int core_id;        // Core ID
    float frequency_mhz; // Frequency in MHz
} CpuFrequencyInfo;

// Function to clean a string of non-numeric characters (except for '.')
void clean_string(char *str) {
    char *src = str;
    char *dst = str;
    while (*src) {
        if (isdigit(*src) || *src == '.') {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';
}

// Function to get swap information from /proc/meminfo
SystemSwapInfo get_system_swap_info()
{
    SystemSwapInfo swap_info = {0};
    FILE *file = fopen("/proc/meminfo", "r");
    if (file != NULL)
    {
        char line[256];
        while (fgets(line, sizeof(line), file))
        {
            if (strncmp(line, "SwapTotal:", 10) == 0)
            {
                sscanf(line, "SwapTotal: %f", &swap_info.total_swap);
            }
            else if (strncmp(line, "SwapFree:", 9) == 0)
            {
                sscanf(line, "SwapFree: %f", &swap_info.free_swap);
            }
        }
        fclose(file);
        swap_info.used_swap = swap_info.total_swap - swap_info.free_swap;
    }
    return swap_info;
}

// Function to return system information
PG_FUNCTION_INFO_V1(system_info);

Datum system_info(PG_FUNCTION_ARGS)
{
    SystemSwapInfo swap_info = get_system_swap_info();

    // Convert values from kilobytes to megabytes
    float total_swap_mb = swap_info.total_swap / 1024.0;
    float used_swap_mb = swap_info.used_swap / 1024.0;
    float free_swap_mb = swap_info.free_swap / 1024.0;

    // Define the return columns
    TupleDesc tupdesc = CreateTemplateTupleDesc(3);
    TupleDescInitEntry(tupdesc, (AttrNumber) 1, "total_swap_mb", FLOAT4OID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber) 2, "used_swap_mb", FLOAT4OID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber) 3, "free_swap_mb", FLOAT4OID, -1, 0);

    // Initialize TupleDesc
    BlessTupleDesc(tupdesc);

    // Prepare values
    Datum values[3];
    bool nulls[3] = {false};

    values[0] = Float4GetDatum(total_swap_mb);
    values[1] = Float4GetDatum(used_swap_mb);
    values[2] = Float4GetDatum(free_swap_mb);

    // Create a tuple
    HeapTuple tuple = heap_form_tuple(tupdesc, values, nulls);

    // Return the tuple
    PG_RETURN_DATUM(HeapTupleGetDatum(tuple));
}

// Function to get the number of CPU cores
int get_cpu_cores() {
    FILE *file;
    int cores = 0;
    char line[256];

    file = fopen("/proc/cpuinfo", "r");
    if (file == NULL) {
        ereport(ERROR,
                (errcode_for_file_access(),
                 errmsg("could not open /proc/cpuinfo: %m")));
    }

    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "processor", 9) == 0) {
            cores++;
        }
    }

    fclose(file);
    return cores;
}

// Function to get the system load average
PG_FUNCTION_INFO_V1(pg_loadavg);

Datum pg_loadavg(PG_FUNCTION_ARGS)
{
    FILE *file;
    float load1, load5, load15;
    int running_processes, total_processes, last_pid;
    int cpu_cores = get_cpu_cores();

    // Open /proc/loadavg file
    file = fopen("/proc/loadavg", "r");
    if (file == NULL)
    {
        ereport(ERROR,
                (errcode_for_file_access(),
                 errmsg("could not open /proc/loadavg: %m")));
    }

    // Read load average and process count
    if (fscanf(file, "%f %f %f %d/%d %d", &load1, &load5, &load15, &running_processes, &total_processes, &last_pid) != 6)
    {
        fclose(file);
        ereport(ERROR,
                (errcode(ERRCODE_DATA_EXCEPTION),
                 errmsg("could not parse /proc/loadavg")));
    }
    fclose(file);

    // Define the return columns
    TupleDesc tupdesc = CreateTemplateTupleDesc(7);
    TupleDescInitEntry(tupdesc, (AttrNumber) 1, "load1", FLOAT4OID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber) 2, "load5", FLOAT4OID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber) 3, "load15", FLOAT4OID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber) 4, "running_processes", INT4OID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber) 5, "total_processes", INT4OID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber) 6, "last_pid", INT4OID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber) 7, "cpu_cores", INT4OID, -1, 0);

    // Initialize TupleDesc
    BlessTupleDesc(tupdesc);

    // Prepare values
    Datum values[7];
    bool nulls[7] = {false};

    values[0] = Float4GetDatum(load1);
    values[1] = Float4GetDatum(load5);
    values[2] = Float4GetDatum(load15);
    values[3] = Int32GetDatum(running_processes);
    values[4] = Int32GetDatum(total_processes);
    values[5] = Int32GetDatum(last_pid);
    values[6] = Int32GetDatum(cpu_cores);

    // Create a tuple
    HeapTuple tuple = heap_form_tuple(tupdesc, values, nulls);

    // Return the tuple
    PG_RETURN_DATUM(HeapTupleGetDatum(tuple));
}

// Function to get the frequency of each CPU core
CpuFrequencyInfo* get_cpu_frequencies(int *num_cores) {
    FILE *file;
    char line[256];
    int core_id = -1;
    float frequency_mhz = 0.0;
    int index = 0;

    file = fopen("/proc/cpuinfo", "r");
    if (file == NULL) {
        ereport(ERROR,
                (errcode_for_file_access(),
                 errmsg("could not open /proc/cpuinfo: %m")));
    }

    // Count the number of cores
    *num_cores = get_cpu_cores();

    CpuFrequencyInfo *frequencies = (CpuFrequencyInfo *) palloc(*num_cores * sizeof(CpuFrequencyInfo));
    if (frequencies == NULL) {
        ereport(ERROR,
                (errcode(ERRCODE_OUT_OF_MEMORY),
                 errmsg("could not allocate memory for CPU frequencies")));
    }

    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "processor", 9) == 0) {
            sscanf(line, "processor : %d", &core_id);
        } else if (strncmp(line, "cpu MHz", 7) == 0) {
            char frequency_str[32];
            if (sscanf(line, "cpu MHz : %s", frequency_str) == 1) {
                clean_string(frequency_str); // Clean the string of non-numeric characters
                frequency_mhz = atof(frequency_str); // Convert to float
            } else {
                frequency_mhz = 0.0; // Set default value
            }
            if (core_id != -1) {
                frequencies[index].core_id = core_id;
                frequencies[index].frequency_mhz = frequency_mhz;
                index++;
                core_id = -1;
            }
        }
    }

    fclose(file);

    return frequencies;
}

// Function to return CPU frequency information
PG_FUNCTION_INFO_V1(cpu_frequencies);

Datum cpu_frequencies(PG_FUNCTION_ARGS) {
    FuncCallContext *funcctx;
    TupleDesc        tupdesc;
    AttInMetadata   *attinmeta;
    MemoryContext    oldcontext;

    if (SRF_IS_FIRSTCALL()) {
        // First call of the function
        funcctx = SRF_FIRSTCALL_INIT();
        oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

        // Define the return columns: core_id, frequency_mhz
        tupdesc = CreateTemplateTupleDesc(2);
        TupleDescInitEntry(tupdesc, (AttrNumber) 1, "core_id", INT4OID, -1, 0);
        TupleDescInitEntry(tupdesc, (AttrNumber) 2, "frequency_mhz", FLOAT4OID, -1, 0);  // Use FLOAT4OID

        // Initialize metadata for string conversion
        attinmeta = TupleDescGetAttInMetadata(tupdesc);
        funcctx->attinmeta = attinmeta;

        // Get CPU frequency information
        int num_cores;
        CpuFrequencyInfo *frequencies = get_cpu_frequencies(&num_cores);

        // Save CPU frequency information in user_fctx
        funcctx->user_fctx = frequencies;
        funcctx->max_calls = num_cores; // Number of rows to return

        MemoryContextSwitchTo(oldcontext);
    }

    funcctx = SRF_PERCALL_SETUP();
    CpuFrequencyInfo *frequencies = (CpuFrequencyInfo *) funcctx->user_fctx;

    // Return data for one core at a time
    if (funcctx->call_cntr < funcctx->max_calls) {
        int core_index = funcctx->call_cntr;
        CpuFrequencyInfo *core_info = &frequencies[core_index];

        // Prepare values
        Datum values[2];
        bool nulls[2] = {false};

        values[0] = Int32GetDatum(core_info->core_id);
        values[1] = Float4GetDatum(core_info->frequency_mhz);  // Return as float

        // Create a tuple
        HeapTuple tuple = heap_form_tuple(funcctx->attinmeta->tupdesc, values, nulls);

        // Return the tuple
        SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
    } else {
        // Free memory allocated for frequencies
        if (frequencies != NULL) {
            pfree(frequencies);
        }

        // End function execution
        SRF_RETURN_DONE(funcctx);
    }
}

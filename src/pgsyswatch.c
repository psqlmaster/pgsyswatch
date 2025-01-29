// src/pgsyswatch.c
// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Alexander Scheglov
#include "postgres.h"
#include "fmgr.h"
#include "utils/elog.h"
#include "utils/array.h"
#include "access/htup.h"
#include "catalog/pg_type.h"
#include "utils/builtins.h"
#include "funcapi.h"        // For SRF (Set Returning Functions)
#include "executor/spi.h"   // For working with tuples
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/time.h>
#include <unistd.h>         // For sysconf

#include "pgsyswatch_common.h" 
#include "system_info.h" 

PG_MODULE_MAGIC;

// Function to retrieve process information by PID
PG_FUNCTION_INFO_V1(proc_monitor);

Datum proc_monitor(PG_FUNCTION_ARGS)
{
    // Get the PID from the function arguments
    int pid = PG_GETARG_INT32(0);

    // Retrieve process information
    ProcessInfo process = get_process_info(pid);

    // Define the structure of the returned columns
    TupleDesc tupdesc = CreateTemplateTupleDesc(14);
    TupleDescInitEntry(tupdesc, (AttrNumber) 1, "pid", INT4OID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber) 2, "res_mb", FLOAT4OID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber) 3, "virt_mb", FLOAT4OID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber) 4, "swap_mb", FLOAT4OID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber) 5, "command", TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber) 6, "state", TEXTOID, -1, 0); 
    TupleDescInitEntry(tupdesc, (AttrNumber) 7, "utime", INT8OID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber) 8, "stime", INT8OID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber) 9, "cpu_usage", FLOAT4OID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber) 10, "read_bytes", INT8OID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber) 11, "write_bytes", INT8OID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber) 12, "voluntary_ctxt_switches", INT4OID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber) 13, "nonvoluntary_ctxt_switches", INT4OID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber) 14, "threads", INT4OID, -1, 0);

    // Initialize the TupleDesc
    BlessTupleDesc(tupdesc);

    Datum values[14];
    bool nulls[14] = {false};

    // Convert process state (char) to a string (text)
    char state_str[2] = {process.state, '\0'};
    values[0] = Int32GetDatum(process.pid);
    values[1] = Float4GetDatum(process.res_mb);
    values[2] = Float4GetDatum(process.virt_mb);
    values[3] = Float4GetDatum(process.swap_mb);
    if (process.command == NULL) {
        process.command = strdup("Unknown");
    }
    values[4] = CStringGetTextDatum(process.command);
    values[5] = CStringGetTextDatum(state_str);
    values[6] = Int64GetDatum(process.utime);
    values[7] = Int64GetDatum(process.stime);
    values[8] = Float4GetDatum(process.cpu_usage);
    values[9] = Int64GetDatum(process.read_bytes);
    values[10] = Int64GetDatum(process.write_bytes);
    values[11] = Int32GetDatum(process.voluntary_ctxt_switches);
    values[12] = Int32GetDatum(process.nonvoluntary_ctxt_switches);
    values[13] = Int32GetDatum(process.threads);

    // Free memory allocated for the command string
    if (process.command != NULL) {
        free(process.command);
    }

    // Create a tuple
    HeapTuple tuple = heap_form_tuple(tupdesc, values, nulls);

    // Return the tuple
    PG_RETURN_DATUM(HeapTupleGetDatum(tuple));
}

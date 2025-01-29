// src/pgsyswatch_all.c
// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Alexander Scheglov
#include "postgres.h"
#include "fmgr.h"
#include "utils/elog.h"
#include "utils/array.h"
#include "access/htup.h"
#include "catalog/pg_type.h"
#include "utils/builtins.h"
#include "funcapi.h"        // For SRF (Set-Returning Functions) support
#include "executor/spi.h"   // For working with tuples
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/time.h>
#include <unistd.h>         // For sysconf

#include "pgsyswatch_common.h"

// Function to retrieve information about all processes
PG_FUNCTION_INFO_V1(proc_monitor_all);

Datum proc_monitor_all(PG_FUNCTION_ARGS)
{
    FuncCallContext *funcctx;
    TupleDesc        tupdesc;
    AttInMetadata   *attinmeta;
    MemoryContext    oldcontext;

    if (SRF_IS_FIRSTCALL())
    {
        funcctx = SRF_FIRSTCALL_INIT();
        oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

        // Define the structure of the returned columns
        tupdesc = CreateTemplateTupleDesc(14); 
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

        // Initialize metadata for string conversion
        attinmeta = TupleDescGetAttInMetadata(tupdesc);
        funcctx->attinmeta = attinmeta;

        // Scan the /proc directory and collect information about all processes
        DIR *dir;
        struct dirent *ent;
        dir = opendir("/proc");
        if (dir == NULL) {
            ereport(ERROR,
                    (errcode_for_file_access(),
                     errmsg("could not open directory /proc")));
        }

        // List to store process information
        List *processes = NIL;

        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_type == DT_DIR && atoi(ent->d_name) > 0) {
                int pid = atoi(ent->d_name);

                // Retrieve process information
                ProcessInfo process = get_process_info(pid);

                // Store process information
                Datum values[14];
                bool nulls[14] = {false};

                char state_str[2] = {process.state, '\0'};  // Create a string from the state character
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

                // Create a tuple and add it to the list
                HeapTuple tuple = heap_form_tuple(tupdesc, values, nulls);
                processes = lappend(processes, tuple);
            }
        }

        closedir(dir);

        // Save the list of processes in user_fctx
        funcctx->user_fctx = processes;
        MemoryContextSwitchTo(oldcontext);
    }

    funcctx = SRF_PERCALL_SETUP();
    List *processes = (List *) funcctx->user_fctx;

    // Return processes one by one
    if (list_length(processes) > 0) {
        HeapTuple tuple = (HeapTuple) linitial(processes);
        processes = list_delete_first(processes);
        funcctx->user_fctx = processes;

        SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
    } else {
        SRF_RETURN_DONE(funcctx);
    }
}

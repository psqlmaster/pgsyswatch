/* pgsyswatch_common.h
SPDX-License-Identifier: Apache-2.0
Copyright 2025 Alexander Scheglov */
#include "postgres.h"
#include "fmgr.h"
#include "funcapi.h"
#include "utils/builtins.h"
#include "access/htup.h"
#include "catalog/pg_type.h"
#include "executor/spi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pgsyswatch_common.h"

/* Function to retrieve general network information */
PG_FUNCTION_INFO_V1(net_monitor);

Datum net_monitor(PG_FUNCTION_ARGS)
{
    FuncCallContext *funcctx;

    /* First call to the function */
    if (SRF_IS_FIRSTCALL()) {
        MemoryContext oldcontext;
        TupleDesc tupdesc;

        /* Create a context for SRF operation */
        funcctx = SRF_FIRSTCALL_INIT();

        /* Switch memory context */
        oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

        /* Create a description of the return type */
        if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE) {
            ereport(ERROR,
                    (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                     errmsg("Function returning record called in context that cannot accept type record")));
        }

        /* Save the type description in the context */
        funcctx->tuple_desc = BlessTupleDesc(tupdesc);

        /* Open the file /proc/net/dev */
        FILE *file = fopen("/proc/net/dev", "r");
        if (file == NULL) {
            ereport(ERROR,
                    (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                     errmsg("Failed to open /proc/net/dev")));
        }

        /* Skip the first two lines (headers) */
        char line[256];
        fgets(line, sizeof(line), file); /* First line */
        fgets(line, sizeof(line), file); /* Second line */

        /* Read interface data and save it in the context */
        StringInfoData interfaces;
        initStringInfo(&interfaces);
        while (fgets(line, sizeof(line), file)) {
            appendStringInfo(&interfaces, "%s", line);
        }
        fclose(file);

        /* Save the data in the context */
        funcctx->user_fctx = interfaces.data;

        /* Restore the memory context */
        MemoryContextSwitchTo(oldcontext);
    }

    /* Subsequent calls to the function */
    funcctx = SRF_PERCALL_SETUP();

    /* Retrieve the saved data */
    char *interfaces = (char *) funcctx->user_fctx;

    /* If the data is exhausted, finish the operation */
    if (interfaces == NULL || *interfaces == '\0') {
        SRF_RETURN_DONE(funcctx);
    }

    /* Parse the interface data line */
    char face[32];
    unsigned long long receive_bytes, receive_packets, receive_errs, receive_drop;
    unsigned long long transmit_bytes, transmit_packets, transmit_errs, transmit_drop;

    if (sscanf(interfaces, "%31[^:]: %llu %llu %llu %llu %*u %*u %*u %*u %llu %llu %llu %llu",
               face, &receive_bytes, &receive_packets, &receive_errs, &receive_drop,
               &transmit_bytes, &transmit_packets, &transmit_errs, &transmit_drop) == 9) {

        /* Prepare data for return */
        Datum values[9];
        bool nulls[9] = {false};

        values[0] = CStringGetTextDatum(face);
        values[1] = Int64GetDatum(receive_bytes);
        values[2] = Int64GetDatum(receive_packets);
        values[3] = Int64GetDatum(receive_errs);
        values[4] = Int64GetDatum(receive_drop);
        values[5] = Int64GetDatum(transmit_bytes);
        values[6] = Int64GetDatum(transmit_packets);
        values[7] = Int64GetDatum(transmit_errs);
        values[8] = Int64GetDatum(transmit_drop);

        /* Create a tuple */
        HeapTuple tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);

        /* Move to the next line of data */
        interfaces = strchr(interfaces, '\n');
        if (interfaces != NULL) {
            interfaces++; /* Skip the newline character */
        }
        funcctx->user_fctx = interfaces;

        /* Return the tuple */
        SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
    }

    /* If the data is exhausted, finish the operation */
    SRF_RETURN_DONE(funcctx);
}

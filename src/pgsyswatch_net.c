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

// Функция для выборки общей сетевой информации
PG_FUNCTION_INFO_V1(net_monitor);

Datum net_monitor(PG_FUNCTION_ARGS)
{
    FuncCallContext *funcctx;

    // Первый вызов функции
    if (SRF_IS_FIRSTCALL()) {
        MemoryContext oldcontext;
        TupleDesc tupdesc;

        // Создаем контекст для работы с SRF
        funcctx = SRF_FIRSTCALL_INIT();

        // Переключаем контекст памяти
        oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

        // Создаем описание возвращаемого типа
        if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE) {
            ereport(ERROR,
                    (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                     errmsg("Function returning record called in context that cannot accept type record")));
        }

        // Сохраняем описание типа в контексте
        funcctx->tuple_desc = BlessTupleDesc(tupdesc);

        // Открываем файл /proc/net/dev
        FILE *file = fopen("/proc/net/dev", "r");
        if (file == NULL) {
            ereport(ERROR,
                    (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                     errmsg("Failed to open /proc/net/dev")));
        }

        // Пропускаем первые две строки (заголовки)
        char line[256];
        fgets(line, sizeof(line), file); // Первая строка
        fgets(line, sizeof(line), file); // Вторая строка

        // Читаем данные интерфейсов и сохраняем их в контексте
        StringInfoData interfaces;
        initStringInfo(&interfaces);
        while (fgets(line, sizeof(line), file)) {
            appendStringInfo(&interfaces, "%s", line);
        }
        fclose(file);

        // Сохраняем данные в контексте
        funcctx->user_fctx = interfaces.data;

        // Восстанавливаем контекст памяти
        MemoryContextSwitchTo(oldcontext);
    }

    // Последующие вызовы функции
    funcctx = SRF_PERCALL_SETUP();

    // Получаем сохраненные данные
    char *interfaces = (char *) funcctx->user_fctx;

    // Если данные закончились, завершаем работу
    if (interfaces == NULL || *interfaces == '\0') {
        DEBUG_PRINT("No more interfaces to process\n");
        SRF_RETURN_DONE(funcctx);
    }

    // Парсим строку с данными интерфейса
    char face[32];
    unsigned long long receive_bytes, receive_packets, receive_errs, receive_drop;
    unsigned long long transmit_bytes, transmit_packets, transmit_errs, transmit_drop;

    if (sscanf(interfaces, "%31[^:]: %llu %llu %llu %llu %*u %*u %*u %*u %llu %llu %llu %llu",
               face, &receive_bytes, &receive_packets, &receive_errs, &receive_drop,
               &transmit_bytes, &transmit_packets, &transmit_errs, &transmit_drop) == 9) {

        DEBUG_PRINT("Processing interface: %s\n", face);

        // Подготовка данных для возврата
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

        // Создание кортежа
        HeapTuple tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);

        // Переходим к следующей строке данных
        interfaces = strchr(interfaces, '\n');
        if (interfaces != NULL) {
            interfaces++; // Пропускаем символ новой строки
        }
        funcctx->user_fctx = interfaces;

        // Возвращаем кортеж
        SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
    }

    // Если данные закончились, завершаем работу
    DEBUG_PRINT("Finished processing interfaces\n");
    SRF_RETURN_DONE(funcctx);
}

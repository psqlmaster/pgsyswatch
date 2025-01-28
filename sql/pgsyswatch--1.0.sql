-- pgsyswatch--1.0.sql
-- Копируем расширение в /usr/local/pgsql/lib/pgsyswatch
-- Создаем схему для расширения
DROP SCHEMA IF EXISTS pgsyswatch CASCADE;
-- Создаем схему для расширения (если она еще не создана)
CREATE SCHEMA IF NOT EXISTS pgsyswatch;

-- Устанавливаем схему по умолчанию для текущего сеанса
SET search_path TO pgsyswatch;

-- Создаем тип для возвращаемых данных о процессах
CREATE TYPE pgsyswatch.proc_monitor_type AS (
    pid INTEGER,
    res_mb FLOAT4,  -- Память (VmRSS)
    virt_mb FLOAT4, -- Виртуальная память (VmSize)
    swap_mb FLOAT4, -- Своп (VmSwap)
    command TEXT, -- cmd
    state TEXT, -- Состояние процесса
    utime BIGINT,   -- Время в пользовательском режиме (тики)
    stime BIGINT,   -- Время в системном режиме (тики)
    cpu_usage FLOAT4, -- Процент использования CPU
    read_bytes BIGINT, -- Количество прочитанных байт с диска
    write_bytes BIGINT, -- Количество записанных байт на диск
    voluntary_ctxt_switches INT4, -- Добровольные переключения контекста
    nonvoluntary_ctxt_switches INT4, -- Принудительные переключения контекста
    threads INT4 -- Количество потоков
);

-- Создаем тип для возвращаемых данных о системе (swap)
CREATE TYPE system_info_type AS (
    total_swap_mb FLOAT4,
    used_swap_mb FLOAT4,
    free_swap_mb FLOAT4
);

-- Создаем функцию для получения информации о swap
CREATE FUNCTION system_swap_info()
RETURNS system_info_type
LANGUAGE c
AS '/usr/local/pgsql/lib/pgsyswatch', 'system_info';

-- Создаем тип для возвращаемых данных о средней нагрузке системы
CREATE TYPE loadavg_type AS (
    load1 FLOAT4,            -- Средняя нагрузка за 1 минуту
    load5 FLOAT4,            -- Средняя нагрузка за 5 минут
    load15 FLOAT4,           -- Средняя нагрузка за 15 минут
    running_processes INT4,  -- Количество выполняющихся процессов
    total_processes INT4,    -- Общее количество процессов
    last_pid INT4,           -- Последний PID
    cpu_cores INT4           -- Количество ядер CPU
);

-- Создаем тип для возвращаемых данных о частоте процессора
CREATE TYPE cpu_frequency_type AS (
    core_id int,
    frequency_mhz FLOAT4
);

-- Создаем функцию для получения информации о частоте процессора
CREATE FUNCTION cpu_frequencies()
RETURNS SETOF cpu_frequency_type
LANGUAGE c
AS '/usr/local/pgsql/lib/pgsyswatch', 'cpu_frequencies';

-- Создаем функцию для получения средней нагрузки системы
CREATE FUNCTION pg_loadavg()
RETURNS loadavg_type
LANGUAGE c
AS '/usr/local/pgsql/lib/pgsyswatch', 'pg_loadavg';

-- Создаем функцию для мониторинга процессов по PID
CREATE FUNCTION proc_monitor(IN pid INTEGER)
RETURNS proc_monitor_type
LANGUAGE c
AS '/usr/local/pgsql/lib/pgsyswatch', 'proc_monitor';

-- Создаем функцию для мониторинга всех процессов
CREATE FUNCTION proc_monitor_all()
RETURNS SETOF proc_monitor_type
LANGUAGE c
AS '/usr/local/pgsql/lib/pgsyswatch', 'proc_monitor_all';

-- Создаем VIEW для отображения информации о процессах
CREATE OR REPLACE VIEW pg_stat_activity_ext AS
SELECT 
    p.pid, 
    a.datname, 
    a.usename, 
    a.application_name, 
    a.state state_q, 
    a.query, 
    p.res_mb, 
    p.virt_mb, 
    p.swap_mb, 
    p.command,
    p.state::text,
    p.utime, 
    p.stime,
    p.cpu_usage "pcpu",
    p.read_bytes,
    p.write_bytes,
    p.voluntary_ctxt_switches,
    p.nonvoluntary_ctxt_switches,
    p.threads
FROM 
    pg_stat_activity a
LEFT JOIN LATERAL (
    SELECT * FROM pgsyswatch.proc_monitor(a.pid)
) p ON true
WHERE 
    a.pid IS NOT NULL;
-- Создаем VIEW для отображения информации обо всех процессах
CREATE OR REPLACE VIEW pg_proc_activity AS
select
	p.pid,
	a.datname,
	a.usename,
	a.application_name,
	a.state state_q,
	a.query,
	p.res_mb,
	p.virt_mb,
	p.swap_mb,
	p.command,
    p.state::text,
	p.utime,
	p.stime,
	p.cpu_usage "pcpu",
	p.read_bytes,
	p.write_bytes,
	p.voluntary_ctxt_switches,
	p.nonvoluntary_ctxt_switches,
	p.threads
from
	pg_stat_activity a
right join pgsyswatch.proc_monitor_all() p
		using(pid);   

-- Создаем VIEW для отображения информации о всех процессах в системе
CREATE VIEW pg_all_processes AS
SELECT 
    p.pid, 
    p.res_mb, 
    p.virt_mb, 
    p.swap_mb, 
    p.command,
    p.state::text,
    p.utime, 
    p.stime,
    p.cpu_usage "pcpu",
    p.read_bytes,
    p.write_bytes,
    p.voluntary_ctxt_switches,
    p.nonvoluntary_ctxt_switches,
    p.threads
FROM 
    proc_monitor_all() p;

-- Создаем основную секционированную таблицу
CREATE TABLE proc_activity_snapshots (
	ts TIMESTAMP DEFAULT NOW(),
    pid INT4,
    datname NAME,
    usename NAME,
    application_name TEXT COMPRESSION pglz,  -- Сжатие для текстового поля
    state_q TEXT COMPRESSION pglz,           -- Сжатие для текстового поля
    query TEXT COMPRESSION pglz,             -- Сжатие для текстового поля
    res_mb FLOAT4,
    virt_mb FLOAT4,
    swap_mb FLOAT4,
    command TEXT COMPRESSION pglz,           -- Сжатие для текстового поля
    state TEXT COMPRESSION pglz,             -- Сжатие для текстового поля
    utime FLOAT4,
    stime FLOAT4,
    pcpu FLOAT4,
    read_bytes INT8,
    write_bytes INT8,
    voluntary_ctxt_switches INT8,
    nonvoluntary_ctxt_switches INT8,
    threads INT4
) PARTITION BY RANGE (ts);  -- Секционирование по диапазону дат
-- Создаем дефолтную партицию
CREATE TABLE proc_activity_snapshots_default PARTITION OF pgsyswatch.proc_activity_snapshots
DEFAULT;

-- Создаем функцию
CREATE OR REPLACE FUNCTION manage_partitions_maintenance()
RETURNS INT4
LANGUAGE plpgsql
VOLATILE
AS $$
DECLARE
    curr_day DATE := CURRENT_DATE;  -- Текущая дата
    next_day DATE := curr_day + INTERVAL '1 day';  -- Следующий день
    partition_name_curr TEXT := 'proc_activity_snapshots_' || TO_CHAR(curr_day, 'YYYYMMDD');  -- Имя партиции для текущего дня
    partition_name_next TEXT := 'proc_activity_snapshots_' || TO_CHAR(next_day, 'YYYYMMDD');  -- Имя партиции для следующего дня
    partition_start_curr TEXT := TO_CHAR(curr_day, 'YYYY-MM-DD');  -- Начало текущей партиции
    partition_end_curr TEXT := TO_CHAR(curr_day + INTERVAL '1 day', 'YYYY-MM-DD');  -- Конец текущей партиции
    partition_start_next TEXT := TO_CHAR(next_day, 'YYYY-MM-DD');  -- Начало следующей партиции
    partition_end_next TEXT := TO_CHAR(next_day + INTERVAL '1 day', 'YYYY-MM-DD');  -- Конец следующей партиции
    oldest_allowed DATE := CURRENT_DATE - INTERVAL '3 day';  -- Самая старая допустимая партиция (3 дня для тестирования)
    existing_partition TEXT;  -- Переменная для хранения имени существующей партиции
BEGIN
    SET client_min_messages TO WARNING;

    -- Отладочная информация: вывод текущих значений переменных
    RAISE NOTICE 'curr_day: %, next_day: %, partition_name_curr: %, partition_name_next: %',
        curr_day, next_day, partition_name_curr, partition_name_next;
    RAISE NOTICE 'partition_start_curr: %, partition_end_curr: %, partition_start_next: %, partition_end_next: %',
        partition_start_curr, partition_end_curr, partition_start_next, partition_end_next;

    -- Проверяем, существует ли партиция для текущего дня
    SELECT child.relname INTO existing_partition
    FROM pg_inherits
    JOIN pg_class parent ON pg_inherits.inhparent = parent.oid
    JOIN pg_class child ON pg_inherits.inhrelid = child.oid
    WHERE parent.relname = 'proc_activity_snapshots'
      AND child.relname = partition_name_curr;

    -- Если партиция для текущего дня не существует, создаем её
    IF existing_partition IS NULL THEN
        RAISE NOTICE 'Creating partition for current day: %', partition_name_curr;
        EXECUTE format('
            CREATE TABLE pgsyswatch.%I PARTITION OF pgsyswatch.proc_activity_snapshots
            FOR VALUES FROM (''%s'') TO (''%s'');
        ', partition_name_curr, partition_start_curr, partition_end_curr);
        RAISE NOTICE 'Partition created: %', partition_name_curr;
    ELSE
        RAISE NOTICE 'Partition % already exists, skipping creation.', partition_name_curr;
    END IF;

    -- Проверяем, существует ли партиция для следующего дня
    SELECT child.relname INTO existing_partition
    FROM pg_inherits
    JOIN pg_class parent ON pg_inherits.inhparent = parent.oid
    JOIN pg_class child ON pg_inherits.inhrelid = child.oid
    WHERE parent.relname = 'proc_activity_snapshots'
      AND child.relname = partition_name_next;

    -- Если партиция для следующего дня не существует, создаем её
    IF existing_partition IS NULL THEN
        RAISE NOTICE 'Creating partition for next day: %', partition_name_next;
        EXECUTE format('
            CREATE TABLE pgsyswatch.%I PARTITION OF pgsyswatch.proc_activity_snapshots
            FOR VALUES FROM (''%s'') TO (''%s'');
        ', partition_name_next, partition_start_next, partition_end_next);
        RAISE NOTICE 'Partition created: %', partition_name_next;
    ELSE
        RAISE NOTICE 'Partition % already exists, skipping creation.', partition_name_next;
    END IF;

    -- Удаляем старые партиции, которые выходят за пределы периода хранения
    FOR existing_partition IN
        SELECT child.relname
        FROM pg_inherits
        JOIN pg_class parent ON pg_inherits.inhparent = parent.oid
        JOIN pg_class child ON pg_inherits.inhrelid = child.oid
        WHERE parent.relname = 'proc_activity_snapshots'
          AND child.relname <> 'proc_activity_snapshots_default'
    LOOP
        BEGIN
            -- Проверяем, что имя партиции соответствует формату даты
            IF existing_partition ~ '^proc_activity_snapshots_\d{8}$' THEN
                -- Извлекаем дату из имени партиции
                IF TO_DATE(SUBSTRING(existing_partition FROM '\d{8}$'), 'YYYYMMDD') < oldest_allowed THEN
                    EXECUTE format('DROP TABLE pgsyswatch.%I', existing_partition);
                    RAISE NOTICE 'Partition % dropped.', existing_partition;
                END IF;
            END IF;
        EXCEPTION
            WHEN OTHERS THEN
                -- Логируем ошибку и продолжаем
                RAISE NOTICE 'Error processing partition %: %', existing_partition, SQLERRM;
        END;
    END LOOP;

    -- Если всё успешно, возвращаем 0
    RETURN 0;

EXCEPTION
    WHEN OTHERS THEN
        -- В случае ошибки логируем её
        RAISE NOTICE 'Error occurred: %', SQLERRM;
        RETURN -1;
END;
$$;

COMMENT ON FUNCTION manage_partitions_maintenance() IS '
Function for managing partitions in the pgsyswatch.proc_activity_snapshots table.

Description:
1. Creates partitions for the current and next day if they do not exist.
2. Drops partitions older than the retention period (default: 3 days for testing).
3. Returns 0 on success and -1 on error.

Usage Recommendations:
- Run this function once a day, e.g., at 00:00.
- This minimizes database load and ensures partitions are up-to-date.

Example Usage:
SELECT pgsyswatch.manage_partitions_maintenance();

Author: @sqlmaster (Telegram)
Version: 1.0.0
';
-- Сбрасываем search_path обратно к значению по умолчанию
RESET search_path;

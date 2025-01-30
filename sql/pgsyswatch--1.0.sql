-- pgsyswatch--1.0.sql
-- Copying the extension to /usr/local/pgsql/lib/pgsyswatch
-- Creating a schema for the extension
DROP SCHEMA IF EXISTS pgsyswatch CASCADE;
-- Creating a schema for the extension (if it has not been created yet)
CREATE SCHEMA IF NOT EXISTS pgsyswatch;

-- Setting the default schema for the current session
SET search_path TO pgsyswatch;

-- Creating a type for returned process monitoring data
CREATE TYPE pgsyswatch.proc_monitor_type AS (
    pid INTEGER,
    res_mb FLOAT4,  -- Memory (VmRSS)
    virt_mb FLOAT4, -- Virtual memory (VmSize)
    swap_mb FLOAT4, -- Swap (VmSwap)
    command TEXT, -- cmd
    state TEXT, -- Process state
    utime BIGINT,   -- User mode time (ticks)
    stime BIGINT,   -- System mode time (ticks)
    cpu_usage FLOAT4, -- CPU usage percentage
    read_bytes BIGINT, -- Number of bytes read from disk
    write_bytes BIGINT, -- Number of bytes written to disk
    voluntary_ctxt_switches INT4, -- Voluntary context switches
    nonvoluntary_ctxt_switches INT4, -- Involuntary context switches
    threads INT4 -- Number of threads
);

-- Creating a type for returned system swap data
CREATE TYPE system_info_type AS (
    total_swap_mb FLOAT4,
    used_swap_mb FLOAT4,
    free_swap_mb FLOAT4
);

-- Creating a function to retrieve swap information
CREATE FUNCTION system_swap_info()
RETURNS system_info_type
LANGUAGE c
AS '/usr/local/pgsql/lib/pgsyswatch', 'system_info';

-- Creating a type for returned system load average data
CREATE TYPE loadavg_type AS (
    load1 FLOAT4,            -- Load average over 1 minute
    load5 FLOAT4,            -- Load average over 5 minutes
    load15 FLOAT4,           -- Load average over 15 minutes
    running_processes INT4,  -- Number of running processes
    total_processes INT4,    -- Total number of processes
    last_pid INT4,           -- Last PID
    cpu_cores INT4           -- Number of CPU cores
);

-- Creating a type for returned CPU frequency data
CREATE TYPE cpu_frequency_type AS (
    core_id int,
    frequency_mhz FLOAT4
);

-- Creating a function to retrieve CPU frequency information
CREATE FUNCTION cpu_frequencies()
RETURNS SETOF cpu_frequency_type
LANGUAGE c
AS '/usr/local/pgsql/lib/pgsyswatch', 'cpu_frequencies';

-- Creating a function to retrieve system load average
CREATE FUNCTION pg_loadavg()
RETURNS loadavg_type
LANGUAGE c
AS '/usr/local/pgsql/lib/pgsyswatch', 'pg_loadavg';

-- Creating a function for monitoring processes by PID
CREATE FUNCTION proc_monitor(IN pid INTEGER)
RETURNS proc_monitor_type
LANGUAGE c
AS '/usr/local/pgsql/lib/pgsyswatch', 'proc_monitor';

-- Creating a function for monitoring all processes
CREATE FUNCTION proc_monitor_all()
RETURNS SETOF proc_monitor_type
LANGUAGE c
AS '/usr/local/pgsql/lib/pgsyswatch', 'proc_monitor_all';

-- Creating a VIEW to display process information
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
-- Creating a VIEW to display information about all processes
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

-- Creating a VIEW to display information about all system processes
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

-- Creating the main partitioned table
CREATE TABLE proc_activity_snapshots (
	ts TIMESTAMP DEFAULT NOW(),
    pid INT4,
    datname NAME,
    usename NAME,
    application_name TEXT COMPRESSION pglz,  -- Compression for text field
    state_q TEXT COMPRESSION pglz,           -- Compression for text field
    query TEXT COMPRESSION pglz,             -- Compression for text field
    res_mb FLOAT4,
    virt_mb FLOAT4,
    swap_mb FLOAT4,
    command TEXT COMPRESSION pglz,           -- Compression for text field
    state TEXT COMPRESSION pglz,             -- Compression for text field
    utime FLOAT4,
    stime FLOAT4,
    pcpu FLOAT4,
    read_bytes INT8,
    write_bytes INT8,
    voluntary_ctxt_switches INT8,
    nonvoluntary_ctxt_switches INT8,
    threads INT4
) PARTITION BY RANGE (ts);  -- Partitioning by date range
-- Creating the default partition
CREATE TABLE proc_activity_snapshots_default PARTITION OF pgsyswatch.proc_activity_snapshots
DEFAULT;

-- Creating the function as manager partitions
CREATE OR REPLACE FUNCTION manage_partitions_maintenance()
RETURNS INT4
LANGUAGE plpgsql
VOLATILE
AS $$
DECLARE
    curr_day DATE := CURRENT_DATE;   -- Current date
    next_day DATE := curr_day + INTERVAL '1 day';  -- Next day
    partition_name_curr TEXT := 'proc_activity_snapshots_' || TO_CHAR(curr_day, 'YYYYMMDD');  -- Partition name for the current day
    partition_name_next TEXT := 'proc_activity_snapshots_' || TO_CHAR(next_day, 'YYYYMMDD');  -- Partition name for the next day
    partition_start_curr TEXT := TO_CHAR(curr_day, 'YYYY-MM-DD');  -- Start of the current partition
    partition_end_curr TEXT := TO_CHAR(curr_day + INTERVAL '1 day', 'YYYY-MM-DD');  -- End of the current partition
    partition_start_next TEXT := TO_CHAR(next_day, 'YYYY-MM-DD');  -- Start of the next partition
    partition_end_next TEXT := TO_CHAR(next_day + INTERVAL '1 day', 'YYYY-MM-DD');  -- End of the next partition
    oldest_allowed DATE := CURRENT_DATE - INTERVAL '1 month';  -- Oldest allowed partition (3 days for testing) !!! This change the interval for your history storage depth !!!!!!!!!!!!!!
    existing_partition TEXT;  -- Variable to store the name of an existing partition
BEGIN
    SET client_min_messages TO WARNING;

    -- Debug information: output current variable values
    RAISE NOTICE 'curr_day: %, next_day: %, partition_name_curr: %, partition_name_next: %',
        curr_day, next_day, partition_name_curr, partition_name_next;
    RAISE NOTICE 'partition_start_curr: %, partition_end_curr: %, partition_start_next: %, partition_end_next: %',
        partition_start_curr, partition_end_curr, partition_start_next, partition_end_next;

    -- Checking if the partition for the current day exists
    SELECT child.relname INTO existing_partition
    FROM pg_inherits
    JOIN pg_class parent ON pg_inherits.inhparent = parent.oid
    JOIN pg_class child ON pg_inherits.inhrelid = child.oid
    WHERE parent.relname = 'proc_activity_snapshots'
      AND child.relname = partition_name_curr;

    -- If the partition for the current day does not exist, create it
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

    -- Check if the partition for the next day exists
    SELECT child.relname INTO existing_partition
    FROM pg_inherits
    JOIN pg_class parent ON pg_inherits.inhparent = parent.oid
    JOIN pg_class child ON pg_inherits.inhrelid = child.oid
    WHERE parent.relname = 'proc_activity_snapshots'
      AND child.relname = partition_name_next;

    -- If the partition for the next day does not exist, create it
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

    -- Delete old partitions older than the allowed period
    FOR existing_partition IN
        SELECT child.relname
        FROM pg_inherits
        JOIN pg_class parent ON pg_inherits.inhparent = parent.oid
        JOIN pg_class child ON pg_inherits.inhrelid = child.oid
        WHERE parent.relname = 'proc_activity_snapshots'
          AND child.relname <> 'proc_activity_snapshots_default'
    LOOP
        BEGIN
            -- Check that the partition name matches the date format
            IF existing_partition ~ '^proc_activity_snapshots_\d{8}$' THEN
                -- Extract date from partition name
                IF TO_DATE(SUBSTRING(existing_partition FROM '\d{8}$'), 'YYYYMMDD') < oldest_allowed THEN
                    EXECUTE format('DROP TABLE pgsyswatch.%I', existing_partition);
                    RAISE NOTICE 'Partition % dropped.', existing_partition;
                END IF;
            END IF;
        EXCEPTION
            WHEN OTHERS THEN
                -- We log the error and continue
                RAISE NOTICE 'Error processing partition %: %', existing_partition, SQLERRM;
        END;
    END LOOP;

    -- If everything is successful, we return 0
    RETURN 0;

EXCEPTION
    WHEN OTHERS THEN
        -- In case of an error, we log it
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
-- Reset search_path back to default
RESET search_path;

INSERT INTO pgsyswatch.proc_activity_snapshots (
    pid, datname, usename, application_name, state_q, 
    query, res_mb, virt_mb, swap_mb, command,state, utime, 
    stime, pcpu, read_bytes, write_bytes, voluntary_ctxt_switches, 
    nonvoluntary_ctxt_switches, threads
)
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
    p.state,
    p.utime,
    p.stime,
    p.cpu_usage,
    p.read_bytes,
    p.write_bytes,
    p.voluntary_ctxt_switches,
    p.nonvoluntary_ctxt_switches,
    p.threads
FROM
    pg_stat_activity a
RIGHT JOIN pgsyswatch.proc_monitor_all() p
    USING(pid);

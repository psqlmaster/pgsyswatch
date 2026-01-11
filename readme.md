<img src="pgsyswatch.png" alt="Описание изображения" width="200" />

**Pgsyswatch** is a PostgreSQL extension designed for monitoring system processes. It provides functions to retrieve detailed information about running processes on the system, such as CPU usage, memory consumption, I/O statistics, and more. 

- This tool is NOT intended for beginners.  It requires extensive knowledge of PostgreSQL.
---
#### Features 

- **Process Monitoring** : Retrieve information about processes by PID or all system processes.
- **Memory Usage** : Monitor RAM, virtual memory, and swap usage.
- **CPU & I/O Stats** : Gather CPU usage and disk I/O statistics.
- **System Load** : Access system load average information.
- **CPU Frequency** : Monitor CPU frequency for all cores.
- **Historical Data** : Store historical process data in partitioned tables.
- **Online binding of the system process** and its consumption by the main metrics to a specific SQL query + history of snapshots once a minute in a partitioned table with automatic creation and deletion of old and new sections.
- **Network Monitoring** : Retrieve detailed network interface statistics, including received/transmitted bytes, packets, errors, and drops, with support for real-time monitoring and historical data storage.

---

#### Preview
<img src="example.gif" width=1000 />

### Installation 

#### Prerequisites 

- GCC or another C compiler
- PostgreSQL (version 9.6 or higher)

#### Installation Steps

1. Clone the repository:
   ```bash
   git clone --depth 1 https://github.com/psqlmaster/pgsyswatch.git
   cd pgsyswatch
   ```

2. Build the extension:
   ```bash
   make
   ```

3. Install the extension:
   ```bash
   sudo make install
   ```
    To check PostgreSQL processes:
   ```bash
   ps auxww | grep postgres | grep -v grep
   ```

   Example output:
   ```
   postgres  135184  0.0  0.0 161240 17964 ?        Ss   15:34   0:00 /usr/local/pgsql/bin/postgres -D /usr/local/pgsql/data
   ```

   To edit PostgreSQL configuration:
   ```bash
   sudo vim /usr/local/pgsql/data/postgresql.conf
   ```

   Add the following line to enable `pgsyswatch`:
   ```bash
   shared_preload_libraries = 'pgsyswatch' # (change requires restart)
   ```
4. Restart PostgreSQL to apply changes:
   ```bash
   sudo systemctl restart postgresql
   ```

5. Create the extension in your PostgreSQL database:
   ```sql
   CREATE EXTENSION pgsyswatch;
   ```
6. Add crontab:
   ```
   * * * * * /your_path/pgsyswatch/sys_proc_maintenance.sh 0
   ```

- if all good mast you get information in log file /logs/import_data_snapshots_20250128.log:
```
2025-01-28 21:54:58 - INSERT 0 446
2025-01-28 22:05:01 - INSERT 0 453
2025-01-28 22:06:02 - INSERT 0 453
```

---

#### Usage 

##### Functions

- **`proc_monitor(pid INTEGER)`** : Retrieves detailed information about a specific process by its PID.
- **`proc_monitor_all()`** : Retrieves detailed information about all running processes.

```sql
\x on
select * from pgsyswatch.proc_monitor_all();
```
| Parameter | Value | Description |
|-----------|-------|-------------|
| pid | 1 | Process ID |
| res_mb | 12.691406 | Resident memory usage in MB |
| virt_mb | 164.53125 | Virtual memory usage in MB |
| swap_mb | 0 | Swap usage in MB |
| command | /sbin/init | Command that started the process |
| state | S | Process state (single letter: R, S, D, Z, etc.) |
| utime | 92 | Time spent in user mode |
| stime | 97 | Time spent in system mode |
| cpu_usage | 0.006027958 | CPU usage percentage |
| read_bytes | 0 | Number of bytes read from disk |
| write_bytes | 0 | Number of bytes written to disk |
| voluntary_ctxt_switches | 3303 | Number of voluntary context switches |
| nonvoluntary_ctxt_switches | 379 | Number of involuntary context switches |
| threads | 1 | Number of threads |
- **`system_swap_info()`** : Returns information about system swap usage.
- **`pg_loadavg()`** : Provides system load average data. (Realtime)
```sql
SELECT * FROM pgsyswatch.pg_loadavg();
```
```
load1|load5|load15|running_processes|total_processes|last_pid|cpu_cores|
-----+-----+------+-----------------+---------------+--------+---------+
 3.88|  3.8|  3.72|                6|           3305|  291506|        8|
```
- **`cpu_frequencies()`** : Returns CPU frequency information for all cores (Realtime trotling monitoring)
```sql
select * from pgsyswatch.cpu_frequencies();
```
```
core_id|frequency_mhz|
-------+-------------+
      0|     4015.276|
      1|     4005.633|
      2|      3999.99|
      3|     4013.485|
      4|     4026.246|
      5|     4027.588|
      6|     4000.008|
      7|     4009.086|
```
- **`pgsyswatch.net_monitor()`** : Retrieve detailed network interface statistics.
```sql
select * from pgsyswatch.net_monitor();
```
```
face  |receive_bytes|receive_packets|receive_errs|receive_drop|transmit_bytes|transmit_packets|transmit_errs|transmit_drop|
------+-------------+---------------+------------+------------+--------------+----------------+-------------+-------------+
    lo|    124907471|         449712|           0|           0|     124907471|          449712|            0|            0|
enp4s0|   1700105660|        5303719|           0|           0|     395151249|         3445988|            0|            0|
```
select * from  pgsyswatch.net_and_loadavg;
select * from  pgsyswatch.net_and_loadavg_snapshots;
##### Views 👀

- **`pg_stat_activity_ext`** : Extends `pg_stat_activity` with process monitoring details.
- **`pg_proc_activity`** : Displays information about all PostgreSQL processes. (Realtime)
- **`pgsyswatch.net_and_loadavg`** : Retrieve detailed network interface & loloadavg information

```sql
select * from  pgsyswatch.net_and_loadavg;
```
```
ts                           |load1|load5|load15|running_processes|total_processes|last_pid|cpu_cores|total_receive_kbytes|total_receive_packets|total_receive_errs|total_receive_drop|total_transmit_kbytes|total_transmit_packets|total_transmit_errs|total_transmit_drop|
-----------------------------+-----+-----+------+-----------------+---------------+--------+---------+--------------------+---------------------+------------------+------------------+---------------------+----------------------+-------------------+-------------------+
2025-02-10 20:20:38.980 +0300| 1.57| 1.22|  1.13|                2|           2598|  171210|        8|             1785310|              5758644|                 0|                 0|               510090|               3900680|                  0|                  0|
```
- **`net_and_loadavg_snapshots`** :  history table for pgsyswatch.net_and_loadavg
```sql
select * from  pgsyswatch.net_and_loadavg_snapshots;
```
```
ts                     |load1|load5|load15|running_processes|total_processes|last_pid|cpu_cores|total_receive_kbytes|total_receive_packets|total_receive_errs|total_receive_drop|total_transmit_kbytes|total_transmit_packets|total_transmit_errs|total_transmit_drop|
-----------------------+-----+-----+------+-----------------+---------------+--------+---------+--------------------+---------------------+------------------+------------------+---------------------+----------------------+-------------------+-------------------+
2025-02-10 20:26:01.386| 0.43| 0.77|  0.95|                1|           2633|  172438|        8|             1793068|              5770176|                 0|                 0|               516675|               3911791|                  0|                  0|
2025-02-10 20:27:01.579| 0.63| 0.78|  0.94|               25|           2627|  172641|        8|             1795185|              5772381|                 0|                 0|               518581|               3913921|                  0|                  0|
2025-02-10 20:28:01.776| 0.88| 0.87|  0.96|                1|           2711|  172953|        8|             1800518|              5776927|                 0|                 0|               522104|               3917605|                  0|                  0|
2025-02-10 20:29:01.971| 1.47| 1.03|  1.01|                2|           2661|  173149|        8|             1801811|              5779111|                 0|                 0|               523082|               3919711|                  0|                  0|
2025-02-10 20:30:02.132| 1.71| 1.15|  1.05|                1|           2660|  173427|        8|             1802861|              5781427|                 0|                 0|               523445|               3922072|                  0|                  0|
```

```sql
select * from  pgsyswatch.pg_proc_activity;
```
```
pid   |datname|usename|application_name                        |state_q|query                                                                                                                                                                                                                                                          |res_mb   |virt_mb  |swap_mb|command                                     |state|utime|stime|pcpu       |read_bytes|write_bytes|voluntary_ctxt_switches|nonvoluntary_ctxt_switches|threads|
------+-------+-------+----------------------------------------+-------+---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+---------+---------+-------+--------------------------------------------+-----+-----+-----+-----------+----------+-----------+-----------------------+--------------------------+-------+
178939|testdb |dba    |psql                                    |idle   |select * from pgsyswatch.proc_monitor_all();                                                                                                                                                                                                                   |12.472656| 159.8164|    0.0|postgres: dba testdb ::1(53758) idle        |S    |    3|    3|0.008594387|         0|          0|                      4|                         8|      1|
182876|testdb |dba    |DBeaver 24.2.3 - Main <testdb>          |idle   |SET search_path = public,public,"$user"                                                                                                                                                                                                                        | 9.855469|159.39844|    0.0|postgres: dba testdb 127.0.0.1(33176) idle  |S    |    0|    0|        0.0|         0|          0|                      9|                         0|      1|
182878|testdb |dba    |DBeaver 24.2.3 - Metadata <testdb>      |idle   |SELECT typinput='pg_catalog.array_in'::regproc as is_array, typtype, typname, pg_type.oid   FROM pg_catalog.pg_type   LEFT JOIN (select ns.oid as nspoid, ns.nspname, r.r           from pg_namespace as ns           join ( select s.r, (current_schemas(false|14.816406| 160.4336|    0.0|postgres: dba testdb 127.0.0.1(33182) idle  |S    |    0|    0|        0.0|         0|          0|                     12|                         0|      1|
182880|testdb |dba    |DBeaver 24.2.3 - SQLEditor <scripts.sql>|active |select * from pgsyswatch.pg_proc_activity where usename~'dba'                                                                                                                                                                                                  |    15.75|161.73828|    0.0|postgres: dba testdb 127.0.0.1(33196) SELECT|R    |   10|    9|  0.3830645|     32768|          0|                     19|                      4182|      1|
```
- **`pg_all_processes`** : Shows details about all system processes.

##### Partitioned Tables 

The extension includes a partitioned table `proc_activity_snapshots` for storing historical process data (`pgsyswatch.proc_monitor_all() JOIN pg_stat_activity`). Partitions are automatically managed by the `manage_partitions_maintenance()` function.

```sql
select * from  pgsyswatch.proc_activity_snapshots;
```
```
ts                     |pid   |datname|usename|application_name                        |state_q|query                                                                                                                                                                                                                                                          |res_mb   |virt_mb  |swap_mb|command                                     |state|utime|stime|pcpu       |read_bytes|write_bytes|voluntary_ctxt_switches|nonvoluntary_ctxt_switches|threads|
-----------------------+------+-------+-------+----------------------------------------+-------+---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+---------+---------+-------+--------------------------------------------+-----+-----+-----+-----------+----------+-----------+-----------------------+--------------------------+-------+
2025-01-28 19:33:48.331|222680|testdb |dba    |DBeaver 24.2.3 - Main <testdb>          |idle   |SET search_path = public,public,"$user"                                                                                                                                                                                                                        |11.019531|159.42578|    0.0|postgres: dba testdb 127.0.0.1(48670) idle  |S    |  0.0|  0.0|        0.0|    933888|          0|                     38|                         0|      1|
2025-01-28 19:33:48.331|222682|testdb |dba    |DBeaver 24.2.3 - Metadata <testdb>      |idle   |SELECT c.relname,a.*,pg_catalog.pg_get_expr(ad.adbin, ad.adrelid, true) as def_value,dsc.description,dep.objid¶FROM pg_catalog.pg_attribute a¶INNER JOIN pg_catalog.pg_class c ON (a.attrelid=c.oid)¶LEFT OUTER JOIN pg_catalog.pg_attrdef ad ON (a.attrelid=ad|14.941406|160.59375|    0.0|postgres: dba testdb 127.0.0.1(48684) idle  |S    |  1.0|  0.0|0.052548602|    618496|          0|                     61|                         2|      1|
2025-01-28 19:33:48.331|222684|testdb |dba    |DBeaver 24.2.3 - SQLEditor <scripts.sql>|active |--вставляем данные 1 раз в минуту¶INSERT INTO pgsyswatch.proc_activity_snapshots (¶    pid, datname, usename, application_name, state_q, ¶    query, res_mb, virt_mb, swap_mb, command,state, utime, ¶    stime, pcpu, read_bytes, write_bytes, voluntary_ctxt_|16.585938|162.17188|    0.0|postgres: dba testdb 127.0.0.1(48686) INSERT|R    |  1.0|  3.0| 0.21019441|   1654784|          0|                    111|                         3|      1|
```
##### Example: Managing Partitions
- Run the following function to create and maintain partitions   
**(default depth is 1 month, partitions older than this are automatically deleted):**
```sql
SELECT pgsyswatch.manage_partitions_maintenance();
```
```
parent_schema|parent_table           |partition_schema|partition_table                 |
-------------+-----------------------+----------------+--------------------------------+
pgsyswatch   |proc_activity_snapshots|pgsyswatch      |proc_activity_snapshots_default |
pgsyswatch   |proc_activity_snapshots|pgsyswatch      |proc_activity_snapshots_20250128|
pgsyswatch   |proc_activity_snapshots|pgsyswatch      |proc_activity_snapshots_20250129|
```
---

#### Testing 

To test the extension, you can use the `make test` command:
```bash
make test
# & 
make clean && make && make install && make test
```
---
#### Cleanup 

To clean up the build artifacts, run:
```bash
make clean
```
---
#### Contributing 🤝

Contributions are welcome! Please open an issue or submit a pull request on the [GitHub repository](https://github.com/psqlmaster/pgsyswatch). 

---

#### License 

This project is licensed under the [Apache License, Version 2.0](LICENSE).  
Copyright © 2025 [Alexander Shcheglov](https://t.me/sqlmaster)

---

For more details, refer to the [source code](https://github.com/psqlmaster/pgsyswatch) and the provided SQL scripts. 

---


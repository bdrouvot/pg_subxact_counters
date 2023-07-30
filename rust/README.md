pg_subxact_counters
===================

Features
--------

Get global subtransactions counters.

Why?
--------

Subtransactions can lead to performance issue, as
mentioned in some posts, see for example:

- [Subtransactions and performance in PostgreSQL](https://www.cybertec-postgresql.com/en/subtransactions-and-performance-in-postgresql/)
- [PostgreSQL Subtransactions Considered Harmful](https://postgres.ai/blog/20210831-postgresql-subtransactions-considered-harmful)
- [Why we spent the last month eliminating PostgreSQL subtransactions](https://about.gitlab.com/blog/2021/09/29/why-we-spent-the-last-month-eliminating-postgresql-subtransactions/)

The purpose of this extension is to provide counters to
monitor the subtransactions (generation rate, overflow, state).  
One idea could be to sample `the pg_subxact_counters` view on a regular basis.

Installation
--------

**Prerequisites**

Download this repo and set up [`pgrx`](https://github.com/tcdi/pgrx):

```
$ cargo install --locked cargo-pgrx
```

After pgrx is set up, use below command to init for PG 15 as an example:

```
$ cargo pgrx init --pg15 /home/postgres/pg_installed/pg15/bin/pg_config
```

(change the `pg_config` path accordingly to your environment).

build the extension package:

```
$ cargo pgrx package
```

The extension is located at path `./target/release/pg_subxact_counters-pg15`. For more information, please visit [pgrx](https://github.com/tcdi/pgrx).

Another option is to:

```
$ cargo pgrx run pg15
```

Change `postgresql.conf` so that:

```
shared_preload_libraries = 'pg_subxact_counters' # (change requires restart)
```

Restart the server and install the extension:

```
postgres=# create extension pg_subxact_counters;
```

Test:

```
$ cargo pgrx test pg15
```

Example
--------

```
postgres=# \dx+ pg_subxact_counters
Objects in extension "pg_subxact_counters"
       Object description
--------------------------------
 function pg_subxact_counters()
 view pg_subxact_counters
(2 rows)

postgres=# select * from pg_subxact_counters;
 subxact_start | subxact_commit | subxact_abort | subxact_overflow
---------------+----------------+---------------+------------------
             0 |              0 |             0 |                0
(1 row)

postgres=# begin;
BEGIN
postgres=*# savepoint a;
SAVEPOINT
postgres=*# savepoint b;
SAVEPOINT
postgres=*# commit;
COMMIT
postgres=# select * from pg_subxact_counters;
 subxact_start | subxact_commit | subxact_abort | subxact_overflow
---------------+----------------+---------------+------------------
             2 |              2 |             0 |                0
(1 row)
```

Fields meaning
--------

* subxact_start: number of substransactions that started
* subxact_commit: number of substransactions that committed
* subxact_abort: number of substransactions that aborted (rolled back)
* subxact_overflow: number of times a top level XID have had substransactions overflowed

Remarks
--------
* subxact_start - subxact_commit - subxact_abort: number of subtransactions that have started and not yet committed/rolled back.
* subxact_overflow does not represent the number of substransactions that exceeded PGPROC_MAX_CACHED_SUBXIDS.
* the counters are global, means they record the activity for all the databases in the instance.
* has been tested on PostgreSQL version 12 and higher.

### Caveats & Limitations

- Windows is not supported, that limitation inherits from `pgrx`.

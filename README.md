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
One idea could be to sample the `pg_subxact_counters` view on a regular basis.

Installation
--------

This repository provides C and Rust code, please
refer to the one of interest.

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

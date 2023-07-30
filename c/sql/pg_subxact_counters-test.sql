CREATE EXTENSION pg_subxact_counters;

begin;
savepoint a;
savepoint b;
savepoint c;
rollback to savepoint a;
commit;

select * from pg_subxact_counters();

CREATE VIEW pg_subxact_counters AS
    SELECT
	subxact_start,
	subxact_commit,
	subxact_abort,
	subxact_overflow
    FROM
        pg_subxact_counters()
    ;

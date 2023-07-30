CREATE FUNCTION pg_subxact_counters(
	OUT subxact_start bigint,
	OUT subxact_commit bigint,
	OUT subxact_abort bigint,
	OUT subxact_overflow bigint)
RETURNS SETOF RECORD
AS 'MODULE_PATHNAME','pg_subxact_counters'
LANGUAGE C STRICT VOLATILE;

CREATE VIEW pg_subxact_counters AS
    SELECT
        subxact_start,
        subxact_commit,
        subxact_abort,
        subxact_overflow
    FROM
        pg_subxact_counters();

/*-------------------------------------------------------------------------
 *
 * pg_subxact_counters.c
 *              Providing subtransactions counters.
 * 
 * IDENTIFICATION
 *        pg_subxact_counters.c
 *
 * This program is open source, licensed under the PostgreSQL license.
 * For license terms, see the LICENSE file.
 * 
 * Copyright (C) 2023: Bertrand Drouvot
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"
#include "storage/ipc.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "access/xact.h"
#include "storage/proc.h"

PG_MODULE_MAGIC;
PG_FUNCTION_INFO_V1(pg_subxact_counters);

/*
 * The counters
 */
typedef struct SubXactCounters
{
	pg_atomic_uint64 subxact_start;
	pg_atomic_uint64 subxact_commit;
	pg_atomic_uint64 subxact_abort;
	pg_atomic_uint64 subxact_overflow;
} SubXactCounters;

/*
 * shmem_startup_hook and shmem_request_hook
 */
static shmem_startup_hook_type sxc_prev_shmem_startup_hook = NULL;
#if (PG_VERSION_NUM >= 150000)
static shmem_request_hook_type sxc_prev_shmem_request_hook = NULL;
#endif

/*
 * Functions
 */
void	_PG_init(void);
void	_PG_fini(void);
static Size sxc_memsize(void);
static void sxc_shmem_and_init(void);
static void sxc_shmem_request(void);
static void sxc_subxact_callback(SubXactEvent event,
								 SubTransactionId mySubid,
								 SubTransactionId parentSubid,
								 void *arg);

static bool  hasoverflowed = false;
SubXactCounters *SubXact_counters = NULL;

/*
 * Size needed for the counters
 */
Size
sxc_memsize(void)
{
	Size        size;
	size = sizeof(SubXactCounters);
	return size;
}

static void
sxc_shmem_request(void)
{
#if PG_VERSION_NUM >= 150000
	if (sxc_prev_shmem_request_hook)
		sxc_prev_shmem_request_hook();
#endif
	/*
	 * Request additional shared resources
	 */
	RequestAddinShmemSpace(sxc_memsize());
}

static void
sxc_shmem_and_init(void)
{
	bool        found;

	if (sxc_prev_shmem_startup_hook)
		sxc_prev_shmem_startup_hook();

	LWLockAcquire(AddinShmemInitLock, LW_EXCLUSIVE);

	/*
	 * The subtransactions counters
	 */
	SubXact_counters = (SubXactCounters *) ShmemInitStruct("subxact counters", sizeof(SubXactCounters), &found);

	if (!found)
	{
		MemSet(SubXact_counters, 0, sizeof(SubXactCounters));
		pg_atomic_init_u64(&SubXact_counters->subxact_start, 0);
		pg_atomic_init_u64(&SubXact_counters->subxact_commit, 0);
		pg_atomic_init_u64(&SubXact_counters->subxact_abort, 0);
		pg_atomic_init_u64(&SubXact_counters->subxact_overflow, 0);
	}

	LWLockRelease(AddinShmemInitLock);
}


void
_PG_init(void)
{

	if (!process_shared_preload_libraries_in_progress)
	{
		 ereport(ERROR, (errmsg("pg_subxact_counters can only be loaded via shared_preload_libraries"),
						 errhint("Add pg_subxact_counters to shared_preload_libraries configuration "
								 "variable in postgresql.conf.")));
	}
#if PG_VERSION_NUM < 150000
	sxc_shmem_request();
#endif
	/* Install CallBack */
	RegisterSubXactCallback(sxc_subxact_callback, NULL);
	/* Install hook */
#if PG_VERSION_NUM >= 150000
	sxc_prev_shmem_request_hook = shmem_request_hook;
	shmem_request_hook = sxc_shmem_request;
#endif
	sxc_prev_shmem_startup_hook = shmem_startup_hook;
	shmem_startup_hook = sxc_shmem_and_init;
}

/*
 * Our subxact callback.
 * This is the place where the counters get incremented.
 */
static void
sxc_subxact_callback(SubXactEvent event, SubTransactionId mySubid, SubTransactionId parentSubid, void *arg)
{
	switch (event)
	{
		case SUBXACT_EVENT_START_SUB:
			pg_atomic_fetch_add_u64(&SubXact_counters->subxact_start,1);
#if PG_VERSION_NUM >= 140000
			if (!hasoverflowed && MyProc->subxidStatus.overflowed) {
				pg_atomic_fetch_add_u64(&SubXact_counters->subxact_overflow, 1);
				hasoverflowed = true;
			}
			else if (!MyProc->subxidStatus.overflowed)
				hasoverflowed = false;
#else
			if (!hasoverflowed && MyPgXact->overflowed) {
				pg_atomic_fetch_add_u64(&SubXact_counters->subxact_overflow, 1);
				hasoverflowed = true;
			}
			else if (!MyPgXact->overflowed)
				hasoverflowed = false;
#endif
			break;
		case SUBXACT_EVENT_COMMIT_SUB:
			pg_atomic_fetch_add_u64(&SubXact_counters->subxact_commit, 1);
			break;
		case SUBXACT_EVENT_ABORT_SUB:
			pg_atomic_fetch_add_u64(&SubXact_counters->subxact_abort, 1);
			break;
		default:
			break;
	}
}

/*
 * Retrieve the counters
 */
Datum
pg_subxact_counters(PG_FUNCTION_ARGS)
{
	Datum           values[4];
	bool            nulls[4];
	TupleDesc       tupdesc;
	HeapTuple       htup;

	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				(errmsg("must be superuser to use this function"))));

	Assert(SubXact_counters != NULL);

	/*
	 * Construct a tuple descriptor for the result row.
	 */
	tupdesc = CreateTemplateTupleDesc(4);
	TupleDescInitEntry(tupdesc, (AttrNumber) 1, "subxact_startxact",
														INT8OID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 2, "subxact_commitxact",
														INT8OID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 3, "subxact_abortxact",
														INT8OID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 4, "total_overflowed_subxact",
														INT8OID, -1, 0);
	tupdesc = BlessTupleDesc(tupdesc);
	/*
	 * Form tuple with appropriate data.
	 */
	MemSet(values, 0, sizeof(values));
	MemSet(nulls, false, sizeof(nulls));

	values[0] = Int64GetDatum(pg_atomic_read_u64(&SubXact_counters->subxact_start));
	values[1] = Int64GetDatum(pg_atomic_read_u64(&SubXact_counters->subxact_commit));
	values[2] = Int64GetDatum(pg_atomic_read_u64(&SubXact_counters->subxact_abort));
	values[3] = Int64GetDatum(pg_atomic_read_u64(&SubXact_counters->subxact_overflow));
	htup = heap_form_tuple(tupdesc, values, nulls);

	PG_RETURN_DATUM(HeapTupleGetDatum(htup));
}

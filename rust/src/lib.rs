/*-------------------------------------------------------------------------
 *
 * rust/src/lib.rs
 *              Providing subtransactions counters.
 * 
 * IDENTIFICATION
 *        rust/src/lib.rs
 *
 * This program is open source, licensed under the PostgreSQL license.
 * For license terms, see the LICENSE file.
 * 
 * Copyright (C) 2023: Bertrand Drouvot
 *
 *-------------------------------------------------------------------------
 */

use pgrx::prelude::*;
use pgrx::{atomics::*, shmem::*};
use std::sync::atomic::{AtomicBool, Ordering};

pgrx::pg_module_magic!();

extension_sql_file!("../sql/finalize.sql", finalize);

static TOTAL_SUBXACT_COMMIT: PgAtomic<std::sync::atomic::AtomicI64> = PgAtomic::new();
static TOTAL_SUBXACT_ABORT: PgAtomic<std::sync::atomic::AtomicI64> = PgAtomic::new();
static TOTAL_SUBXACT_OVERFLOW: PgAtomic<std::sync::atomic::AtomicI64> = PgAtomic::new();
static TOTAL_SUBXACT_START: PgAtomic<std::sync::atomic::AtomicI64> = PgAtomic::new();
static HAS_OVERFLOWED: AtomicBool = AtomicBool::new(false);
static PROC_OR_XACT_HAS_OVERFLOWED: AtomicBool = AtomicBool::new(false);

#[pg_guard]
unsafe extern "C" fn _PG_init() {
    pgrx::pg_shmem_init!(TOTAL_SUBXACT_COMMIT);
    pgrx::pg_shmem_init!(TOTAL_SUBXACT_ABORT);
    pgrx::pg_shmem_init!(TOTAL_SUBXACT_OVERFLOW);
    pgrx::pg_shmem_init!(TOTAL_SUBXACT_START);

    pg_sys::RegisterSubXactCallback(Some(sxc_subxact_callback), std::ptr::null_mut());
}

#[pg_guard]
extern "C" fn sxc_subxact_callback(event: pg_sys::SubXactEvent,
                                   _mysubid: pg_sys::SubTransactionId,
                                   _parent_subid: pg_sys::SubTransactionId, 
                                   _arg: *mut std::os::raw::c_void) {

    if event == pg_sys::SubXactEvent_SUBXACT_EVENT_COMMIT_SUB {
        TOTAL_SUBXACT_COMMIT.get().fetch_add(1, Ordering::SeqCst);
    } else if event == pg_sys::SubXactEvent_SUBXACT_EVENT_ABORT_SUB {
        TOTAL_SUBXACT_ABORT.get().fetch_add(1, Ordering::SeqCst);
    } else if event == pg_sys::SubXactEvent_SUBXACT_EVENT_START_SUB {
        #[cfg(any(feature = "pg14", feature = "pg15"))]
        {
            let myproc = unsafe { pg_sys::MyProc };
            PROC_OR_XACT_HAS_OVERFLOWED.store(unsafe { (*myproc).subxidStatus.overflowed }, Ordering::Relaxed);
        }
        #[cfg(any(feature = "pg12", feature = "pg13"))]
        {
            let mypgxact = unsafe { pg_sys::MyPgXact };
            PROC_OR_XACT_HAS_OVERFLOWED.store(unsafe { (*mypgxact).overflowed }, Ordering::Relaxed);
        }
        TOTAL_SUBXACT_START.get().fetch_add(1, Ordering::SeqCst);
        if !HAS_OVERFLOWED.load(Ordering::Relaxed) && PROC_OR_XACT_HAS_OVERFLOWED.load(Ordering::Relaxed) {
            TOTAL_SUBXACT_OVERFLOW.get().fetch_add(1, Ordering::SeqCst);
            HAS_OVERFLOWED.store(true, Ordering::Relaxed);
        } else if !PROC_OR_XACT_HAS_OVERFLOWED.load(Ordering::Relaxed) {
            HAS_OVERFLOWED.store(false, Ordering::Relaxed);
        }
    }
}

#[pg_extern]
fn pg_subxact_counters() -> TableIterator<
    'static,
    (
        name!(subxact_start, i64),
        name!(subxact_commit, i64),
        name!(subxact_abort, i64),
        name!(subxact_overflow, i64),
    ),
> {
    if !unsafe { pg_sys::session_auth_is_superuser }
    {
        panic!("You must be a superuser to query pg_subxact_counters");
    }

    TableIterator::once((TOTAL_SUBXACT_START.get().load(Ordering::SeqCst),
                         TOTAL_SUBXACT_COMMIT.get().load(Ordering::SeqCst),
                         TOTAL_SUBXACT_ABORT.get().load(Ordering::SeqCst),
                         TOTAL_SUBXACT_OVERFLOW.get().load(Ordering::SeqCst))
                       )
}

#[cfg(any(test, feature = "pg_test"))]
extension_sql!(
    r#"
    create table test_subxact(n integer primary key);

    create or replace procedure my_test(n int) as $$
        begin
         insert into test_subxact(n) values(n);
        exception when others then
         raise notice '%',sqlerrm;
        end;
       $$ language plpgsql;
"#,
    name = "create_test_subxact",
);
#[cfg(any(test, feature = "pg_test"))]
#[pg_schema]
mod tests {
    use pgrx::*;
    
    #[pg_test]
    fn test_counters() -> Result<(), spi::Error> {
        let (s, c, a) = Spi::connect(|mut client| {
            client.update("call my_test(1);", None, None)?;
            client.update("call my_test(1);", None, None)?;
            client.update("call my_test(1);", None, None)?;
            client.update("call my_test(1);", None, None)?;
            client.select("select subxact_start, subxact_commit, subxact_abort from pg_subxact_counters", None, None)?.first().get_three::<i64, i64, i64>()
        })?;

        assert_eq!(Some(4), s);
        assert_eq!(Some(1), c);
        assert_eq!(Some(3), a);
        Ok(())
    }
}

/// This module is required by `cargo pgrx test` invocations.
/// It must be visible at the root of your extension crate.
#[cfg(test)]
pub mod pg_test {
    pub fn setup(_options: Vec<&str>) {
        // perform one-off initialization when the pg_test framework starts
    }

    pub fn postgresql_conf_options() -> Vec<&'static str> {
        // return any postgresql.conf settings that are required for your tests
        vec!["shared_preload_libraries = 'pg_subxact_counters'",]
    }
}

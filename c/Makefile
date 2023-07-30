REGRESS_OPTS =--temp-config=./pg_subxact_counters.conf --temp-instance=./tmp_check
REGRESS = pg_subxact_counters-test

MODULE_big = pg_subxact_counters
OBJS = pg_subxact_counters.o $(WIN32RES)

EXTENSION = pg_subxact_counters
DATA = pg_subxact_counters--1.0.sql
PGFILEDESC = "pg_subxact_counters"

LDFLAGS_SL += $(filter -lm, $(LIBS))

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

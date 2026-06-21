OVSQLITEOBJECTS = ovsqlite/ovsqlite-server.o ovsqlite/sql-main.o \
	ovsqlite/sql-init.o \
	ovsqlite/ovsqlite-private.o
OVSQLITELOBJECTS = $(OVSQLITEOBJECTS:.o=.lo)

ovsqlite/ovsqlite-server: $(OVSQLITEOBJECTS) libinnstorage.$(EXTLIB)
	$(LIBLD) $(LDFLAGS) $(SQLITE3_LDFLAGS) -o $@ $(OVSQLITELOBJECTS) \
	$(LIBSTORAGE) $(LIBHIST) $(LIBINN) $(STORAGE_LIBS) $(SQLITE3_LIBS) \
	$(LIBS)

ovsqlite/ovsqlite-util: ovsqlite/ovsqlite-util.in $(FIXSCRIPT)
	$(FIX) ovsqlite/ovsqlite-util.in

##  sqlite-helper-gen now lives in lib/ (shared with the hissqlite history
##  method); lib is built before storage so it is available here.

ovsqlite/sql-main.c: ovsqlite/sql-main.sql ../lib/sqlite-helper-gen
	../lib/sqlite-helper-gen ovsqlite/sql-main.sql

ovsqlite/sql-main.h: ovsqlite/sql-main.c ;

ovsqlite/sql-init.c: ovsqlite/sql-init.sql ../lib/sqlite-helper-gen
	../lib/sqlite-helper-gen ovsqlite/sql-init.sql

ovsqlite/sql-init.h: ovsqlite/sql-init.c ;

ovsqlite/sql-read.c: ovsqlite/sql-read.sql ../lib/sqlite-helper-gen
	../lib/sqlite-helper-gen ovsqlite/sql-read.sql

ovsqlite/sql-read.h: ovsqlite/sql-read.c ;

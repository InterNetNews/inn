OVSQLITEOBJECTS = ovsqlite/ovsqlite-server.o ovsqlite/sql-main.o \
	ovsqlite/sql-init.o ovsqlite/sqlite-helper.o \
	ovsqlite/ovsqlite-private.o
OVSQLITELOBJECTS = $(OVSQLITEOBJECTS:.o=.lo)

ovsqlite/ovsqlite-server: $(OVSQLITEOBJECTS) libinnstorage.$(EXTLIB)
	$(LIBLD) $(LDFLAGS) $(SQLITE3_LDFLAGS) -o $@ $(OVSQLITELOBJECTS) \
	$(LIBSTORAGE) $(LIBHIST) $(LIBINN) $(STORAGE_LIBS) $(SQLITE3_LIBS) \
	$(LIBS)

ovsqlite/sqlite-helper-gen: ovsqlite/sqlite-helper-gen.in $(FIXSCRIPT)
	$(FIX) -i ovsqlite/sqlite-helper-gen.in

ovsqlite/sql-main.c: ovsqlite/sql-main.sql ovsqlite/sqlite-helper-gen
	ovsqlite/sqlite-helper-gen ovsqlite/sql-main.sql

ovsqlite/sql-main.h: ovsqlite/sql-main.c ;

ovsqlite/sql-init.c: ovsqlite/sql-init.sql ovsqlite/sqlite-helper-gen
	ovsqlite/sqlite-helper-gen ovsqlite/sql-init.sql

ovsqlite/sql-init.h: ovsqlite/sql-init.c ;


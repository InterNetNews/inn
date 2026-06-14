##  Build rules for the hissqlite history method.  Included into
##  history/Make.methods by buildconfig.  The .c/.h are generated from the
##  .sql files by the shared sqlite-helper-gen, which lives in lib/ (built
##  before history, so it is available here).

hissqlite/hissqlite-init.c: hissqlite/hissqlite-init.sql ../lib/sqlite-helper-gen
	../lib/sqlite-helper-gen hissqlite/hissqlite-init.sql
hissqlite/hissqlite-init.h: hissqlite/hissqlite-init.c ;

hissqlite/hissqlite-main.c: hissqlite/hissqlite-main.sql ../lib/sqlite-helper-gen
	../lib/sqlite-helper-gen hissqlite/hissqlite-main.sql
hissqlite/hissqlite-main.h: hissqlite/hissqlite-main.c ;

hissqlite/hissqlite-read.c: hissqlite/hissqlite-read.sql ../lib/sqlite-helper-gen
	../lib/sqlite-helper-gen hissqlite/hissqlite-read.sql
hissqlite/hissqlite-read.h: hissqlite/hissqlite-read.c ;

##  hissqlite.c and the generated translation units include the generated
##  headers; make that explicit so a parallel build orders correctly.
hissqlite/hissqlite.o hissqlite/hissqlite.lo: \
	hissqlite/hissqlite-init.h hissqlite/hissqlite-main.h \
	hissqlite/hissqlite-read.h

##  Programs: the migration converter (C) and the inspect/dump utility (Perl).
hissqlite/hissqlite-convert: hissqlite/hissqlite-convert.o libinnhist.la \
	$(LIBSTORAGE) $(LIBINN)
	$(LIBLD) $(LDFLAGS) $(SQLITE3_LDFLAGS) -o $@ \
	    hissqlite/hissqlite-convert.o libinnhist.la \
	    $(LIBSTORAGE) $(LIBINN) $(STORAGE_LIBS) $(LIBS)

hissqlite/hissqlite-util: hissqlite/hissqlite-util.in $(FIXSCRIPT)
	$(FIX) hissqlite/hissqlite-util.in

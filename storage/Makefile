##  $Id$

include ../Makefile.global

top	      = ..
CFLAGS	      = $(GCFLAGS) -I. $(BERKELEY_DB_CFLAGS)

SOURCES	      = expire.c interface.c methods.c ov.c overdata.c overview.c \
		ovmethods.c $(METHOD_SOURCES)
OBJECTS	      = $(SOURCES:.c=.o)
LOBJECTS      = $(OBJECTS:.o=.lo)

.SUFFIXES: .lo

all: library programs

# Included here after the all target, since additional rules are defined in
# Make.methods to be sure that we recurse properly to build the methods.
include Make.methods

warnings:
	$(MAKE) COPT='$(WARNINGS)' all

install: all
	$(LI_XPUB) libstorage.$(EXTLIB) $D$(PATHLIB)/libstorage.$(EXTLIB)
	for F in $(PROGRAMS) ; do \
	    $(LI_XPRI) $$F $D$(PATHBIN)/`basename $$F` ; \
	done

library: libstorage.$(EXTLIB)

programs: $(PROGRAMS)

clobber clean distclean:
	rm -f *.o *.lo */*.o */*.lo libstorage.la libstorage.a
	rm -f $(PROGRAMS) libstorage_pure_*.a .pure
	rm -f buildconfig methods.c methods.h ovmethods.c ovmethods.h
	rm -f libstorage$(PROFSUFFIX).a
	rm -rf .libs */.libs

tags ctags: $(SOURCES)
	$(CTAGS) $(SOURCES) ../include/*.h ../include/inn/*.h

$(FIXSCRIPT):
	@echo Run configure before running make.  See INSTALL for details.
	@exit 1

libstorage.la: $(OBJECTS) $(LIBINN)
	$(LIBLD) $(LDFLAGS) -o $@ $(LOBJECTS) \
	    $(LIBINN) $(STORAGE_LIBS) $(LIBS) \
	    -rpath $(PATHLIB) -version-info 2:0:0

libstorage.a: $(OBJECTS)
	ar r $@ $(OBJECTS)
	$(RANLIB) libstorage.a

# Make.methods is included in the distribution tarball since some non-GNU
# makes can't deal with including a non-existent file, so don't depend on
# it.  The dependencies aren't entirely accurate; you really want to re-run
# buildconfig each time a new subdirectory is added to the directory.  But
# adding a dependency on . is a bit too non-portable for my taste and causes
# too many rebuilds.
Make.methods methods.h ovmethods.c ovmethods.h methods.c: buildconfig
	./buildconfig

buildconfig: buildconfig.in $(FIXSCRIPT)
	$(FIXSCRIPT) -i buildconfig.in

.c.o .c.lo:
	$(LIBCC) $(CFLAGS) $(DB_CPPFLAGS) $(CCOUTPUT)

ovtest:	ov.c libstorage.$(EXTLIB) $(LIBINN)
	$(CC) $(CFLAGS) -D_TEST_ -o ovtest ov.c \
	    libstorage.$(EXTLIB) $(LIBINN) $(STORAGE_LIBS) $(LIBS)

$(LIBINN):      ; (cd ../lib ; $(MAKE))
$(LIBHIST):	; (cd ../history ; $(MAKE))


##  Profiling.  The rules are a bit brute-force, but good enough.

profiled: libstorage$(PROFSUFFIX).a

libstorage$(PROFSUFFIX).a: $(SOURCES)
	rm -f $(OBJECTS)
	$(MAKEPROFILING) libstorage.a
	mv libstorage.a libstorage$(PROFSUFFIX).a
	$(RANLIB) libstorage$(PROFSUFFIX).a
	rm -f $(OBJECTS)


##  Dependencies.  Default list, below, is probably good enough.

depend:	Makefile $(SOURCES) $(EXTRA_SOURCES)
	$(MAKEDEPEND) '$(CFLAGS)' $(SOURCES) $(EXTRA_SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
expire.o: expire.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/innconf.h ../include/libinn.h \
 ../include/ov.h ../include/storage.h ../include/inn/history.h \
 ovinterface.h ../include/paths.h
interface.o: interface.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/conffile.h ../include/inn/innconf.h \
 ../include/inn/wire.h interface.h ../include/storage.h \
 ../include/libinn.h methods.h ../include/paths.h
methods.o: methods.c interface.h ../include/config.h \
 ../include/inn/defines.h ../include/storage.h methods.h cnfs/cnfs.h \
 timecaf/timecaf.h timehash/timehash.h tradspool/tradspool.h \
 trash/trash.h
ov.o: ov.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/innconf.h ../include/libinn.h \
 ../include/ov.h ../include/storage.h ../include/inn/history.h \
 ovinterface.h ovmethods.h
overdata.o: overdata.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/buffer.h \
 ../include/inn/innconf.h ../include/inn/messages.h \
 ../include/inn/qio.h ../include/inn/wire.h ../include/inn/vector.h \
 ../include/libinn.h ovinterface.h ../include/ov.h \
 ../include/storage.h ../include/inn/history.h ../include/paths.h
ovmethods.o: ovmethods.c ovinterface.h ../include/config.h \
 ../include/inn/defines.h ../include/ov.h ../include/storage.h \
 ../include/inn/history.h buffindexed/buffindexed.h ovdb/ovdb.h \
 tradindexed/tradindexed.h
buffindexed/buffindexed.o: buffindexed/buffindexed.c ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h \
 ../include/portable/mmap.h ../include/inn/innconf.h \
 ../include/libinn.h ../include/ov.h ../include/storage.h \
 ../include/inn/history.h ../include/paths.h ovinterface.h \
 buffindexed/buffindexed.h
cnfs/cnfs.o: cnfs/cnfs.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/portable/mmap.h \
 ../include/portable/time.h ../include/inn/innconf.h interface.h \
 ../include/storage.h ../include/libinn.h methods.h ../include/paths.h \
 ../include/inn/wire.h ../include/inn/mmap.h cnfs/cnfs.h \
 cnfs/cnfs-private.h
ovdb/ovdb.o: ovdb/ovdb.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/portable/socket.h \
 ../include/portable/time.h ../include/conffile.h \
 ../include/inn/innconf.h ../include/inn/messages.h \
 ../include/libinn.h ../include/paths.h ../include/storage.h \
 ../include/ov.h ../include/inn/history.h ovinterface.h ovdb/ovdb.h \
 ovdb/ovdb-private.h
timecaf/caf.o: timecaf/caf.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h timecaf/caf.h
timecaf/timecaf.o: timecaf/timecaf.c ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h \
 ../include/portable/mmap.h timecaf/caf.h ../include/inn/innconf.h \
 ../include/inn/wire.h ../include/libinn.h methods.h interface.h \
 ../include/storage.h timecaf/timecaf.h ../include/paths.h
timehash/timehash.o: timehash/timehash.c ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h \
 ../include/portable/mmap.h ../include/inn/innconf.h \
 ../include/inn/wire.h ../include/libinn.h methods.h interface.h \
 ../include/storage.h ../include/paths.h timehash/timehash.h
tradindexed/tdx-cache.o: tradindexed/tdx-cache.c ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h \
 ../include/inn/hashtab.h ../include/inn/messages.h \
 ../include/libinn.h ../include/storage.h tradindexed/tdx-private.h
tradindexed/tdx-data.o: tradindexed/tdx-data.c ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h \
 ../include/portable/mmap.h ../include/inn/history.h \
 ../include/inn/innconf.h ../include/inn/messages.h \
 ../include/inn/mmap.h ../include/libinn.h ../include/ov.h \
 ../include/storage.h ovinterface.h tradindexed/tdx-private.h \
 tradindexed/tdx-structure.h
tradindexed/tdx-group.o: tradindexed/tdx-group.c ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h \
 ../include/portable/mmap.h ../include/inn/hashtab.h \
 ../include/inn/innconf.h ../include/inn/messages.h \
 ../include/inn/mmap.h ../include/inn/qio.h ../include/libinn.h \
 ../include/paths.h tradindexed/tdx-private.h ../include/storage.h \
 tradindexed/tdx-structure.h
tradindexed/tradindexed.o: tradindexed/tradindexed.c ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h \
 ../include/inn/innconf.h ../include/inn/messages.h \
 ../include/libinn.h ../include/ov.h ../include/storage.h \
 ../include/inn/history.h tradindexed/tdx-private.h \
 tradindexed/tdx-structure.h tradindexed/tradindexed.h
tradspool/tradspool.o: tradspool/tradspool.c ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h \
 ../include/portable/mmap.h ../include/inn/innconf.h \
 ../include/inn/qio.h ../include/inn/wire.h ../include/libinn.h \
 ../include/paths.h methods.h interface.h ../include/storage.h \
 tradspool/tradspool.h
trash/trash.o: trash/trash.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h methods.h interface.h \
 ../include/storage.h trash/trash.h
tradindexed/tdx-util.o: tradindexed/tdx-util.c ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h \
 ../include/inn/buffer.h ../include/inn/history.h \
 ../include/inn/innconf.h ../include/inn/messages.h \
 ../include/inn/vector.h ../include/libinn.h ../include/ov.h \
 ../include/storage.h ovinterface.h ../include/paths.h \
 tradindexed/tdx-private.h tradindexed/tdx-structure.h

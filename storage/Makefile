##  $Id$

include ../Makefile.global

# This version number should be increased with every change to the library
# source following the rules laid out in the libtool manual.  This will also
# force the file name of the shared library to change so that one can
# recover from make update.  We can't use .OLD extensions for the shared
# library since ldconfig will think .OLD sorts after the regular library and
# will point the binaries at the old library.
LTVERSION     = 3:0:0

top	      = ..
CFLAGS	      = $(GCFLAGS) -I. $(BDB_CPPFLAGS)

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
	$(LI_LPUB) libstorage.$(EXTLIB) $D$(PATHLIB)/libstorage.$(EXTLIB)
	for F in $(PROGRAMS) ; do \
	    $(LI_XPRI) $$F $D$(PATHBIN)/`basename $$F` ; \
	done

bootstrap: Make.methods

library: libstorage.$(EXTLIB)

programs: $(PROGRAMS)

clean clobber distclean:
	rm -f *.o *.lo */*.o */*.lo libstorage.la libstorage.a
	rm -f $(PROGRAMS) libstorage_pure_*.a .pure
	rm -f buildconfig libstorage$(PROFSUFFIX).a
	rm -rf .libs */.libs

maintclean: distclean
	rm -f Make.methods methods.c methods.h ovmethods.c ovmethods.h

$(FIXSCRIPT):
	@echo Run configure before running make.  See INSTALL for details.
	@exit 1

libstorage.la: $(OBJECTS) $(LIBINN)
	$(LIBLD) $(LDFLAGS) -o $@ $(LOBJECTS) \
	    $(LIBINN) $(STORAGE_LIBS) $(LIBS) \
	    -rpath $(PATHLIB) -version-info $(LTVERSION)

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
	$(LIBCC) $(CFLAGS) $(CCOUTPUT)

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
  ../include/inn/system.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/options.h ../include/clibrary.h ../include/config.h \
  ../include/inn/macros.h ../include/portable/stdbool.h \
  ../include/inn/innconf.h ../include/inn/defines.h \
  ../include/inn/libinn.h ../include/inn/xmalloc.h ../include/inn/xwrite.h \
  ../include/inn/ov.h ../include/inn/storage.h ../include/inn/options.h \
  ../include/inn/history.h ovinterface.h ../include/inn/storage.h \
  ../include/inn/paths.h ../include/inn/vector.h
interface.o: interface.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/options.h ../include/clibrary.h ../include/config.h \
  ../include/inn/macros.h ../include/portable/stdbool.h \
  ../include/conffile.h ../include/inn/innconf.h ../include/inn/defines.h \
  ../include/inn/messages.h ../include/inn/wire.h interface.h \
  ../include/inn/storage.h ../include/inn/options.h \
  ../include/inn/libinn.h ../include/inn/xmalloc.h ../include/inn/xwrite.h \
  methods.h ../include/inn/paths.h
methods.o: methods.c interface.h ../include/config.h \
  ../include/inn/defines.h ../include/inn/system.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/options.h ../include/inn/storage.h \
  ../include/inn/defines.h ../include/inn/options.h methods.h cnfs/cnfs.h \
  timecaf/timecaf.h interface.h timehash/timehash.h tradspool/tradspool.h \
  trash/trash.h
ov.o: ov.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/options.h ../include/clibrary.h ../include/config.h \
  ../include/inn/macros.h ../include/portable/stdbool.h \
  ../include/inn/innconf.h ../include/inn/defines.h \
  ../include/inn/messages.h ../include/inn/wire.h ../include/inn/vector.h \
  ../include/inn/libinn.h ../include/inn/xmalloc.h ../include/inn/xwrite.h \
  ../include/inn/ov.h ../include/inn/storage.h ../include/inn/options.h \
  ../include/inn/history.h ovinterface.h ../include/inn/storage.h \
  ovmethods.h
overdata.o: overdata.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/options.h ../include/clibrary.h ../include/config.h \
  ../include/inn/macros.h ../include/portable/stdbool.h \
  ../include/inn/buffer.h ../include/inn/innconf.h \
  ../include/inn/defines.h ../include/inn/messages.h ../include/inn/wire.h \
  ../include/inn/vector.h ../include/inn/libinn.h ../include/inn/xmalloc.h \
  ../include/inn/xwrite.h ovinterface.h ../include/inn/history.h \
  ../include/inn/ov.h ../include/inn/storage.h ../include/inn/options.h \
  ../include/inn/storage.h
overview.o: overview.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/options.h ../include/clibrary.h ../include/config.h \
  ../include/inn/macros.h ../include/portable/stdbool.h \
  ../include/inn/buffer.h ../include/inn/innconf.h \
  ../include/inn/defines.h ../include/inn/messages.h \
  ../include/inn/overview.h ../include/inn/storage.h \
  ../include/inn/options.h ../include/inn/wire.h ../include/inn/vector.h \
  ../include/inn/libinn.h ../include/inn/xmalloc.h ../include/inn/xwrite.h \
  ../include/inn/ov.h ../include/inn/storage.h ../include/inn/history.h \
  ovinterface.h ovmethods.h
ovmethods.o: ovmethods.c ovinterface.h ../include/config.h \
  ../include/inn/defines.h ../include/inn/system.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/options.h ../include/inn/history.h \
  ../include/inn/defines.h ../include/inn/ov.h ../include/inn/storage.h \
  ../include/inn/options.h ../include/inn/storage.h \
  buffindexed/buffindexed.h ovdb/ovdb.h tradindexed/tradindexed.h
buffindexed/buffindexed.o: buffindexed/buffindexed.c ../include/config.h \
  ../include/inn/defines.h ../include/inn/system.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/options.h ../include/clibrary.h ../include/config.h \
  ../include/inn/macros.h ../include/portable/stdbool.h \
  ../include/portable/mmap.h ../include/inn/innconf.h \
  ../include/inn/defines.h ../include/inn/messages.h \
  ../include/inn/libinn.h ../include/inn/xmalloc.h ../include/inn/xwrite.h \
  ../include/inn/ov.h ../include/inn/storage.h ../include/inn/options.h \
  ../include/inn/history.h ../include/inn/paths.h ovinterface.h \
  ../include/inn/storage.h ../include/inn/fdflag.h \
  ../include/inn/portable-socket.h ../include/inn/portable-getaddrinfo.h \
  ../include/inn/portable-getnameinfo.h buffindexed/shmem.h \
  buffindexed/buffindexed.h
buffindexed/shmem.o: buffindexed/shmem.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/options.h ../include/clibrary.h ../include/config.h \
  ../include/inn/macros.h ../include/portable/stdbool.h \
  ../include/inn/messages.h ../include/inn/libinn.h \
  ../include/inn/defines.h ../include/inn/xmalloc.h \
  ../include/inn/xwrite.h buffindexed/shmem.h
cnfs/cnfs.o: cnfs/cnfs.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/options.h ../include/clibrary.h ../include/config.h \
  ../include/inn/macros.h ../include/portable/stdbool.h \
  ../include/portable/mmap.h ../include/inn/fdflag.h \
  ../include/inn/portable-socket.h ../include/inn/portable-getaddrinfo.h \
  ../include/inn/portable-getnameinfo.h ../include/inn/innconf.h \
  ../include/inn/defines.h ../include/inn/messages.h ../include/inn/mmap.h \
  ../include/inn/wire.h interface.h ../include/inn/storage.h \
  ../include/inn/options.h ../include/inn/libinn.h \
  ../include/inn/xmalloc.h ../include/inn/xwrite.h methods.h interface.h \
  ../include/inn/paths.h cnfs/cnfs.h cnfs/cnfs-private.h
ovdb/ovdb.o: ovdb/ovdb.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/options.h ../include/clibrary.h ../include/config.h \
  ../include/inn/macros.h ../include/portable/stdbool.h \
  ../include/portable/socket.h ../include/portable/macros.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/conffile.h ../include/inn/fdflag.h \
  ../include/inn/portable-socket.h ../include/inn/innconf.h \
  ../include/inn/defines.h ../include/inn/libinn.h \
  ../include/inn/xmalloc.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/newsuser.h \
  ../include/inn/paths.h ../include/inn/storage.h ../include/inn/options.h \
  ../include/inn/ov.h ../include/inn/storage.h ../include/inn/history.h \
  ovinterface.h ovdb/ovdb.h ovdb/ovdb-private.h \
  ../include/portable/socket-unix.h
timecaf/caf.o: timecaf/caf.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/options.h ../include/clibrary.h ../include/config.h \
  ../include/inn/macros.h ../include/portable/stdbool.h \
  ../include/inn/messages.h ../include/inn/libinn.h \
  ../include/inn/defines.h ../include/inn/xmalloc.h \
  ../include/inn/xwrite.h timecaf/caf.h
timecaf/timecaf.o: timecaf/timecaf.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/options.h ../include/clibrary.h ../include/config.h \
  ../include/inn/macros.h ../include/portable/stdbool.h \
  ../include/portable/mmap.h timecaf/caf.h ../include/inn/fdflag.h \
  ../include/inn/portable-socket.h ../include/inn/portable-getaddrinfo.h \
  ../include/inn/portable-getnameinfo.h ../include/inn/innconf.h \
  ../include/inn/defines.h ../include/inn/messages.h ../include/inn/wire.h \
  ../include/inn/libinn.h ../include/inn/xmalloc.h ../include/inn/xwrite.h \
  methods.h interface.h ../include/inn/storage.h ../include/inn/options.h \
  timecaf/timecaf.h interface.h ../include/inn/paths.h
timehash/timehash.o: timehash/timehash.c ../include/config.h \
  ../include/inn/defines.h ../include/inn/system.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/options.h ../include/clibrary.h ../include/config.h \
  ../include/inn/macros.h ../include/portable/stdbool.h \
  ../include/portable/mmap.h ../include/inn/innconf.h \
  ../include/inn/defines.h ../include/inn/messages.h ../include/inn/wire.h \
  ../include/inn/libinn.h ../include/inn/xmalloc.h ../include/inn/xwrite.h \
  methods.h interface.h ../include/inn/storage.h ../include/inn/options.h \
  ../include/inn/paths.h timehash/timehash.h interface.h
tradindexed/tdx-cache.o: tradindexed/tdx-cache.c ../include/config.h \
  ../include/inn/defines.h ../include/inn/system.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/options.h ../include/clibrary.h ../include/config.h \
  ../include/inn/macros.h ../include/portable/stdbool.h \
  ../include/inn/hashtab.h ../include/inn/defines.h \
  ../include/inn/messages.h ../include/inn/libinn.h \
  ../include/inn/xmalloc.h ../include/inn/xwrite.h \
  ../include/inn/storage.h ../include/inn/options.h \
  tradindexed/tdx-private.h
tradindexed/tdx-data.o: tradindexed/tdx-data.c ../include/config.h \
  ../include/inn/defines.h ../include/inn/system.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/options.h ../include/clibrary.h ../include/config.h \
  ../include/inn/macros.h ../include/portable/stdbool.h \
  ../include/portable/mmap.h ../include/inn/fdflag.h \
  ../include/inn/portable-socket.h ../include/inn/portable-getaddrinfo.h \
  ../include/inn/portable-getnameinfo.h ../include/inn/history.h \
  ../include/inn/defines.h ../include/inn/innconf.h \
  ../include/inn/messages.h ../include/inn/mmap.h ../include/inn/libinn.h \
  ../include/inn/xmalloc.h ../include/inn/xwrite.h ../include/inn/ov.h \
  ../include/inn/storage.h ../include/inn/options.h ovinterface.h \
  ../include/inn/storage.h tradindexed/tdx-private.h \
  tradindexed/tdx-structure.h
tradindexed/tdx-group.o: tradindexed/tdx-group.c ../include/config.h \
  ../include/inn/defines.h ../include/inn/system.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/options.h ../include/clibrary.h ../include/config.h \
  ../include/inn/macros.h ../include/portable/stdbool.h \
  ../include/portable/mmap.h ../include/inn/fdflag.h \
  ../include/inn/portable-socket.h ../include/inn/portable-getaddrinfo.h \
  ../include/inn/portable-getnameinfo.h ../include/inn/hashtab.h \
  ../include/inn/defines.h ../include/inn/innconf.h \
  ../include/inn/messages.h ../include/inn/mmap.h ../include/inn/qio.h \
  ../include/inn/vector.h ../include/inn/libinn.h ../include/inn/xmalloc.h \
  ../include/inn/xwrite.h ../include/inn/paths.h tradindexed/tdx-private.h \
  ../include/inn/storage.h ../include/inn/options.h \
  tradindexed/tdx-structure.h
tradindexed/tradindexed.o: tradindexed/tradindexed.c ../include/config.h \
  ../include/inn/defines.h ../include/inn/system.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/options.h ../include/clibrary.h ../include/config.h \
  ../include/inn/macros.h ../include/portable/stdbool.h \
  ../include/inn/innconf.h ../include/inn/defines.h \
  ../include/inn/messages.h ../include/inn/libinn.h \
  ../include/inn/xmalloc.h ../include/inn/xwrite.h ../include/inn/ov.h \
  ../include/inn/storage.h ../include/inn/options.h \
  ../include/inn/history.h ../include/inn/storage.h \
  tradindexed/tdx-private.h tradindexed/tdx-structure.h \
  tradindexed/tradindexed.h
tradspool/tradspool.o: tradspool/tradspool.c ../include/config.h \
  ../include/inn/defines.h ../include/inn/system.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/options.h ../include/clibrary.h ../include/config.h \
  ../include/inn/macros.h ../include/portable/stdbool.h \
  ../include/portable/mmap.h ../include/inn/innconf.h \
  ../include/inn/defines.h ../include/inn/messages.h ../include/inn/qio.h \
  ../include/inn/wire.h ../include/inn/libinn.h ../include/inn/xmalloc.h \
  ../include/inn/xwrite.h ../include/inn/paths.h methods.h interface.h \
  ../include/inn/storage.h ../include/inn/options.h tradspool/tradspool.h \
  interface.h
trash/trash.o: trash/trash.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/options.h ../include/clibrary.h ../include/config.h \
  ../include/inn/macros.h ../include/portable/stdbool.h \
  ../include/inn/libinn.h ../include/inn/defines.h \
  ../include/inn/xmalloc.h ../include/inn/xwrite.h methods.h interface.h \
  ../include/inn/storage.h ../include/inn/options.h trash/trash.h \
  interface.h
tradindexed/tdx-util.o: tradindexed/tdx-util.c ../include/config.h \
  ../include/inn/defines.h ../include/inn/system.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/options.h ../include/clibrary.h ../include/config.h \
  ../include/inn/macros.h ../include/portable/stdbool.h \
  ../include/inn/buffer.h ../include/inn/libinn.h ../include/inn/defines.h \
  ../include/inn/xmalloc.h ../include/inn/xwrite.h \
  ../include/inn/history.h ../include/inn/innconf.h \
  ../include/inn/messages.h ../include/inn/newsuser.h ../include/inn/ov.h \
  ../include/inn/storage.h ../include/inn/options.h ../include/inn/paths.h \
  ../include/inn/vector.h ../include/inn/wire.h ovinterface.h \
  ../include/inn/storage.h tradindexed/tdx-private.h \
  tradindexed/tdx-structure.h

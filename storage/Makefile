include ../Makefile.global

# This version number should be increased with every change to the library
# source following the rules laid out in the libtool manual.  This will also
# force the file name of the shared library to change so that one can
# recover from make update.  We can't use .OLD extensions for the shared
# library since ldconfig will think .OLD sorts after the regular library and
# will point the binaries at the old library.
LTVERSION     = 4:1:1

top	      = ..
CFLAGS	      = $(GCFLAGS) -I. $(BDB_CPPFLAGS) $(SQLITE3_CPPFLAGS)

SOURCES	      = expire.c interface.c methods.c ov.c overdata.c overview.c \
		ovmethods.c $(METHOD_SOURCES)
OBJECTS	      = $(SOURCES:.c=.o)
LOBJECTS      = $(OBJECTS:.o=.lo)

.SUFFIXES: .lo

all: library programs

$(FIXSCRIPT):
	@echo Run configure before running make.  See INSTALL for details.
	@exit 1

FIX	= $(SHELL) $(FIXSCRIPT)

# Included here after the FIX definition and all target, since additional rules
# are defined in Make.methods.  Be sure that we recurse properly to build the
# methods.
include Make.methods

warnings:
	$(MAKE) COPT="$(COPT) $(WARNINGS)" all

install: all
	$(LI_LPUB) libinnstorage.$(EXTLIB) $D$(PATHLIB)/libinnstorage.$(EXTLIB)
	for F in $(PROGRAMS) ; do \
	    $(LI_XPRI) $$F $D$(PATHBIN)/`basename $$F` ; \
	done

bootstrap: Make.methods

library: libinnstorage.$(EXTLIB)

programs: $(PROGRAMS)

clean:
	rm -f *.o *.lo */*.o */*.lo libinnstorage.la libinnstorage.a
	rm -f $(PROGRAMS) libinnstorage_pure_*.a .pure
	rm -f buildconfig libinnstorage$(PROFSUFFIX).a
	rm -rf .libs */.libs
	rm -f $(RM_CLEAN)

clobber distclean: clean
	rm -f $(RM_DISTCLEAN)

maintclean: distclean
	rm -f Make.methods methods.c methods.h ovmethods.c ovmethods.h
	rm -f $(RM_MAINTCLEAN)

libinnstorage.la: $(OBJECTS) $(LOBJECTS) $(LIBINN)
	$(LIBLD) $(LDFLAGS) -o $@ $(LOBJECTS) \
	    $(LIBINN) $(STORAGE_LIBS) $(LIBS) \
	    -rpath $(PATHLIB) -version-info $(LTVERSION)

libinnstorage.a: $(OBJECTS)
	ar r $@ $(OBJECTS)
	$(RANLIB) libinnstorage.a

# Make.methods is included in the distribution tarball since some non-GNU
# makes can't deal with including a non-existent file, so don't depend on
# it.  The dependencies aren't entirely accurate; you really want to re-run
# buildconfig each time a new subdirectory is added to the directory.  But
# adding a dependency on . is a bit too non-portable for my taste and causes
# too many rebuilds.
Make.methods methods.h ovmethods.c ovmethods.h methods.c: buildconfig
	./buildconfig

buildconfig: buildconfig.in $(FIXSCRIPT)
	$(FIX) -i buildconfig.in

.c.o .c.lo:
	$(LIBCC) $(CFLAGS) $(CCOUTPUT)

ovtest:	ov.c libinnstorage.$(EXTLIB) $(LIBINN)
	$(CC) $(CFLAGS) -D_TEST_ -o ovtest ov.c \
	    libinnstorage.$(EXTLIB) $(LIBINN) $(STORAGE_LIBS) $(LIBS)

$(LIBINN):      ; (cd ../lib ; $(MAKE))
$(LIBHIST):	; (cd ../history ; $(MAKE))


##  Profiling.  The rules are a bit brute-force, but good enough.

profiled: libinnstorage$(PROFSUFFIX).a

libinnstorage$(PROFSUFFIX).a: $(SOURCES)
	rm -f $(OBJECTS)
	$(MAKEPROFILING) libinnstorage.a
	mv libinnstorage.a libinnstorage$(PROFSUFFIX).a
	$(RANLIB) libinnstorage$(PROFSUFFIX).a
	rm -f $(OBJECTS)


# TODO bodge in missing dependency.  This isn't a full solution
# but it works for now.
#
# See https://github.com/InterNetNews/inn/issues/206
ovsqlite/ovsqlite-server.lo: ovsqlite/sql-init.h

##  Dependencies.  Default list, below, is probably good enough.

depend:	Makefile $(SOURCES) $(EXTRA_SOURCES)
	$(MAKEDEPEND) '$(CFLAGS)' $(SOURCES) $(EXTRA_SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
expire.o: expire.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h ../include/inn/ov.h \
  ../include/inn/history.h ../include/inn/storage.h \
  ../include/inn/options.h ../include/inn/overview.h \
  ../include/inn/paths.h ../include/inn/vector.h ovinterface.h
interface.o: interface.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/conffile.h \
  ../include/portable/macros.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/paths.h ../include/inn/wire.h \
  interface.h ../include/inn/storage.h ../include/inn/options.h methods.h
methods.o: methods.c interface.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/inn/storage.h \
  ../include/inn/macros.h ../include/inn/options.h \
  ../include/inn/portable-stdbool.h methods.h cnfs/cnfs.h \
  timecaf/timecaf.h interface.h timehash/timehash.h tradspool/tradspool.h \
  trash/trash.h
ov.o: ov.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/ov.h ../include/inn/history.h \
  ../include/inn/storage.h ../include/inn/options.h \
  ../include/inn/vector.h ../include/inn/wire.h ovinterface.h ovmethods.h
overdata.o: overdata.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/buffer.h \
  ../include/inn/portable-stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/libinn.h ../include/inn/concat.h \
  ../include/inn/xmalloc.h ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/overview.h \
  ../include/inn/storage.h ../include/inn/options.h \
  ../include/inn/vector.h ../include/inn/wire.h ovinterface.h \
  ../include/inn/history.h ../include/inn/ov.h
overview.o: overview.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/buffer.h \
  ../include/inn/portable-stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/libinn.h ../include/inn/concat.h \
  ../include/inn/xmalloc.h ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/ov.h ../include/inn/history.h \
  ../include/inn/storage.h ../include/inn/options.h \
  ../include/inn/overview.h ../include/inn/vector.h ../include/inn/wire.h \
  ovinterface.h ovmethods.h
ovmethods.o: ovmethods.c ovinterface.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/inn/history.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/ov.h ../include/inn/storage.h ../include/inn/options.h \
  ovmethods.h buffindexed/buffindexed.h ovdb/ovdb.h ovsqlite/ovsqlite.h \
  tradindexed/tradindexed.h
buffindexed/buffindexed.o: buffindexed/buffindexed.c ../include/portable/system.h \
  ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/portable/macros.h ../include/portable/stdbool.h \
  ../include/portable/mmap.h ../include/inn/fdflag.h \
  ../include/inn/portable-socket.h ../include/inn/system.h \
  ../include/inn/portable-getaddrinfo.h \
  ../include/inn/portable-getnameinfo.h ../include/inn/portable-stdbool.h \
  ../include/inn/innconf.h ../include/inn/macros.h ../include/inn/libinn.h \
  ../include/inn/concat.h ../include/inn/xmalloc.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/ov.h ../include/inn/history.h \
  ../include/inn/storage.h ../include/inn/options.h ../include/inn/paths.h \
  ovinterface.h buffindexed/shmem.h buffindexed/buffindexed.h
buffindexed/shmem.o: buffindexed/shmem.c ../include/portable/system.h \
  ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/portable/macros.h ../include/portable/stdbool.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/macros.h \
  ../include/inn/portable-stdbool.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/messages.h buffindexed/shmem.h
cnfs/cnfs.o: cnfs/cnfs.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/portable/mmap.h \
  ../include/inn/fdflag.h ../include/inn/portable-socket.h \
  ../include/inn/system.h ../include/inn/portable-getaddrinfo.h \
  ../include/inn/portable-getnameinfo.h ../include/inn/portable-stdbool.h \
  ../include/inn/innconf.h ../include/inn/macros.h ../include/inn/libinn.h \
  ../include/inn/concat.h ../include/inn/xmalloc.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/mmap.h ../include/inn/paths.h \
  ../include/inn/wire.h interface.h ../include/inn/storage.h \
  ../include/inn/options.h methods.h interface.h cnfs/cnfs-private.h \
  cnfs/cnfs.h
ovdb/ovdb.o: ovdb/ovdb.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/conffile.h ../include/portable/macros.h \
  ../include/inn/fdflag.h ../include/inn/portable-socket.h \
  ../include/inn/portable-stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/libinn.h ../include/inn/concat.h \
  ../include/inn/xmalloc.h ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/newsuser.h \
  ../include/inn/paths.h ../include/inn/storage.h ../include/inn/options.h \
  ../include/portable/socket-unix.h ../include/inn/ov.h \
  ../include/inn/history.h ovdb/ovdb-private.h ovdb/ovdb.h ovinterface.h
ovsqlite/ovsqlite-private.o: ovsqlite/ovsqlite-private.c \
  ovsqlite/ovsqlite-private.h ../include/portable/system.h \
  ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/portable/macros.h ../include/portable/stdbool.h \
  ../include/inn/buffer.h ../include/inn/portable-stdbool.h \
  ../include/inn/xmalloc.h ../include/inn/system.h
ovsqlite/ovsqlite.o: ovsqlite/ovsqlite.c ovsqlite/ovsqlite.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/inn/ov.h \
  ../include/inn/history.h ../include/inn/macros.h \
  ../include/inn/portable-stdbool.h ../include/inn/storage.h \
  ../include/inn/options.h ../include/inn/messages.h \
  ovsqlite/ovsqlite-private.h ../include/portable/system.h \
  ../include/portable/macros.h ../include/portable/stdbool.h \
  ../include/inn/buffer.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/portable/socket-unix.h ../include/conffile.h \
  ../include/portable/macros.h ../include/inn/fdflag.h \
  ../include/inn/portable-socket.h ../include/inn/innconf.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/newsuser.h ../include/inn/paths.h \
  ovsqlite/../ovinterface.h
timecaf/caf.o: timecaf/caf.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/libinn.h \
  ../include/inn/concat.h ../include/inn/macros.h \
  ../include/inn/portable-stdbool.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/messages.h timecaf/caf.h
timecaf/timecaf.o: timecaf/timecaf.c ../include/portable/system.h \
  ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/portable/macros.h ../include/portable/stdbool.h \
  ../include/portable/mmap.h timecaf/caf.h ../include/inn/fdflag.h \
  ../include/inn/portable-socket.h ../include/inn/system.h \
  ../include/inn/portable-getaddrinfo.h \
  ../include/inn/portable-getnameinfo.h ../include/inn/portable-stdbool.h \
  ../include/inn/innconf.h ../include/inn/macros.h ../include/inn/libinn.h \
  ../include/inn/concat.h ../include/inn/xmalloc.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/paths.h ../include/inn/wire.h \
  methods.h interface.h ../include/inn/storage.h ../include/inn/options.h \
  timecaf/timecaf.h interface.h
timehash/timehash.o: timehash/timehash.c ../include/portable/system.h \
  ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/portable/macros.h ../include/portable/stdbool.h \
  ../include/portable/mmap.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/paths.h ../include/inn/wire.h \
  methods.h interface.h ../include/inn/storage.h ../include/inn/options.h \
  timehash/timehash.h interface.h
tradindexed/tdx-cache.o: tradindexed/tdx-cache.c ../include/portable/system.h \
  ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/portable/macros.h ../include/portable/stdbool.h \
  ../include/inn/hashtab.h ../include/inn/macros.h \
  ../include/inn/portable-stdbool.h ../include/inn/libinn.h \
  ../include/inn/concat.h ../include/inn/xmalloc.h ../include/inn/system.h \
  ../include/inn/xwrite.h ../include/inn/messages.h \
  ../include/inn/storage.h ../include/inn/options.h \
  tradindexed/tdx-private.h
tradindexed/tdx-data.o: tradindexed/tdx-data.c ../include/portable/system.h \
  ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/portable/macros.h ../include/portable/stdbool.h \
  ../include/portable/mmap.h ../include/inn/fdflag.h \
  ../include/inn/portable-socket.h ../include/inn/system.h \
  ../include/inn/portable-getaddrinfo.h \
  ../include/inn/portable-getnameinfo.h ../include/inn/portable-stdbool.h \
  ../include/inn/history.h ../include/inn/macros.h \
  ../include/inn/innconf.h ../include/inn/libinn.h ../include/inn/concat.h \
  ../include/inn/xmalloc.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/mmap.h ../include/inn/ov.h \
  ../include/inn/storage.h ../include/inn/options.h \
  ../include/inn/overview.h ovinterface.h tradindexed/tdx-private.h \
  tradindexed/tdx-structure.h
tradindexed/tdx-group.o: tradindexed/tdx-group.c ../include/portable/system.h \
  ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/portable/macros.h ../include/portable/stdbool.h \
  ../include/portable/mmap.h ../include/inn/fdflag.h \
  ../include/inn/portable-socket.h ../include/inn/system.h \
  ../include/inn/portable-getaddrinfo.h \
  ../include/inn/portable-getnameinfo.h ../include/inn/portable-stdbool.h \
  ../include/inn/hashtab.h ../include/inn/macros.h \
  ../include/inn/innconf.h ../include/inn/libinn.h ../include/inn/concat.h \
  ../include/inn/xmalloc.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/mmap.h ../include/inn/paths.h \
  ../include/inn/qio.h ../include/inn/vector.h tradindexed/tdx-private.h \
  ../include/inn/storage.h ../include/inn/options.h \
  tradindexed/tdx-structure.h
tradindexed/tradindexed.o: tradindexed/tradindexed.c ../include/portable/system.h \
  ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/portable/macros.h ../include/portable/stdbool.h \
  ../include/inn/innconf.h ../include/inn/macros.h \
  ../include/inn/portable-stdbool.h ../include/inn/libinn.h \
  ../include/inn/concat.h ../include/inn/xmalloc.h ../include/inn/system.h \
  ../include/inn/xwrite.h ../include/inn/messages.h ../include/inn/ov.h \
  ../include/inn/history.h ../include/inn/storage.h \
  ../include/inn/options.h tradindexed/tdx-private.h \
  tradindexed/tdx-structure.h tradindexed/tradindexed.h
tradspool/tradspool.o: tradspool/tradspool.c ../include/portable/system.h \
  ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/portable/macros.h ../include/portable/stdbool.h \
  ../include/portable/mmap.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/paths.h ../include/inn/qio.h \
  ../include/inn/wire.h methods.h interface.h ../include/inn/storage.h \
  ../include/inn/options.h tradspool/tradspool.h interface.h
trash/trash.o: trash/trash.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/libinn.h \
  ../include/inn/concat.h ../include/inn/macros.h \
  ../include/inn/portable-stdbool.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h methods.h interface.h \
  ../include/inn/storage.h ../include/inn/options.h trash/trash.h \
  interface.h
ovsqlite/ovsqlite-server.o: ovsqlite/ovsqlite-server.c ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/inn/messages.h \
  ovsqlite/ovsqlite-private.h ../include/portable/system.h \
  ../include/portable/macros.h ../include/portable/stdbool.h \
  ../include/inn/buffer.h ../include/inn/portable-stdbool.h \
  ../include/portable/setproctitle.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/portable/socket-unix.h ../include/inn/concat.h \
  ../include/inn/macros.h ../include/inn/confparse.h \
  ../include/inn/fdflag.h ../include/inn/portable-socket.h \
  ../include/inn/innconf.h ../include/inn/libinn.h \
  ../include/inn/xmalloc.h ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/storage.h ../include/inn/options.h ovsqlite/sql-init.h \
  ovsqlite/sqlite-helper.h ovsqlite/sql-main.h
ovsqlite/sql-init.o: ovsqlite/sql-init.c ovsqlite/sql-init.h \
  ovsqlite/sqlite-helper.h ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h
ovsqlite/sql-main.o: ovsqlite/sql-main.c ovsqlite/sql-main.h \
  ovsqlite/sqlite-helper.h ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h
ovsqlite/sqlite-helper.o: ovsqlite/sqlite-helper.c ovsqlite/sqlite-helper.h \
  ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h
tradindexed/tdx-util.o: tradindexed/tdx-util.c ../include/portable/system.h \
  ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/portable/macros.h ../include/portable/stdbool.h \
  ../include/inn/buffer.h ../include/inn/portable-stdbool.h \
  ../include/inn/history.h ../include/inn/macros.h \
  ../include/inn/innconf.h ../include/inn/libinn.h ../include/inn/concat.h \
  ../include/inn/xmalloc.h ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/newsuser.h ../include/inn/ov.h \
  ../include/inn/storage.h ../include/inn/options.h \
  ../include/inn/overview.h ../include/inn/paths.h ../include/inn/vector.h \
  ../include/inn/wire.h ovinterface.h tradindexed/tdx-private.h \
  tradindexed/tdx-structure.h

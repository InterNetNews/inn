##  $Id$

include ../Makefile.global
include Make.methods

CFLAGS	      = $(GCFLAGS) -I.

SOURCES	      = interface.c methods.c ov.c ovmethods.c
OBJECTS	      = $(SOURCES:.c=.o) $(METHOD_OBJECTS)
LOBJECTS      = $(OBJECTS:.o=.lo)

.SUFFIXES: .lo

all:		methods libstorage.$(EXTLIB)

warnings:
	$(MAKE) COPT='$(WARNINGS)' all

install:	all $(D)$(PATHLIB)/libstorage.$(EXTLIB) 

methods: methods.c buildconfig
	@for D in $(SUBDIRS) ; do \
	    cd $$D && $(MAKE) all || exit 1 ; cd .. ; \
	done

clobber clean:
	rm -f *.o *.lo libstorage.la libstorage.a
	rm -f buildconfig methods.c methods.h ovmethods.c ovmethods.h
	rm -f profiled libstorage_p.a
	rm -rf .libs
	@for D in $(SUBDIRS) ; do \
	    cd $$D && $(MAKE) clean || exit 1 ; cd .. ; \
	done

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES) ../include/*.h ../include/inn/*.h

libstorage.la:	$(OBJECTS) $(LOBJECTS)
	$(LIBLD) $(LDFLAGS) -o $@ $(LOBJECTS) \
	    -rpath $(PATHLIB) -version-info 2:0:0

libstorage.a:	$(OBJECTS)
	ar r $@ $(OBJECTS)
	$(RANLIB) libstorage.a

# Try to set up these rules so that buildconfig is only run once.
# Make.methods is included in the distribution tarball since some non-GNU
# makes can't deal with including a non-existent file, so don't depend on
# it.  The dependencies aren't entirely accurate; you really want to re-run
# buildconfig each time a new subdirectory is added to the directory.  But
# adding a dependency on . is a bit too non-portable for my taste and causes
# too many rebuilds.
Make.methods methods.h ovmethods.c ovmethods.h: methods.c buildconfig
methods.c: buildconfig
	./buildconfig

buildconfig: buildconfig.in $(FIXSCRIPT)
	$(FIXSCRIPT) -i buildconfig.in

.c.o .c.lo:
	$(LIBCC) $(CFLAGS) -c $*.c -o $@

ovtest:		ov.c libstorage.$(EXTLIB) ../lib/libinn.$(EXTLIB)
	$(CC) $(CFLAGS) -D_TEST_ -o ovtest ov.c \
	    libstorage.$(EXTLIB) $(LIBINN) $(EXTSTORAGELIBS) $(LIBS)

$(LIBINN):      ; (cd ../lib ; $(MAKE))

##  Profiling.  The rules are a bit brute-force, but good enough.
profiled:	../libstorage_p.a
	date >$@

../libstorage_p.a:	$(SOURCES)
	rm -f $(OBJECTS)
	$(MAKE) libstorage.a CFLAGS="$(CFLAGS) $(PROF)"
	mv libstorage.a ../libstorage_p.a
	$(RANLIB) ../libstorage_p.a
	rm -f $(OBJECTS)

##  Low-level install actions.
$(D)$(PATHLIB)/libstorage.$(EXTLIB): libstorage.$(EXTLIB)
	$(LI_XPUB) libstorage.$(EXTLIB) $@

##  Dependencies.  Default list, below, is probably good enough.

depend:	Makefile $(SOURCES)
	$(MAKEDEPEND) $(CFLAGS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
interface.o: interface.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/conffile.h interface.h \
 ../include/storage.h ../include/libinn.h ../include/macros.h \
 methods.h ../include/paths.h
methods.o: methods.c interface.h ../include/config.h \
 ../include/inn/defines.h ../include/storage.h methods.h cnfs/cnfs.h \
 timecaf/timecaf.h timehash/timehash.h tradspool/tradspool.h \
 trash/trash.h
ov.o: ov.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/macros.h ../include/libinn.h \
 ../include/ov.h ../include/storage.h ../include/inn/history.h \
 ovinterface.h ovmethods.h ../include/paths.h
ovmethods.o: ovmethods.c ovinterface.h ../include/config.h \
 ../include/inn/defines.h ../include/ov.h ../include/storage.h \
 ../include/inn/history.h buffindexed/buffindexed.h ovdb/ovdb.h \
 ov3/tradindexed.h

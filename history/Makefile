##  $Id$

include ../Makefile.global

top           = ..
CFLAGS        = $(GCFLAGS) -I.

SOURCES       = his.c hismethods.c $(METHOD_SOURCES)
OBJECTS       = $(SOURCES:.c=.o)
LOBJECTS      = $(OBJECTS:.o=.lo)

.SUFFIXES: .lo

all: library programs

# Included here after the all target, since additional rules are defined in
# Make.methods to be sure that we recurse properly to build the methods.
include Make.methods

warnings:
	$(MAKE) COPT='$(WARNINGS)' all

install: all
	$(LI_XPUB) libinnhist.$(EXTLIB) $D$(PATHLIB)/libinnhist.$(EXTLIB)

library: libinnhist.$(EXTLIB)

programs: $(PROGRAMS)

clobber clean distclean:
	rm -f *.o *.lo */*.o */*.lo libinnhist.la libinnhist.a
	rm -f libinnhist_pure_*.a .pure $(PROGRAMS)
	rm -f buildconfig hismethods.c hismethods.h
	rm -f profiled libinnhist$(PROFSUFFIX).a
	rm -rf .libs */.libs

tags ctags: $(SOURCES)
	$(CTAGS) $(SOURCES) ../include/*.h

$(FIXSCRIPT):
	@echo Run configure before running make.  See INSTALL for details.
	@exit 1

libinnhist.la: $(OBJECTS) $(LIBSTORAGE) $(LIBINN)
	$(LIBLD) $(LDFLAGS) -o $@ $(LOBJECTS) \
	    $(LIBSTORAGE) $(LIBINN) $(STORAGE_LIBS) $(LIBS) \
	    -rpath $(PATHLIB) -version-info 2:0:0

libinnhist.a: $(OBJECTS)
	ar r $@ $(OBJECTS)
	$(RANLIB) libinnhist.a

# Try to set up these rules so that buildconfig is only run once.
# Make.methods is included in the distribution tarball since some non-GNU
# makes can't deal with including a non-existent file, so don't depend on
# it.  The dependencies aren't entirely accurate; you really want to re-run
# buildconfig each time a new subdirectory is added to the directory.  But
# adding a dependency on . is a bit too non-portable for my taste and causes
# too many rebuilds.
Make.methods hismethods.h: hismethods.c buildconfig
hismethods.c: buildconfig
	./buildconfig

buildconfig: buildconfig.in $(FIXSCRIPT)
	$(FIXSCRIPT) -i buildconfig.in

.c.o .c.lo:
	$(LIBCC) $(CFLAGS) $(CCOUTPUT)

$(LIBINN):      ; (cd ../lib ; $(MAKE))
$(LIBSTORAGE):	; (cd ../storage ; $(MAKE) library)


##  Profiling.  The rules are a bit brute-force, but good enough.

profiled: libinnhist$(PROFSUFFIX).a
	date >$@

libinnhist$(PROFSUFFIX).a: $(SOURCES)
	rm -f $(OBJECTS)
	$(MAKEPROFILING) libinnhist.a
	mv libinnhist.a libinnhist$(PROFSUFFIX).a
	$(RANLIB) libinnhist$(PROFSUFFIX).a
	rm -f $(OBJECTS)


##  Dependencies.  Default list, below, is probably good enough.

depend:	$(SOURCES) $(EXTRA_SOURCES)
	$(MAKEDEPEND) '$(CFLAGS)' $(SOURCES) $(EXTRA_SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
his.o: his.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/portable/time.h \
 ../include/inn/history.h ../include/inn/messages.h \
 ../include/inn/timer.h ../include/libinn.h ../include/storage.h \
 hisinterface.h hismethods.h
hismethods.o: hismethods.c hisinterface.h ../include/config.h \
 ../include/inn/defines.h hismethods.h hisv6/hisv6.h
hisv6/hisv6.o: hisv6/hisv6.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h hisinterface.h hisv6/hisv6.h \
 hisv6/hisv6-private.h ../include/inn/history.h ../include/storage.h \
 ../include/libinn.h ../include/dbz.h ../include/inn/timer.h \
 ../include/inn/qio.h ../include/inn/sequence.h ../include/inndcomm.h

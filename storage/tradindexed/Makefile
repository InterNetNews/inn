##  $Id$

include ../../Makefile.global

top		= ../..
CFLAGS		= $(GCFLAGS) -I..

PROGRAMS	= tdx-util
SOURCES 	= tdx-cache.c tdx-data.c tdx-group.c tdx-util.c tradindexed.c
OBJECTS 	= tdx-cache.o tdx-data.o tdx-group.o tradindexed.o

INSTALLED	= $(D)$(PATHBIN)/tdx-util

STORELIBS	= $(LIBSTORAGE) $(LIBHIST) $(LIBINN) $(EXTSTORAGELIBS) $(LIBS)

.SUFFIXES: .lo

all: objects programs

objects: $(OBJECTS)

programs: $(PROGRAMS)

warnings:
	$(MAKE) COPT='$(WARNINGS)' all

.c.o .c.lo:
	$(LIBCC) $(CFLAGS) -c $*.c -o $@

tdx-util: tdx-util.o $(OBJECTS)
	$(LIBLD) $(LDFLAGS) -o $@ tdx-util.o $(OBJECTS) $(STORELIBS)

clobber clean distclean:
	rm -f *.o *.lo $(ALL)
	rm -rf .libs

tags ctags: $(SOURCES)
	$(CTAGS) $(SOURCES) ../../include/*.h ../*.h


##  Installation rules.  Installation commands set in Makefile.global.

install: $(INSTALLED)

$(D)$(PATHBIN)/tdx-util:	tdx-util	; $(LI_XPRI) $? $@


##  Dependencies.  Default list, below, is probably good enough.

depend:	Makefile $(SOURCES)
	$(MAKEDEPEND) '$(CFLAGS)' $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
tdx-cache.o: tdx-cache.c ../../include/config.h \
 ../../include/inn/defines.h ../../include/clibrary.h \
 ../../include/inn/hashtab.h ../../include/inn/messages.h \
 ../../include/libinn.h ../../include/storage.h tdx-private.h
tdx-data.o: tdx-data.c ../../include/config.h \
 ../../include/inn/defines.h ../../include/clibrary.h \
 ../../include/portable/mmap.h ../../include/inn/history.h \
 ../../include/inn/innconf.h ../../include/inn/messages.h \
 ../../include/inn/mmap.h ../../include/libinn.h ../../include/ov.h \
 ../../include/storage.h ../ovinterface.h tdx-private.h \
 tdx-structure.h
tdx-group.o: tdx-group.c ../../include/config.h \
 ../../include/inn/defines.h ../../include/clibrary.h \
 ../../include/portable/mmap.h ../../include/inn/hashtab.h \
 ../../include/inn/innconf.h ../../include/inn/messages.h \
 ../../include/inn/mmap.h ../../include/inn/qio.h \
 ../../include/libinn.h ../../include/paths.h tdx-private.h \
 ../../include/storage.h tdx-structure.h
tdx-util.o: tdx-util.c ../../include/config.h \
 ../../include/inn/defines.h ../../include/clibrary.h \
 ../../include/inn/buffer.h ../../include/inn/history.h \
 ../../include/inn/innconf.h ../../include/inn/messages.h \
 ../../include/inn/vector.h ../../include/libinn.h ../../include/ov.h \
 ../../include/storage.h ../ovinterface.h ../../include/paths.h \
 tdx-private.h tdx-structure.h
tradindexed.o: tradindexed.c ../../include/config.h \
 ../../include/inn/defines.h ../../include/clibrary.h \
 ../../include/inn/innconf.h ../../include/inn/messages.h \
 ../../include/libinn.h ../../include/ov.h ../../include/storage.h \
 ../../include/inn/history.h tdx-private.h tdx-structure.h \
 tradindexed.h

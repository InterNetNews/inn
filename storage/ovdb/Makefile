##  $Id$

include ../../Makefile.global

top	= ../..
CFLAGS	= $(GCFLAGS) -I.. $(BERKELEY_DB_CFLAGS)

SOURCES	= ovdb.c
OBJECTS	= ovdb.o

.SUFFIXES: .lo

all: objects

objects: $(OBJECTS)

install programs:

warnings:
	$(MAKE) COPT='$(WARNINGS)' all

.c.o .c.lo:
	$(LIBCC) $(CFLAGS) -c $*.c -o $@

clobber clean distclean:
	rm -f *.o *.lo
	rm -rf .libs

tags ctags: $(SOURCES)
	$(CTAGS) $(SOURCES) ../../include/*.h ../*.h

##  Dependencies.  Default list, below, is probably good enough.

depend:	Makefile $(SOURCES)
	$(MAKEDEPEND) $(CFLAGS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
ovdb.o: ovdb.c ../../include/config.h ../../include/inn/defines.h \
 ../../include/clibrary.h ../../include/macros.h \
 ../../include/conffile.h ../../include/libinn.h ../../include/paths.h \
 ../../include/storage.h ../../include/ov.h \
 ../../include/inn/history.h ../ovinterface.h ovdb.h ovdb-private.h \
 ../../include/inn/version.h ../../include/portable/time.h

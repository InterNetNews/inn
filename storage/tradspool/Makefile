##  $Id$

include ../../Makefile.global

top	= ../..
CFLAGS	= $(GCFLAGS) -I..

SOURCES	= tradspool.c
OBJECTS	= tradspool.o

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
	$(MAKEDEPEND) '$(CFLAGS)' $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
tradspool.o: tradspool.c ../../include/config.h \
 ../../include/inn/defines.h ../../include/clibrary.h \
 ../../include/portable/mmap.h ../../include/inn/innconf.h \
 ../../include/inn/qio.h ../../include/inn/wire.h \
 ../../include/libinn.h ../../include/macros.h ../../include/paths.h \
 ../methods.h ../interface.h ../../include/storage.h tradspool.h

##  $Id$

include ../../Makefile.global

top     = ../..
CFLAGS  = $(GCFLAGS) -I..

SOURCES	= caf.c timecaf.c
OBJECTS	= caf.o timecaf.o

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
caf.o: caf.c ../../include/config.h ../../include/inn/defines.h \
 ../../include/clibrary.h ../../include/libinn.h \
 ../../include/macros.h caf.h
timecaf.o: timecaf.c ../../include/config.h \
 ../../include/inn/defines.h ../../include/clibrary.h \
 ../../include/portable/mmap.h caf.h ../../include/inn/innconf.h \
 ../../include/inn/wire.h ../../include/libinn.h \
 ../../include/macros.h ../methods.h ../interface.h \
 ../../include/storage.h timecaf.h ../../include/paths.h

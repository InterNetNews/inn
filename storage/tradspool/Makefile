##  $Id$

include ../../Makefile.global

CFLAGS  = $(GCFLAGS) -I../../include -I..

SOURCES = tradspool.c
OBJECTS = tradspool.o
LOBJECTS = tradspool.lo
ROBJECTS = tradspool.$(EXTOBJ)

.SUFFIXES: .lo

all:			$(OBJECTS)

warnings:
	$(MAKE) COPT='$(WARNINGS)' all

.c.o .c.lo:
	$(UPLIBCC) $(CFLAGS) -c $*.c -o $@

clobber clean:
	rm -f *.o *.lo obj/*.o *~
	rm -f profiled
	rm -f all install lint lint.all
	rm -rf .libs

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES) ../../include/*.h ../*.h

##  Dependencies.  Default list, below, is probably good enough.

depend:	Makefile $(SOURCES)
	$(UPMAKEDEPEND) $(CFLAGS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
tradspool.o: tradspool.c ../../include/config.h \
 ../../include/inn/defines.h ../../include/clibrary.h \
 ../../include/inn/qio.h ../../include/libinn.h ../../include/macros.h \
 ../../include/paths.h ../methods.h tradspool.h

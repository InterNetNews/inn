##  $Id$

include ../../Makefile.global

CFLAGS	= $(GCFLAGS) -I../../include -I..

SOURCES = timehash.c
OBJECTS = timehash.o
LOBJECTS = timehash.lo
ROBJECTS = timehash.$(EXTOBJ)

.SUFFIXES: .lo

all:			$(OBJECTS)

warnings:
	$(MAKE) COPT='$(WARNINGS)' all

.c.o .c.lo:
	$(UPLIBCC) $(CFLAGS) -c $*.c -o $@

clobber clean:
	rm -f *.o *.lo obj/*.o profiled
	rm -rf .libs

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES) ../../include/*.h ../*.h

##  Dependencies.  Default list, below, is probably good enough.

depend:	Makefile $(SOURCES)
	$(UPMAKEDEPEND) $(CFLAGS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
timehash.o: timehash.c ../../include/config.h \
 ../../include/inn/defines.h ../../include/clibrary.h \
 ../../include/libinn.h ../../include/macros.h ../methods.h \
 ../../include/paths.h timehash.h

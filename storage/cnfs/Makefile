##  $Id$

include ../../Makefile.global

CFLAGS	 = $(GCFLAGS) -I../../include -I.. 

SOURCES  = cnfs.c
OBJECTS  = cnfs.o
LOBJECTS = cnfs.lo
ROBJECTS = cnfs.$(EXTOBJ)

.SUFFIXES: .lo

all:		$(OBJECTS)

warnings:
	$(MAKE) COPT='$(WARNINGS)' all

.c.o .c.lo:
	$(UPLIBCC) $(CFLAGS) -c $*.c

clobber clean:
	rm -f *.o *.lo obj/*.o
	rm -f profiled
	rm -f all install
	rm -rf .libs

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES) ../../include/*.h ../*.h

##  Dependencies.  Default list, below, is probably good enough.

depend:	Makefile $(SOURCES)
	$(UPMAKEDEPEND) $(CFLAGS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
cnfs.o: cnfs.c ../../include/config.h ../../include/clibrary.h \
 ../interface.h ../../include/libinn.h ../../include/macros.h \
 ../methods.h cnfs.h cnfs-private.h ../../include/paths.h

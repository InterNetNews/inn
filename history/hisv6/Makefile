##  $Id$

include ../../Makefile.global

CFLAGS	= $(GCFLAGS) -I../../include -I.. $(BERKELEY_DB_CFLAGS)

SOURCES = hisv6.c
OBJECTS = hisv6.o
LOBJECTS = hisv6.lo
ROBJECTS = hisv6.$(EXTOBJ)

all:			$(OBJECTS)

.c.o:
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

##  $Revision$
include ../../Makefile.global

CP	= $(UPLIBTOOL) cp

CFLAGS	= $(GCFLAGS) -I../../include -I..

SOURCES = timehash.c
OBJECTS = timehash.o
LOBJECTS = timehash.lo
ROBJECTS = timehash.$(EXTOBJ)

all:			$(OBJECTS) moveobjs

.c.o: $*.c
	$(UPLIBCC) $(CFLAGS) -c $*.c -o $@

moveobjs: $(OBJECTS)
	$(CP) -p $(ROBJECTS) ../objs

clobber clean:
	rm -f *.o *.lo obj/*.o *~
	rm -f profiled
	rm -f all install 

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES) ../../include/*.h ../*.h

##  Dependencies.  Default list, below, is probably good enough.
depend:		Makefile $(SOURCES)
	makedepend $(DEFS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.

interface.o: methods.h interface.h
methods.o: interface.h

##  $Revision$
include ../../Makefile.global

CC	= ../$(LIBCC)
CP	= ../$(LIBTOOL) cp

CFLAGS	= $(GCFLAGS) -I../../include -I..

IFTRUE	= @$(SHELL) ../iftrue.sh

SOURCES = timehash.c
OBJECTS = timehash.o
LOBJECTS = timehash.lo

all:			$(OBJECTS) moveobjs

moveobjs: $(OBJECTS)
	$(CP) -p $(LOBJECTS) ../objs

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

##  $Id$

include ../../Makefile.global

CP	= $(UPLIBTOOL) cp
CFLAGS	= $(GCFLAGS) -I../../include -I..

SOURCES = timehash.c
OBJECTS = timehash.o
LOBJECTS = timehash.lo
ROBJECTS = timehash.$(EXTOBJ)

all:			$(OBJECTS) moveobjs

.c.o:
	$(UPLIBCC) $(CFLAGS) -c $*.c -o $@

moveobjs: $(OBJECTS)
	$(CP) -p $(ROBJECTS) ../objs

clobber clean:
	rm -f *.o *.lo obj/*.o profiled
	rm -rf .libs

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES) ../../include/*.h ../*.h

##  Dependencies.  Default list, below, is probably good enough.

depend:	Makefile $(SOURCES)
	$(UPMAKEDEPEND) $(CFLAGS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
timehash.o: timehash.c ../../include/config.h ../../include/clibrary.h \
 ../../include/libinn.h ../../include/macros.h ../methods.h \
 ../../include/paths.h timehash.h

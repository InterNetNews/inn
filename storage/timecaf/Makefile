##  $Revision$
include ../../Makefile.global

CP	= $(UPLIBTOOL) cp

CFLAGS  = $(GCFLAGS) -I../../include -I..

SOURCES = caf.c timecaf.c
OBJECTS = caf.o timecaf.o
LOBJECTS = caf.lo timecaf.lo
ROBJECTS = caf.$(EXTOBJ) timecaf.$(EXTOBJ)

all:			$(OBJECTS) moveobjs

.c.o: $*.c
	$(UPLIBCC) $(CFLAGS) -c $*.c -o $@

moveobjs: $(OBJECTS)
	$(CP) -p caf.$(EXTOBJ) ../objs
	$(CP) -p timecaf.$(EXTOBJ) ../objs


clobber clean:
	rm -f *.o *.lo obj/*.o *~
	rm -f profiled
	rm -f all install lint lint.all

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES) ../../include/*.h ../*.h

##  Dependencies.  Default list, below, is probably good enough.
depend:		Makefile $(SOURCES)
	makedepend $(DEFS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.

caf.o: caf.h
timecaf.o: caf.h timecaf.h

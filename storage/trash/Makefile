##  $Revision$
include ../../Makefile.global

CP	= $(UPLIBTOOL) cp

CFLAGS	= $(GCFLAGS) -I../../include -I..

SOURCES = trash.c
OBJECTS = trash.o
LOBJECTS = trash.lo
ROBJECTS = trash.$(EXTOBJ)

all:			$(OBJECTS) moveobjs

.c.o: $*.c
	$(UPLIBCC) $(CFLAGS) -c $*.c -o $@

moveobjs: $(OBJECTS)
	$(CP) -p $(ROBJECTS) ../objs

clobber clean:
	rm -f *.o *.lo obj/*.o *~
	rm -f profiled
	rm -f all install lint lint.all

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES) ../../include/*.h ../*.h

##  Dependencies.  Default list, below, is probably good enough.
depend:		Makefile $(SOURCES)
	makedepend $(DEFS) $(SOURCES)

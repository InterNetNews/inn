##  $Revision$
include ../../Makefile.global

CP	= $(UPLIBTOOL) cp
CFLAGS  = $(GCFLAGS) -I../../include -I..

SOURCES = tradspool.c
OBJECTS = tradspool.o
LOBJECTS = tradspool.lo
ROBJECTS = tradspool.$(EXTOBJ)

all:			$(OBJECTS) moveobjs

.c.o:
	$(UPLIBCC) $(CFLAGS) -c $*.c -o $@

moveobjs: $(OBJECTS)
	$(CP) -p $(ROBJECTS) ../objs

clobber clean:
	rm -f *.o *.lo obj/*.o *~
	rm -f profiled
	rm -f all install lint lint.all
	rm -rf .libs

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES) ../../include/*.h ../*.h

##  Dependencies.  Default list, below, is probably good enough.
depend:		Makefile $(SOURCES)
	makedepend $(DEFS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.


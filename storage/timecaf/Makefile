##  $Revision$
include ../../Makefile.global

CC	= $(UPLIBCC)
CP	= $(UPLIBTOOL) cp

CFLAGS  = $(GCFLAGS)	-I../../include -I.. -Wunused

IFTRUE	= @$(SHELL) ../iftrue.sh

##  =()<P	= @<P>@>()=
P	= 

##  =()<CTAGS		= @<CTAGS>@>()=
CTAGS		= /usr/local/bin/ctags -t -w
##  =()<PROF	= @<PROF>@>()=
PROF	= -pg

SOURCES = caf.c timecaf.c
OBJECTS = caf.o timecaf.o
LOBJECTS = caf.lo timecaf.lo
ROBJECTS = caf.$(EXTOBJ) timecaf.$(EXTOBJ)

all:			$(OBJECTS) moveobjs

moveobjs: $(OBJECTS)
	$(CP) -p $(ROBJECTS) ../objs
#	$(CP) -p caf.lo ../objs
#	$(CP) -p timecaf.lo ../objs


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

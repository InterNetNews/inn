##  $Revision$
include ../../Makefile.global

CC	= $(UPLIBCC)
CP	= $(UPLIBTOOL) cp

CFLAGS  = $(GCFLAGS)	-I../../include -I..

IFTRUE	= @$(SHELL) ../iftrue.sh

##  =()<P	= @<P>@>()=
P	= 

##  =()<CTAGS		= @<CTAGS>@>()=
CTAGS		= /usr/local/bin/ctags -t -w
##  =()<PROF	= @<PROF>@>()=
PROF	= -pg

SOURCES = tradspool.c
OBJECTS = tradspool.o
LOBJECTS = tradspool.lo
ROBJECTS = tradspool.$(EXTOBJ)

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

interface.o: methods.h interface.h
methods.o: interface.h

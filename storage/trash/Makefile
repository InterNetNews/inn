##  $Revision$
include ../../Makefile.global

CC	= ../$(LIBCC)
CP	= ../$(LIBTOOL) cp

CFLAGS	= $(GCFLAGS) -I../../include -I..

IFTRUE	= @$(SHELL) ../iftrue.sh

##  =()<P	= @<P>@>()=
P	= 

##  =()<CTAGS		= @<CTAGS>@>()=
CTAGS		= /usr/bin/ctags -t -w
##  =()<PROF	= @<PROF>@>()=
PROF	= -pg

SOURCES = trash.c
OBJECTS = trash.o
LOBJECTS = trash.lo

all:			$(OBJECTS) moveobjs

moveobjs: $(OBJECTS)
	$(CP) -p $(LOBJECTS) ../objs

clobber clean:
	rm -f *.o *.lo obj/*.o *~
	rm -f profiled
	rm -f all install lint lint.all

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES) ../../include/*.h ../*.h

##  Dependencies.  Default list, below, is probably good enough.
depend:		Makefile $(SOURCES)
	makedepend $(DEFS) $(SOURCES)

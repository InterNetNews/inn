##  $Revision$
include ../../Makefile.global

CFLAGS +=	-I../../include -I..

IFTRUE	= @$(SHELL) ../iftrue.sh

##  =()<P	= @<P>@>()=
P	= 

##  =()<CTAGS		= @<CTAGS>@>()=
CTAGS		= /usr/bin/ctags -t -w
##  =()<PROF	= @<PROF>@>()=
PROF	= -pg

SOURCES = trash.c
OBJECTS = trash.o

all:			$(OBJECTS) moveobjs

moveobjs: $(OBJECTS)
	cp -p $(OBJECTS) ../objs

clobber clean:
	rm -f *.o obj/*.o *~
	rm -f profiled
	rm -f all install lint lint.all

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES) ../../include/*.h ../*.h

##  Dependencies.  Default list, below, is probably good enough.
depend:		Makefile $(SOURCES)
	makedepend $(DEFS) $(SOURCES)

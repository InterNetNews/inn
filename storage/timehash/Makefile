##  $Revision$
SHELL	= /bin/sh
MAKE	= make
IFTRUE	= @$(SHELL) ../iftrue.sh

##  =()<P	= @<P>@>()=
P	= 

##  =()<CC	= @<CC>@>()=
CC	= gcc
##  =()<DEFS	= -I.. -I../../include @<DEFS>@>()=
DEFS	= -I.. -I../../include -I../include
##  =()<CFLAGS	= -I. @<CFLAGS>@ $(PERLINC)>()=
CFLAGS	= -I. $(DEFS) -g -Wall $(PERLINC)
##  =()<LDFLAGS	= @<LDFLAGS>@>()=
LDFLAGS	= 
##  =()<CTAGS		= @<CTAGS>@>()=
CTAGS		= ctags -t -w
##  =()<PROF	= @<PROF>@>()=
PROF	= -pg

##  =()<RANLIB	= @<RANLIB>@>()=
RANLIB	= ranlib
##  =()<LIBS	= @<LIBS>@>()=
LIBS	= 

SOURCES = timehash.c
OBJECTS = timehash.o

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

# DO NOT DELETE THIS LINE -- make depend depends on it.

interface.o: methods.h interface.h
methods.o: interface.h

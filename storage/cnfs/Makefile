# Generated automatically from Makefile.in by configure.
##  $Revision$
SHELL	= /bin/sh
MAKE	= make
IFTRUE	= @$(SHELL) ../iftrue.sh

##  =()<P	= @<P>@>()=
P	= 

CC      = gcc
CFLAGS  = -I../include -I.. -I../../include   
LDFLAGS = 
LIBS    = -lelf -lsocket -lnsl  -lresolv
RANLIB	= :

##  =()<CTAGS		= @<CTAGS>@>()=
CTAGS		= /usr/local/bin/ctags -t -w
##  =()<PROF	= @<PROF>@>()=
PROF	= -pg


SOURCES = cnfs.c
OBJECTS = cnfs.o

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

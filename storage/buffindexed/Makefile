##  $Revision$
include ../../Makefile.global

CC	= $(UPLIBCC)
CP	= $(UPLIBTOOL) cp

CFLAGS	= $(GCFLAGS) -I../../include -I..

IFTRUE	= @$(SHELL) ../iftrue.sh

##  =()<P	= @<P>@>()=
P	= 

##  =()<CTAGS		= @<CTAGS>@>()=
CTAGS		= /usr/bin/ctags -t -w
##  =()<PROF	= @<PROF>@>()=
PROF	= -pg

SOURCES = buffindexed.c
OBJECTS = buffindexed.o
LOBJECTS = buffindexed.lo
ROBJECTS = buffindexed.$(EXTOBJ)

all:			$(OBJECTS) moveobjs

moveobjs: $(OBJECTS)
	$(CP) -p $(ROBJECTS) ../objs

debug:	buffindexed

buffindexed:
	rm -f buffindexed.o ../objs/*.o
	$(CC) $(CFLAGS) -DDEBUG -c buffindexed.c
	$(CC) $(LDFLAGS) -o $@ ../*/*.o ../interface.o ../qio.o ../methods.o ../../lib/libinn.$(EXTLIB) $(LIBS)

clobber clean:
	rm -f *.o *.lo obj/*.o *~
	rm -f profiled
	rm -f all install lint lint.all

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES) ../../include/*.h ../*.h

##  Dependencies.  Default list, below, is probably good enough.
depend:		Makefile $(SOURCES)
	makedepend $(DEFS) $(SOURCES)

##  $Id$

include ../../Makefile.global

CP      = $(UPLIBTOOL) cp
CFLAGS  = $(GCFLAGS) -I../../include -I..

SOURCES = buffindexed.c
OBJECTS = buffindexed.o
ROBJECTS= buffindexed.$(EXTOBJ)

all: $(OBJECTS) moveobjs

.c.o: $*.c
	$(UPLIBCC) $(CFLAGS) -c $*.c -o $@

moveobjs: $(OBJECTS)
	$(CP) -p $(ROBJECTS) ../objs

debug:	buffindexed

buffindexed:
	rm -f buffindexed.o ../objs/*.o
	$(CC) $(CFLAGS) -DDEBUG -c buffindexed.c
	$(CC) $(LDFLAGS) -o $@ ../*/*.o ../interface.o ../methods.o ../../lib/libinn.$(EXTLIB) $(LIBS)

clobber clean:
	rm -f *.o *.lo obj/*.o
	rm -f profiled
	rm -f all install lint lint.all
	rm -rf .libs

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES) ../../include/*.h ../*.h

##  Dependencies.  Default list, below, is probably good enough.
depend:		Makefile $(SOURCES)
	makedepend $(DEFS) $(SOURCES)

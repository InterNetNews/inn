##  $Revision$
include ../../Makefile.global

CP	= $(UPLIBTOOL) cp

CFLAGS	= $(GCFLAGS) -I../../include -I.. $(BERKELEY_DB_CFLAGS)

SOURCES = ovdb.c
OBJECTS = ovdb.o
LOBJECTS = ovdb.lo
ROBJECTS = ovdb.$(EXTOBJ)

all:			$(OBJECTS) moveobjs

.c.o:
	$(UPLIBCC) $(CFLAGS) -c $*.c -o $@

ovdb:
	rm -f ovdb.o ../objs/*.o
	$(CC) $(CFLAGS) -DTEST_BDB -c ovdb.c
	$(CC) $(LDFLAGS) -o $@ ../*/*.o ../interface.o ../methods.o ../../lib/libinn.$(EXTLIB) $(EXTSTORAGELIBS) $(LIBS)

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

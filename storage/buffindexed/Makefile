##  $Id$

include ../../Makefile.global

CFLAGS  = $(GCFLAGS) -I../../include -I..

SOURCES = buffindexed.c
OBJECTS = buffindexed.o
ROBJECTS= buffindexed.$(EXTOBJ)

all: $(OBJECTS)

.c.o:
	$(UPLIBCC) $(CFLAGS) -c $*.c -o $@

debug:	buffindexed

buffindexed:
	rm -f buffindexed.o ../objs/*.o
	$(CC) $(CFLAGS) -DDEBUG -c buffindexed.c
	$(CC) $(LDFLAGS) -o $@ ../*/*.o ../interface.o ../methods.o ../ov.o ../ovmethods.o ../../lib/libinn.$(EXTLIB) $(LIBS)

clobber clean:
	rm -f *.o *.lo obj/*.o
	rm -f profiled
	rm -f all install lint lint.all
	rm -rf .libs

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES) ../../include/*.h ../*.h

##  Dependencies.  Default list, below, is probably good enough.

depend:	$(SOURCES)
	$(UPMAKEDEPEND) $(CFLAGS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
buffindexed.o: buffindexed.c ../../include/config.h \
 ../../include/clibrary.h ../../include/libinn.h \
 ../../include/macros.h ../../include/ov.h ../../include/storage.h \
 ../../include/paths.h ../../include/qio.h ../ovinterface.h \
 buffindexed.h

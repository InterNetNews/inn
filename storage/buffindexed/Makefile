##  $Id$

include ../../Makefile.global

top     = ../..
CFLAGS  = $(GCFLAGS) -I..

SOURCES = buffindexed.c
OBJECTS = buffindexed.o
ROBJECTS= buffindexed.$(EXTOBJ)

.SUFFIXES: .lo

all: $(OBJECTS)

warnings:
	$(MAKE) COPT='$(WARNINGS)' all

.c.o .c.lo:
	$(LIBCC) $(CFLAGS) -c $*.c -o $@

debug:	buffindexed

buffindexed:
	rm -f buffindexed.o ../objs/*.o
	$(CC) $(CFLAGS) -DDEBUG -c buffindexed.c
	$(LIBLD) $(LDFLAGS) -o $@ ../*/*.o ../interface.o ../methods.o ../ov.o ../ovmethods.o $(LIBINN) $(LIBS)

clobber clean:
	rm -f *.o *.lo obj/*.o
	rm -f profiled
	rm -f all install lint lint.all
	rm -rf .libs

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES) ../../include/*.h ../*.h

##  Dependencies.  Default list, below, is probably good enough.

depend:	$(SOURCES)
	$(MAKEDEPEND) $(CFLAGS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
buffindexed.o: buffindexed.c ../../include/config.h \
 ../../include/inn/defines.h ../../include/clibrary.h \
 ../../include/portable/mmap.h ../../include/libinn.h \
 ../../include/macros.h ../../include/ov.h ../../include/storage.h \
 ../../include/inn/history.h ../../include/paths.h ../ovinterface.h \
 buffindexed.h

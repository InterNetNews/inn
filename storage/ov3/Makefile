##  $Id$

include ../../Makefile.global

CFLAGS	= $(GCFLAGS) -I../../include -I..

ALL = tdx-util

SOURCES = ov3.c
OBJECTS = ov3.o
LOBJECTS = ov3.lo
ROBJECTS = ov3.$(EXTOBJ)

.SUFFIXES: .lo

all: $(ALL) $(OBJECTS)

.c.o .c.lo:
	$(UPLIBCC) $(CFLAGS) -c $*.c -o $@

tdx-util: group.o tdx-util.o
	$(UPLIBLD) $(LDFLAGS) -o $@ group.o tdx-util.o ../$(LIBINN) $(LIBS)

clobber clean:
	rm -f *.o *.lo obj/*.o $(ALL)
	rm -rf .libs

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES) ../../include/*.h ../*.h

##  Dependencies.  Default list, below, is probably good enough.

depend:	Makefile $(SOURCES)
	$(UPMAKEDEPEND) $(CFLAGS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
ov3.o: ov3.c ../../include/config.h ../../include/inn/defines.h \
 ../../include/clibrary.h ../../include/portable/mmap.h \
 ../../include/libinn.h ../../include/macros.h ../../include/ov.h \
 ../../include/storage.h ../../include/inn/history.h \
 ../../include/paths.h ../ovinterface.h tradindexed.h structure.h

##  $Id$

include ../../Makefile.global

CFLAGS	= $(GCFLAGS) -I../../include -I..

ALL	= tdx-util
SOURCES = group.c tdx-util.c
OBJECTS = group.o

.SUFFIXES: .lo

all: $(ALL) $(OBJECTS)

warnings:
	$(MAKE) COPT='$(WARNINGS)' all

.c.o .c.lo:
	$(UPLIBCC) $(CFLAGS) -c $*.c -o $@

tdx-util: group.o tdx-util.o
	$(UPLIBLD) $(LDFLAGS) -o $@ tdx-util.o $(OBJECTS) ../$(LIBINN) $(LIBS)

clobber clean:
	rm -f *.o *.lo obj/*.o $(ALL)
	rm -rf .libs

tags ctags: $(SOURCES)
	$(CTAGS) $(SOURCES) ../../include/*.h ../*.h

##  Dependencies.  Default list, below, is probably good enough.

depend:	Makefile $(SOURCES)
	$(UPMAKEDEPEND) $(CFLAGS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
group.o: group.c ../../include/config.h ../../include/inn/defines.h \
 ../../include/clibrary.h ../../include/portable/mmap.h \
 ../../include/libinn.h ../../include/ov.h ../../include/storage.h \
 ../../include/inn/history.h tdx-private.h tdx-structure.h
tdx-util.o: tdx-util.c ../../include/config.h \
 ../../include/inn/defines.h ../../include/clibrary.h \
 ../../include/libinn.h ../../include/ov.h ../../include/storage.h \
 ../../include/inn/history.h tdx-private.h

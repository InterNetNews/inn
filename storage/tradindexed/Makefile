##  $Id$

include ../../Makefile.global

top	= ../..
CFLAGS	= $(GCFLAGS) -I..

ALL	= tdx-util
SOURCES = tdx-group.c tdx-util.c
OBJECTS = tdx-group.o

.SUFFIXES: .lo

all: $(ALL) $(OBJECTS)

warnings:
	$(MAKE) COPT='$(WARNINGS)' all

.c.o .c.lo:
	$(LIBCC) $(CFLAGS) -c $*.c -o $@

tdx-util: group.o tdx-util.o
	$(LIBLD) $(LDFLAGS) -o $@ tdx-util.o $(OBJECTS) ../$(LIBINN) $(LIBS)

clobber clean:
	rm -f *.o *.lo obj/*.o $(ALL)
	rm -rf .libs

tags ctags: $(SOURCES)
	$(CTAGS) $(SOURCES) ../../include/*.h ../*.h

##  Dependencies.  Default list, below, is probably good enough.

depend:	Makefile $(SOURCES)
	$(MAKEDEPEND) $(CFLAGS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
tdx-group.o: tdx-group.c ../../include/config.h \
 ../../include/inn/defines.h ../../include/clibrary.h \
 ../../include/portable/mmap.h ../../include/inn/hashtab.h \
 ../../include/inn/qio.h ../../include/libinn.h ../../include/ov.h \
 ../../include/storage.h ../../include/inn/history.h \
 ../../include/paths.h tdx-private.h tdx-structure.h
tdx-util.o: tdx-util.c ../../include/config.h \
 ../../include/inn/defines.h ../../include/clibrary.h \
 ../../include/libinn.h ../../include/ov.h ../../include/storage.h \
 ../../include/inn/history.h tdx-private.h

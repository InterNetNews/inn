##  $Id$

include ../../Makefile.global

top	= ../..
CFLAGS	= $(GCFLAGS) -I..

SOURCES = hisv6.c
OBJECTS = hisv6.o

all: objects

objects: $(OBJECTS)

warnings:
	$(MAKE) COPT='$(WARNINGS)' all

.c.o .c.lo:
	$(LIBCC) $(CFLAGS) -c $*.c -o $@

clobber clean distclean:
	rm -f *.o *.lo
	rm -rf .libs

tags ctags: $(SOURCES)
	$(CTAGS) $(SOURCES) ../../include/*.h ../*.h


##  Dependencies.  Default list, below, is probably good enough.

depend:	Makefile $(SOURCES)
	$(MAKEDEPEND) '$(CFLAGS)' $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
hisv6.o: hisv6.c ../../include/config.h ../../include/inn/defines.h \
 ../../include/clibrary.h ../hisinterface.h hisv6.h hisv6-private.h \
 ../../include/inn/history.h ../../include/storage.h \
 ../../include/libinn.h ../../include/macros.h ../../include/dbz.h \
 ../../include/inn/timer.h ../../include/inn/qio.h \
 ../../include/inn/sequence.h ../../include/inndcomm.h

##  $Id$

include ../../Makefile.global

CFLAGS	= $(GCFLAGS) -I../../include -I.. $(BERKELEY_DB_CFLAGS)

SOURCES = ovdb.c
OBJECTS = ovdb.o
LOBJECTS = ovdb.lo
ROBJECTS = ovdb.$(EXTOBJ)

.SUFFIXES: .lo

all:			$(OBJECTS)

warnings:
	$(MAKE) COPT='$(WARNINGS)' all

.c.o .c.lo:
	$(UPLIBCC) $(CFLAGS) -c $*.c -o $@

clobber clean:
	rm -f *.o *.lo obj/*.o *~
	rm -f profiled
	rm -f all install lint lint.all
	rm -rf .libs

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES) ../../include/*.h ../*.h

##  Dependencies.  Default list, below, is probably good enough.

depend:	Makefile $(SOURCES)
	$(UPMAKEDEPEND) $(CFLAGS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
ovdb.o: ovdb.c ../../include/config.h ../../include/clibrary.h \
 ../../include/macros.h ../../include/conffile.h \
 ../../include/libinn.h ../../include/paths.h ../../include/storage.h \
 ../../include/ov.h ../ovinterface.h ovdb.h ovdb-private.h

##  $Id$

include ../../Makefile.global

CP	= $(UPLIBTOOL) cp
CFLAGS  = $(GCFLAGS) -I../../include -I..

SOURCES = caf.c timecaf.c
OBJECTS = caf.o timecaf.o
LOBJECTS = caf.lo timecaf.lo
ROBJECTS = caf.$(EXTOBJ) timecaf.$(EXTOBJ)

all:			$(OBJECTS) moveobjs

.c.o:
	$(UPLIBCC) $(CFLAGS) -c $*.c -o $@

moveobjs: $(OBJECTS)
	$(CP) -p caf.$(EXTOBJ) ../objs
	$(CP) -p timecaf.$(EXTOBJ) ../objs

clobber clean:
	rm -f *.o *.lo obj/*.o *~ profiled
	rm -rf .libs

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES) ../../include/*.h ../*.h

##  Dependencies.  Default list, below, is probably good enough.

depend:	Makefile $(SOURCES)
	$(UPMAKEDEPEND) $(CFLAGS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
caf.o: caf.c ../../include/configdata.h ../../include/config.h \
 ../../include/clibrary.h caf.h ../../include/macros.h \
 ../../include/libinn.h
timecaf.o: timecaf.c ../../include/config.h ../../include/clibrary.h \
 caf.h ../../include/libinn.h ../../include/macros.h ../methods.h \
 timecaf.h ../../include/paths.h

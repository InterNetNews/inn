##  $Revision$
include ../../Makefile.global

CFLAGS +=	-I../../include -I..

IFTRUE	= @$(SHELL) ../iftrue.sh

SOURCES = cnfs.c
OBJECTS = cnfs.o

all:			$(OBJECTS) moveobjs

moveobjs: $(OBJECTS)
	cp -p $(OBJECTS) ../objs

clobber clean:
	rm -f *.o obj/*.o *~
	rm -f profiled
	rm -f all install 

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES) ../../include/*.h ../*.h

##  Dependencies.  Default list, below, is probably good enough.
depend:		Makefile $(SOURCES)
	makedepend $(DEFS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.

interface.o: methods.h interface.h
methods.o: interface.h

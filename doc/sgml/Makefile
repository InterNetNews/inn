##  $Revision$
include ../../Makefile.global

CFLAGS	= $(GCFLAGS)
SHELL	= /bin/sh
MAKE	= make
DESTDIR	=
D	= $(DESTDIR)

ALL	= 

all:		$(P) $(ALL)

install:

clobber clean:
	rm -f all install profiled

tags ctags:
	cp /dev/null tags

profiled:
	cp /dev/null profiled

.PRECIOUS:	$(ALL)

##  Dependencies.  Default list, below, is probably good enough.
depend:		Makefile $(SOURCES)
	makedepend $(DEFS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.

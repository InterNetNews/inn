##  $Revision$
include ../Makefile.global

CFLAGS	= $(GCFLAGS)
SHELL	= /bin/sh
MAKE	= make
DESTDIR	=
D	= $(DESTDIR)

DIRS    = man sgml

ALL	= 

all:		
	$(MAKE) $(FLAGS) WHAT_TO_MAKE=all DESTDIR=$(DESTDIR) common

##  Install everything.
install:	
	$(MAKE) $(FLAGS) WHAT_TO_MAKE=install DESTDIR=$(DESTDIR) common
##  Other generic targets.
depend tags ctags profiled:
	@$(MAKE) $(FLAGS) WHAT_TO_MAKE=$@ common

etags:
	etags */*.c */*.h

clean:
	@$(MAKE) $(FLAGS) WHAT_TO_MAKE=$@ common

clobber: clean

##  Common target.
common:
	@for D in $(DIRS) ; do \
	    echo "" ; \
	    echo "cd $$D ; $(MAKE) $(FLAGS) DESTDIR=$(DESTDIR) $(WHAT_TO_MAKE) ; cd .." ; \
	    cd $$D; $(MAKE) $(FLAGS) DESTDIR=$(DESTDIR) $(WHAT_TO_MAKE) || exit 1 ; cd .. ; \
	done

# DO NOT DELETE THIS LINE -- make depend depends on it.

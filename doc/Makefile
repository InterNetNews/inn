##  $Id$
##
##  The only target that this Makefile need support is install.  Everything
##  else is a null target (and the top level Makefile shouldn't even attempt
##  them in this directory).

include ../Makefile.global

DIRS = man

all depend tags ctags etags clean clobber profiled:

install:
	@for D in $(DIRS) ; do \
	    cd $$D && $(MAKE) install || exit 1 ; cd .. ; \
	done

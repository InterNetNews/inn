##  $Id$
##
##  The only target that this Makefile need support is install.  Everything
##  else is a null target (and the top level Makefile shouldn't even attempt
##  them in this directory).

include ../Makefile.global

top	= ..

TOPDOCS	= CONTRIBUTORS HACKING INSTALL LICENSE NEWS README TODO

DOCS	= GPL compliance-nntp config-design config-semantics config-syntax \
	history hook-perl hook-python hook-tcl sample-control

DIRS = man

all:
clobber clean distclean:
tags ctags:
profiled:
depend:

install: install-doc
	@for D in $(DIRS) ; do \
	    cd $$D && $(MAKE) install || exit 1 ; cd .. ; \
	done

install-doc:
	for F in $(TOPDOCS) ; do \
	    $(CP_RPUB) ../$$F $D$(PATHDOC)/$$F ; \
	done
	for F in $(DOCS) ; do \
	    $(CP_RPUB) $$F $D$(PATHDOC)/$$F ; \
	done

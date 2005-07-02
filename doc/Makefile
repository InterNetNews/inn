##  $Id$
##
##  The only target that this Makefile need support is install.  Everything
##  else is a null target (and the top level Makefile shouldn't even attempt
##  them in this directory).

include ../Makefile.global

top	= ..

TOPDOCS	= CONTRIBUTORS HACKING INSTALL LICENSE NEWS README TODO

DOCS	= GPL compliance-nntp config-design config-semantics config-syntax \
	external-auth history history-innfeed hook-perl hook-python \
	sample-control

DIRS = pod man

all:
	cd pod && $(MAKE)

tags ctags:
profiled:
depend:

clean clobber distclean maintclean:
	cd pod && $(MAKE) $@

install:
	for F in $(TOPDOCS) ; do \
	    $(CP_RPUB) $(top)/$$F $D$(PATHDOC)/$$F ; \
	done
	for F in $(DOCS) ; do \
	    $(CP_RPUB) $$F $D$(PATHDOC)/$$F ; \
	done
	if [ -r $(top)/README.snapshot ] ; then \
	    $(CP_RPUB) $(top)/README.snapshot $D$(PATHDOC)/README.snapshot ; \
	fi
	cd man && $(MAKE) install

bootstrap:
	cd pod && $(MAKE) bootstrap

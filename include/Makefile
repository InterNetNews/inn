##  $Id$
##
##  Currently just handles creation of the automatically generated header
##  files.  Eventually, rules for installing INN's header files will go
##  here.

include ../Makefile.global

top	      = ..

ALL	      = inn/system.h inn/version.h

EXTRA	      = config.h paths.h

all: $(ALL)

clean:
	rm -f $(ALL)

clobber distclean: clean
	rm -f $(EXTRA)

depend tags ctags profiled:


##  Build rules.

inn/system.h: config.h $(top)/support/mksystem
	$(top)/support/mksystem $(AWK) config.h > $@

inn/version.h: $(top)/support/mkversion $(top)/Makefile.global
	$(top)/support/mkversion '$(VERSION)' '$(VERSION_EXTRA)' > $@


##  Installation rules.  Don't install anything yet.

install:

##  $Id$
##
##  Currently just handles creation of the automatically generated header
##  files.  Eventually, rules for installing INN's header files will go
##  here.

include ../Makefile.global

ALL	      = inn/system.h inn/version.h

EXTRA	      = config.h paths.h

all: $(ALL)

clean:
	rm -f $(ALL)

clobber distclean: clean
	rm -f $(EXTRA)

depend tags ctags profiled:


##  Build rules.

inn/system.h: config.h ../support/mksystem
	../support/mksystem config.h > $@

inn/version.h: ../support/mkversion ../Makefile.global
	../support/mkversion '$(VERSION)' '$(VERSION_EXTRA)' > $@


##  Installation rules.  Don't install anything yet.

install:

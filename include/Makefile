##  $Id$
##
##  Currently just handles creation of the automatically generated header
##  files.  Eventually, rules for installing INN's header files will go
##  here.

include ../Makefile.global

top	      = ..

ALL	      = inn/system.h inn/version.h

EXTRA	      = config.h inn/paths.h

all: $(ALL) $(EXTRA)

warnings: all

bootstrap:

clean:
	rm -f $(ALL)

clobber distclean maintclean: clean
	rm -f $(EXTRA)

depend:

profiled: all

$(EXTRA) $(FIXSCRIPT):
	@echo Run configure before running make.  See INSTALL for details.
	@exit 1


##  Build rules.

inn/system.h: config.h $(top)/support/mksystem
	$(top)/support/mksystem $(AWK) config.h > $@

inn/version.h: $(top)/support/mkversion $(top)/Makefile.global
	$(top)/support/mkversion '$(VERSION)' '$(VERSION_EXTRA)' > $@


##  Installation rules.

install:
	$(top)/support/install-sh $(OWNER) -m 0755 -d $D$(PATHINCLUDE)/inn
	for F in inn/*.h ; do \
	    $(CP_RPUB) $$F $D$(PATHINCLUDE)/$$F ; \
	done

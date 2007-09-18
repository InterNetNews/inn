##  $Id$

include ../Makefile.global

top	      = ..

ALL           =
EXTRA         = INN/Config.pm

all: $(ALL) $(EXTRA)

install: all
	for F in $(EXTRA) ; do \
	    $(CP_RPUB) $$F $D$(PATHLIBPERL)/$$F ; \
	done

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

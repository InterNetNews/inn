##  $Id$

include ../Makefile.global

top	      = ..

ALL           =
EXTRA         = INN/Config.pm

##  The double underscore '__' will be converted into two colons '::'
##  during the install process (otherwise, make would complain in this
##  file, and we cannot generate the right file with '::' in the
##  Subversion man directory because it would always be regenerated:
##  the file containing '__' would indeed not be found).
MAN           = ../doc/man/INN__Config.3pm

all: $(ALL) $(EXTRA) $(MAN)

install: all
	for F in $(EXTRA) ; do \
	    $(CP_RPUB) $$F $D$(PATHLIBPERL)/$$F ; \
	done

bootstrap: $(MAN)

clean:
	rm -f $(ALL)

clobber distclean: clean
	rm -f $(EXTRA)

maintclean: distclean
	rm -f $(MAN)

depend:

profiled: all

$(EXTRA) $(FIXSCRIPT):
	@echo Run configure before running make.  See INSTALL for details.
	@exit 1

../doc/man/INN__Config.3pm: INN/Config.pm
	$(POD2MAN) -s '$(MAN3PM_EXT)' -n "INN::Config" $? > $@


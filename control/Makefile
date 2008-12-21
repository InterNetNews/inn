##  $Id$

include ../Makefile.global

top	      = ..

ALL	      = controlbatch controlchan docheckgroups perl-nocem \
		pgpverify signcontrol

MAN	      = ../doc/man/perl-nocem.8 ../doc/man/pgpverify.1

all: $(ALL) $(MAN)

install: all
	for F in $(ALL) ; do \
	    $(CP_XPUB) $$F $D$(PATHBIN)/$$F ; \
	done
	for M in modules/*.pl ; do \
	    $(CP_RPUB) $$M $D$(PATHCONTROL)/`basename $$M` ; \
	done

bootstrap: $(MAN)

clean clobber distclean:
	rm -f $(ALL)

maintclean: distclean
	rm -f $(MAN)

profiled: all

depend:

$(FIXSCRIPT):
	@echo Run configure before running make.  See INSTALL for details.
	@exit 1


##  Build rules.

FIX = $(FIXSCRIPT)

controlbatch:	controlbatch.in  $(FIX) ; $(FIX) controlbatch.in
controlchan:	controlchan.in   $(FIX) ; $(FIX) controlchan.in
docheckgroups:	docheckgroups.in $(FIX) ; $(FIX) docheckgroups.in
perl-nocem:	perl-nocem.in    $(FIX) ; $(FIX) perl-nocem.in
pgpverify:	pgpverify.in     $(FIX) ; $(FIX) pgpverify.in
signcontrol:	signcontrol.in   $(FIX) ; $(FIX) -i signcontrol.in

../doc/man/perl-nocem.8: perl-nocem.in
	$(POD2MAN) -s 8 -n "PERL-NOCEM" $? > $@

../doc/man/pgpverify.1: pgpverify.in
	$(POD2MAN) -s 1 -n "PGPVERIFY" $? > $@

##  $Id$

include ../Makefile.global

top	      = ..

ALL	      = controlbatch controlchan docheckgroups gpgverify perl-nocem \
		pgpverify signcontrol

MAN	      = ../doc/man/perl-nocem.8

INSTALLED     = $(D)$(PATHBIN)/controlbatch	\
		$(D)$(PATHBIN)/controlchan	\
		$(D)$(PATHBIN)/docheckgroups	\
		$(D)$(PATHBIN)/gpgverify	\
		$(D)$(PATHBIN)/perl-nocem	\
		$(D)$(PATHBIN)/pgpverify	\
		$(D)$(PATHBIN)/signcontrol

all: $(ALL)

man: $(MAN)

clean clobber distclean:
	rm -f $(ALL)


##  Build rules.

FIX = $(FIXSCRIPT)

controlbatch:	controlbatch.in  $(FIX) ; $(FIX) controlbatch.in
controlchan:	controlchan.in   $(FIX) ; $(FIX) controlchan.in
docheckgroups:	docheckgroups.in $(FIX) ; $(FIX) docheckgroups.in
gpgverify:	gpgverify.in     $(FIX) ; $(FIX) gpgverify.in
perl-nocem:	perl-nocem.in    $(FIX) ; $(FIX) perl-nocem.in
pgpverify:	pgpverify.in     $(FIX) ; $(FIX) pgpverify.in
signcontrol:	signcontrol.in   $(FIX) ; $(FIX) -i signcontrol.in

../doc/man/perl-nocem.8: perl-nocem
	$(POD2MAN) -s 8 $? > $@


##  Installation rules.  Installation commands set in Makefile.global.

COPY = ../$(INSTALL) $(OWNER) -m 644 -B .OLD

install: $(INSTALLED)
	cd modules && for M in *.pl ; do \
	    $(COPY) $$M $D$(PATHCONTROL)/$$M ; \
	done

$(D)$(PATHBIN)/controlbatch:	controlbatch	; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/controlchan:	controlchan	; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/docheckgroups:	docheckgroups	; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/gpgverify:	gpgverify	; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/perl-nocem:	perl-nocem	; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/pgpverify:	pgpverify	; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/signcontrol:	signcontrol	; $(CP_XPUB) $? $@

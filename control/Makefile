##  $Id$

include ../Makefile.global

ALL	      = controlbatch controlchan docheckgroups gpgverify pgpverify

INSTALLED     = $(D)$(PATHBIN)/controlbatch	\
		$(D)$(PATHBIN)/controlchan	\
		$(D)$(PATHBIN)/docheckgroups	\
		$(D)$(PATHBIN)/gpgverify	\
		$(D)$(PATHBIN)/pgpverify

all: $(ALL)

clean clobber distclean:
	rm -f $(ALL)


##  Build rules.

FIX = $(FIXSCRIPT)

controlbatch:	controlbatch.in  $(FIX) ; $(FIX) controlbatch.in
controlchan:	controlchan.in   $(FIX) ; $(FIX) controlchan.in
docheckgroups:	docheckgroups.in $(FIX) ; $(FIX) docheckgroups.in
gpgverify:	gpgverify.in     $(FIX) ; $(FIX) gpgverify.in
pgpverify:	pgpverify.in     $(FIX) ; $(FIX) pgpverify.in


##  Installation rules.  Installation commands set in Makefile.global.

COPY = $(UPINSTALL) $(OWNER) -m 644 -B .OLD

install: $(INSTALLED)
	cd modules && for M in *.pl ; do \
	    $(COPY) $$M $D$(PATHCONTROL)/$$M ; \
	done

$(D)$(PATHBIN)/controlbatch:	controlbatch	; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/controlchan:	controlchan	; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/docheckgroups:	docheckgroups	; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/gpgverify:	gpgverify	; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/pgpverify:	pgpverify	; $(CP_XPUB) $? $@

##  $Id$
##
##  All the actual installation work of any files in the samples directory
##  is done via the site directory, so that one can maintain one's news
##  configuration in the site directory and use make commands to update the
##  server automatically.  All this Makefile does is run fixscript on a few
##  files that don't need the full power of configure (and clean up after
##  them on make clean).

include ../Makefile.global

ALL           = default docheckgroups ihave newgroup parsecontrol rmgroup \
		sendme sendsys senduuname version

EXTRA         = actsync.cfg checkgroups inn.conf innreport.conf \
		innwatch.ctl newsfeeds nnrpd_auth.pl startup.tcl \
		nnrpd_auth.py sasl.conf

all: $(ALL)

clean:
	rm -f $(ALL)

clobber: clean
	rm -f $(EXTRA)

install:


##  Build rules.

FIX             = $(FIXSCRIPT)

default:	default.in       $(FIX)	; $(FIX) default.in
docheckgroups:	docheckgroups.in $(FIX)	; $(FIX) docheckgroups.in
ihave:		ihave.in         $(FIX)	; $(FIX) ihave.in
newgroup:	newgroup.in      $(FIX)	; $(FIX) newgroup.in
parsecontrol:	parsecontrol.in  $(FIX)	; $(FIX) parsecontrol.in
rmgroup:	rmgroup.in       $(FIX)	; $(FIX) rmgroup.in
sendme:		sendme.in        $(FIX)	; $(FIX) sendme.in
sendsys:	sendsys.in       $(FIX)	; $(FIX) sendsys.in
senduuname:	senduuname.in    $(FIX)	; $(FIX) senduuname.in
version:	version.in       $(FIX)	; $(FIX) version.in

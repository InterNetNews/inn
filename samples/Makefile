##  $Id$
##
##  All the actual installation work of any files in the samples directory
##  is done via the site directory, so that one can maintain one's news
##  configuration in the site directory and use make commands to update the
##  server automatically.  All this Makefile does is run fixscript on a few
##  files that don't need the full power of configure (and clean up after
##  them on make clean).

include ../Makefile.global

ALL           = checkgroups default docheckgroups ihave newgroup \
		nnrpd_auth.pl parsecontrol rmgroup sendme sendsys \
		senduuname version

EXTRA         = inn.conf innreport.conf newsfeeds sasl.conf

all: $(ALL)

clean:
	rm -f $(ALL)

clobber: clean
	rm -f $(EXTRA)

install:


##  Build rules.

FIX             = $(FIXSCRIPT)

checkgroups:	checkgroups.in   $(FIX)	; $(FIX) checkgroups.in
default:	default.in       $(FIX)	; $(FIX) default.in
docheckgroups:	docheckgroups.in $(FIX)	; $(FIX) docheckgroups.in
ihave:		ihave.in         $(FIX)	; $(FIX) ihave.in
newgroup:	newgroup.in      $(FIX)	; $(FIX) newgroup.in
nnrpd_auth.pl:	nnrpd_auth.pl.in $(FIX)	; $(FIX) nnrpd_auth.pl.in
parsecontrol:	parsecontrol.in  $(FIX)	; $(FIX) parsecontrol.in
rmgroup:	rmgroup.in       $(FIX)	; $(FIX) rmgroup.in
sendme:		sendme.in        $(FIX)	; $(FIX) sendme.in
sendsys:	sendsys.in       $(FIX)	; $(FIX) sendsys.in
senduuname:	senduuname.in    $(FIX)	; $(FIX) senduuname.in
version:	version.in       $(FIX)	; $(FIX) version.in

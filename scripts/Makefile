##  $Id$
##
##  Files that can be handled by fixscript (and need to be so handled) need
##  a rule to build them from the .in version, and then all files need an
##  installation rule.  Do the installation rules individually so as to
##  avoid needless work if the files haven't changed.  We also need lists
##  of files to build and files to install for the all and install rules.

include ../Makefile.global

top	      = ..

ALL           = innmail innreport innstat innupgrade innwatch rc.news \
		scanlogs simpleftp tally.control writelog

EXTRA         = inncheck innshellvars innshellvars.pl innshellvars.tcl \
		news.daily

all: $(ALL) $(EXTRA)

install: all
	for F in innmail innreport simpleftp ; do \
	    $(CP_XPUB) $$F $D$(PATHBIN)/$$F ; \
	done
	for F in inncheck innstat innupgrade innwatch news.daily rc.news \
	         scanlogs tally.control writelog ; do \
	    $(CP_XPRI) $$F $D$(PATHBIN)/$$F ; \
	done
	for F in innreport_inn.pm innshellvars innshellvars.pl \
	         innshellvars.tcl ; do \
	    $(CP_RPUB) $$F $D$(PATHLIB)/$$F ; \
	done

clean:
	rm -f $(ALL)

clobber distclean: clean
	rm -f $(EXTRA)

depend:

profiled: all

$(EXTRA) $(FIXSCRIPT):
	@echo Run configure before running make.  See INSTALL for details.
	@exit 1


##  Build rules.

FIX             = $(FIXSCRIPT)

innmail:	innmail.in       $(FIX)	; $(FIX) innmail.in
innreport:	innreport.in     $(FIX)	; $(FIX) innreport.in
innstat:	innstat.in       $(FIX)	; $(FIX) innstat.in
innupgrade:	innupgrade.in    $(FIX) ; $(FIX) -i innupgrade.in
innwatch:	innwatch.in      $(FIX)	; $(FIX) innwatch.in
rc.news:	rc.news.in       $(FIX)	; $(FIX) rc.news.in
scanlogs:	scanlogs.in      $(FIX)	; $(FIX) scanlogs.in
simpleftp:	simpleftp.in     $(FIX)	; $(FIX) simpleftp.in
tally.control:	tally.control.in $(FIX)	; $(FIX) tally.control.in
writelog:	writelog.in      $(FIX)	; $(FIX) writelog.in

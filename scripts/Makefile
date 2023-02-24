##  Files that can be handled by fixscript (and need to be so handled) need
##  a rule to build them from the .in version, and then all files need an
##  installation rule.  Do the installation rules individually so as to
##  avoid needless work if the files haven't changed.  We also need lists
##  of files to build and files to install for the all and install rules.

include ../Makefile.global

top	      = ..

ALL           = inncheck innmail innreport innstat innupgrade innwatch \
		news.daily rc.news scanlogs simpleftp tally.control writelog

EXTRA         = innshellvars innshellvars.pl innshellvars.tcl

all: $(ALL) $(EXTRA)

install: all
	for F in innmail innreport simpleftp ; do \
	    $(CP_XPUB) $$F $D$(PATHBIN)/$$F ; \
	done
	for F in inncheck innstat innupgrade innwatch news.daily rc.news \
	         scanlogs tally.control writelog ; do \
	    $(CP_XPRI) $$F $D$(PATHBIN)/$$F ; \
	done
	for F in innreport-display.conf innreport_inn.pm \
	         innshellvars innshellvars.pl innshellvars.tcl ; do \
	    $(CP_RPUB) $$F $D$(PATHLIB)/$$F ; \
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


##  Build rules.

FIX             = $(SHELL) $(FIXSCRIPT)

inncheck:	inncheck.in      $(FIXSCRIPT)	; $(FIX) inncheck.in
innmail:	innmail.in       $(FIXSCRIPT)	; $(FIX) innmail.in
innreport:	innreport.in     $(FIXSCRIPT)	; $(FIX) innreport.in
innstat:	innstat.in       $(FIXSCRIPT)	; $(FIX) innstat.in
innupgrade:	innupgrade.in    $(FIXSCRIPT)	; $(FIX) -i innupgrade.in
innwatch:	innwatch.in      $(FIXSCRIPT)	; $(FIX) innwatch.in
news.daily:	news.daily.in    $(FIXSCRIPT)	; $(FIX) news.daily.in
rc.news:	rc.news.in       $(FIXSCRIPT)	; $(FIX) rc.news.in
scanlogs:	scanlogs.in      $(FIXSCRIPT)	; $(FIX) scanlogs.in
simpleftp:	simpleftp.in     $(FIXSCRIPT)	; $(FIX) -i simpleftp.in
tally.control:	tally.control.in $(FIXSCRIPT)	; $(FIX) tally.control.in
writelog:	writelog.in      $(FIXSCRIPT)	; $(FIX) writelog.in

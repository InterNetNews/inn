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

INSTALLED     = $(D)$(PATHBIN)/inncheck		\
		$(D)$(PATHBIN)/innmail		\
		$(D)$(PATHBIN)/innreport	\
		$(D)$(PATHBIN)/innstat		\
		$(D)$(PATHBIN)/innupgrade	\
		$(D)$(PATHBIN)/innwatch		\
		$(D)$(PATHBIN)/news.daily	\
		$(D)$(PATHBIN)/rc.news		\
		$(D)$(PATHBIN)/scanlogs		\
		$(D)$(PATHBIN)/simpleftp	\
		$(D)$(PATHBIN)/tally.control	\
		$(D)$(PATHBIN)/writelog		\
		$(D)$(PATHLIB)/innreport_inn.pm	\
		$(D)$(PATHLIB)/innshellvars	\
		$(D)$(PATHLIB)/innshellvars.pl	\
		$(D)$(PATHLIB)/innshellvars.tcl

all: $(ALL)

clean:
	rm -f $(ALL)

clobber distclean: clean
	rm -f $(EXTRA)

depend:

profiled: all


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


##  Installation rules.  Installation commands set in Makefile.global.

install: $(INSTALLED)

$(D)$(PATHBIN)/inncheck:            inncheck            ; $(CP_XPRI) $? $@
$(D)$(PATHBIN)/innmail:             innmail             ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/innreport:           innreport           ; $(CP_XPRI) $? $@
$(D)$(PATHBIN)/innstat:             innstat             ; $(CP_XPRI) $? $@
$(D)$(PATHBIN)/innupgrade:          innupgrade		; $(CP_XPRI) $? $@
$(D)$(PATHBIN)/innwatch:            innwatch            ; $(CP_XPRI) $? $@
$(D)$(PATHBIN)/news.daily:          news.daily          ; $(CP_XPRI) $? $@
$(D)$(PATHBIN)/rc.news:             rc.news             ; $(CP_XPRI) $? $@
$(D)$(PATHBIN)/scanlogs:            scanlogs            ; $(CP_XPRI) $? $@
$(D)$(PATHBIN)/simpleftp:           simpleftp           ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/tally.control:       tally.control       ; $(CP_XPRI) $? $@
$(D)$(PATHBIN)/writelog:            writelog            ; $(CP_XPRI) $? $@
$(D)$(PATHLIB)/innreport_inn.pm:    innreport_inn.pm    ; $(CP_RPRI) $? $@
$(D)$(PATHLIB)/innshellvars:        innshellvars        ; $(CP_RPUB) $? $@
$(D)$(PATHLIB)/innshellvars.pl:     innshellvars.pl     ; $(CP_RPUB) $? $@
$(D)$(PATHLIB)/innshellvars.tcl:    innshellvars.tcl    ; $(CP_RPUB) $? $@

##  $Revision$
include ../Makefile.global

##  If you want to do ctlinnd pause/reload/go, uncomment these lines.
#PAUSE		= pause
#RELOAD_AND_GO	= reload go

# Added a default rule for ".csh" because Digital UNIX has a builtin
# rule which would overwite the innshellvars file.
.csh:

CTLINND			= ${PATHBIN}/ctlinnd
CTLBIN			= ${PATHCONTROL}
FILTBIN			= ${PATHFILTER}
PATH_PERL_STARTUP_INND	= ${PATHFILTER}/startup_innd.pl
PATH_PERL_FILTER_INND	= ${PATHFILTER}/filter_innd.pl
PATH_PERL_FILTER_NNRPD	= ${PATHFILTER}/filter_nnrpd.pl
PATH_TCL_STARTUP	= ${PATHFILTER}/startup.tcl
PATH_TCL_FILTER		= ${PATHFILTER}/filter.tcl
PATH_NNRPAUTH           = ${PATHFILTER}/nnrpd_auth.pl
PATH_NEWSBOOT		= ${PATHBIN}/rc.news
PATH_PARSECTL		= ${PATHBIN}/parsecontrol

PATH_CONFIG		= ${PATHETC}/inn.conf
PATH_CONTROLCTL		= ${PATHETC}/control.ctl
PATH_EXPIRECTL		= ${PATHETC}/expire.ctl
PATH_INNDHOSTS		= ${PATHETC}/incoming.conf
PATH_MODERATORS		= ${PATHETC}/moderators
PATH_DISTPATS		= ${PATHETC}/distrib.pats
PATH_NEWSFEEDS		= ${PATHETC}/newsfeeds
PATH_NNRPACCESS		= ${PATHETC}/nnrp.access
PATH_READERSCONF	= ${PATHETC}/readers.conf
PATH_NNRPDTRACK		= ${PATHETC}/nnrpd.track
PATH_SCHEMA		= ${PATHETC}/overview.fmt
PATH_PERL_SHELLVARS	= ${PATHLIB}/innshellvars.pl
PATH_TCL_SHELLVARS	= ${PATHLIB}/innshellvars.tcl
PATH_SHELLVARS		= ${PATHLIB}/innshellvars
PATH_NNTPPASS		= ${PATHETC}/passwd.nntp
PATH_CTLWATCH		= ${PATHETC}/innwatch.ctl
PATH_ACTSYNC_IGN	= ${PATHETC}/actsync.ign
PATH_ACTSYNC_CFG	= ${PATHETC}/actsync.cfg
PATH_MOTD		= ${PATHETC}/motd.news
PATH_STORAGECONF	= ${PATHETC}/storage.conf
PATH_OVERVIEWCTL	= ${PATHETC}/overview.ctl
PATH_CYCBUFFCONFIG	= ${PATHETC}/cycbuff.conf
PATH_INNFEEDCTL		= ${PATHETC}/innfeed.conf

##  Order:  innd, control, expire, inews, sending, misc
MOST	= rc.news checkgroups default ihave newgroup \
	rmgroup sendme sendsys senduuname version parsecontrol writelog \
	docheckgroups news.daily scanlogs controlbatch controlchan expirerm \
	tally.control nntpsend send-ihave send-nntp send-uucp \
	sendbatch inncheck innstat pgpverify innwatch innreport \
	innshellvars innshellvars.pl innshellvars.tcl \
	scanspool innmail checkgroups.pl ihave.pl newgroup.pl rmgroup.pl \
	sendme.pl sendsys.pl senduuname.pl version.pl cnfsstat \
	mod-active simpleftp mailpost news2mail pullnews

##  Scripts from above, plus site-specific config files.
REST		= \
	newsfeeds incoming.conf nnrp.access nnrpd.track passwd.nntp \
	inn.conf moderators innreport.conf innreport_inn.pm \
	control.ctl expire.ctl nntpsend.ctl overview.fmt \
	innwatch.ctl distrib.pats actsync.cfg actsync.ign \
	motd.news storage.conf overview.ctl cycbuff.conf \
	innfeed.conf startup_innd.pl filter_innd.pl filter_nnrpd.pl \
	startup.tcl filter.tcl nnrpd_auth.pl news2mail.cf readers.conf

ALL		= $(MOST) $(REST)

MOST_INSTALLED	= $D$(PATH_NEWSBOOT) $D$(CTLBIN)/checkgroups \
	$D$(CTLBIN)/default $D$(CTLBIN)/ihave $D$(CTLBIN)/newgroup \
	$D$(CTLBIN)/rmgroup $D$(CTLBIN)/sendme $D$(CTLBIN)/sendsys \
	$D$(CTLBIN)/senduuname $D$(CTLBIN)/version $D$(PATH_PARSECTL) \
	$D$(PATHBIN)/writelog $D$(PATHLIB)/docheckgroups \
	$D$(PATHBIN)/news.daily $D$(PATHBIN)/scanlogs $D$(PATHBIN)/controlchan \
	$D$(PATHBIN)/expirerm $D$(PATHBIN)/tally.control $D$(PATHBIN)/innmail \
	$D$(PATHBIN)/nntpsend $D$(PATHBIN)/send-ihave $D$(PATHBIN)/send-nntp \
	$D$(PATHBIN)/send-uucp $D$(PATHBIN)/sendbatch $D$(PATHBIN)/inncheck \
	$D$(PATHBIN)/innstat $D$(PATHBIN)/pgpverify $D$(PATHBIN)/innwatch \
	$D$(PATHBIN)/controlbatch $D$(PATHBIN)/innreport $D$(PATH_SHELLVARS) \
	$D$(PATH_PERL_SHELLVARS) $D$(PATH_TCL_SHELLVARS) \
	$D$(PATH_CSH_SHELLVARS) $D$(PATHBIN)/scanspool $D$(PATHBIN)/cnfsstat \
	$D$(PATHBIN)/mod-active $D$(PATHBIN)/simpleftp \
	$D$(CTLBIN)/checkgroups.pl $D$(CTLBIN)/ihave.pl \
	$D$(PATHBIN)/mailpost $D$(PATHBIN)/news2mail $D$(PATHBIN)/pullnews \
	$D$(CTLBIN)/newgroup.pl $D$(CTLBIN)/rmgroup.pl $D$(CTLBIN)/sendme.pl \
	$D$(CTLBIN)/sendsys.pl $D$(CTLBIN)/senduuname.pl $D$(CTLBIN)/version.pl

REST_INSTALLED	= \
	$D$(PATH_NEWSFEEDS) $D$(PATH_INNDHOSTS) \
	$D$(PATH_NNRPACCESS) $D$(PATH_NNRPDTRACK) $D$(PATH_NNTPPASS) \
	$D$(PATH_CONFIG) $D$(PATH_MODERATORS) \
	$D$(PATH_CONTROLCTL) $D$(PATH_EXPIRECTL) $D$(PATHETC)/nntpsend.ctl \
	$D$(PATHETC)/innreport.conf $D$(PATHLIB)/innreport_inn.pm \
	$D$(PATH_CTLWATCH) $D$(PATH_DISTPATS) $D$(PATH_SCHEMA) \
	$D$(PATH_ACTSYNC_CFG) $D$(PATH_ACTSYNC_IGN) \
	$D$(PATH_MOTD) $D$(PATH_STORAGECONF) \
	$D$(PATH_OVERVIEWCTL) $D$(PATH_CYCBUFFCONFIG) \
	$D$(PATH_INNFEEDCTL) $D$(PATH_PERL_STARTUP_INND) \
	$D$(PATH_PERL_FILTER_INND) $D$(PATH_PERL_FILTER_NNRPD) \
	$D$(PATH_TCL_STARTUP) $D$(PATH_TCL_FILTER) \
	$D$(PATH_NNRPAUTH) $D$(PATHETC)/news2mail.cf $D$(PATH_READERSCONF)

ALL_INSTALLED	= $(MOST_INSTALLED) $(REST_INSTALLED)

##  Get new versions of everything from samples directory.
all:		$(P) $(ALL) config

##  Get only scripts, not per-host config files.
most:		$(MOST)

##  Show changes between files here and ones in samples.
diff:
	@$(MAKE) COPY=-diff all

##  Show changes between files here and installed versions.
diff-installed:
	@$(MAKE) COPY_RPRI=-diff COPY_RPUB=-diff COPY_XPRI=-diff COPY_XPUB=-diff $(ALL_INSTALLED)

##  Show what would be copied from samples directory.
what:
	@$(MAKE) -s 'COPY=@echo' $(ALL) | ${AWK} 'NF==2 { print $$2; }'

config:		$(ALL)
	$(SHELL) ./do-subst.sh $?
	date >config

##  Don't use parallel rules -- we want this to be viewed carefully.
install:	all $(PAUSE) install-config $(RELOAD_AND_GO)
reload-install:	all pause    install-config reload go
install-config:		update $(REST_INSTALLED)

##  Install scripts, not per-host config files.
update:		all $(MOST_INSTALLED)
	@echo "" ; echo inn.conf in site directory may have newly added parameters
	@echo which installed inn.conf does not have.  Check those parameters
	@echo before you run innd. ; echo ""
	date >update

##  Remove files that are unchanged from the release version.
clean:
	rm -f all install 
	@-for I in $(ALL) ; do \
	    cmp -s $$I ../samples/$$I && echo rm -f $$I && rm -f $$I ; \
	done

clobber:	clean
	rm -f $(ALL) tags profiled

tags ctags:
	cp /dev/null tags

profiled:
	cp /dev/null profiled

##  Commands to make private or public, read or executable files.
COPY_RPRI	= $(SHELL) ../installit.sh $(OWNER) -m 0640 -b .OLD
COPY_RPUB	= $(SHELL) ../installit.sh $(OWNER) -m 0644 -b .OLD
COPY_XPRI	= $(SHELL) ../installit.sh $(OWNER) -m 0550 -b .OLD
COPY_XPUB	= $(SHELL) ../installit.sh $(OWNER) -m 0555 -b .OLD

##  Order:  innd, control, expire, inews, sending, misc
$D$(PATH_INNDHOSTS):	incoming.conf	; $(COPY_RPRI) $? $@
$D$(PATH_NEWSFEEDS):	newsfeeds	; $(COPY_RPUB) $? $@
$D$(PATH_NNRPACCESS):	nnrp.access	; $(COPY_RPRI) $? $@
$D$(PATH_READERSCONF):	readers.conf	; $(COPY_RPRI) $? $@
$D$(PATH_NNRPDTRACK):	nnrpd.track	; $(COPY_RPRI) $? $@
$D$(PATH_NEWSBOOT):	rc.news		; $(COPY_XPRI) $? $@
$D$(PATH_SCHEMA):	overview.fmt	; $(COPY_RPUB) $? $@
$D$(CTLBIN)/checkgroups: checkgroups	; $(COPY_XPRI) $? $@
$D$(CTLBIN)/checkgroups.pl: checkgroups.pl	; $(COPY_XPRI) $? $@
$D$(CTLBIN)/default:	default		; $(COPY_XPRI) $? $@
$D$(CTLBIN)/ihave:	ihave		; $(COPY_XPRI) $? $@
$D$(CTLBIN)/ihave.pl:	ihave.pl		; $(COPY_XPRI) $? $@
$D$(CTLBIN)/newgroup:	newgroup	; $(COPY_XPRI) $? $@
$D$(CTLBIN)/newgroup.pl:	newgroup.pl	; $(COPY_XPRI) $? $@
$D$(CTLBIN)/rmgroup:	rmgroup		; $(COPY_XPRI) $? $@
$D$(CTLBIN)/rmgroup.pl:	rmgroup.pl		; $(COPY_XPRI) $? $@
$D$(CTLBIN)/sendme:	sendme		; $(COPY_XPRI) $? $@
$D$(CTLBIN)/sendme.pl:	sendme.pl		; $(COPY_XPRI) $? $@
$D$(CTLBIN)/sendsys:	sendsys		; $(COPY_XPRI) $? $@
$D$(CTLBIN)/sendsys.pl:	sendsys.pl		; $(COPY_XPRI) $? $@
$D$(CTLBIN)/senduuname:	senduuname	; $(COPY_XPRI) $? $@
$D$(CTLBIN)/senduuname.pl:	senduuname.pl	; $(COPY_XPRI) $? $@
$D$(CTLBIN)/version:	version		; $(COPY_XPRI) $? $@
$D$(CTLBIN)/version.pl:	version.pl		; $(COPY_XPRI) $? $@
$D$(PATH_CONTROLCTL):	control.ctl	; $(COPY_RPRI) $? $@
$D$(PATH_CTLWATCH):	innwatch.ctl	; $(COPY_RPRI) $? $@
$D$(PATH_PARSECTL):	parsecontrol	; $(COPY_XPRI) $? $@
$D$(PATHBIN)/writelog:	writelog	; $(COPY_XPRI) $? $@
$D$(PATHLIB)/docheckgroups: docheckgroups ; $(COPY_XPRI) $? $@
$D$(PATHBIN)/news.daily: news.daily	; $(COPY_XPRI) $? $@
$D$(PATHBIN)/scanlogs:	scanlogs	; $(COPY_XPRI) $? $@
$D$(PATHBIN)/expirerm:	expirerm	; $(COPY_XPRI) $? $@
$D$(PATHBIN)/controlbatch:	controlbatch	; $(COPY_XPRI) $? $@
$D$(PATHBIN)/controlchan:	controlchan	; $(COPY_XPRI) $? $@
$D$(PATHBIN)/innmail:	innmail		; $(COPY_XPRI) $? $@
$D$(PATHBIN)/tally.control: tally.control ; $(COPY_XPRI) $? $@
$D$(PATH_EXPIRECTL):	expire.ctl	; $(COPY_RPRI) $? $@
$D$(PATH_CONFIG):	inn.conf	; $(COPY_RPUB) $? $@
$D$(PATH_MODERATORS):	moderators	; $(COPY_RPUB) $? $@
$D$(PATH_DISTPATS):	distrib.pats	; $(COPY_RPUB) $? $@
$D$(PATH_NNTPPASS):	passwd.nntp	; $(COPY_RPRI) $? $@
$D$(PATHBIN)/nntpsend:	nntpsend	; $(COPY_XPRI) $? $@
$D$(PATHBIN)/sendbatch:	sendbatch	; $(COPY_XPRI) $? $@
$D$(PATHETC)/nntpsend.ctl: nntpsend.ctl	; $(COPY_RPRI) $? $@
$D$(PATHBIN)/send-ihave: send-ihave	; $(COPY_XPRI) $? $@
$D$(PATHBIN)/send-nntp:	send-nntp	; $(COPY_XPRI) $? $@
$D$(PATHBIN)/send-uucp:	send-uucp	; $(COPY_XPRI) $? $@
$D$(PATHBIN)/inncheck:	inncheck	; $(COPY_XPRI) $? $@
$D$(PATHBIN)/innstat:	innstat		; $(COPY_XPRI) $? $@
$D$(PATHBIN)/pgpverify:	pgpverify	; $(COPY_XPRI) $? $@
$D$(PATHBIN)/cnfsstat:	cnfsstat	; $(COPY_XPRI) $? $@
$D$(PATHBIN)/innwatch:	innwatch	; $(COPY_XPRI) $? $@
$D$(PATHBIN)/mod-active: mod-active	; $(COPY_XPUB) $? $@
$D$(PATHBIN)/simpleftp:	simpleftp	; $(COPY_XPUB) $? $@
$D$(PATHBIN)/mailpost:	mailpost	; $(COPY_XPUB) $? $@
$D$(PATHBIN)/pullnews: pullnews		; $(COPY_XPUB) $? $@
$D$(PATHBIN)/news2mail: news2mail	; $(COPY_XPUB) $? $@
$D$(PATHETC)/news2mail.cf: news2mail.cf	; $(COPY_RPRI) $? $@
$D$(PATH_SHELLVARS):	innshellvars	; $(COPY_RPUB) $? $@
$D$(PATH_PERL_SHELLVARS): innshellvars.pl ; $(COPY_RPUB) $? $@
$D$(PATH_TCL_SHELLVARS): innshellvars.tcl ; $(COPY_RPUB) $? $@
$D$(PATHBIN)/innreport: innreport	; $(COPY_XPRI) $? $@
$D$(PATHLIB)/innreport_inn.pm: innreport_inn.pm	; $(COPY_RPRI) $? $@
$D$(PATHETC)/innreport.conf: innreport.conf	; $(COPY_RPRI) $? $@
$D$(PATH_STORAGECONF): storage.conf	; $(COPY_RPRI) $? $@
$D$(PATH_OVERVIEWCTL): overview.ctl	; $(COPY_RPRI) $? $@
$D$(PATH_CYCBUFFCONFIG): cycbuff.conf	; $(COPY_RPRI) $? $@
$D$(PATH_PERL_STARTUP_INND): startup_innd.pl ; $(COPY_RPRI) $? $@
$D$(PATH_PERL_FILTER_INND): filter_innd.pl ; $(COPY_RPRI) $? $@
$D$(PATH_PERL_FILTER_NNRPD): filter_nnrpd.pl ; $(COPY_RPRI) $? $@
$D$(PATH_TCL_STARTUP): startup.tcl	; $(COPY_RPRI) $? $@
$D$(PATH_TCL_FILTER): filter.tcl	; $(COPY_RPRI) $? $@
$D$(PATH_NNRPAUTH): nnrpd_auth.pl       ; $(COPY_RPRI) $? $@
$D$(PATH_ACTSYNC_CFG): actsync.cfg	; $(COPY_RPRI) $? $@
$D$(PATH_ACTSYNC_IGN): actsync.ign	; $(COPY_RPRI) $? $@
$D$(PATHBIN)/scanspool: scanspool	; $(COPY_XPRI) $? $@
$D$(PATH_MOTD): motd.news		; $(COPY_RPRI) $? $@
$D$(PATH_INNFEEDCTL): innfeed.conf	; $(COPY_RPRI) $? $@

REASON	= 'Installing site config files from site/Makefile'
go pause:
	-${CTLINND} $@ $(REASON)
reload:
	-${CTLINND} reload all $(REASON)

##  Use this to just replace any changed files you might have made.  Only
##  do this after you've examined the output of "make -n"!
replace:
	$(MAKE) COPY=cp all

##  Get files from the samples directory.
COPY	= $(SHELL) ./getsafe.sh
actsync.cfg:	../samples/actsync.cfg		; $(COPY) $? $@
actsync.ign:	../samples/actsync.ign		; $(COPY) $? $@
checkgroups:	../samples/checkgroups		; $(COPY) $? $@
checkgroups.pl:	../samples/checkgroups.pl	; $(COPY) $? $@
control.ctl:	../samples/control.ctl		; $(COPY) $? $@
controlbatch:	../samples/controlbatch		; $(COPY) $? $@
controlchan:	../samples/controlchan		; $(COPY) $? $@
default:	../samples/default		; $(COPY) $? $@
docheckgroups:	../samples/docheckgroups	; $(COPY) $? $@
expire.ctl:	../samples/expire.ctl		; $(COPY) $? $@
filter.tcl:	../samples/filter.tcl		; $(COPY) $? $@
nnrpd_auth.pl:  ../samples/nnrpd_auth.pl	; $(COPY) $? $@
filter_innd.pl:	../samples/filter_innd.pl	; $(COPY) $? $@
filter_nnrpd.pl: ../samples/filter_nnrpd.pl	; $(COPY) $? $@
expirerm:	../samples/expirerm		; $(COPY) $? $@
innmail:	../samples/innmail		; $(COPY) $? $@
incoming.conf:	../samples/incoming.conf	; $(COPY) $? $@
ihave:		../samples/ihave		; $(COPY) $? $@
ihave.pl:	../samples/ihave.pl		; $(COPY) $? $@
inn.conf:	../samples/inn.conf		; $(COPY) $? $@
inncheck:	../samples/inncheck		; $(COPY) $? $@
innreport:	../samples/innreport		; $(COPY) $? $@
innreport_inn.pm: ../samples/innreport_inn.pm	; $(COPY) $? $@
innreport.conf:	../samples/innreport.conf	; $(COPY) $? $@
storage.conf:	../samples/storage.conf		; $(COPY) $? $@
overview.ctl:	../samples/overview.ctl		; $(COPY) $? $@
cycbuff.conf:	../samples/cycbuff.conf		; $(COPY) $? $@
innstat:	../samples/innstat		; $(COPY) $? $@
innwatch:	../samples/innwatch		; $(COPY) $? $@
innwatch.ctl:	../samples/innwatch.ctl		; $(COPY) $? $@
innfeed.conf:	../samples/innfeed.conf		; $(COPY) $? $@
innshellvars:	../samples/innshellvars		; $(COPY) $? $@
innshellvars.pl: ../samples/innshellvars.pl	; $(COPY) $? $@
innshellvars.tcl: ../samples/innshellvars.tcl	; $(COPY) $? $@
mailpost:	../samples/mailpost		; $(COPY) $? $@
mod-active:	../samples/mod-active		; $(COPY) $? $@
moderators:	../samples/moderators		; $(COPY) $? $@
distrib.pats:	../samples/distrib.pats		; $(COPY) $? $@
motd.news:	../samples/motd.news		; $(COPY) $? $@
newgroup:	../samples/newgroup		; $(COPY) $? $@
newgroup.pl:	../samples/newgroup.pl		; $(COPY) $? $@
news.daily:	../samples/news.daily		; $(COPY) $? $@
news2mail:	../samples/news2mail		; $(COPY) $? $@
news2mail.cf:	../samples/news2mail.cf		; $(COPY) $? $@
newsfeeds:	../samples/newsfeeds		; $(COPY) $? $@
nnrp.access:	../samples/nnrp.access		; $(COPY) $? $@
nnrpd.track:	../samples/nnrpd.track		; $(COPY) $? $@
nntpsend.ctl:	../samples/nntpsend.ctl		; $(COPY) $? $@
nntpsend:	../samples/nntpsend		; $(COPY) $? $@
overview.fmt:	../samples/overview.fmt		; $(COPY) $? $@
parsecontrol:	../samples/parsecontrol		; $(COPY) $? $@
pullnews:	../samples/pullnews		; $(COPY) $? $@
writelog:	../samples/writelog		; $(COPY) $? $@
passwd.nntp:	../samples/passwd.nntp		; $(COPY) $? $@
pgpverify:	../samples/pgpverify		; $(COPY) $? $@
rc.news:	../samples/rc.news		; $(COPY) $? $@
readers.conf:	../samples/readers.conf		; $(COPY) $? $@
rmgroup:	../samples/rmgroup		; $(COPY) $? $@
rmgroup.pl:	../samples/rmgroup.pl		; $(COPY) $? $@
scanlogs:	../samples/scanlogs		; $(COPY) $? $@
scanspool:	../samples/scanspool		; $(COPY) $? $@
send-ihave:	../samples/send-ihave		; $(COPY) $? $@
send-nntp:	../samples/send-nntp		; $(COPY) $? $@
send-uucp:	../samples/send-uucp		; $(COPY) $? $@
sendbatch:	../samples/sendbatch		; $(COPY) $? $@
sendme:		../samples/sendme		; $(COPY) $? $@
sendme.pl:	../samples/sendme.pl		; $(COPY) $? $@
sendsys:	../samples/sendsys		; $(COPY) $? $@
sendsys.pl:	../samples/sendsys.pl		; $(COPY) $? $@
senduuname:	../samples/senduuname		; $(COPY) $? $@
senduuname.pl:	../samples/senduuname.pl	; $(COPY) $? $@
simpleftp:	../samples/simpleftp		; $(COPY) $? $@
startup.tcl:	../samples/startup.tcl		; $(COPY) $? $@
startup_innd.pl: ../samples/startup_innd.pl	; $(COPY) $? $@
tally.control:	../samples/tally.control	; $(COPY) $? $@
version:	../samples/version		; $(COPY) $? $@
version.pl:	../samples/version.pl		; $(COPY) $? $@
cnfsstat:	../samples/cnfsstat		; $(COPY) $? $@

##  Dependencies.  Default list, below, is probably good enough.
depend:		Makefile $(SOURCES)
	makedepend $(DEFS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.

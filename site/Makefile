##  $Revision$
include ../Makefile.global

##  =()<P	= @<P>@>()=
P	= 

##  If you want to do ctlinnd pause/reload/go, uncomment these lines.
#PAUSE		= pause
#RELOAD_AND_GO	= reload go

# Added a default rule for ".csh" because Digital UNIX has a builtin
# rule which would overwite the innshellvars file.
.csh:

CTLINND			= ${NEWSBIN}/ctlinnd
CTLBIN			= ${NEWSBIN}/control
PATH_PERL_STARTUP_INND	= ${NEWSBIN}/control/startup_innd.pl
PATH_PERL_FILTER_INND	= ${NEWSBIN}/control/filter_innd.pl
PATH_PERL_FILTER_NNRPD	= ${NEWSBIN}/control/filter_nnrpd.pl
PATH_TCL_STARTUP	= ${NEWSBIN}/control/startup.tcl
PATH_TCL_FILTER		= ${NEWSBIN}/control/filter.tcl
PATH_NEWSBOOT		= ${NEWSBIN}/rc.news
PATH_PARSECTL		= ${NEWSBIN}/parsecontrol

PATH_CONFIG		= ${NEWSLIB}/inn.conf
PATH_CONTROLCTL		= ${NEWSLIB}/control.ctl
PATH_EXPIRECTL		= ${NEWSLIB}/expire.ctl
PATH_INNDHOSTS		= ${NEWSLIB}/incoming.conf
PATH_MODERATORS		= ${NEWSLIB}/moderators
PATH_DISTPATS		= ${NEWSLIB}/distrib.pats
PATH_NEWSFEEDS		= ${NEWSLIB}/newsfeeds
PATH_NNRPACCESS		= ${NEWSLIB}/nnrp.access
PATH_NNRPDTRACK		= ${NEWSLIB}/nnrpd.track
PATH_SCHEMA		= ${NEWSLIB}/overview.fmt
PATH_PERL_SHELLVARS	= ${NEWSLIB}/innshellvars.pl
PATH_TCL_SHELLVARS	= ${NEWSLIB}/innshellvars.tcl
PATH_CSH_SHELLVARS	= ${NEWSLIB}/innshellvars.csh
PATH_SHELLVARS		= ${NEWSLIB}/innshellvars
PATH_NNTPPASS		= ${NEWSLIB}/passwd.nntp
PATH_CTLWATCH		= ${NEWSLIB}/innwatch.ctl
PATH_ACTSYNC_IGN	= ${NEWSLIB}/actsync.ign
PATH_ACTSYNC_CFG	= ${NEWSLIB}/actsync.cfg
PATH_MOTD		= ${NEWSLIB}/motd.news
PATH_STORAGECTL		= ${NEWSLIB}/storage.ctl
PATH_OVERVIEWCTL	= ${NEWSLIB}/overview.ctl
PATH_CYCBUFFCONFIG	= ${NEWSLIB}/cycbuff.conf

##  Order:  innd, control, expire, inews, sending, misc
MOST	= rc.news overview.fmt checkgroups default ihave newgroup \
	rmgroup sendme sendsys senduuname version parsecontrol writelog \
	docheckgroups news.daily scanlogs expirerm \
	tally.control nntpsend send-ihave send-nntp send-uucp \
	sendbatch inncheck innstat pgpverify innwatch innreport \
	innshellvars \
	innshellvars.pl innshellvars.csh innshellvars.tcl filter.tcl \
	startup.tcl startup_innd.pl filter_innd.pl filter_nnrpd.pl \
	actsync.cfg actsync.ign scanspool innmail

MOST_INSTALLED	= $D$(PATH_NEWSBOOT) $D$(PATH_SCHEMA) $D$(CTLBIN)/checkgroups \
	$D$(CTLBIN)/default $D$(CTLBIN)/ihave $D$(CTLBIN)/newgroup \
	$D$(CTLBIN)/rmgroup $D$(CTLBIN)/sendme $D$(CTLBIN)/sendsys \
	$D$(CTLBIN)/senduuname $D$(CTLBIN)/version $D$(PATH_PARSECTL) \
	$D$(NEWSBIN)/writelog $D$(CTLBIN)/docheckgroups \
	$D$(NEWSBIN)/news.daily $D$(NEWSBIN)/scanlogs \
	$D$(NEWSBIN)/expirerm $D$(NEWSBIN)/tally.control $D$(NEWSBIN)/innmail \
	$D$(NEWSBIN)/nntpsend $D$(NEWSLIB)/send-ihave $D$(NEWSLIB)/send-nntp \
	$D$(NEWSLIB)/send-uucp $D$(NEWSBIN)/sendbatch $D$(NEWSBIN)/inncheck \
	$D$(NEWSBIN)/innstat $D$(NEWSBIN)/pgpverify $D$(NEWSBIN)/innwatch \
	$D$(NEWSBIN)/innreport $D$(PATH_SHELLVARS) \
	$D$(PATH_PERL_SHELLVARS) $D$(PATH_PERL_STARTUP_INND) \
	$D$(PATH_PERL_FILTER_INND) $D$(PATH_PERL_FILTER_NNRPD) \
	$D$(PATH_TCL_SHELLVARS) $D$(PATH_TCL_STARTUP) $D$(PATH_CSH_SHELLVARS) \
	$D$(PATH_TCL_FILTER) $D$(NEWSBIN)/scanspool $D$(PATH_MOTD)

##  Scripts from above, plus site-specific config files.
REST		= \
	newsfeeds nnrp.access nnrpd.track passwd.nntp \
	inn.conf moderators innreport.conf innreport_inn.pm \
	control.ctl expire.ctl nntpsend.ctl \
	innwatch.ctl distrib.pats actsync.cfg actsync.ign \
	sample.control motd.news storage.ctl overview.ctl cycbuff.conf

ALL		= $(MOST) $(REST)

REST_INSTALLED	= \
	$D$(PATH_NEWSFEEDS) $D$(PATH_INNDHOSTS) \
	$D$(PATH_NNRPACCESS) $D$(PATH_NNTPPASS) \
	$D$(PATH_CONFIG) $D$(PATH_MODERATORS) \
	$D$(PATH_CONTROLCTL) $D$(PATH_EXPIRECTL) $D$(NEWSLIB)/nntpsend.ctl \
	$D$(NEWSLIB)/innreport.conf $D$(NEWSLIB)/innreport_inn.pm \
	$D$(PATH_CTLWATCH) $D$(PATH_DISTPATS) \
	$D$(PATH_ACTSYNC_CFG) $D$(PATH_ACTSYNC_IGN) \
	$D$(CTLBIN)/sample.control $D$(PATH_MOTD) $D$(PATH_STORAGECTL) \
	$D$(PATH_OVERVIEWCTL) $D$(PATH_CYCBUFFCONFIG)

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
update:		$(MOST_INSTALLED)
	date >update

##  Remove files that are unchanged from the release version.
clean:
	rm -f all install lint
	@-for I in $(ALL) ; do \
	    cmp -s $$I ../samples/$$I && echo rm -f $$I && rm -f $$I ; \
	done

clobber:	clean
	rm -f $(ALL) tags lint profiled

tags ctags:
	cp /dev/null tags

lint:
	cp /dev/null lint

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
$D$(PATH_NNRPDTRACK):	nnrpd.track	; $(COPY_RPRI) $? $@
$D$(PATH_NEWSBOOT):	rc.news		; $(COPY_XPRI) $? $@
$D$(PATH_SCHEMA):	overview.fmt	; $(COPY_RPUB) $? $@
$D$(CTLBIN)/checkgroups: checkgroups	; $(COPY_XPRI) $? $@
$D$(CTLBIN)/default:	default		; $(COPY_XPRI) $? $@
$D$(CTLBIN)/ihave:	ihave		; $(COPY_XPRI) $? $@
$D$(CTLBIN)/newgroup:	newgroup	; $(COPY_XPRI) $? $@
$D$(CTLBIN)/rmgroup:	rmgroup		; $(COPY_XPRI) $? $@
$D$(CTLBIN)/sendme:	sendme		; $(COPY_XPRI) $? $@
$D$(CTLBIN)/sendsys:	sendsys		; $(COPY_XPRI) $? $@
$D$(CTLBIN)/senduuname:	senduuname	; $(COPY_XPRI) $? $@
$D$(CTLBIN)/version:	version		; $(COPY_XPRI) $? $@
$D$(PATH_CONTROLCTL):	control.ctl	; $(COPY_RPRI) $? $@
$D$(PATH_CTLWATCH):	innwatch.ctl	; $(COPY_RPRI) $? $@
$D$(PATH_PARSECTL):	parsecontrol	; $(COPY_XPRI) $? $@
$D$(NEWSBIN)/writelog:	writelog	; $(COPY_XPRI) $? $@
$D$(CTLBIN)/docheckgroups: docheckgroups ; $(COPY_XPRI) $? $@
$D$(NEWSBIN)/news.daily: news.daily	; $(COPY_XPRI) $? $@
$D$(NEWSBIN)/scanlogs:	scanlogs	; $(COPY_XPRI) $? $@
$D$(NEWSBIN)/expirerm:	expirerm	; $(COPY_XPRI) $? $@
$D$(NEWSBIN)/innmail:	innmail		; $(COPY_XPRI) $? $@
$D$(NEWSBIN)/tally.control: tally.control ; $(COPY_XPRI) $? $@
$D$(PATH_EXPIRECTL):	expire.ctl	; $(COPY_RPRI) $? $@
$D$(PATH_CONFIG):	inn.conf	; $(COPY_RPUB) $? $@
$D$(PATH_MODERATORS):	moderators	; $(COPY_RPUB) $? $@
$D$(PATH_DISTPATS):	distrib.pats	; $(COPY_RPUB) $? $@
$D$(PATH_NNTPPASS):	passwd.nntp	; $(COPY_RPRI) $? $@
$D$(NEWSBIN)/nntpsend:	nntpsend	; $(COPY_XPRI) $? $@
$D$(NEWSBIN)/sendbatch:	sendbatch	; $(COPY_XPRI) $? $@
$D$(NEWSLIB)/nntpsend.ctl: nntpsend.ctl	; $(COPY_RPRI) $? $@
$D$(NEWSLIB)/send-ihave: send-ihave	; $(COPY_XPRI) $? $@
$D$(NEWSLIB)/send-nntp:	send-nntp	; $(COPY_XPRI) $? $@
$D$(NEWSLIB)/send-uucp:	send-uucp	; $(COPY_XPRI) $? $@
$D$(NEWSBIN)/inncheck:	inncheck	; $(COPY_XPRI) $? $@
$D$(NEWSBIN)/innstat:	innstat		; $(COPY_XPRI) $? $@
$D$(NEWSBIN)/pgpverify:	pgpverify	; $(COPY_XPRI) $? $@
$D$(NEWSBIN)/innwatch:	innwatch	; $(COPY_XPRI) $? $@
$D$(PATH_SHELLVARS):	innshellvars	; $(COPY_RPRI) $? $@
$D$(PATH_PERL_SHELLVARS): innshellvars.pl ; $(COPY_RPRI) $? $@
$D$(PATH_TCL_SHELLVARS): innshellvars.tcl ; $(COPY_RPRI) $? $@
$D$(PATH_CSH_SHELLVARS): innshellvars.csh ; $(COPY_RPRI) $? $@
$D$(NEWSBIN)/innreport: innreport	; $(COPY_XPRI) $? $@
$D$(NEWSLIB)/innreport_inn.pm: innreport_inn.pm	; $(COPY_RPRI) $? $@
$D$(NEWSLIB)/innreport.conf: innreport.conf	; $(COPY_RPRI) $? $@
$D$(PATH_STORAGECTL): storage.ctl	; $(COPY_RPRI) $? $@
$D$(PATH_OVERVIEWCTL): overview.ctl	; $(COPY_RPRI) $? $@
$D$(PATH_CYCBUFFCONFIG): cycbuff.conf	; $(COPY_RPRI) $? $@
$D$(PATH_PERL_STARTUP_INND): startup_innd.pl ; $(COPY_RPRI) $? $@
$D$(PATH_PERL_FILTER_INND): filter_innd.pl ; $(COPY_RPRI) $? $@
$D$(PATH_PERL_FILTER_NNRPD): filter_nnrpd.pl ; $(COPY_RPRI) $? $@
$D$(PATH_TCL_STARTUP): startup.tcl	; $(COPY_RPRI) $? $@
$D$(PATH_TCL_FILTER): filter.tcl	; $(COPY_RPRI) $? $@
$D$(PATH_ACTSYNC_CFG): actsync.cfg	; $(COPY_RPRI) $? $@
$D$(PATH_ACTSYNC_IGN): actsync.ign	; $(COPY_RPRI) $? $@
$D$(NEWSBIN)/scanspool: scanspool	; $(COPY_XPRI) $? $@
$D$(CTLBIN)/sample.control: sample.control ; $(COPY_RPRI) $? $@
$D$(PATH_MOTD): motd.news		; $(COPY_RPRI) $? $@

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
control.ctl:	../samples/control.ctl		; $(COPY) $? $@
default:	../samples/default		; $(COPY) $? $@
docheckgroups:	../samples/docheckgroups	; $(COPY) $? $@
expire.ctl:	../samples/expire.ctl		; $(COPY) $? $@
filter.tcl:	../samples/filter.tcl		; $(COPY) $? $@
filter_innd.pl:	../samples/filter_innd.pl	; $(COPY) $? $@
filter_nnrpd.pl: ../samples/filter_nnrpd.pl	; $(COPY) $? $@
expirerm:	../samples/expirerm		; $(COPY) $? $@
innmail:	../samples/innmail		; $(COPY) $? $@
incoming.conf:	../samples/incoming.conf		; $(COPY) $? $@
ihave:		../samples/ihave		; $(COPY) $? $@
inn.conf:	../samples/inn.conf		; $(COPY) $? $@
inncheck:	../samples/inncheck		; $(COPY) $? $@
innreport:	../samples/innreport		; $(COPY) $? $@
innreport_inn.pm: ../samples/innreport_inn.pm	; $(COPY) $? $@
innreport.conf:	../samples/innreport.conf	; $(COPY) $? $@
storage.ctl:	../samples/storage.ctl		; $(COPY) $? $@
overview.ctl:	../samples/overview.ctl		; $(COPY) $? $@
cycbuff.conf:	../samples/cycbuff.conf		; $(COPY) $? $@
innstat:	../samples/innstat		; $(COPY) $? $@
innwatch:	../samples/innwatch		; $(COPY) $? $@
innwatch.ctl:	../samples/innwatch.ctl		; $(COPY) $? $@
innshellvars:	../samples/innshellvars		; $(COPY) $? $@
innshellvars.pl: ../samples/innshellvars.pl	; $(COPY) $? $@
innshellvars.tcl: ../samples/innshellvars.tcl	; $(COPY) $? $@
innshellvars.csh: ../samples/innshellvars.csh	; $(COPY) $? $@
moderators:	../samples/moderators		; $(COPY) $? $@
distrib.pats:	../samples/distrib.pats		; $(COPY) $? $@
motd.news:	../samples/motd.news		; $(COPY) $? $@
newgroup:	../samples/newgroup		; $(COPY) $? $@
news.daily:	../samples/news.daily		; $(COPY) $? $@
newsfeeds:	../samples/newsfeeds		; $(COPY) $? $@
nnrp.access:	../samples/nnrp.access		; $(COPY) $? $@
nnrpd.track:	../samples/nnrpd.track		; $(COPY) $? $@
nntpsend.ctl:	../samples/nntpsend.ctl		; $(COPY) $? $@
nntpsend:	../samples/nntpsend		; $(COPY) $? $@
overview.fmt:	../samples/overview.fmt		; $(COPY) $? $@
parsecontrol:	../samples/parsecontrol		; $(COPY) $? $@
writelog:	../samples/writelog		; $(COPY) $? $@
passwd.nntp:	../samples/passwd.nntp		; $(COPY) $? $@
pgpverify:	../samples/pgpverify		; $(COPY) $? $@
rc.news:	../samples/rc.news		; $(COPY) $? $@
rmgroup:	../samples/rmgroup		; $(COPY) $? $@
sample.control: ../samples/sample.control	; $(COPY) $? $@
scanlogs:	../samples/scanlogs		; $(COPY) $? $@
scanspool:	../samples/scanspool		; $(COPY) $? $@
send-ihave:	../samples/send-ihave		; $(COPY) $? $@
send-nntp:	../samples/send-nntp		; $(COPY) $? $@
send-uucp:	../samples/send-uucp		; $(COPY) $? $@
sendbatch:	../samples/sendbatch		; $(COPY) $? $@
sendme:		../samples/sendme		; $(COPY) $? $@
sendsys:	../samples/sendsys		; $(COPY) $? $@
senduuname:	../samples/senduuname		; $(COPY) $? $@
startup.tcl:	../samples/startup.tcl		; $(COPY) $? $@
startup_innd.pl: ../samples/startup_innd.pl	; $(COPY) $? $@
tally.control:	../samples/tally.control	; $(COPY) $? $@
version:	../samples/version		; $(COPY) $? $@

##  Dependencies.  Default list, below, is probably good enough.
depend:		Makefile $(SOURCES)
	makedepend $(DEFS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.

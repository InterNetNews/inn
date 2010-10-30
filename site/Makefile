##  $Revision$
include ../Makefile.global
top = ..

##  If you want to do ctlinnd pause/reload/go, uncomment these lines.
#PAUSE		= pause
#RELOAD_AND_GO	= reload go
DIFF="diff"

# Added a default rule for ".csh" because Digital UNIX has a builtin
# rule which would overwite the innshellvars file.
.csh:

CTLINND			= ${PATHBIN}/ctlinnd
FILTBIN			= ${PATHFILTER}
PATH_PERL_STARTUP_INND	= ${PATHFILTER}/startup_innd.pl
PATH_PERL_FILTER_INND	= ${PATHFILTER}/filter_innd.pl
PATH_PERL_FILTER_NNRPD	= ${PATHFILTER}/filter_nnrpd.pl
PATH_PYTHON_FILTER_INND = ${PATHFILTER}/filter_innd.py
PATH_PYTHON_INN_MODULE  = ${PATHFILTER}/INN.py
PATH_PYTHON_NNRPD_MODULE= ${PATHFILTER}/nnrpd.py
PATH_NNRPAUTH           = ${PATHFILTER}/nnrpd_auth.pl
PATH_NNRPYAUTH          = ${PATHFILTER}/nnrpd_auth.py
PATH_NNRPACCESS         = ${PATHFILTER}/nnrpd_access.pl
PATH_NNRPYACCESS        = ${PATHFILTER}/nnrpd_access.py
PATH_NNRPYDYNAMIC       = ${PATHFILTER}/nnrpd_dynamic.py

PATH_CONFIG		= ${PATHETC}/inn.conf
PATH_CONTROLCTL		= ${PATHETC}/control.ctl
PATH_CONTROLCTLLOCAL	= ${PATHETC}/control.ctl.local
PATH_EXPIRECTL		= ${PATHETC}/expire.ctl
PATH_INNDHOSTS		= ${PATHETC}/incoming.conf
PATH_MODERATORS		= ${PATHETC}/moderators
PATH_DISTPATS		= ${PATHETC}/distrib.pats
PATH_DISTRIBUTIONS	= ${PATHETC}/distributions
PATH_NEWSFEEDS		= ${PATHETC}/newsfeeds
PATH_READERSCONF	= ${PATHETC}/readers.conf
PATH_NNRPDTRACK		= ${PATHETC}/nnrpd.track
PATH_NNTPPASS		= ${PATHETC}/passwd.nntp
PATH_CTLWATCH		= ${PATHETC}/innwatch.ctl
PATH_ACTSYNC_IGN	= ${PATHETC}/actsync.ign
PATH_ACTSYNC_CFG	= ${PATHETC}/actsync.cfg
PATH_MOTD_INND		= ${PATHETC}/motd.innd
PATH_MOTD_NNRPD		= ${PATHETC}/motd.nnrpd
PATH_STORAGECONF	= ${PATHETC}/storage.conf
PATH_CYCBUFFCONFIG	= ${PATHETC}/cycbuff.conf
PATH_INNFEEDCTL		= ${PATHETC}/innfeed.conf
PATH_BUFFINDEXED	= ${PATHETC}/buffindexed.conf
PATH_RADIUS_CONF	= ${PATHETC}/radius.conf
PATH_OVDB_CONF		= ${PATHETC}/ovdb.conf
PATH_SENDUUCP_CF	= ${PATHETC}/send-uucp.cf
PATH_SUBSCRIPTIONS	= ${PATHETC}/subscriptions

PATH_ACTIVE		= ${PATHDB}/active
PATH_ACTIVE_TIMES	= ${PATHDB}/active.times
PATH_HISTORY		= ${PATHDB}/history
PATH_NEWSGROUPS		= ${PATHDB}/newsgroups

##  Scripts from above, plus site-specific config files.
REST		= \
	newsfeeds incoming.conf nnrpd.track passwd.nntp \
	inn.conf moderators innreport.conf innreport.css localgroups \
	control.ctl control.ctl.local expire.ctl nntpsend.ctl \
	innwatch.ctl distrib.pats distributions actsync.cfg actsync.ign \
	motd.innd motd.nnrpd storage.conf cycbuff.conf buffindexed.conf \
	innfeed.conf startup_innd.pl filter_innd.pl filter_nnrpd.pl \
	filter_innd.py INN.py \
	innshellvars.local innshellvars.pl.local innshellvars.tcl.local \
	nnrpd.py \
	nnrpd_auth.pl nnrpd_access.pl nocem.ctl \
        news2mail.cf readers.conf \
	radius.conf nnrpd_auth.py nnrpd_access.py nnrpd_dynamic.py \
	ovdb.conf active.minimal \
	newsgroups.minimal send-uucp.cf subscriptions

ALL		= $(MOST) $(REST)

REST_INSTALLED	= \
	$D$(PATH_NEWSFEEDS) $D$(PATH_INNDHOSTS) \
	$D$(PATH_NNRPDTRACK) $D$(PATH_NNTPPASS) \
	$D$(PATH_CONFIG) $D$(PATH_MODERATORS) \
	$D$(PATH_CONTROLCTL) $D$(PATH_CONTROLCTLLOCAL) $D$(PATH_EXPIRECTL) \
	$D$(PATHETC)/nntpsend.ctl \
	$D$(PATHETC)/innreport.conf $D$(PATHHTTP)/innreport.css \
	$D$(PATHETC)/localgroups \
	$D$(PATH_CTLWATCH) $D$(PATH_DISTPATS) $D$(PATH_DISTRIBUTIONS) \
	$D$(PATH_ACTSYNC_CFG) $D$(PATH_ACTSYNC_IGN) \
	$D$(PATH_MOTD_INND) $D$(PATH_MOTD_NNRPD) $D$(PATH_STORAGECONF) \
	$D$(PATH_CYCBUFFCONFIG) $D$(PATH_BUFFINDEXED) \
	$D$(PATH_INNFEEDCTL) $D$(PATH_PERL_STARTUP_INND) \
	$D$(PATH_PERL_FILTER_INND) $D$(PATH_PERL_FILTER_NNRPD) \
	$D$(PATH_PYTHON_FILTER_INND) $D$(PATH_PYTHON_INN_MODULE) \
	$D$(PATH_PYTHON_NNRPD_MODULE) \
	$D$(PATH_TCL_STARTUP) $D$(PATH_TCL_FILTER) \
	$D$(PATHETC)/innshellvars.local $D$(PATHETC)/innshellvars.pl.local \
	$D$(PATHETC)/innshellvars.tcl.local \
	$D$(PATHETC)/nocem.ctl \
	$D$(PATH_NNRPAUTH) $D$(PATHETC)/news2mail.cf $D$(PATH_READERSCONF) \
	$D$(PATH_RADIUS_CONF) $D$(PATH_NNRPYAUTH) $D$(PATH_NNRPYACCESS) $D$(PATH_NNRPYDYNAMIC) \
	$D$(PATH_OVDB_CONF) \
	$D$(PATH_SENDUUCP_CF) $D$(PATH_SUBSCRIPTIONS) $D$(PATH_NNRPACCESS)

ALL_INSTALLED	= $(MOST_INSTALLED) $(REST_INSTALLED)

SPECIAL		= $D$(PATH_ACTIVE) $D$(PATH_ACTIVE_TIMES) \
		$D$(PATH_NEWSGROUPS) $D$(PATH_HISTORY)

##  Get new versions of everything from samples directory.
all:		$(ALL) config

##  Get only scripts, not per-host config files.
most:		$(MOST)

##  Show changes between files here and ones in samples.
diff:
	@$(MAKE) COPY=-${DIFF} all

##  Show changes between files here and installed versions.
diff-installed:
	@$(MAKE) COPY_RPRI=-${DIFF} COPY_RPUB=-${DIFF} COPY_XPRI=-${DIFF} COPY_XPUB=-${DIFF} $(ALL_INSTALLED)

##  Show what would be copied from samples directory.
what:
	@$(MAKE) -s 'COPY=@echo' $(ALL) | ${AWK} 'NF==2 { print $$2; }'

config:		$(ALL)
	date >config

##  Don't use parallel rules -- we want this to be viewed carefully.
install:	all $(PAUSE) install-config $(RELOAD_AND_GO)
reload-install:	all pause    install-config reload go
install-config:		update $(REST_INSTALLED) $(SPECIAL)

##  Install scripts, not per-host config files.
update:		all $(MOST_INSTALLED)
	@echo "" ; echo inn.conf in site directory may have newly added parameters
	@echo which installed inn.conf does not have.  Check those parameters
	@echo before you run innd. ; echo ""
	date >update

##  Special rules for files that should never be overwritten if they are
##  already installed.  These are used only for the initial install of a
##  brand new server.
$D$(PATH_ACTIVE): 	; $(CP_DATA) active.minimal $@
$D$(PATH_NEWSGROUPS):	; $(CP_DATA) newsgroups.minimal $@
$D$(PATH_ACTIVE_TIMES):
	touch $@
	chown $(RUNASUSER) $@
	chgrp $(RUNASGROUP) $@
	chmod $(FILEMODE) $@
$D$(PATH_HISTORY):
	touch $@
	chown $(RUNASUSER) $@
	chgrp $(RUNASGROUP) $@
	chmod $(FILEMODE) $@
	test -z "$D" && $(PATHBIN)/makedbz -i -o

bootstrap:

##  Remove files that are unchanged from the release version.
clean:
	@-for I in $(ALL) ; do \
	    cmp -s $$I ../samples/$$I && echo rm -f $$I && rm -f $$I ; \
	done

clobber distclean maintclean:
	rm -f $(ALL) tags profiled config update

tags ctags:
	cp /dev/null tags

profiled:
	cp /dev/null profiled

depend:

##  Commands to make private or public, read or executable files.
COPY_RPRI	= $(CP_RPRI)
COPY_RPUB	= $(CP_RPUB)
COPY_XPRI	= $(CP_XPRI)
COPY_XPUB	= $(CP_XPUB)

##  Files to copy.
$D$(PATH_INNDHOSTS):	incoming.conf	; $(COPY_RPRI) $? $@
$D$(PATH_NEWSFEEDS):	newsfeeds	; $(COPY_RPUB) $? $@
$D$(PATH_READERSCONF):	readers.conf	; $(COPY_RPUB) $? $@
$D$(PATH_RADIUS_CONF):	radius.conf	; $(COPY_RPRI) $? $@
$D$(PATH_NNRPDTRACK):	nnrpd.track	; $(COPY_RPUB) $? $@
$D$(PATH_CONTROLCTL):	control.ctl	; $(COPY_RPUB) $? $@
$D$(PATH_CONTROLCTLLOCAL): control.ctl.local	; $(COPY_RPUB) $? $@
$D$(PATH_CTLWATCH):	innwatch.ctl	; $(COPY_RPUB) $? $@
$D$(PATH_EXPIRECTL):	expire.ctl	; $(COPY_RPUB) $? $@
$D$(PATH_CONFIG):	inn.conf	; $(COPY_RPUB) $? $@
$D$(PATH_MODERATORS):	moderators	; $(COPY_RPUB) $? $@
$D$(PATH_DISTPATS):	distrib.pats	; $(COPY_RPUB) $? $@
$D$(PATH_DISTRIBUTIONS): distributions  ; $(COPY_RPUB) $? $@
$D$(PATH_NNTPPASS):	passwd.nntp	; $(COPY_RPRI) $? $@
$D$(PATHETC)/nntpsend.ctl: nntpsend.ctl	; $(COPY_RPUB) $? $@
$D$(PATHETC)/news2mail.cf: news2mail.cf	; $(COPY_RPUB) $? $@
$D$(PATHETC)/innreport.conf: innreport.conf	; $(COPY_RPUB) $? $@
$D$(PATHHTTP)/innreport.css: innreport.css	; $(COPY_RPUB) $? $@
$D$(PATHETC)/innshellvars.local: innshellvars.local	; $(COPY_XPUB) $? $@
$D$(PATHETC)/innshellvars.pl.local: innshellvars.pl.local	; $(COPY_XPUB) $? $@
$D$(PATHETC)/innshellvars.tcl.local: innshellvars.tcl.local	; $(COPY_XPUB) $? $@
$D$(PATHETC)/localgroups: localgroups	; $(COPY_RPUB) $? $@
$D$(PATH_STORAGECONF): storage.conf	; $(COPY_RPUB) $? $@
$D$(PATH_CYCBUFFCONFIG): cycbuff.conf	; $(COPY_RPUB) $? $@
$D$(PATH_BUFFINDEXED): buffindexed.conf	; $(COPY_RPUB) $? $@
$D$(PATH_OVDB_CONF): ovdb.conf		; $(COPY_RPUB) $? $@
$D$(PATH_PERL_STARTUP_INND): startup_innd.pl ; $(COPY_RPUB) $? $@
$D$(PATH_PERL_FILTER_INND): filter_innd.pl ; $(COPY_RPUB) $? $@
$D$(PATH_PERL_FILTER_NNRPD): filter_nnrpd.pl ; $(COPY_RPUB) $? $@
$D$(PATH_PYTHON_FILTER_INND): filter_innd.py ; $(COPY_RPUB) $? $@
$D$(PATH_PYTHON_INN_MODULE): INN.py ;	$(COPY_RPUB) $? $@
$D$(PATH_PYTHON_NNRPD_MODULE): nnrpd.py ;   $(COPY_RPUB) $? $@
$D$(PATH_NNRPAUTH): nnrpd_auth.pl       ; $(COPY_RPUB) $? $@
$D$(PATH_NNRPACCESS): nnrpd_access.pl   ; $(COPY_RPUB) $? $@
$D$(PATH_NNRPYAUTH): nnrpd_auth.py      ; $(COPY_RPUB) $? $@
$D$(PATH_NNRPYACCESS): nnrpd_access.py  ; $(COPY_RPUB) $? $@
$D$(PATH_NNRPYDYNAMIC): nnrpd_dynamic.py     ; $(COPY_RPUB) $? $@
$D$(PATHETC)/nocem.ctl: nocem.ctl       ; $(COPY_RPUB) $? $@
$D$(PATH_ACTSYNC_CFG): actsync.cfg	; $(COPY_RPUB) $? $@
$D$(PATH_ACTSYNC_IGN): actsync.ign	; $(COPY_RPUB) $? $@
$D$(PATH_MOTD_INND): motd.innd		; $(COPY_RPUB) $? $@
$D$(PATH_MOTD_NNRPD): motd.nnrpd	; $(COPY_RPUB) $? $@
$D$(PATH_INNFEEDCTL): innfeed.conf	; $(COPY_RPRI) $? $@
$D$(PATH_SENDUUCP_CF): send-uucp.cf	; $(COPY_RPUB) $? $@
$D$(PATH_SUBSCRIPTIONS): subscriptions	; $(COPY_RPUB) $? $@

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
control.ctl:	../samples/control.ctl		; $(COPY) $? $@
control.ctl.local: ../samples/control.ctl.local	; $(COPY) $? $@
expire.ctl:	../samples/expire.ctl		; $(COPY) $? $@
nnrpd_auth.pl:  ../samples/nnrpd_auth.pl	; $(COPY) $? $@
nnrpd_access.pl:  ../samples/nnrpd_access.pl	; $(COPY) $? $@
nnrpd_auth.py:  ../samples/nnrpd_auth.py	; $(COPY) $? $@
nnrpd_access.py: ../samples/nnrpd_access.py     ; $(COPY) $? $@
nnrpd_dynamic.py: ../samples/nnrpd_dynamic.py   ; $(COPY) $? $@
filter_innd.pl:	../samples/filter_innd.pl	; $(COPY) $? $@
filter_nnrpd.pl: ../samples/filter_nnrpd.pl	; $(COPY) $? $@
filter_innd.py:	../samples/filter_innd.py	; $(COPY) $? $@
INN.py:		../samples/INN.py		; $(COPY) $? $@
nnrpd.py:       ../samples/nnrpd.py             ; $(COPY) $? $@
incoming.conf:	../samples/incoming.conf	; $(COPY) $? $@
inn.conf:	../samples/inn.conf		; $(COPY) $? $@
innreport.conf:	../samples/innreport.conf	; $(COPY) $? $@
innreport.css:	../samples/innreport.css	; $(COPY) $? $@
innshellvars.local: ../samples/innshellvars.local	; $(COPY) $? $@
innshellvars.pl.local: ../samples/innshellvars.pl.local	; $(COPY) $? $@
innshellvars.tcl.local: ../samples/innshellvars.tcl.local	; $(COPY) $? $@
localgroups:	../samples/localgroups		; $(COPY) $? $@
storage.conf:	../samples/storage.conf		; $(COPY) $? $@
cycbuff.conf:	../samples/cycbuff.conf		; $(COPY) $? $@
buffindexed.conf: ../samples/buffindexed.conf	; $(COPY) $? $@
ovdb.conf:	../samples/ovdb.conf		; $(COPY) $? $@
innwatch.ctl:	../samples/innwatch.ctl		; $(COPY) $? $@
innfeed.conf:	../samples/innfeed.conf		; $(COPY) $? $@
moderators:	../samples/moderators		; $(COPY) $? $@
distrib.pats:	../samples/distrib.pats		; $(COPY) $? $@
distributions:	../samples/distributions	; $(COPY) $? $@
motd.innd:	../samples/motd.innd		; $(COPY) $? $@
motd.nnrpd:	../samples/motd.nnrpd		; $(COPY) $? $@
news2mail.cf:	../samples/news2mail.cf		; $(COPY) $? $@
newsfeeds:	../samples/newsfeeds		; $(COPY) $? $@
nnrpd.track:	../samples/nnrpd.track		; $(COPY) $? $@
nntpsend.ctl:	../samples/nntpsend.ctl		; $(COPY) $? $@
nocem.ctl:	../samples/nocem.ctl		; $(COPY) $? $@
parsecontrol:	../samples/parsecontrol		; $(COPY) $? $@
passwd.nntp:	../samples/passwd.nntp		; $(COPY) $? $@
readers.conf:	../samples/readers.conf		; $(COPY) $? $@
radius.conf:	../samples/radius.conf		; $(COPY) $? $@
startup_innd.pl: ../samples/startup_innd.pl	; $(COPY) $? $@
send-uucp.cf:	../samples/send-uucp.cf		; $(COPY) $? $@
subscriptions:	../samples/subscriptions	; $(COPY) $? $@
active.minimal:	../samples/active.minimal	; $(COPY) $? $@
newsgroups.minimal: ../samples/newsgroups.minimal ; $(COPY) $? $@

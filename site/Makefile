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
PATH_TCL_STARTUP	= ${PATHFILTER}/startup.tcl
PATH_TCL_FILTER		= ${PATHFILTER}/filter.tcl
PATH_PYTHON_FILTER_INND = ${PATHFILTER}/filter_innd.py
PATH_PYTHON_INN_MODULE  = ${PATHFILTER}/INN.py
PATH_NNRPAUTH           = ${PATHFILTER}/nnrpd_auth.pl
PATH_NNRPYAUTH          = ${PATHFILTER}/nnrpd_auth.py

PATH_CONFIG		= ${PATHETC}/inn.conf
PATH_CONTROLCTL		= ${PATHETC}/control.ctl
PATH_EXPIRECTL		= ${PATHETC}/expire.ctl
PATH_INNDHOSTS		= ${PATHETC}/incoming.conf
PATH_MODERATORS		= ${PATHETC}/moderators
PATH_DISTPATS		= ${PATHETC}/distrib.pats
PATH_NEWSFEEDS		= ${PATHETC}/newsfeeds
PATH_READERSCONF	= ${PATHETC}/readers.conf
PATH_NNRPDTRACK		= ${PATHETC}/nnrpd.track
PATH_SCHEMA		= ${PATHETC}/overview.fmt
PATH_NNTPPASS		= ${PATHETC}/passwd.nntp
PATH_CTLWATCH		= ${PATHETC}/innwatch.ctl
PATH_ACTSYNC_IGN	= ${PATHETC}/actsync.ign
PATH_ACTSYNC_CFG	= ${PATHETC}/actsync.cfg
PATH_MOTD		= ${PATHETC}/motd.news
PATH_STORAGECONF	= ${PATHETC}/storage.conf
PATH_CYCBUFFCONFIG	= ${PATHETC}/cycbuff.conf
PATH_INNFEEDCTL		= ${PATHETC}/innfeed.conf
PATH_BUFFINDEXED	= ${PATHETC}/buffindexed.conf
PATH_RADIUS_CONF	= ${PATHETC}/radius.conf
PATH_OVDB_CONF		= ${PATHETC}/ovdb.conf
PATH_SASL_CONF		= ${PATHETC}/sasl.conf
PATH_SUBSCRIPTIONS	= ${PATHETC}/subscriptions

PATH_ACTIVE		= ${PATHDB}/active
PATH_ACTIVE_TIMES	= ${PATHDB}/active.times
PATH_HISTORY		= ${PATHDB}/history
PATH_NEWSGROUPS		= ${PATHDB}/newsgroups

##  Scripts from above, plus site-specific config files.
REST		= \
	newsfeeds incoming.conf nnrpd.track passwd.nntp \
	inn.conf moderators innreport.conf \
	control.ctl expire.ctl nntpsend.ctl overview.fmt \
	innwatch.ctl distrib.pats actsync.cfg actsync.ign \
	motd.news storage.conf cycbuff.conf buffindexed.conf \
	innfeed.conf startup_innd.pl filter_innd.pl filter_nnrpd.pl \
	filter_innd.py INN.py \
	startup.tcl filter.tcl nnrpd_auth.pl nnrpd_access.pl \
        news2mail.cf readers.conf \
	radius.conf nnrpd_auth.py ovdb.conf sasl.conf active.minimal \
	newsgroups.minimal subscriptions

ALL		= $(MOST) $(REST)

REST_INSTALLED	= \
	$D$(PATH_NEWSFEEDS) $D$(PATH_INNDHOSTS) \
	$D$(PATH_NNRPDTRACK) $D$(PATH_NNTPPASS) \
	$D$(PATH_CONFIG) $D$(PATH_MODERATORS) \
	$D$(PATH_CONTROLCTL) $D$(PATH_EXPIRECTL) $D$(PATHETC)/nntpsend.ctl \
	$D$(PATHETC)/innreport.conf \
	$D$(PATH_CTLWATCH) $D$(PATH_DISTPATS) $D$(PATH_SCHEMA) \
	$D$(PATH_ACTSYNC_CFG) $D$(PATH_ACTSYNC_IGN) \
	$D$(PATH_MOTD) $D$(PATH_STORAGECONF) \
	$D$(PATH_OVERVIEWCTL) $D$(PATH_CYCBUFFCONFIG) $D$(PATH_BUFFINDEXED) \
	$D$(PATH_INNFEEDCTL) $D$(PATH_PERL_STARTUP_INND) \
	$D$(PATH_PERL_FILTER_INND) $D$(PATH_PERL_FILTER_NNRPD) \
	$D$(PATH_PYTHON_FILTER_INND) $D$(PATH_PYTHON_INN_MODULE) \
	$D$(PATH_TCL_STARTUP) $D$(PATH_TCL_FILTER) \
	$D$(PATH_NNRPAUTH) $D$(PATHETC)/news2mail.cf $D$(PATH_READERSCONF) \
	$D$(PATH_RADIUS_CONF) $D$(PATH_NNRPYAUTH) $D$(PATH_OVDB_CONF) \
	$D$(PATH_SASL_CONF) $D$(PATH_SUBSCRIPTIONS)

ALL_INSTALLED	= $(MOST_INSTALLED) $(REST_INSTALLED)

SPECIAL		= $D$(PATH_ACTIVE) $D$(PATH_ACTIVE_TIMES) \
		$D$(PATH_NEWSGROUPS) $D$(PATH_HISTORY)

##  Get new versions of everything from samples directory.
all:		$(P) $(ALL) config

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

##  Special rules for files that sould never be overwritten if they are
##  already installed.  These are used only for the initial install of a
##  brand new server.
$D$(PATH_ACTIVE): 	; $(CP_DATA) active.minimal $@
$D$(PATH_NEWSGROUPS):	; $(CP_DATA) newsgroups.minimal $@
$D$(PATH_ACTIVE_TIMES):
	touch $@
	chown $(NEWSUSER) $@
	chgrp $(NEWSGROUP) $@
	chmod $(FILEMODE) $@
$D$(PATH_HISTORY):
	touch $@
	$(PATHBIN)/makedbz -i -o

##  Remove files that are unchanged from the release version.
clean:
	@-for I in $(ALL) ; do \
	    cmp -s $$I ../samples/$$I && echo rm -f $$I && rm -f $$I ; \
	done

clobber distclean:
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

##  Order:  innd, control, expire, inews, sending, misc
$D$(PATH_INNDHOSTS):	incoming.conf	; $(COPY_RPRI) $? $@
$D$(PATH_NEWSFEEDS):	newsfeeds	; $(COPY_RPUB) $? $@
$D$(PATH_READERSCONF):	readers.conf	; $(COPY_RPUB) $? $@
$D$(PATH_RADIUS_CONF):	radius.conf	; $(COPY_RPRI) $? $@
$D$(PATH_NNRPDTRACK):	nnrpd.track	; $(COPY_RPUB) $? $@
$D$(PATH_SCHEMA):	overview.fmt	; $(COPY_RPUB) $? $@
$D$(PATH_CONTROLCTL):	control.ctl	; $(COPY_RPUB) $? $@
$D$(PATH_CTLWATCH):	innwatch.ctl	; $(COPY_RPUB) $? $@
$D$(PATH_EXPIRECTL):	expire.ctl	; $(COPY_RPUB) $? $@
$D$(PATH_CONFIG):	inn.conf	; $(COPY_RPUB) $? $@
$D$(PATH_MODERATORS):	moderators	; $(COPY_RPUB) $? $@
$D$(PATH_DISTPATS):	distrib.pats	; $(COPY_RPUB) $? $@
$D$(PATH_NNTPPASS):	passwd.nntp	; $(COPY_RPRI) $? $@
$D$(PATHETC)/nntpsend.ctl: nntpsend.ctl	; $(COPY_RPUB) $? $@
$D$(PATHETC)/news2mail.cf: news2mail.cf	; $(COPY_RPUB) $? $@
$D$(PATHETC)/innreport.conf: innreport.conf	; $(COPY_RPUB) $? $@
$D$(PATH_STORAGECONF): storage.conf	; $(COPY_RPUB) $? $@
$D$(PATH_CYCBUFFCONFIG): cycbuff.conf	; $(COPY_RPUB) $? $@
$D$(PATH_BUFFINDEXED): buffindexed.conf	; $(COPY_RPUB) $? $@
$D$(PATH_OVDB_CONF): ovdb.conf		; $(COPY_RPUB) $? $@
$D$(PATH_PERL_STARTUP_INND): startup_innd.pl ; $(COPY_RPUB) $? $@
$D$(PATH_PERL_FILTER_INND): filter_innd.pl ; $(COPY_RPUB) $? $@
$D$(PATH_PERL_FILTER_NNRPD): filter_nnrpd.pl ; $(COPY_RPUB) $? $@
$D$(PATH_PYTHON_FILTER_INND): filter_innd.py ; $(COPY_RPUB) $? $@
$D$(PATH_PYTHON_INN_MODULE): INN.py ;	$(COPY_RPUB) $? $@
$D$(PATH_TCL_STARTUP): startup.tcl	; $(COPY_RPUB) $? $@
$D$(PATH_TCL_FILTER): filter.tcl	; $(COPY_RPUB) $? $@
$D$(PATH_NNRPAUTH): nnrpd_auth.pl       ; $(COPY_RPUB) $? $@
$D$(PATH_NNRPACCESS): nnrpd_access.pl     ; $(COPY_RPUB) $? $@
$D$(PATH_NNRPYAUTH): nnrpd_auth.py      ; $(COPY_RPUB) $? $@
$D$(PATH_ACTSYNC_CFG): actsync.cfg	; $(COPY_RPUB) $? $@
$D$(PATH_ACTSYNC_IGN): actsync.ign	; $(COPY_RPUB) $? $@
$D$(PATH_MOTD): motd.news		; $(COPY_RPUB) $? $@
$D$(PATH_INNFEEDCTL): innfeed.conf	; $(COPY_RPRI) $? $@
$D$(PATH_SASL_CONF):	sasl.conf	; $(COPY_RPUB) $? $@
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
expire.ctl:	../samples/expire.ctl		; $(COPY) $? $@
filter.tcl:	../samples/filter.tcl		; $(COPY) $? $@
nnrpd_auth.pl:  ../samples/nnrpd_auth.pl	; $(COPY) $? $@
nnrpd_access.pl:  ../samples/nnrpd_access.pl	; $(COPY) $? $@
nnrpd_auth.py:  ../samples/nnrpd_auth.py	; $(COPY) $? $@
filter_innd.pl:	../samples/filter_innd.pl	; $(COPY) $? $@
filter_nnrpd.pl: ../samples/filter_nnrpd.pl	; $(COPY) $? $@
filter_innd.py:	../samples/filter_innd.py	; $(COPY) $? $@
INN.py:		../samples/INN.py		; $(COPY) $? $@
incoming.conf:	../samples/incoming.conf	; $(COPY) $? $@
inn.conf:	../samples/inn.conf		; $(COPY) $? $@
innreport.conf:	../samples/innreport.conf	; $(COPY) $? $@
storage.conf:	../samples/storage.conf		; $(COPY) $? $@
cycbuff.conf:	../samples/cycbuff.conf		; $(COPY) $? $@
buffindexed.conf: ../samples/buffindexed.conf	; $(COPY) $? $@
ovdb.conf:	../samples/ovdb.conf		; $(COPY) $? $@
innwatch.ctl:	../samples/innwatch.ctl		; $(COPY) $? $@
innfeed.conf:	../samples/innfeed.conf		; $(COPY) $? $@
moderators:	../samples/moderators		; $(COPY) $? $@
distrib.pats:	../samples/distrib.pats		; $(COPY) $? $@
motd.news:	../samples/motd.news		; $(COPY) $? $@
news2mail.cf:	../samples/news2mail.cf		; $(COPY) $? $@
newsfeeds:	../samples/newsfeeds		; $(COPY) $? $@
nnrpd.track:	../samples/nnrpd.track		; $(COPY) $? $@
nntpsend.ctl:	../samples/nntpsend.ctl		; $(COPY) $? $@
overview.fmt:	../samples/overview.fmt		; $(COPY) $? $@
parsecontrol:	../samples/parsecontrol		; $(COPY) $? $@
passwd.nntp:	../samples/passwd.nntp		; $(COPY) $? $@
readers.conf:	../samples/readers.conf		; $(COPY) $? $@
radius.conf:	../samples/radius.conf		; $(COPY) $? $@
startup.tcl:	../samples/startup.tcl		; $(COPY) $? $@
startup_innd.pl: ../samples/startup_innd.pl	; $(COPY) $? $@
subscriptions:	../samples/subscriptions	; $(COPY) $? $@
sasl.conf:	../samples/sasl.conf		; $(COPY) $? $@
active.minimal:	../samples/active.minimal	; $(COPY) $? $@
newsgroups.minimal: ../samples/newsgroups.minimal ; $(COPY) $? $@

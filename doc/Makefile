##  $Revision$
include ../Makefile.global

SHELL	= /bin/sh
MAKE	= make
DESTDIR	=
D	= $(DESTDIR)

##  =()<MISSING_MAN = @<MISSING_MAN>@>()=
MISSING_MAN = 

##  Edit these if you need to.  For example, add "-b .OLD" to the MANFLAGS
##  variable (without quotes).
MANFLAGS = $(OWNER) -m 0444

SEC1	= convdate.1 getlist.1 grephistory.1 inews.1 innconfval.1 installit.1 \
	nntpget.1 rnews.1 shlock.1 shrinkfile.1 subst.1 innfeed.1 \
	startinnfeed.1
SEC3	= $(MISSING_MAN) \
	clientlib.3 dbz.3 inndcomm.3 libinn.3 libstorage.3 parsedate.3 qio.3 \
	wildmat.3
SEC5	= active.5 control.ctl.5 distrib.pats.5 expire.ctl.5 history.5 \
	inn.conf.5 innwatch.ctl.5 moderators.5 newsfeeds.5 nnrpd.track.5 \
	newslog.5 nnrp.access.5 nntpsend.ctl.5 overview.fmt.5 passwd.nntp.5 \
	innfeed.conf.5 motd.news.5 overview.ctl.5 storage.ctl.5 cycbuff.conf.5 \
	incoming.conf.5
SEC8	= actsync.8 archive.8 batcher.8 buffchan.8 crosspost.8 ctlinnd.8 \
	cvtbatch.8 expire.8 expireover.8 expirerm.8 fastrm.8 filechan.8 \
	inncheck.8 innd.8 innlog.pl.8 innstat.8 innwatch.8 innxbatch.8 \
	innxmit.8 makeactive.8 makehistory.8 news-recovery.8 news.daily.8 \
	newslog.8 newsrequeue.8 nnrpd.8 nntpsend.8 overchan.8 \
	pgpverify.8 prunehistory.8 scanlogs.8 \
	tally.control.8 tally.unwanted.8 writelog.8 expireindex.8 sm.8

INSTALLED_SEC1 =	$D$(MAN1)/convdate.1 $D$(MAN1)/getlist.1 \
			$D$(MAN1)/grephistory.1 $D$(MAN1)/inews.1 \
			$D$(MAN1)/innconfval.1 $D$(MAN1)/installit.1 \
			$D$(MAN1)/nntpget.1 $D$(MAN1)/rnews.1 \
			$D$(MAN1)/shlock.1 $D$(MAN1)/shrinkfile.1 \
			$D$(MAN1)/subst.1 $D$(MAN1)/innfeed.1 \
			$D$(MAN1)/startinnfeed.1

INSTALLED_SEC3 =	$D$(MAN3)/clientlib.3 $D$(MAN3)/dbz.3 \
			$D$(MAN3)/inndcomm.3 $D$(MAN3)/libinn.3 \
			$D$(MAN3)/libstorage.3 \
			$D$(MAN3)/parsedate.3 $D$(MAN3)/qio.3 \
			$D$(MAN3)/wildmat.3

INSTALLED_SEC5 =	$D$(MAN5)/active.5 $D$(MAN5)/control.ctl.5 \
			$D$(MAN5)/distrib.pats.5 $D$(MAN5)/expire.ctl.5 \
			$D$(MAN5)/history.5 \
			$D$(MAN5)/inn.conf.5 $D$(MAN5)/innwatch.ctl.5 \
			$D$(MAN5)/moderators.5 $D$(MAN5)/newsfeeds.5 \
			$D$(MAN5)/newslog.5 $D$(MAN5)/nnrp.access.5 \
			$D$(MAN5)/newslog.5 $D$(MAN5)/nnrpd.track.5 \
			$D$(MAN5)/nntpsend.ctl.5 $D$(MAN5)/overview.fmt.5 \
			$D$(MAN5)/passwd.nntp.5 $D$(MAN5)/innfeed.conf.5 \
			$D$(MAN5)/motd.news.5 $D$(MAN5)/overview.ctl.5 \
			$D$(MAN5)/storage.ctl.5 $D$(MAN5)/cycbuff.conf.5 \
			$D$(MAN5)/incoming.conf.5

INSTALLED_SEC8 =	$D$(MAN8)/actsync.8 $D$(MAN8)/archive.8 \
			$D$(MAN8)/batcher.8 $D$(MAN8)/buffchan.8 \
			$D$(MAN8)/crosspost.8 $D$(MAN8)/ctlinnd.8 \
			$D$(MAN8)/cvtbatch.8 $D$(MAN8)/expire.8 \
			$D$(MAN8)/expireover.8 $D$(MAN8)/expirerm.8 \
			$D$(MAN8)/fastrm.8 $D$(MAN8)/filechan.8 \
			$D$(MAN8)/inncheck.8 $D$(MAN8)/innd.8 \
			$D$(MAN8)/innlog.pl.8 $D$(MAN8)/innstat.8 \
			$D$(MAN8)/innwatch.8 $D$(MAN8)/innxbatch.8 \
			$D$(MAN8)/innxmit.8 $D$(MAN8)/makeactive.8 \
			$D$(MAN8)/makehistory.8 $D$(MAN8)/news-recovery.8 \
			$D$(MAN8)/news.daily.8 $D$(MAN8)/newslog.8 \
			$D$(MAN8)/newsrequeue.8 $D$(MAN8)/nnrpd.8 \
			$D$(MAN8)/nntpsend.8 $D$(MAN8)/overchan.8 \
			$D$(MAN8)/pgpverify.8 $D$(MAN8)/prunehistory.8 \
			$D$(MAN8)/scanlogs.8 $D$(MAN8)/tally.control.8 \
			$D$(MAN8)/tally.unwanted.8 $D$(MAN8)/writelog.8 \
			$D$(MAN8)/expireindex.8 $D$(MAN8)/sm.8

ALL	= $(SEC1) $(SEC3) $(SEC5) $(SEC8)

all:		$(P) $(ALL)

install:	install-man1 install-man3 install-man5 install-man8

install-man1:   $(INSTALLED_SEC1)

install-man3:	$(INSTALLED_SEC3)

install-man5:	$(INSTALLED_SEC5)

install-man8:	$(INSTALLED_SEC8)

clobber clean:
	rm -f all install profiled

tags ctags:
	cp /dev/null tags

profiled:
	cp /dev/null profiled

.PRECIOUS:	$(ALL)

##  Low-level install actions.
COPY	= $(SHELL) ./putman.sh $(MANPAGESTYLE) "$(MANFLAGS)"

$D$(MAN1)/convdate.1:		convdate.1	   ; $(COPY) $? $@
$D$(MAN1)/getlist.1:		getlist.1	   ; $(COPY) $? $@
$D$(MAN1)/grephistory.1:	grephistory.1	   ; $(COPY) $? $@
$D$(MAN1)/inews.1:		inews.1		   ; $(COPY) $? $@
$D$(MAN1)/innconfval.1:		innconfval.1	   ; $(COPY) $? $@
$D$(MAN1)/installit.1:		installit.1	   ; $(COPY) $? $@
$D$(MAN1)/nntpget.1:		nntpget.1	   ; $(COPY) $? $@
$D$(MAN1)/rnews.1:		rnews.1		   ; $(COPY) $? $@
$D$(MAN1)/shlock.1:		shlock.1	   ; $(COPY) $? $@
$D$(MAN1)/shrinkfile.1:		shrinkfile.1	   ; $(COPY) $? $@
$D$(MAN1)/subst.1:		subst.1		   ; $(COPY) $? $@
$D$(MAN1)/innfeed.1:		innfeed.1	   ; $(COPY) $? $@
$D$(MAN1)/startinnfeed.1:	startinnfeed.1	   ; $(COPY) $? $@

$D$(MAN3)/clientlib.3:		clientlib.3	   ; $(COPY) $? $@
$D$(MAN3)/dbz.3:		dbz.3		   ; $(COPY) $? $@
$D$(MAN3)/inndcomm.3:		inndcomm.3	   ; $(COPY) $? $@
$D$(MAN3)/libinn.3:		libinn.3	   ; $(COPY) $? $@
$D$(MAN3)/libstorage.3:		libstorage.3	   ; $(COPY) $? $@
$D$(MAN3)/parsedate.3:		parsedate.3	   ; $(COPY) $? $@
$D$(MAN3)/qio.3:		qio.3		   ; $(COPY) $? $@
$D$(MAN3)/wildmat.3:		wildmat.3	   ; $(COPY) $? $@

$D$(MAN5)/active.5:		active.5	   ; $(COPY) $? $@
$D$(MAN5)/control.ctl.5:	control.ctl.5	   ; $(COPY) $? $@
$D$(MAN5)/distrib.pats.5:	distrib.pats.5 	   ; $(COPY) $? $@
$D$(MAN5)/expire.ctl.5:		expire.ctl.5	   ; $(COPY) $? $@
$D$(MAN5)/history.5:		history.5	   ; $(COPY) $? $@
$D$(MAN5)/inn.conf.5:		inn.conf.5	   ; $(COPY) $? $@
$D$(MAN5)/innwatch.ctl.5:	innwatch.ctl.5 	   ; $(COPY) $? $@
$D$(MAN5)/moderators.5:		moderators.5	   ; $(COPY) $? $@
$D$(MAN5)/newsfeeds.5:		newsfeeds.5	   ; $(COPY) $? $@
$D$(MAN5)/newslog.5:		newslog.5	   ; $(COPY) $? $@
$D$(MAN5)/nnrp.access.5:	nnrp.access.5	   ; $(COPY) $? $@
$D$(MAN5)/nnrpd.track.5:	nnrpd.track.5	   ; $(COPY) $? $@
$D$(MAN5)/nntpsend.ctl.5:	nntpsend.ctl.5 	   ; $(COPY) $? $@
$D$(MAN5)/overview.fmt.5:	overview.fmt.5 	   ; $(COPY) $? $@
$D$(MAN5)/passwd.nntp.5:	passwd.nntp.5	   ; $(COPY) $? $@
$D$(MAN5)/innfeed.conf.5:	innfeed.conf.5	   ; $(COPY) $? $@
$D$(MAN5)/motd.news.5:		motd.news.5	   ; $(COPY) $? $@
$D$(MAN5)/overview.ctl.5:	overview.ctl.5	   ; $(COPY) $? $@
$D$(MAN5)/storage.ctl.5:	storage.ctl.5	   ; $(COPY) $? $@
$D$(MAN5)/cycbuff.conf.5:	cycbuff.conf.5     ; $(COPY) $? $@
$D$(MAN5)/incoming.conf.5:	incoming.conf.5    ; $(COPY) $? $@
$D$(MAN8)/actsync.8:		actsync.8	   ; $(COPY) $? $@
$D$(MAN8)/archive.8:		archive.8          ; $(COPY) $? $@
$D$(MAN8)/batcher.8:		batcher.8          ; $(COPY) $? $@
$D$(MAN8)/buffchan.8:		buffchan.8         ; $(COPY) $? $@
$D$(MAN8)/crosspost.8:		crosspost.8        ; $(COPY) $? $@
$D$(MAN8)/ctlinnd.8:		ctlinnd.8          ; $(COPY) $? $@
$D$(MAN8)/cvtbatch.8:		cvtbatch.8         ; $(COPY) $? $@
$D$(MAN8)/expire.8:		expire.8           ; $(COPY) $? $@
$D$(MAN8)/expireover.8:		expireover.8       ; $(COPY) $? $@
$D$(MAN8)/expirerm.8:		expirerm.8         ; $(COPY) $? $@
$D$(MAN8)/fastrm.8:		fastrm.8           ; $(COPY) $? $@
$D$(MAN8)/filechan.8:		filechan.8         ; $(COPY) $? $@
$D$(MAN8)/inncheck.8:		inncheck.8         ; $(COPY) $? $@
$D$(MAN8)/innd.8:		innd.8             ; $(COPY) $? $@
$D$(MAN8)/innlog.pl.8:		innlog.pl.8        ; $(COPY) $? $@
$D$(MAN8)/innstat.8:		innstat.8          ; $(COPY) $? $@
$D$(MAN8)/innwatch.8:		innwatch.8         ; $(COPY) $? $@
$D$(MAN8)/innxbatch.8:		innxbatch.8        ; $(COPY) $? $@
$D$(MAN8)/innxmit.8:		innxmit.8          ; $(COPY) $? $@
$D$(MAN8)/makeactive.8:		makeactive.8       ; $(COPY) $? $@
$D$(MAN8)/makehistory.8:	makehistory.8      ; $(COPY) $? $@
$D$(MAN8)/news-recovery.8:	news-recovery.8    ; $(COPY) $? $@
$D$(MAN8)/news.daily.8:		news.daily.8       ; $(COPY) $? $@
$D$(MAN8)/newslog.8:		newslog.8          ; $(COPY) $? $@
$D$(MAN8)/newsrequeue.8:	newsrequeue.8      ; $(COPY) $? $@
$D$(MAN8)/nnrpd.8:		nnrpd.8            ; $(COPY) $? $@
$D$(MAN8)/nntpsend.8:		nntpsend.8         ; $(COPY) $? $@
$D$(MAN8)/overchan.8:		overchan.8         ; $(COPY) $? $@
$D$(MAN8)/prunehistory.8:	prunehistory.8     ; $(COPY) $? $@
$D$(MAN8)/pgpverify.8:		pgpverify.8        ; $(COPY) $? $@
$D$(MAN8)/scanlogs.8:		scanlogs.8         ; $(COPY) $? $@
$D$(MAN8)/tally.control.8:	tally.control.8    ; $(COPY) $? $@
$D$(MAN8)/tally.unwanted.8:	tally.unwanted.8   ; $(COPY) $? $@
$D$(MAN8)/writelog.8:		writelog.8         ; $(COPY) $? $@
$D$(MAN8)/expireindex.8:	expireindex.8      ; $(COPY) $? $@
$D$(MAN8)/sm.8:			sm.8               ; $(COPY) $? $@

##  Dependencies.  Default list, below, is probably good enough.
depend:		Makefile $(SOURCES)
	makedepend $(DEFS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.

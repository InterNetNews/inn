##  $Id$

include ../../Makefile.global

top	= ../..

SEC1	= convdate.1 fastrm.1 getlist.1 grephistory.1 inews.1 innconfval.1 \
	innfeed.1 innmail.1 nntpget.1 pgpverify.1 pullnews.1 rnews.1 \
	shlock.1 shrinkfile.1 simpleftp.1 sm.1

SEC3	= clientlib.3 dbz.3 inndcomm.3 libauth.3 libinn.3 libinnhist.3 \
	libstorage.3 list.3 qio.3 tst.3 uwildmat.3

SEC5	= active.5 active.times.5 buffindexed.conf.5 control.ctl.5 \
	cycbuff.conf.5 distrib.pats.5 expire.ctl.5 history.5 incoming.conf.5 \
	inn.conf.5 innfeed.conf.5 innwatch.ctl.5 moderators.5 motd.news.5 \
	newsfeeds.5 newsgroups.5 newslog.5 nnrpd.track.5 nntpsend.ctl.5 ovdb.5 \
	overview.fmt.5 passwd.nntp.5 radius.conf.5 readers.conf.5 \
	storage.conf.5 subscriptions.5

SEC8	= actsync.8 archive.8 auth_smb.8 batcher.8 buffchan.8 ckpasswd.8 \
	cnfsheadconf.8 cnfsstat.8 controlchan.8 ctlinnd.8 cvtbatch.8 \
	docheckgroups.8 domain.8 expire.8 expireover.8 expirerm.8 \
	filechan.8 ident.8 \
	innbind.8 inncheck.8 innd.8 inndf.8 innreport.8 innstat.8 \
	innupgrade.8 innwatch.8 innxbatch.8 innxmit.8 mailpost.8 makedbz.8 \
	makehistory.8 mod-active.8 news.daily.8 news2mail.8 ninpaths.8 \
	nnrpd.8 nntpsend.8 ovdb_init.8 ovdb_monitor.8 ovdb_server.8 \
	ovdb_stat.8 overchan.8 perl-nocem.8 prunehistory.8 radius.8 \
	rc.news.8 scanlogs.8 send-nntp.8 send-uucp.8 sendinpaths.8 \
	tally.control.8 tdx-util.8 tinyleaf.8 writelog.8

all:
clobber clean distclean:
tags ctags:
profiled:

install: install-man1 install-man3 install-man5 install-man8

install-man1:
	for M in $(SEC1) ; do \
	    $(CP_MAN) $$M $D$(MAN1)/$$M ; \
	done

install-man3:
	for M in $(SEC3) ; do \
	    $(CP_MAN) $$M $D$(MAN3)/$$M ; \
	done

install-man5:
	for M in $(SEC5) ; do \
	    $(CP_MAN) $$M $D$(MAN5)/$$M ; \
	done

# auth_krb5 is conditionally compiled, so handle it specially.  actsync(8)
# also covers actsyncd.
install-man8:
	for M in $(SEC8) ; do \
	    $(CP_MAN) $$M $D$(MAN8)/$$M ; \
	done
	rm -f $(MAN8)/actsyncd.8
	cd $(MAN8) && $(LN_S) actsync.8 actsyncd.8
	if [ x"$(KRB5_AUTH)" != x ] ; then \
	    $(CP_MAN) auth_krb5.8 $D$(MAN8)/auth_krb5.8 ; \
	fi

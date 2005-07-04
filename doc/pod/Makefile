##  $Id$
##
##  This Makefile contains rules to generate the files derived from POD
##  source.  Normal make commands at the top level of the source tree don't
##  recurse into this directory.  These targets are only used by
##  maintainers.

include ../../Makefile.global

TEXT	= ../../HACKING ../../INSTALL ../../NEWS ../../README ../hook-perl \
	../hook-python ../external-auth ../checklist

MAN1	= ../man/convdate.1 ../man/fastrm.1 ../man/getlist.1 \
	../man/grephistory.1 ../man/inews.1 ../man/innconfval.1 \
	../man/innmail.1 ../man/rnews.1 ../man/simpleftp.1 ../man/sm.1

MAN3	= ../man/libauth.3 ../man/libinnhist.3 ../man/list.3 ../man/qio.3 \
	../man/tst.3 ../man/uwildmat.3

MAN5	= ../man/active.5 ../man/active.times.5 ../man/buffindexed.conf.5 \
	../man/control.ctl.5 ../man/cycbuff.conf.5 ../man/distrib.pats.5 \
	../man/expire.ctl.5 ../man/inn.conf.5 ../man/moderators.5 \
	../man/motd.news.5 ../man/newsfeeds.5 ../man/ovdb.5 \
	../man/passwd.nntp.5 ../man/radius.conf.5 ../man/readers.conf.5 \
	../man/subscriptions.5

MAN8	= ../man/actsync.8 ../man/archive.8 ../man/auth_krb5.8 \
	../man/auth_smb.8 ../man/batcher.8 ../man/buffchan.8 \
	../man/ckpasswd.8 ../man/ctlinnd.8 ../man/domain.8 \
	../man/expireover.8 ../man/ident.8 ../man/innd.8 ../man/inndf.8 \
	../man/nnrpd.8 ../man/innbind.8 ../man/innupgrade.8 \
	../man/mailpost.8 ../man/makehistory.8 ../man/ninpaths.8 \
	../man/ovdb_init.8 ../man/ovdb_monitor.8 ../man/ovdb_server.8 \
	../man/ovdb_stat.8 ../man/overchan.8 ../man/radius.8 \
	../man/rc.news.8 ../man/sendinpaths.8 ../man/tdx-util.8 \
	../man/tinyleaf.8

ALL	= $(TEXT) $(MAN1) $(MAN3) $(MAN5) $(MAN8)

all bootstrap: $(ALL)

clean clobber distclean:

maintclean: distclean
	rm -f $(ALL)

../../HACKING:		hacking.pod		; $(POD2TEXT) $? > $@
../../INSTALL:		install.pod		; $(POD2TEXT) $? > $@
../../NEWS:		news.pod		; $(POD2TEXT) $? > $@
../../README:		readme.pod		; $(POD2TEXT) $? > $@
../checklist:		checklist.pod		; $(POD2TEXT) $? > $@
../external-auth:	external-auth.pod	; $(POD2TEXT) $? > $@
../hook-perl:		hook-perl.pod		; $(POD2TEXT) $? > $@
../hook-python:		hook-python.pod		; $(POD2TEXT) $? > $@

../man/convdate.1:	convdate.pod		; $(POD2MAN) -s 1 $? > $@
../man/fastrm.1:	fastrm.pod		; $(POD2MAN) -s 1 $? > $@
../man/getlist.1:	getlist.pod		; $(POD2MAN) -s 1 $? > $@
../man/grephistory.1:	grephistory.pod		; $(POD2MAN) -s 1 $? > $@
../man/inews.1:		inews.pod		; $(POD2MAN) -s 1 $? > $@
../man/innconfval.1:	innconfval.pod		; $(POD2MAN) -s 1 $? > $@
../man/innmail.1:	innmail.pod		; $(POD2MAN) -s 1 $? > $@
../man/rnews.1:		rnews.pod		; $(POD2MAN) -s 1 $? > $@
../man/simpleftp.1:	simpleftp.pod		; $(POD2MAN) -s 1 $? > $@
../man/sm.1:		sm.pod			; $(POD2MAN) -s 1 $? > $@

../man/libauth.3:	libauth.pod		; $(POD2MAN) -s 3 $? > $@
../man/libinnhist.3:	libinnhist.pod		; $(POD2MAN) -s 3 $? > $@
../man/list.3:		list.pod		; $(POD2MAN) -s 3 $? > $@
../man/qio.3:		qio.pod			; $(POD2MAN) -s 3 $? > $@
../man/tst.3:		tst.pod			; $(POD2MAN) -s 3 $? > $@
../man/uwildmat.3:	uwildmat.pod		; $(POD2MAN) -s 3 $? > $@

../man/active.5:	active.pod		; $(POD2MAN) -s 5 $? > $@
../man/active.times.5:	active.times.pod	; $(POD2MAN) -s 5 $? > $@
../man/buffindexed.conf.5: buffindexed.conf.pod	; $(POD2MAN) -s 5 $? > $@
../man/control.ctl.5:	control.ctl.pod		; $(POD2MAN) -s 5 $? > $@
../man/cycbuff.conf.5:	cycbuff.conf.pod	; $(POD2MAN) -s 5 $? > $@
../man/distrib.pats.5:	distrib.pats.pod	; $(POD2MAN) -s 5 $? > $@
../man/expire.ctl.5:	expire.ctl.pod		; $(POD2MAN) -s 5 $? > $@
../man/inn.conf.5:	inn.conf.pod		; $(POD2MAN) -s 5 $? > $@
../man/moderators.5:	moderators.pod		; $(POD2MAN) -s 5 $? > $@
../man/motd.news.5:     motd.news.pod           ; $(POD2MAN) -s 5 $? > $@
../man/newsfeeds.5:	newsfeeds.pod		; $(POD2MAN) -s 5 $? > $@
../man/ovdb.5:		ovdb.pod		; $(POD2MAN) -s 5 $? > $@
../man/passwd.nntp.5:	passwd.nntp.pod		; $(POD2MAN) -s 5 $? > $@
../man/radius.conf.5:	radius.conf.pod		; $(POD2MAN) -s 5 $? > $@
../man/readers.conf.5:	readers.conf.pod	; $(POD2MAN) -s 5 $? > $@
../man/subscriptions.5:	subscriptions.pod	; $(POD2MAN) -s 5 $? > $@

../man/actsync.8:	actsync.pod		; $(POD2MAN) -s 8 $? > $@
../man/archive.8:	archive.pod		; $(POD2MAN) -s 8 $? > $@
../man/auth_krb5.8:	auth_krb5.pod		; $(POD2MAN) -s 8 $? > $@
../man/auth_smb.8:	auth_smb.pod		; $(POD2MAN) -s 8 $? > $@
../man/batcher.8:	batcher.pod		; $(POD2MAN) -s 8 $? > $@
../man/buffchan.8:	buffchan.pod		; $(POD2MAN) -s 8 $? > $@
../man/ckpasswd.8:	ckpasswd.pod		; $(POD2MAN) -s 8 $? > $@
../man/ctlinnd.8:	ctlinnd.pod		; $(POD2MAN) -s 8 $? > $@
../man/domain.8:	domain.pod		; $(POD2MAN) -s 8 $? > $@
../man/expireover.8:	expireover.pod		; $(POD2MAN) -s 8 $? > $@
../man/ident.8:		ident.pod		; $(POD2MAN) -s 8 $? > $@
../man/innbind.8:	innbind.pod		; $(POD2MAN) -s 8 $? > $@
../man/innd.8:		innd.pod		; $(POD2MAN) -s 8 $? > $@
../man/inndf.8:		inndf.pod		; $(POD2MAN) -s 8 $? > $@
../man/innupgrade.8:	innupgrade.pod		; $(POD2MAN) -s 8 $? > $@
../man/mailpost.8:	mailpost.pod		; $(POD2MAN) -s 8 $? > $@
../man/makehistory.8:	makehistory.pod		; $(POD2MAN) -s 8 $? > $@
../man/ninpaths.8:	ninpaths.pod		; $(POD2MAN) -s 8 $? > $@
../man/nnrpd.8:		nnrpd.pod		; $(POD2MAN) -s 8 $? > $@
../man/ovdb_init.8:	ovdb_init.pod		; $(POD2MAN) -s 8 $? > $@
../man/ovdb_monitor.8:	ovdb_monitor.pod	; $(POD2MAN) -s 8 $? > $@
../man/ovdb_server.8:	ovdb_server.pod		; $(POD2MAN) -s 8 $? > $@
../man/ovdb_stat.8:	ovdb_stat.pod		; $(POD2MAN) -s 8 $? > $@
../man/overchan.8:	overchan.pod		; $(POD2MAN) -s 8 $? > $@
../man/radius.8:	radius.pod		; $(POD2MAN) -s 8 $? > $@
../man/rc.news.8:	rc.news.pod		; $(POD2MAN) -s 8 $? > $@
../man/sendinpaths.8:	sendinpaths.pod		; $(POD2MAN) -s 8 $? > $@
../man/tdx-util.8:	tdx-util.pod		; $(POD2MAN) -s 8 $? > $@
../man/tinyleaf.8:	tinyleaf.pod		; $(POD2MAN) -s 8 $? > $@

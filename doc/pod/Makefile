##  $Id$
##
##  This Makefile contains rules to generate the files derived from POD
##  source.  Normal make commands at the top level of the source tree don't
##  recurse into this directory.  These targets are only used by
##  maintainers.

include ../../Makefile.global

TEXT	= ../../HACKING ../../INSTALL ../../NEWS ../../README ../hook-perl \
	../hook-python

MAN1	= ../man/ckpasswd.1 ../man/convdate.1 ../man/fastrm.1 \
	../man/innmail.1

MAN3	= ../man/qio.3 ../man/wildmat.3 ../man/libinnhist.3

MAN5	= ../man/inn.conf.5 ../man/newsfeeds.5 ../man/ovdb.5 \
	../man/passwd.nntp.5 ../man/readers.conf.5 ../man/sasl.conf.5 \
	../man/subscriptions.5

MAN8	= ../man/expireover.8 ../man/innd.8 ../man/inndf.8 \
	../man/inndstart.8 ../man/makehistory.8 ../man/ovdb_init.8 \
	../man/ovdb_monitor.8 ../man/ovdb_server.8 ../man/ovdb_stat.8

all: $(TEXT) $(MAN1) $(MAN3) $(MAN5) $(MAN8)

../../HACKING:		hacking.pod		; $(POD2TEXT) $? > $@
../../INSTALL:		install.pod		; $(POD2TEXT) $? > $@
../../NEWS:		news.pod		; $(POD2TEXT) $? > $@
../../README:		readme.pod		; $(POD2TEXT) $? > $@
../hook-perl:		hook-perl.pod		; $(POD2TEXT) $? > $@
../hook-python:		hook-python.pod		; $(POD2TEXT) $? > $@

../man/ckpasswd.1:	ckpasswd.pod		; $(POD2MAN) -s 1 $? > $@
../man/convdate.1:	convdate.pod		; $(POD2MAN) -s 1 $? > $@
../man/fastrm.1:	fastrm.pod		; $(POD2MAN) -s 1 $? > $@
../man/innmail.1:	innmail.pod		; $(POD2MAN) -s 1 $? > $@

../man/qio.3:		qio.pod			; $(POD2MAN) -s 3 $? > $@
../man/wildmat.3:	wildmat.pod		; $(POD2MAN) -s 3 $? > $@
../man/libinnhist.3:	libinnhist.pod		; $(POD2MAN) -s 3 $? > $@

../man/inn.conf.5:	inn.conf.pod		; $(POD2MAN) -s 5 $? > $@
../man/newsfeeds.5:	newsfeeds.pod		; $(POD2MAN) -s 5 $? > $@
../man/ovdb.5:		ovdb.pod		; $(POD2MAN) -s 5 $? > $@
../man/passwd.nntp.5:	passwd.nntp.pod		; $(POD2MAN) -s 5 $? > $@
../man/readers.conf.5:	readers.conf.pod	; $(POD2MAN) -s 5 $? > $@
../man/sasl.conf.5:	sasl.conf.pod		; $(POD2MAN) -s 5 $? > $@
../man/subscriptions.5:	subscriptions.pod	; $(POD2MAN) -s 5 $? > $@

../man/expireover.8:	expireover.pod		; $(POD2MAN) -s 8 $? > $@
../man/innd.8:		innd.pod		; $(POD2MAN) -s 8 $? > $@
../man/inndf.8:		inndf.pod		; $(POD2MAN) -s 8 $? > $@
../man/inndstart.8:	inndstart.pod		; $(POD2MAN) -s 8 $? > $@
../man/makehistory.8:	makehistory.pod		; $(POD2MAN) -s 8 $? > $@
../man/ovdb_init.8:	ovdb_init.pod		; $(POD2MAN) -s 8 $? > $@
../man/ovdb_monitor.8:	ovdb_monitor.pod	; $(POD2MAN) -s 8 $? > $@
../man/ovdb_server.8:	ovdb_server.pod		; $(POD2MAN) -s 8 $? > $@
../man/ovdb_stat.8:	ovdb_stat.pod		; $(POD2MAN) -s 8 $? > $@

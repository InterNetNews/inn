##  $Revision$

include Makefile.global

CFLAGS = $(GCFLAGS)

RELEASE=2
PATCHLEVEL=2.3
VERSION=$(RELEASE).$(PATCHLEVEL)

#TARDIR=inn
#TARFILE=inn.tar
TARDIR=inn-$(VERSION)
TARFILE=inn-$(VERSION).tar
SQUASH=gzip

RCSCOFLAGS	= -u

##  The first two directories must be config and lib.
PROGS	= config lib storage frontends innd nnrpd backends expire doc innfeed
DIRS	= $(PROGS) site

##  We invoke an extra process and set this to be what to make.
WHAT_TO_MAKE	= all

##  Delete the first two lines and all lines that contain (Directory).
##  Print only the first field of all other lines.  This gets us just
##  the list of files from the MANIFEST.
SEDCOMMANDS = -e 1,2d -e '/(Directory)/d' -e 's/ .*//'
SEDDIRCMDS = -e '1,2d' -e '/(Directory)/!d' -e 's/ .*//' -e 's;^;$(TARDIR)/;'

##  Major target -- build everything.
all:
	$(MAKE) $(FLAGS) WHAT_TO_MAKE=all DESTDIR=$(DESTDIR) common

##  Install everything.
install:	directories
	$(MAKE) $(FLAGS) WHAT_TO_MAKE=install DESTDIR=$(DESTDIR) common
	@echo "" ; echo Do not forget to update your cron entries.
	@echo Also run makehistory if you have to.
	@echo Create/obtain an active file and run 'makehistory -o' if
	@echo this is a first time install

##  Directories where files get put.
directories:
	chmod +x ./makedirs.sh
	DESTDIR=$(DESTDIR) ./makedirs.sh;

##  Other generic targets.
depend tags ctags profiled:
	@$(MAKE) $(FLAGS) WHAT_TO_MAKE=$@ common

etags:
	etags */*.c */*.h

clean:
	@$(MAKE) $(FLAGS) WHAT_TO_MAKE=$@ common
	rm -f *~ libinn_p.a llib-linn.ln FILELIST config.log

##  Common target.
common:
	@for D in $(DIRS) ; do \
	    echo "" ; \
	    echo "cd $$D ; $(MAKE) $(FLAGS) DESTDIR=$(DESTDIR) $(WHAT_TO_MAKE) ; cd .." ; \
	    cd $$D; $(MAKE) $(FLAGS) DESTDIR=$(DESTDIR) $(WHAT_TO_MAKE) || exit 1 ; cd .. ; \
	done

##  Software update -- install just the programs and documentation.
update:
	@for D in $(PROGS) ; do \
	    echo "" ; \
	    echo "cd $$D ; $(MAKE) $(FLAGS) DESTDIR=$(DESTDIR) install ; cd .." ; \
	    cd $$D; $(MAKE) $(FLAGS) DESTDIR=$(DESTDIR) install || exit 1 ; cd .. ; \
	done

##  Build subst (for configuration).
subst c sh quiet sedtest:
	cd config ; $(MAKE) $(FLAGS) $@ ; cd ..

##  Build installation document.
Install.ms:	Install.ms.1 Install.ms.2
	@rm -f Install.ms
	cat Install.ms.1 Install.ms.2 >Install.ms
	chmod 444 Install.ms

##  Additional cleanups.
clobber realclean distclean:	clean
	@echo ""
	rm -f Install.ms inn*.tar.Z inn*.tar.gz Part0? MANIFEST.BAK
	rm -rf inews.* rnews.* nntplib.*
	rm -f tags */tags core */core a.out */a.out foo */foo
	rm -f CHANGES *~
	rm -fr $(TARDIR)
	rm -f config.cache config.log config.status libtool
	rm -f BUILD makedirs.sh config/config.data backends/actmerge.sh
	rm -f backends/actsyncd.sh backends/sendxbatches.sh
	rm -f frontends/c7unbatch.sh frontends/gunbatch.sh
	rm -f include/autoconfig.h include/clibrary.h include/config.h
	rm -f include/paths.h innfeed/innfeed-convcfg innfeed/procbatch
	rm -f samples/actsync.cfg samples/checkgroups samples/checkgroups.pl
	rm -f samples/cnfsstat samples/controlbatch samples/controlchan
	rm -f samples/default samples/docheckgroups samples/expirerm
	rm -f samples/ihave samples/ihave.pl samples/inn.conf samples/inncheck
	rm -f samples/innmail samples/innreport samples/innreport.conf
	rm -f samples/innshellvars samples/innshellvars.csh
	rm -f samples/innshellvars.pl samples/innshellvars.tcl
	rm -f samples/innstat samples/innwatch samples/innwatch.ctl
	rm -f samples/mailpost samples/mod-active samples/news.daily
	rm -f samples/news2mail samples/newgroup samples/newgroup.pl
	rm -f samples/nnrpd_auth.pl samples/nntpsend samples/parsecontrol
	rm -f samples/pgpverify samples/pullnews samples/rc.news
	rm -f samples/rmgroup samples/rmgroup.pl samples/scanlogs
	rm -f samples/scanspool samples/send-ihave samples/send-nntp
	rm -f samples/send-uucp samples/sendbatch samples/sendme
	rm -f samples/sendme.pl samples/sendsys samples/sendsys.pl
	rm -f samples/senduuname samples/senduuname.pl samples/signcontrol
	rm -f samples/simpleftp samples/startup.tcl samples/tally.control
	rm -f samples/version samples/version.pl samples/writelog
	rm -f site/config storage/buildconfig syslog/syslog.conf
	@echo ""
	cd site ; make clobber ; cd ..
	rm -f Makefile.global 

##  Update syslog.
syslogfix:
	rm -f include/syslog.h lib/syslog.c
	cp syslog/syslog.h include
	cp syslog/syslog.c lib
	cp syslog/syslog.3 doc
	-cd syslog; $(CC) -I../include -o syslogd syslogd.c ; cd ..
	@echo "Install syslogd and syslog.conf as appropriate"

##  Configure and compile
world:		Install.ms
	cd config ; $(MAKE) $(FLAGS) subst quiet ; cd ..
	cd lib ; $(MAKE) $(FLAGS) ; cd ..

##  Make a distribution.
#shar:
#	makekit -m -k40 -s70k

release: include/innversion.h samples/version tar

include/innversion.h::
	sed -e 's/^\(#define RELEASE\).*/\1 "$(RELEASE)"/' \
	    -e 's/^\(#define PATCHLEVEL\).*/\1 "$(PATCHLEVEL)"/' \
	    -e 's/^\(#define DATE\).*/\1 "'"`date '+%d-%b-%Y'`"'"/' \
		$@ > $@.new ;\
	mv $@.new $@

samples/version::
	sed -e 's/^\(VERSION="\).*/\1INN $(RELEASE).$(PATCHLEVEL)"/' \
		$@ > $@.new ;
	mv $@.new $@

tardir:
	rm -f inn*.tar.Z inn*.tar.gz
	rm -fr $(TARDIR)
	mkdir $(TARDIR)
	set -x ; for i in `sed $(SEDDIRCMDS) < MANIFEST`; do mkdir $$i;done

tar:	tardir
	for i in `sed $(SEDCOMMANDS) <MANIFEST`;do \
		cp $$i $(TARDIR)/$$i; \
	done
	find $(TARDIR) -type f -print | xargs touch -t `date +%m%d%H%M.%S`
	tar cf $(TARFILE) $(TARDIR)
	$(SQUASH) $(TARFILE)

FAQ: FORCE

CHANGES: FORCE
	-for i in ChangeLog */ChangeLog;do\
		[ -f $$i -a "X$$i" != Xsite/ChangeLog ] && {\
			echo "==================================";\
			echo "From $$i" ; echo "" ;\
			cat $$i ; echo "" ; echo "" ;\
		}\
	done > CHANGES ; exit 0

##  Local convention; for xargs.
list:	FORCE
	@sed $(SEDCOMMANDS) <MANIFEST >FILELIST

FORCE:

# DO NOT DELETE THIS LINE -- make depend depends on it.

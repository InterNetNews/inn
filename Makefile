##  $Revision$

include Makefile.global

CFLAGS = $(GCFLAGS)

RELEASE=2
PATCHLEVEL=2
VERSION=$(RELEASE).$(PATCHLEVEL)

#TARDIR=inn
#TARFILE=inn.tar
TARDIR=inn-$(VERSION)
TARFILE=inn-$(VERSION).tar
SQUASH=gzip

RCSCOFLAGS	= -u

##  The first two directories must be config and lib.
PROGS	= config lib storage frontends innd nnrpd backends expire doc doc/man doc/sgml innfeed authprogs
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
	rm -f BUILD makedirs.sh config/config.data config/config.data.in
	rm -f backends/actmerge.sh backends/actsyncd.sh
	rm -f backends/sendxbatches.sh frontends/c7unbatch.sh
	rm -f frontends/gunbatch.sh includes/autoconfig.h include/clibrary.h
	rm -f include/config.h include/paths.h innfeed/innfeed-convcfg
	rm -f innfeed/procbatch samples/actsync.cfg samples/checkgroups
	rm -f samples/checkgroups.pl samples/cnfsheadconf samples/cnfsstat
	rm -f samples/controlbatch samples/controlchan samples/default
	rm -f samples/docheckgroups samples/expirerm samples/ihave
	rm -f samples/ihave.pl samples/inn.conf samples/inncheck
	rm -f samples/innmail samples/innreport samples/innreport.conf
	rm -f samples/innshellvars samples/innshellvars.csh
	rm -f samples/innshellvars.pl samples/innshellvars.tcl
	rm -f samples/innstat samples/innwatch samples/innwatch.ctl
	rm -f samples/mailpost samples/mod-active samples/news.daily
	rm -f samples/news2mail samples/newgroup samples/newgroup.pl
	rm -f samples/newsfeeds samples/nnrpd_auth.pl samples/nntpsend
	rm -f samples/parsecontrol samples/pgpverify samples/pullnews
	rm -f samples/rc.news samples/rmgroup samples/rmgroup.pl
	rm -f samples/scanlogs samples/scanspool samples/send-ihave
	rm -f samples/send-nntp samples/send-uucp samples/sendbatch
	rm -f samples/sendme samples/sendme.pl samples/sendsys
	rm -f samples/sendsys.pl samples/senduuname samples/senduuname.pl
	rm -f samples/signcontrol samples/simpleftp samples/startup.tcl
	rm -f samples/tally.control samples/version samples/version.pl
	rm -f samples/writelog site/config storage/buildconfig
	@echo ""
	cd site ; make clobber ; cd ..
	rm -f Makefile.global 

##  Configure and compile
world:		Install.ms
	cd config ; $(MAKE) $(FLAGS) subst quiet ; cd ..
	cd lib ; $(MAKE) $(FLAGS) ; cd ..

##  Make a distribution.
#shar:
#	makekit -m -k40 -s70k

release: include/patchlevel.h samples/version tar

include/patchlevel.h: Makefile
	-set -x ; [ -f $@ ] || co -u $@ ; \
	sed -e 's/^\(#define RELEASE\).*/\1 "$(RELEASE)"/' \
	    -e 's/^\(#define PATCHLEVEL\).*/\1 "$(PATCHLEVEL)"/' \
	    -e 's/^\(#define DATE\).*/\1 "'"`date '+%d-%b-%Y'`"'"/' \
		$@ > $@.new ;\
	if diff -u $@ $@.new > PATCH.$$$$ 2>&1;then \
		: ;\
	elif rcsdiff $@ > /dev/null 2>&1; then \
		rcsclean -u $@ ;\
		co -l $@ ;\
		patch < PATCH.$$$$ ;\
		ci -u -m'new release' $@ ;\
	else \
		ci -l -m'unknown snapshot' $@ ;\
		patch < PATCH.$$$$ ;\
		ci -u -m'new release' $@ ;\
	fi ;\
	rm -f PATCH.$$$$


samples/version: Makefile
	-set -x ; rcsclean -u $@ ;\
	co -l $@ ;\
	sed -e 's/^\(VERSION="\).*/\1INN $(RELEASE).$(PATCHLEVEL)"/' \
		$@ > $@.new ;\
	if cmp $@ $@.new > /dev/null 2>&1; then \
		rm -f $@.new ;\
		rcsdiff $@ > /dev/null 2>&1 && rcs -u $@ ;\
	else \
		mv $@.new $@ ;\
		ci -u -m'new version $(RELEASE).$(PATCHLEVEL)' $@ ;\
	fi


tardir: MANIFEST CHANGES
	rm -f inn*.tar.Z inn*.tar.gz
	rm -fr $(TARDIR)
	mkdir $(TARDIR)
	set -x ; for i in `sed $(SEDDIRCMDS) < MANIFEST`; do mkdir $$i;done

tar:	tardir
	for i in `sed $(SEDCOMMANDS) <MANIFEST`;do \
		[ -f $$i ] || co $$i ; cp $$i $(TARDIR)/$$i;done
	find $(TARDIR) -type f -print | xargs touch -t `date +%m%d%H%M.%S`
	tar cf $(TARFILE) $(TARDIR)
	$(SQUASH) $(TARFILE)

FAQ: FORCE
	-cd FAQ && co -q RCS/*

rcsclean: FORCE
	-for i in . *;do\
		if [ -d $$i -a -d $$i/RCS ]; then\
			echo "RCS Cleaning $$i";\
			(cd $$i && rcsclean -q -u);\
		fi;\
	done

rcscoall: FORCE
	-for i in . *;do\
		if [ -d $$i -a -d $$i/RCS ]; then\
			echo "Checking out in $$i";\
			(cd $$i && co -q $(RCSCOFLAGS) RCS/*);\
		fi;\
	done

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

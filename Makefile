##  $Revision$

include Makefile.global

CFLAGS = $(GCFLAGS)

RELEASE=2
PATCHLEVEL=3
VERSION=$(RELEASE).$(PATCHLEVEL)

#TARDIR=inn
#TARFILE=inn.tar
TARDIR=inn-$(VERSION)
TARFILE=inn-$(VERSION).tar
SQUASH=gzip

RCSCOFLAGS	= -u

##  The first two directories must be lib and storage.
PROGS = lib storage innd nnrpd innfeed expire frontends backends authprogs \
	scripts doc
DIRS  = $(PROGS) site

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

##  Additional cleanups.
clobber realclean distclean:	clean
	@echo ""
	rm -f inn*.tar.Z inn*.tar.gz Part0? MANIFEST.BAK
	rm -rf inews.* rnews.* nntplib.*
	rm -f tags */tags core */core a.out */a.out foo */foo
	rm -f CHANGES *~
	rm -fr $(TARDIR)
	rm -f config.cache config.log config.status libtool
	rm -f BUILD makedirs.sh
	rm -f backends/actmerge backends/actsyncd backends/controlbatch
	rm -f backends/controlchan backends/mod-active backends/news2mail
	rm -f backends/nntpsend backends/pgpverify backends/send-ihave
	rm -f backends/send-nntp backends/send-uucp backends/sendbatch
	rm -f backends/sendxbatches expire/expirerm frontends/c7unbatch
	rm -f frontends/cnfsheadconf frontends/cnfsstat frontends/gunbatch
	rm -f frontends/mailpost frontends/pullnews frontends/scanspool
	rm -f frontends/signcontrol include/autoconfig.h include/config.h
	rm -f include/paths.h innfeed/innfeed-convcfg innfeed/procbatch
	rm -f samples/actsync.cfg samples/checkgroups samples/checkgroups.pl
	rm -f samples/default samples/docheckgroups samples/ihave
	rm -f samples/ihave.pl samples/inn.conf samples/innreport.conf
	rm -f samples/innwatch.ctl samples/newgroup samples/newgroup.pl
	rm -f samples/newsfeeds samples/nnrpd_auth.pl samples/parsecontrol
	rm -f samples/rmgroup samples/rmgroup.pl samples/sendme
	rm -f samples/sendme.pl samples/sendsys samples/sendsys.pl
	rm -f samples/senduuname samples/senduuname.pl samples/startup.tcl
	rm -f samples/version samples/version.pl scripts/inncheck
	rm -f scripts/innmail scripts/innreport scripts/innshellvars
	rm -f scripts/innshellvars.pl scripts/innshellvars.tcl
	rm -f scripts/innstat scripts/innwatch scripts/news.daily
	rm -f scripts/rc.news scripts/scanlogs scripts/simpleftp
	rm -f scripts/tally.control scripts/writelog site/config
	rm -f storage/buildconfig
	@echo ""
	cd site ; make clobber ; cd ..
	rm -f Makefile.global 

##  Configure and compile
world:
	cd lib ; $(MAKE) $(FLAGS) ; cd ..

##  Make a distribution.
#shar:
#	makekit -m -k40 -s70k

release: include/innversion.h samples/version tar

include/innversion.h: Makefile
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

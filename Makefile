##  $Id$

include Makefile.global

RELEASE=2
PATCHLEVEL=3
VERSION=$(RELEASE).$(PATCHLEVEL)

##  All installation directories except for $(PATHRUN), which has a
##  different mode than the rest.
INSTDIRS      = $(PATHNEWS) $(PATHBIN) $(PATHAUTH) $(PATHAUTHRESOLV) \
		$(PATHAUTHPASSWD) $(PATHCONTROL) $(PATHFILTER) \
		$(PATHRNEWS) $(PATHDB) $(PATHETC) $(PATHLIB) $(PATHMAN) \
		$(MAN1) $(MAN3) $(MAN5) $(MAN8) $(PATHSPOOL) $(PATHTMP) \
		$(PATHARCHIVE) $(PATHARTICLES) $(PATHINCOMING) \
		$(PATHINBAD) $(PATHTAPE) $(PATHOVERVIEW) $(PATHOUTGOING) \
		$(PATHLOG) $(PATHLOG)/OLD

TARDIR=inn-$(VERSION)
TARFILE=inn-$(VERSION).tar
SQUASH=gzip

LIBDIRS    = lib storage
PROGDIRS   = innd nnrpd innfeed expire frontends backends authprogs scripts
UPDATEDIRS = $(LIBDIRS) $(PROGDIRS) doc
ALLDIRS    = $(UPDATEDIRS) samples site

##  Delete the first two lines and all lines that contain (Directory).
##  Print only the first field of all other lines.  This gets us just
##  the list of files from the MANIFEST.
SEDCOMMANDS = -e 1,2d -e '/(Directory)/d' -e 's/ .*//'
SEDDIRCMDS = -e '1,2d' -e '/(Directory)/!d' -e 's/ .*//' -e 's;^;$(TARDIR)/;'


##  Major target -- build everything.  Rather than just looping through
##  all the directories, use a set of parallel rules so that make -j can
##  work on more than one directory at a time.
all: all-libraries all-programs
	cd doc     && $(MAKE) all
	cd samples && $(MAKE) all
	cd site    && $(MAKE) all

all-libraries:	all-lib all-storage
all-lib:			; cd lib       && $(MAKE) all
all-storage:			; cd storage   && $(MAKE) all

all-programs:	all-innd all-nnrpd all-innfeed all-expire all-frontends \
		all-backends all-authprogs all-scripts

all-authprogs:	all-lib		; cd authprogs && $(MAKE) all
all-backends:	all-libraries	; cd backends  && $(MAKE) all
all-expire:	all-libraries	; cd expire    && $(MAKE) all
all-frontends:	all-libraries	; cd frontends && $(MAKE) all
all-innd:	all-libraries	; cd innd      && $(MAKE) all
all-innfeed:	all-libraries	; cd innfeed   && $(MAKE) all
all-nnrpd:	all-libraries	; cd nnrpd     && $(MAKE) all
all-scripts:			; cd scripts   && $(MAKE) all


##  Installation rules.  make install installs everything; make update only
##  updates the binaries, scripts, and documentation and leaves
##  configuration files alone.
install: directories
	@for D in $(ALLDIRS) ; do \
	    cd $$D && $(MAKE) install || exit 1 ; cd .. ; \
	done
	@echo ''
	@echo Do not forget to update your cron entries, and also run
	@echo makehistory if you need to.  Create/obtain an active file and
	@echo run 'makehistory -o' if this is a first-time installation.
	@echo ''

directories:
	@chmod +x support/install-sh
	for D in $(INSTDIRS) ; do \
	    support/install-sh $(OWNER) -m 0755 -d $$D ; \
	done
	support/install-sh $(OWNER) -m 0750 -d $(PATHRUN)

update: 
	@chmod +x support/install-sh
	@for D in $(UPDATEDIRS) ; do \
	    echo '' ; \
	    cd $$D && $(MAKE) install || exit 1 ; cd .. ; \
	done

## for starttls
cert:
	$(SSLBIN)/openssl req -new -x509 -nodes \
	-out $(PATHLIB)/cert.pem -days 366 \
	-keyout $(PATHLIB)/cert.pem
	chown news:news $(PATHLIB)/cert.pem
	chmod 640 $(PATHLIB)/cert.pem

##  Cleanup targets.  clean deletes all compilation results but leaves the
##  configure results.  distclean or clobber removes everything not part of
##  the distribution tarball.
clean:
	@for D in $(ALLDIRS) ; do \
	    echo '' ; \
	    cd $$D && $(MAKE) clean || exit 1 ; cd .. ; \
	done
	@echo ''
	rm -f config.log FILELIST

clobber realclean distclean:
	@for D in $(ALLDIRS) ; do \
	    echo '' ; \
	    cd $$D && $(MAKE) $(FLAGS) clobber && cd ..; \
	done
	@echo ''
	rm -rf inews.* rnews.* $(TARDIR)
	rm -f inn*.tar.gz CHANGES MANIFEST.BAK tags core
	rm -f config.cache config.log config.status libtool makedirs.sh
	rm -f include/autoconfig.h include/config.h include/paths.h
	rm -f support/fixscript Makefile.global


##  Other generic targets.
depend tags ctags profiled:
	@for D in $(ALLDIRS) ; do \
	    echo '' ; \
	    cd $$D && $(MAKE) $@ || exit 1 ; cd .. ; \
	done

TAGS etags:
	etags */*.c */*.h */*/*.c */*/*.h


##  If someone tries to run make before running configure, tell them to run
##  configure first.
Makefile.global:
	@echo Run ./configure before running make.  See INSTALL for details.
	@exit 1


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
	rm -rf $(TARDIR)
	rm -f inn*.tar.gz
	mkdir $(TARDIR)
	set -x ; for i in `sed $(SEDDIRCMDS) < MANIFEST` ; do mkdir $$i ; done

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

##  $Id$

include Makefile.global

##  All installation directories except for $(PATHRUN), which has a
##  different mode than the rest.
INSTDIRS      = $(PATHNEWS) $(PATHBIN) $(PATHAUTH) $(PATHAUTHRESOLV) \
		$(PATHAUTHPASSWD) $(PATHCONTROL) $(PATHFILTER) \
		$(PATHRNEWS) $(PATHDB) $(PATHDOC) $(PATHETC) $(PATHHTTP) $(PATHLIB) \
		$(PATHLIBPERL) $(PATHLIBPERL)/INN $(PATHDATASHARE) \
		$(PATHMAN) $(MAN1) $(MAN3) $(MAN3PM) $(MAN5) $(MAN8) $(PATHSPOOL) \
		$(PATHTMP) $(PATHARCHIVE) $(PATHARTICLES) $(PATHINCOMING) \
		$(PATHINBAD) $(PATHTAPE) $(PATHOVERVIEW) $(PATHOUTGOING) \
		$(PATHLOG) $(PATHLOGOLD) $(PATHINCLUDE)

##  LIBDIRS are built before PROGDIRS, make update runs in all UPDATEDIRS,
##  and make install runs in all ALLDIRS.  Nothing runs in test except the
##  test target itself and the clean targets.  Currently, include is built
##  before anything else but nothing else runs in it except clean targets.
LIBDIRS     = include lib storage history perl
PROGDIRS    = innd nnrpd innfeed control expire frontends backends authprogs \
              scripts
UPDATEDIRS  = $(LIBDIRS) $(PROGDIRS) doc
ALLDIRS     = $(UPDATEDIRS) samples site
CLEANDIRS   = $(ALLDIRS) tests

##  The directory name and tar file to use when building a release.
TARDIR      = inn-$(VERSION)$(RELEASENUMBER)
TARFILE     = $(TARDIR).tar

##  The directory to use when building a snapshot.
SNAPDIR     = inn-$(SNAPSHOT)-$(SNAPDATE)

##  DISTDIRS gets all directories from the MANIFEST, SNAPDIRS gets the same
##  but for a snapshot, and DISTFILES gets all regular files.  Anything not
##  listed in the MANIFEST will not be included in a distribution.  These are
##  arguments to sed.
DISTDIRS    = -e 1,2d -e '/(Directory)/!d' -e 's/ .*//' -e 's;^;$(TARDIR)/;'
SNAPDIRS    = -e 1,2d -e '/(Directory)/!d' -e 's/ .*//' -e 's;^;$(SNAPDIR)/;'
DISTFILES   = -e 1,2d -e '/(Directory)/d' -e 's/ .*//'


##  Major target -- build everything.  Rather than just looping through
##  all the directories, use a set of parallel rules so that make -j can
##  work on more than one directory at a time.
##  Be careful of a non-GNU make:  after a completed command, it does not
##  necessarily return the script back to the starting directory.
all: all-include all-libraries all-programs
	cd doc     && $(MAKE) all || exit 1 ; cd ..
	cd samples && $(MAKE) all || exit 1 ; cd ..
	cd site    && $(MAKE) all || exit 1 ; cd ..

all-include:			; cd include   && $(MAKE) all

all-libraries:	all-lib all-storage all-history all-perl

all-lib:	all-include	; cd lib       && $(MAKE) all
all-storage:	all-lib		; cd storage   && $(MAKE) library
all-history:	all-storage	; cd history   && $(MAKE) all
all-perl:	all-history	; cd perl      && $(MAKE) all

all-programs:	all-innd all-nnrpd all-innfeed all-control all-expire \
		all-frontends all-backends all-authprogs all-scripts \
		all-store-util

all-authprogs:	all-lib		; cd authprogs && $(MAKE) all
all-backends:	all-libraries	; cd backends  && $(MAKE) all
all-control:			; cd control   && $(MAKE) all
all-expire:	all-libraries	; cd expire    && $(MAKE) all
all-frontends:	all-libraries	; cd frontends && $(MAKE) all
all-innd:	all-libraries	; cd innd      && $(MAKE) all
all-innfeed:	all-libraries	; cd innfeed   && $(MAKE) all
all-nnrpd:	all-libraries	; cd nnrpd     && $(MAKE) all
all-scripts:			; cd scripts   && $(MAKE) all
all-store-util:	all-libraries	; cd storage   && $(MAKE) programs


##  If someone tries to run make before running configure, tell them to run
##  configure first.
Makefile.global:
	@echo 'Run ./configure before running make.  See INSTALL for details.'
	@exit 1


##  Installation rules.  make install installs everything; make update only
##  updates the binaries, scripts, and documentation and leaves config
##  files alone.  make install-root does only the installation that needs
##  to be done as root and can be run after doing the regular install as the
##  news user if the installation directory is news-writable.
install: directories
	@for D in $(ALLDIRS) ; do \
	    echo '' ; \
	    cd $$D && $(MAKE) install || exit 1 ; cd .. ; \
	done
	@echo ''
	@echo 'If this is a first-time installation, a minimal active file and'
	@echo 'history database have been installed.  Do not forget to update'
	@echo 'your cron entries and configure INN.  See INSTALL for more'
	@echo 'information.'
	@echo ''

directories:
	@chmod +x support/install-sh
	for D in $(INSTDIRS) ; do \
	    support/install-sh $(OWNER) -m 0755 -d $(D)$$D ; \
	done
	support/install-sh $(OWNER) -m 0750 -d $(D)$(PATHRUN)

update: 
	@chmod +x support/install-sh
	@for D in $(UPDATEDIRS) ; do \
	    echo '' ; \
	    cd $$D && $(MAKE) install || exit 1 ; cd .. ; \
	done
	test -z "$D" && $(PERL) -Tw $(PATHBIN)/innupgrade $(PATHAUTHPASSWD)
	test -z "$D" && $(PERL) -Tw $(PATHBIN)/innupgrade $(PATHBIN)
	test -z "$D" && $(PERL) -Tw $(PATHBIN)/innupgrade $(PATHDOC)
	test -z "$D" && $(PERL) -Tw $(PATHBIN)/innupgrade $(PATHETC)
	test -z "$D" && $(PERL) -Tw $(PATHBIN)/innupgrade ${PATHFILTER}
	test -z "$D" && $(PERL) -Tw $(PATHBIN)/innupgrade $(MAN1)
	test -z "$D" && $(PERL) -Tw $(PATHBIN)/innupgrade $(MAN3)
	test -z "$D" && $(PERL) -Tw $(PATHBIN)/innupgrade $(MAN5)
	test -z "$D" && $(PERL) -Tw $(PATHBIN)/innupgrade $(MAN8)

install-root:
	@chmod +x support/install-sh
	support/install-sh $(OWNER) -m 0755 -d $(D)$(PATHBIN)
	cd backends && $(MAKE) install-root || exit 1 ; cd ..

##  Install a certificate for TLS/SSL support.
cert:
	umask 077 ; \
	$(SSLBIN) req -new -x509 -nodes \
	    -out $(D)$(PATHETC)/cert.pem -days 366 \
	    -keyout $(D)$(PATHETC)/key.pem
	chown $(RUNASUSER) $(D)$(PATHETC)/cert.pem
	chgrp $(RUNASGROUP) $(D)$(PATHETC)/cert.pem
	chmod 640 $(D)$(PATHETC)/cert.pem
	chown $(RUNASUSER) $(D)$(PATHETC)/key.pem
	chgrp $(RUNASGROUP) $(D)$(PATHETC)/key.pem
	chmod 600 $(D)$(PATHETC)/key.pem


##  Cleanup targets.  clean deletes all compilation results but leaves the
##  configure results.  distclean or clobber removes everything not part of
##  the distribution tarball.  maintclean removes some additional files
##  created as part of the release process.
clean:
	@for D in $(CLEANDIRS) ; do \
	    echo '' ; \
	    cd $$D && $(MAKE) clean || exit 1 ; cd .. ; \
	done

clobber realclean distclean:
	@for D in $(CLEANDIRS) ; do \
	    echo '' ; \
	    cd $$D && $(MAKE) $(FLAGS) distclean || exit 1 ; cd .. ; \
	done
	@echo ''
	rm -f LIST.* Makefile.global TAGS tags config.cache config.log
	rm -f config.status libtool support/fixconfig support/fixscript
	rm -f config.status.lineno configure.lineno

# The removal list is unfortunately duplicated here, both to avoid doing work
# twice and because we can't just depend on distclean since it removes
# Makefile.global and then nothing works right.
maintclean:
	@for D in $(CLEANDIRS) ; do \
	    echo '' ; \
	    cd $$D && $(MAKE) $(FLAGS) maintclean || exit 1 ; cd .. ; \
	done
	@echo ''
	rm -f LIST.* Makefile.global TAGS tags config.cache config.log
	rm -f config.status libtool support/fixconfig support/fixscript
	rm -f config.status.lineno configure.lineno
	rm -f CHANGES ChangeLog inn*.tar.gz configure include/config.h.in
	rm -rf $(TARDIR)

##  Other generic targets.
bootstrap depend tags ctags profiled:
	@for D in $(ALLDIRS) ; do \
	    echo '' ; \
	    cd $$D && $(MAKE) $@ || exit 1 ; cd .. ; \
	done

TAGS etags:
	etags */*.c */*.h */*/*.c */*/*.h


##  Run the test suite.
check test tests: all
	cd tests && $(MAKE) test || exit 1 ; cd ..


##  For maintainers, build the entire source base with warnings enabled.
warnings:
	$(MAKE) COPT="$(WARNINGS) $(COPT)" all


##  Make a release.  We create a release by recreating the directory
##  structure and then copying over all files listed in the MANIFEST.  If it
##  isn't in the MANIFEST, it doesn't go into the release.  We also update
##  the version information in Makefile.global.in to remove the prerelease
##  designation and update all timestamps to the date the release is made.
##  If RELEASENUMBER is set, it is a beta release or a release candidate.
release: ChangeLog
	rm -rf $(TARDIR)
	rm -f inn*.tar.gz
	mkdir $(TARDIR)
	for d in `$(SED) $(DISTDIRS) MANIFEST` ; do $(MKDIR_P) $$d ; done
	for f in `$(SED) $(DISTFILES) MANIFEST` ; do \
	    cp $$f $(TARDIR)/$$f || exit 1 ; \
	done
	if [ "x$(RELEASENUMBER)" != "x" ] ; then \
	    cp README.$(RELEASEEXTENSION) $(TARDIR)/ ; \
	    $(SED) 's/= prerelease/= $(RELEASENUMBER) version/' \
	        Makefile.global.in > $(TARDIR)/Makefile.global.in ; \
	else \
	    $(SED) 's/= prerelease/=/' Makefile.global.in \
	        > $(TARDIR)/Makefile.global.in ; \
	fi
	cp ChangeLog $(TARDIR)/
	find $(TARDIR) -type f -print | xargs touch -t `date +%m%d%H%M.%S`
	tar cf $(TARFILE) $(TARDIR)
	$(GZIP) -9 $(TARFILE)

##  Generate the ChangeLog using support/mkchangelog.  This should only be
##  run by a maintainer since it depends on svn log working and also
##  requires svn2cl be available somewhere.
ChangeLog:
	$(PERL) support/mkchangelog


##  Check the MANIFEST against the files present in the current tree,
##  building a list with find and running diff between the lists.
check-manifest:
	$(SED) -e 1,2d -e 's/ .*//' MANIFEST > LIST.manifest
	$(PERL) support/mkmanifest > LIST.real
	diff -u LIST.manifest LIST.real


##  Make a snapshot.  This is like making a release, except that we don't do
##  the ChangeLog thing and we don't change the version number.  We also
##  assume that SNAPSHOT has been set to the appropriate current branch.
snapshot:
	rm -rf $(SNAPDIR)
	rm -f inn*.tar.gz
	mkdir $(SNAPDIR)
	set -e ; for d in `$(SED) $(SNAPDIRS) MANIFEST` ; do $(MKDIR_P) $$d ; done
	set -e ; for f in `$(SED) $(DISTFILES) MANIFEST` ; do \
	    cp $$f $(SNAPDIR)/$$f ; \
	done
	cp README.snapshot $(SNAPDIR)/
	$(SED) 's/= prerelease/= $(SNAPDATE) snapshot/' \
	    Makefile.global.in > $(SNAPDIR)/Makefile.global.in
	find $(SNAPDIR) -type f -print | xargs touch -t `date +%m%d%H%M.%S`
	tar cf $(SNAPDIR).tar $(SNAPDIR)
	$(GZIP) -9 $(SNAPDIR).tar

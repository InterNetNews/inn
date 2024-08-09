include Makefile.global

##  All installation directories except for $(PATHRUN), which has a
##  different mode than the rest.
INSTDIRS      = $(PATHNEWS) $(PATHBIN) $(PATHAUTH) $(PATHAUTHRESOLV) \
		$(PATHAUTHPASSWD) $(PATHCONTROL) $(PATHFILTER) \
		$(PATHRNEWS) $(PATHDB) $(PATHDOC) $(PATHETC) $(PATHHTTP) \
		$(PATHLIB) $(PATHLIBPERL) $(PATHLIBPERL)/INN \
		$(PATHLIBPERL)/INN/Utils $(PATHDATASHARE) \
		$(PATHMAN) $(MAN1) $(MAN3) $(MAN3PM) $(MAN5) $(MAN8) \
		$(PATHSPOOL) \
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
CLEANDIRS   = $(ALLDIRS) contrib tests

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


##  Major target -- build everything.
##
##  We have to loop through all the directories, because otherwise the build
##  fails if make -j works on more than one directory at a time.
##  libinnstorage depends on libinnhist, but some of the storage/...
##  programs depend on libinnhist, hence the two calls into storage.
##
##  Be careful of a non-GNU make: after a completed command, it does not
##  necessarily return the script back to the starting directory.
all:
	cd include     && $(MAKE) all      || exit 1 ; cd ..
	cd lib         && $(MAKE) all      || exit 1 ; cd ..
	cd storage     && $(MAKE) library  || exit 1 ; cd ..
	cd history     && $(MAKE) all      || exit 1 ; cd ..
	cd innd        && $(MAKE) all      || exit 1 ; cd ..
	cd nnrpd       && $(MAKE) all      || exit 1 ; cd ..
	cd innfeed     && $(MAKE) all      || exit 1 ; cd ..
	cd control     && $(MAKE) all      || exit 1 ; cd ..
	cd expire      && $(MAKE) all      || exit 1 ; cd ..
	cd frontends   && $(MAKE) all      || exit 1 ; cd ..
	cd backends    && $(MAKE) all      || exit 1 ; cd ..
	cd authprogs   && $(MAKE) all      || exit 1 ; cd ..
	cd scripts     && $(MAKE) all      || exit 1 ; cd ..
	cd perl        && $(MAKE) all      || exit 1 ; cd ..
	cd storage     && $(MAKE) programs || exit 1 ; cd ..
	cd doc         && $(MAKE) all      || exit 1 ; cd ..
	cd samples     && $(MAKE) all      || exit 1 ; cd ..
	cd site        && $(MAKE) all      || exit 1 ; cd ..


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
	    cd $$D && $(MAKE) D="$(DESTDIR)" install || exit 1 ; cd .. ; \
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
	    support/install-sh $(OWNER) -m 0755 -d $(DESTDIR)$$D ; \
	done
	support/install-sh $(OWNER) -m 0750 -d $(DESTDIR)$(PATHRUN)

update:
	@chmod +x support/install-sh
	@for D in $(UPDATEDIRS) ; do \
	    echo '' ; \
	    cd $$D && $(MAKE) D="$(DESTDIR)" install || exit 1 ; cd .. ; \
	done
	$(PERL) -Tw $(PATHBIN)/innupgrade $(DESTDIR)$(PATHAUTHPASSWD)
	$(PERL) -Tw $(PATHBIN)/innupgrade $(DESTDIR)$(PATHBIN)
	$(PERL) -Tw $(PATHBIN)/innupgrade $(DESTDIR)$(PATHCONTROL)
	$(PERL) -Tw $(PATHBIN)/innupgrade $(DESTDIR)$(PATHDOC)
	$(PERL) -Tw $(PATHBIN)/innupgrade $(DESTDIR)$(PATHETC)
	$(PERL) -Tw $(PATHBIN)/innupgrade $(DESTDIR)$(PATHFILTER)
	$(PERL) -Tw $(PATHBIN)/innupgrade $(DESTDIR)$(PATHINCLUDE)/inn
	$(PERL) -Tw $(PATHBIN)/innupgrade $(DESTDIR)$(MAN1)
	$(PERL) -Tw $(PATHBIN)/innupgrade $(DESTDIR)$(MAN3)
	$(PERL) -Tw $(PATHBIN)/innupgrade $(DESTDIR)$(MAN5)
	$(PERL) -Tw $(PATHBIN)/innupgrade $(DESTDIR)$(MAN8)

install-root:
	@chmod +x support/install-sh
	support/install-sh $(OWNER) -m 0755 -d $(DESTDIR)$(PATHBIN)
	cd backends && $(MAKE) D="$(DESTDIR)" install-root || exit 1 ; cd ..

##  Install a certificate for TLS/SSL support.
cert:
	umask 077 ; \
	$(SSLBIN) req -new -x509 -nodes \
	    -out $(DESTDIR)$(PATHETC)/cert.pem -days 366 \
	    -keyout $(DESTDIR)$(PATHETC)/key.pem
	@ME=`$(WHOAMI)` ; \
	if [ x"$$ME" = xroot ] ; then \
	    chown $(RUNASUSER) $(DESTDIR)$(PATHETC)/cert.pem ; \
	    chgrp $(RUNASGROUP) $(DESTDIR)$(PATHETC)/cert.pem ; \
	    chown $(RUNASUSER) $(DESTDIR)$(PATHETC)/key.pem ; \
	    chgrp $(RUNASGROUP) $(DESTDIR)$(PATHETC)/key.pem ; \
	fi
	chmod 640 $(DESTDIR)$(PATHETC)/cert.pem
	chmod 600 $(DESTDIR)$(PATHETC)/key.pem


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
	rm -f LIST.* Makefile.global config.cache config.log
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
	rm -f LIST.* Makefile.global config.cache config.log
	rm -f config.status libtool support/fixconfig support/fixscript
	rm -f config.status.lineno configure.lineno
	rm -f inn*.tar.gz configure include/config.h.in
	rm -rf $(TARDIR)

##  Other generic targets.
bootstrap depend profiled:
	@for D in $(ALLDIRS) ; do \
	    echo '' ; \
	    cd $$D && $(MAKE) $@ || exit 1 ; cd .. ; \
	done


##  Run the test suite.
check test tests: all
	cd tests && $(MAKE) test || exit 1 ; cd ..


##  For maintainers, build the entire source base with warnings enabled.
warnings:
	$(MAKE) COPT="$(COPT) $(WARNINGS)" all


##  Make a release.  We create a release by recreating the directory
##  structure and then copying over all files listed in the MANIFEST.  If it
##  isn't in the MANIFEST, it doesn't go into the release.  We also update
##  the version information in Makefile.global.in to remove the prerelease
##  designation and update all timestamps to the date the release is made.
##  If RELEASENUMBER is set, it is a beta release or a release candidate.
release:
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
	find $(TARDIR) -type f -print | xargs touch -t `date +%m%d%H%M.%S`
	tar cf $(TARFILE) $(TARDIR)
	$(GZIP) -9 $(TARFILE)


##  Check the MANIFEST against the files present in the current tree,
##  building a list with find and running diff between the lists.
check-manifest:
	$(SED) -e 1,2d -e 's/ .*//' MANIFEST > LIST.manifest
	$(PERL) support/mkmanifest > LIST.real
	diff -u LIST.manifest LIST.real


##  Make a snapshot.  This is like making a release, except that we don't
##  change the version number.  We also assume that SNAPSHOT has been set to
##  the appropriate current branch.
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


##  Check code for nits and potential errors by running:
##    * perltidy warnings for Perl scripts.
##  This should only be run by a maintainer since it depends on the presence
##  of these programs, and sometimes a specific version.
code-check:
	@if command -v "perltidy" &> /dev/null ; then \
	    echo "Running perltidy to check Perl code..." ; \
	    (grep --include=\*.in -Rin perl . | grep ':1:' | cut -f1 -d':' ; \
	        find . \( -name '*.pl' -o -name '*.pl.in' -o -name '*.pm' \
	        -o -name '*.pm.in' \) \
	        \! -wholename ./perl/INN/Config.pm \
	        \! -wholename ./perl/INN/Utils/Shlock.pm \
	        \! -wholename ./samples/nnrpd_access.pl \
	        \! -wholename ./samples/nnrpd_auth.pl \
	        \! -wholename ./scripts/innshellvars.pl \
	        ; echo ./support/mkmanifest) \
	        | grep -v pgpverify | grep -v '^./site/' | sort -u \
	        | xargs perltidy -se -wma -wmauc=0 -wmr \
	            -wvt='c p r u' -wvxl='*_unused' ; \
	else \
	    echo "Skipping Perl code checking (perltidy not found)" ; \
	fi


##  Reformat all source code using:
##    * black for Python scripts,
##    * clang-format for C code,
##    * perltidy for Perl scripts,
##    * shfmt for shell scripts.
##  This should only be run by a maintainer since it depends on the presence
##  of these formatters, and sometimes a specific version.
##  Exclude the files generated by the Bison parser.
##  Note we also have C source code in innfeed/configfile.l and configfile.y
##  but it should be manually handled as it contains mixed yacc code.
reformat:
	@if command -v "black" &> /dev/null ; then \
	    echo "Running black to reformat Python code..." ; \
	    black --line-length 79 --quiet \
	        --include "contrib/mm_ckpasswd|\.py$$" . ; \
	else \
	    echo "Skipping Python code reformatting (black not found)" ; \
	fi
	@if command -v "clang-format" &> /dev/null ; then \
	    echo "Running clang-format to reformat C code..." ; \
	    find . \( -name '*.[ch]' -o -name paths.h.in \) \
	        \! -wholename ./history/hismethods.\* \
	        \! -wholename ./include/config.h \
	        \! -wholename ./include/inn/paths.h \
	        \! -wholename ./include/inn/portable-\* \
	        \! -wholename ./include/inn/system.h \
	        \! -wholename ./include/inn/version.h \
	        \! -wholename ./innfeed/config_l.c \
	        \! -wholename ./innfeed/config_y.\* \
	        \! -wholename ./storage/methods.\* \
	        \! -wholename ./storage/ovmethods.\* \
	        \! -wholename ./storage/ovsqlite/sql-init.\* \
	        \! -wholename ./storage/ovsqlite/sql-main.\* \
	        -print | xargs clang-format --style=file -i ; \
	else \
	    echo "Skipping C code reformatting (clang-format not found)" ; \
	fi
	@if command -v "perltidy" &> /dev/null ; then \
	    echo "Running perltidy to reformat Perl code..." ; \
	    (grep --include=\*.in -Rin perl . | grep ':1:' | cut -f1 -d':' ; \
	        find . \( -name '*.pl' -o -name '*.pl.in' -o -name '*.pm' \
	        -o -name '*.pm.in' \) \
	        \! -wholename ./perl/INN/Config.pm \
	        \! -wholename ./perl/INN/Utils/Shlock.pm \
	        \! -wholename ./samples/nnrpd_access.pl \
	        \! -wholename ./samples/nnrpd_auth.pl \
	        \! -wholename ./scripts/innshellvars.pl \
	        ; echo ./support/mkmanifest) \
	        | grep -v pgpverify | grep -v '^./site/' | sort -u \
	        | xargs perltidy ; \
	else \
	    echo "Skipping Perl code reformatting (perltidy not found)" ; \
	fi
	@if command -v "shfmt" &> /dev/null ; then \
	    echo "Running shfmt to reformat shell code..." ; \
	    (grep --include=\*.in --include=\*.t --exclude-dir=.libs \
	        -Rin sh * | grep ':1:' | cut -f1 -d':' ; \
	        echo samples/innshellvars.local ; \
	        shfmt -f autogen ci site support tests/tap) \
	        | grep -v install-sh | grep -v ltmain.sh | grep -v xmalloc.t \
	        | sort -u | xargs shfmt -w -s -ln=posix -i 4 -bn ; \
	else \
	    echo "Skipping shell code reformatting (shfmt not found)" ; \
	fi

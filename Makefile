##  $Revision$

RELEASE=1
PATCHLEVEL=5.1
VERSION=$(RELEASE).$(PATCHLEVEL)

#TARDIR=inn
#TARFILE=inn.tar
TARDIR=inn-$(VERSION)
TARFILE=inn-$(VERSION).tar
SQUASH=gzip

SHELL	= /bin/sh
MAKE	= make
DESTDIR	=

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

##  Major target -- install library, build everything else.
all:
	@for D in $(DIRS) ; do \
	    TARGET=$(WHAT_TO_MAKE); \
	    case $$D in lib|storage) TARGET=install ;; esac ; \
	    echo "" ; \
	    echo "cd $$D ; $(MAKE) $(FLAGS) $$TARGET ; cd .." ; \
	    cd $$D; $(MAKE) $(FLAGS) DESTDIR=$(DESTDIR) $$TARGET || exit 1 ; cd .. ; \
	done

##  Install everything.
install:	directories
	$(MAKE) $(FLAGS) WHAT_TO_MAKE=install DESTDIR=$(DESTDIR) common
	@echo "" ; echo Do not forget to update your cron entries.
	@echo Also run makehistory if you have to.

##  Directories where files get put.
directories:
	chmod +x ./makedirs.sh
	DESTDIR=$(DESTDIR) ./makedirs.sh;

##  Other generic targets.
lint depend tags ctags profiled:
	@$(MAKE) $(FLAGS) WHAT_TO_MAKE=$@ common

etags:
	etags */*.c */*.h

clean:
	@$(MAKE) $(FLAGS) WHAT_TO_MAKE=$@ common
	rm -f *~ libstorage.a libinn.a libinn_p.a llib-linn.ln FILELIST

##  Common target.
common:
	@for D in $(DIRS) ; do \
	    echo "" ; \
	    echo "cd $$D ; $(MAKE) $(FLAGS) $(WHAT_TO_MAKE) ; cd .." ; \
	    cd $$D; $(MAKE) $(FLAGS) $(WHAT_TO_MAKE) || exit 1 ; cd .. ; \
	done

##  Software update -- install just the programs and documentation.
update:
	@for D in $(PROGS) ; do \
	    echo "" ; \
	    echo "cd $$D ; $(MAKE) $(FLAGS) install ; cd .." ; \
	    cd $$D; $(MAKE) $(FLAGS) install || exit 1 ; cd .. ; \
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
clobber realclean:	clean
	@echo ""
	rm -f Install.ms inn*.tar.Z inn*.tar.gz Part0? MANIFEST.BAK
	rm -rf inews.* rnews.* nntplib.*
	rm -f tags */tags core */core a.out */a.out foo */foo
	rm -f CHANGES *~
	rm -fr $(TARDIR)
	@echo ""
	cd site ; make clobber ; cd ..

##  Update syslog.
syslogfix:
	rm -f include/syslog.h lib/syslog.c
	cp syslog/syslog.h include
	cp syslog/syslog.c lib
	cp syslog/syslog.3 doc
	-cd syslog; $(CC) -I../include -o syslogd syslogd.c ; cd ..
	@echo "Install syslogd and syslog.conf as appropriate"

##  Configure, compile, and lint.
world:		Install.ms
	cd config ; $(MAKE) $(FLAGS) subst quiet ; cd ..
	cd lib ; $(MAKE) $(FLAGS) lint ; cd ..
	cat lib/lint
	cd lib ; $(MAKE) $(FLAGS) install ; cd ..
	$(MAKE) $(FLAGS) lint

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

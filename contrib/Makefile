##  $Id$
##
##  There are no installation rules or other top-level rules for this
##  directory as it's not properly part of INN.  Installation should be
##  done by the user by hand for those files that they're interested in.

include ../Makefile.global

top           = ..
CFLAGS        = $(GCFLAGS)

ALL	      = archivegz backlogstat backupfeed cleannewsgroups delayer \
		findreadgroups makeexpctl makestorconf mlockfile newsresp \
		pullart reset-cnfs respool showtoken stathist thdexpire \
		tunefeed

all: $(ALL)

warnings:
	$(MAKE) COPT='$(WARNINGS)' all

clean clobber distclean maintclean:
	rm -f *.o $(ALL)
	rm -rf .libs

$(FIXSCRIPT):
	@echo Run configure before running make.  See INSTALL for details.
	@exit 1


##  Compilation rules.

LINK 		= $(LIBLD) $(LDFLAGS) -o $@
FIX		= $(FIXSCRIPT)

STORELIBS	= $(LIBSTORAGE) $(LIBINN) $(STORAGE_LIBS) $(LIBS)

expirectl:	expirectl.o	; $(LINK) expirectl.o
mlockfile:	mlockfile.o	; $(LINK) mlockfile.o
newsresp:	newsresp.o	; $(LINK) newsresp.o $(LIBS)
pullart:	pullart.o	; $(LINK) pullart.o $(LIBINN)
reset-cnfs:	reset-cnfs.o	; $(LINK) reset-cnfs.o
respool:	respool.o	; $(LINK) respool.o $(STORELIBS)

archivegz:       archivegz.in       $(FIX) ; $(FIX) -i archivegz.in
backlogstat:     backlogstat.in     $(FIX) ; $(FIX) backlogstat.in
backupfeed:      backupfeed.in      $(FIX) ; $(FIX) -i backupfeed.in
cleannewsgroups: cleannewsgroups.in $(FIX) ; $(FIX) cleannewsgroups.in
delayer:         delayer.in         $(FIX) ; $(FIX) -i delayer.in
findreadgroups:  findreadgroups.in  $(FIX) ; $(FIX) findreadgroups.in
makeexpctl:      makeexpctl.in      $(FIX) ; $(FIX) makeexpctl.in
makestorconf:    makestorconf.in    $(FIX) ; $(FIX) makestorconf.in
showtoken:       showtoken.in       $(FIX) ; $(FIX) -i showtoken.in
stathist:        stathist.in        $(FIX) ; $(FIX) -i stathist.in
thdexpire:       thdexpire.in       $(FIX) ; $(FIX) thdexpire.in
tunefeed:        tunefeed.in        $(FIX) ; $(FIX) -i tunefeed.in

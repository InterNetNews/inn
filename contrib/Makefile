##  $Id$
##
##  There are no installation rules or other top-level rules for this
##  directory as it's not properly part of INN.  Installation should be
##  done by the user by hand for those files that they're interested in.

include ../Makefile.global

top           = ..
CFLAGS        = $(GCFLAGS)

ALL	      = archivegz backupfeed cleannewsgroups delayer findreadgroups \
		makeexpctl makestorconf mlockfile newsresp pathinfo pullart \
		showtoken stathist thdexpire tunefeed

all: $(ALL)

clean clobber distclean:
	rm -f *.o $(ALL)
	rm -rf .libs


##  Compilation rules.

LINK 		= $(LIBLD) $(LDFLAGS) -o $@
FIX		= $(FIXSCRIPT)

expirectl:	expirectl.o	; $(LINK) expirectl.o
mlockfile:	mlockfile.o	; $(LINK) mlockfile.o
newsresp:	newsresp.o	; $(LINK) newsresp.o $(LIBS)
pullart:	pullart.o	; $(LINK) pullart.o

archivegz:       archivegz.in       $(FIX) ; $(FIX) -i archivegz.in
backupfeed:      backupfeed.in      $(FIX) ; $(FIX) -i backupfeed.in
cleannewsgroups: cleannewsgroups.in $(FIX) ; $(FIX) cleannewsgroups.in
delayer:         delayer.in         $(FIX) ; $(FIX) -i delayer.in
findreadgroups:  findreadgroups.in  $(FIX) ; $(FIX) findreadgroups.in
makeexpctl:      makeexpctl.in      $(FIX) ; $(FIX) makeexpctl.in
makestorconf:    makestorconf.in    $(FIX) ; $(FIX) makestorconf.in
pathinfo:        pathinfo.in        $(FIX) ; $(FIX) -i pathinfo.in
showtoken:       showtoken.in       $(FIX) ; $(FIX) -i showtoken.in
stathist:        stathist.in        $(FIX) ; $(FIX) -i stathist.in
thdexpire:       thdexpire.in       $(FIX) ; $(FIX) thdexpire.in
tunefeed:        tunefeed.in        $(FIX) ; $(FIX) -i tunefeed.in

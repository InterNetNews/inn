##  There are no installation rules or other top-level rules for this
##  directory as it's not properly part of INN.  Installation should be
##  done by the user by hand for those files that they're interested in.

include ../Makefile.global

top           = ..
CFLAGS        = $(GCFLAGS)

ALL	      = analyze-traffic archivegz authmysql auth_pass backlogstat \
		cleannewsgroups count_overview delayer expirectl \
		findreadgroups fixhist innconfcheck makeexpctl makestorconf \
		mkbuf mlockfile newsresp \
		nnrp.access2readers.conf pullart reset-cnfs respool \
		stathist thdexpire \
		tunefeed

all: $(ALL)

warnings:
	$(MAKE) COPT="$(COPT) $(WARNINGS)" all

clean clobber distclean maintclean:
	rm -f *.o $(ALL)
	rm -rf .libs

$(FIXSCRIPT):
	@echo Run configure before running make.  See INSTALL for details.
	@exit 1


##  Compilation rules.

LINK 		= $(LIBLD) $(LDFLAGS) -o $@
FIX		= $(SHELL) $(FIXSCRIPT)

STORELIBS	= $(LIBSTORAGE) $(LIBHIST) $(LIBINN) $(STORAGE_LIBS) $(LIBS)

auth_pass:	auth_pass.o	; $(LINK) auth_pass.o $(LIBINN) $(CRYPT_LIBS)
expirectl:	expirectl.o	; $(LINK) expirectl.o
mlockfile:	mlockfile.o	; $(LINK) mlockfile.o
newsresp:	newsresp.o	; $(LINK) newsresp.o $(LIBS)
pullart:	pullart.o	; $(LINK) pullart.o $(LIBINN)
reset-cnfs:	reset-cnfs.o	; $(LINK) reset-cnfs.o
respool:	respool.o	; $(LINK) respool.o $(STORELIBS)

analyze-traffic: analyze-traffic.in $(FIXSCRIPT) ; $(FIX) -i analyze-traffic.in
archivegz:       archivegz.in       $(FIXSCRIPT) ; $(FIX) -i archivegz.in
authmysql:       authmysql.in       $(FIXSCRIPT) ; $(FIX) -i authmysql.in
backlogstat:     backlogstat.in     $(FIXSCRIPT) ; $(FIX) backlogstat.in
cleannewsgroups: cleannewsgroups.in $(FIXSCRIPT) ; $(FIX) cleannewsgroups.in
count_overview:  count_overview.in  $(FIXSCRIPT) ; $(FIX) -i count_overview.in
delayer:         delayer.in         $(FIXSCRIPT) ; $(FIX) -i delayer.in
findreadgroups:  findreadgroups.in  $(FIXSCRIPT) ; $(FIX) findreadgroups.in
fixhist:         fixhist.in         $(FIXSCRIPT) ; $(FIX) -i fixhist.in
innconfcheck:    innconfcheck.in    $(FIXSCRIPT) ; $(FIX) -i innconfcheck.in
makeexpctl:      makeexpctl.in      $(FIXSCRIPT) ; $(FIX) makeexpctl.in
makestorconf:    makestorconf.in    $(FIXSCRIPT) ; $(FIX) makestorconf.in
mkbuf:           mkbuf.in           $(FIXSCRIPT) ; $(FIX) -i mkbuf.in
nnrp.access2readers.conf: nnrp.access2readers.conf.in $(FIXSCRIPT)
	$(FIX) -i nnrp.access2readers.conf.in
stathist:        stathist.in        $(FIXSCRIPT) ; $(FIX) -i stathist.in
thdexpire:       thdexpire.in       $(FIXSCRIPT) ; $(FIX) thdexpire.in
tunefeed:        tunefeed.in        $(FIXSCRIPT) ; $(FIX) -i tunefeed.in

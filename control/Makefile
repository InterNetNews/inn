include ../Makefile.global

top	      = ..
CFLAGS	      = $(GCFLAGS)

ALL	      = controlbatch controlchan docheckgroups gencancel perl-nocem \
		pgpverify

MAN	      = ../doc/man/perl-nocem.8 ../doc/man/pgpverify.1

SOURCES	      = gencancel.c

all: $(ALL) $(MAN)

warnings:
	$(MAKE) COPT='$(WARNINGS)' all

install: all
	$(LI_XPRI) gencancel $D$(PATHBIN)/gencancel
	for F in controlbatch controlchan docheckgroups perl-nocem \
	    pgpverify ; do \
	    $(CP_XPUB) $$F $D$(PATHBIN)/$$F ; \
	done
	for M in modules/*.pl ; do \
	    $(CP_RPUB) $$M $D$(PATHCONTROL)/`basename $$M` ; \
	done

bootstrap: $(MAN)

clean clobber distclean:
	rm -f *.o $(ALL)
	rm -rf .libs

maintclean: distclean
	rm -f $(MAN)

profiled: all

$(FIXSCRIPT):
	@echo Run configure before running make.  See INSTALL for details.
	@exit 1


##  Build rules.
LINK	= $(LIBLD) $(LDFLAGS) -o $@

FIX	= $(FIXSCRIPT)

controlbatch:	controlbatch.in  $(FIX) ; $(FIX) controlbatch.in
controlchan:	controlchan.in   $(FIX) ; $(FIX) controlchan.in
docheckgroups:	docheckgroups.in $(FIX) ; $(FIX) docheckgroups.in
perl-nocem:	perl-nocem.in    $(FIX) ; $(FIX) perl-nocem.in
pgpverify:	pgpverify.in     $(FIX) ; $(FIX) pgpverify.in

../doc/man/perl-nocem.8: perl-nocem.in
	$(POD2MAN) -s 8 -n "PERL-NOCEM" $? > $@

../doc/man/pgpverify.1: pgpverify.in
	$(POD2MAN) -s 1 -n "PGPVERIFY" $? > $@

gencancel: gencancel.o $(LIBINN) $(LIBSTORAGE) $(LIBHIST)
	$(LINK) gencancel.o $(LIBINN) $(LIBHIST) $(LIBSTORAGE) $(STORAGE_LIBS) \
	    $(CANLOCK_LDFLAGS) $(CANLOCK_LIBS) $(LIBS)

gencancel.o: gencancel.c
	$(CC) $(CFLAGS) $(CANLOCK_CPPFLAGS) -c $<

$(LIBINN):      ; (cd ../lib ; $(MAKE))
$(LIBSTORAGE):  ; (cd ../storage ; $(MAKE) library)
$(LIBHIST):     ; (cd ../history ; $(MAKE))

##  Dependencies.  Default list, below, is probably good enough.

depend: Makefile $(SOURCES)
	$(MAKEDEPEND) '$(CFLAGS)' $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
gencancel.o: gencancel.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/history.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/innconf.h ../include/inn/libinn.h ../include/inn/concat.h \
  ../include/inn/xmalloc.h ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/paths.h \
  ../include/inn/secrets.h ../include/inn/storage.h \
  ../include/inn/options.h ../include/inn/wire.h

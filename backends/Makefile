include ../Makefile.global

top	      = ..
CFLAGS        = $(GCFLAGS)

ALL           = actmerge actsync actsyncd archive batcher buffchan \
		cvtbatch delayer innbind inndf innxmit innxbatch mod-active \
		news2mail ninpaths nntpget nntpsend overchan send-ihave \
		send-uucp sendinpaths sendxbatches shlock \
		shrinkfile

MAN	      = ../doc/man/delayer.8 ../doc/man/send-uucp.8

SOURCES       = actsync.c archive.c batcher.c buffchan.c cvtbatch.c \
		innbind.c inndf.c innxbatch.c innxmit.c \
		ninpaths.c nntpget.c overchan.c shlock.c shrinkfile.c

all: $(ALL) $(MAN)

warnings:
	$(MAKE) COPT="$(COPT) $(WARNINGS)" all

install: all
	for F in actmerge actsyncd delayer news2mail nntpsend send-ihave \
	         send-uucp sendinpaths sendxbatches ; do \
	    $(CP_XPUB) $$F $D$(PATHBIN)/$$F ; \
	done
	$(CP_XPRI) mod-active $D$(PATHBIN)/mod-active
	$(LI_XPRI) overchan $D$(PATHBIN)/overchan
	for F in actsync archive batcher buffchan cvtbatch inndf \
	         innxbatch innxmit ninpaths nntpget shlock shrinkfile ; do \
	    $(LI_XPUB) $$F $D$(PATHBIN)/$$F ; \
	done
	@ME=`$(WHOAMI)` ; \
	if [ x"$$ME" = xroot ] ; then \
	    echo $(LI_SPRI) innbind $D$(PATHBIN)/innbind ; \
	    $(LI_SPRI) innbind $D$(PATHBIN)/innbind ; \
	else \
	    echo $(LI_XPRI) innbind $D$(PATHBIN)/innbind ; \
	    $(LI_XPRI) innbind $D$(PATHBIN)/innbind ; \
	    echo '' ; \
	    echo '========================' ; \
	    echo 'NOTE NOTE NOTE NOTE NOTE' ; \
	    ls -l $D$(PATHBIN)/innbind ; \
	    echo '$D$(PATHBIN)/innbind needs to be installed setuid root' ; \
	    echo '' ; echo ; \
	fi

install-root:
	$(LI_SPRI) innbind $D$(PATHBIN)/innbind

bootstrap: $(MAN)

clean clobber distclean:
	rm -f *.o $(ALL)
	rm -rf .libs

maintclean: distclean
	rm -f $(MAN)

profiled:
	$(MAKEPROFILING) all

##  Compilation rules.

# $(LIBSTORAGE) must be listed twice to resolve circular dependencies
# when doing static linking.
BOTH		= $(LIBSTORAGE) $(LIBHIST) $(LIBSTORAGE) $(LIBINN)

LINK 		= $(LIBLD) $(LDFLAGS) -o $@
LINKDEPS	= $(LIBLDDEPS) $(LDFLAGS) -o $@
INNLIBS		= $(LIBINN) $(LIBS)
STORELIBS	= $(BOTH) $(STORAGE_LIBS) $(LIBS)

FIX		= $(SHELL) $(FIXSCRIPT)

$(FIXSCRIPT):
	@echo Run configure before running make.  See INSTALL for details.
	@exit 1

actsync:	actsync.o    $(LIBINN)	; $(LINK) actsync.o    $(INNLIBS)
archive:	archive.o    $(BOTH)	; $(LINKDEPS) archive.o  $(STORELIBS)
batcher:	batcher.o    $(BOTH)	; $(LINKDEPS) batcher.o  $(STORELIBS)
cvtbatch:	cvtbatch.o   $(BOTH)	; $(LINKDEPS) cvtbatch.o $(STORELIBS)
innbind:	innbind.o    $(LIBINN)	; $(LINK) innbind.o    $(INNLIBS)
inndf:		inndf.o      $(BOTH)	; $(LINKDEPS) inndf.o  $(STORELIBS)
innxbatch:	innxbatch.o  $(LIBINN)	; $(LINK) innxbatch.o  $(INNLIBS)
innxmit:	innxmit.o    $(BOTH)	; $(LINK) innxmit.o    $(STORELIBS)
ninpaths:	ninpaths.o		; $(LINK) ninpaths.o
nntpget:	nntpget.o    $(BOTH)	; $(LINK) nntpget.o    $(STORELIBS)
overchan:	overchan.o   $(BOTH)	; $(LINKDEPS) overchan.o $(STORELIBS)
shlock:		shlock.o     $(LIBINN)	; $(LINK) shlock.o     $(INNLIBS)
shrinkfile:	shrinkfile.o $(LIBINN)	; $(LINK) shrinkfile.o $(INNLIBS)

buffchan:	buffchan.o $(LIBINN)
	$(LINK) buffchan.o $(LIBINN) $(LIBS)

actmerge:	actmerge.in      $(FIXSCRIPT)	; $(FIX) actmerge.in
actsyncd:	actsyncd.in      $(FIXSCRIPT)	; $(FIX) actsyncd.in
delayer:	delayer.in       $(FIXSCRIPT)	; $(FIX) -i delayer.in
mod-active:	mod-active.in    $(FIXSCRIPT)	; $(FIX) mod-active.in
news2mail:	news2mail.in     $(FIXSCRIPT)	; $(FIX) news2mail.in
nntpsend:	nntpsend.in      $(FIXSCRIPT)	; $(FIX) nntpsend.in
send-ihave:	send-ihave.in    $(FIXSCRIPT)	; $(FIX) send-ihave.in
send-uucp:	send-uucp.in     $(FIXSCRIPT)	; $(FIX) send-uucp.in
sendinpaths:	sendinpaths.in   $(FIXSCRIPT)	; $(FIX) sendinpaths.in
sendxbatches:	sendxbatches.in  $(FIXSCRIPT)	; $(FIX) sendxbatches.in

$(LIBINN):	; (cd ../lib ; $(MAKE))
$(LIBSTORAGE):	; (cd ../storage ; $(MAKE) library)
$(LIBHIST):	; (cd ../history ; $(MAKE))

../doc/man/delayer.8: delayer.in
	$(POD2MAN) -s 8 -n "DELAYER" $? > $@

../doc/man/send-uucp.8: send-uucp.in
	$(POD2MAN) -s 8 -n "SEND-UUCP" $? > $@


##  Dependencies.  Default list, below, is probably good enough.

depend:	Makefile $(SOURCES)
	$(MAKEDEPEND) '$(CFLAGS)' $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
actsync.o: actsync.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/hashtab.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/innconf.h ../include/inn/libinn.h ../include/inn/concat.h \
  ../include/inn/xmalloc.h ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/paths.h ../include/inn/qio.h
archive.o: archive.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/buffer.h \
  ../include/inn/portable-stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/libinn.h ../include/inn/concat.h \
  ../include/inn/xmalloc.h ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/paths.h ../include/inn/qio.h \
  ../include/inn/storage.h ../include/inn/options.h \
  ../include/inn/vector.h ../include/inn/wire.h
batcher.o: batcher.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/paths.h \
  ../include/inn/storage.h ../include/inn/options.h ../include/inn/timer.h \
  ../include/inn/wire.h
buffchan.o: buffchan.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/paths.h ../include/inn/qio.h
cvtbatch.o: cvtbatch.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/paths.h ../include/inn/qio.h \
  ../include/inn/storage.h ../include/inn/options.h ../include/inn/wire.h
innbind.o: innbind.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/macros.h \
  ../include/inn/portable-stdbool.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/newsuser.h \
  ../include/inn/vector.h
inndf.o: inndf.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/overview.h \
  ../include/inn/storage.h ../include/inn/options.h ../include/inn/paths.h \
  ../include/inn/qio.h
innxbatch.o: innxbatch.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/inn/innconf.h ../include/inn/macros.h \
  ../include/inn/portable-stdbool.h ../include/inn/libinn.h \
  ../include/inn/concat.h ../include/inn/xmalloc.h ../include/inn/system.h \
  ../include/inn/xwrite.h ../include/inn/messages.h ../include/inn/nntp.h \
  ../include/inn/timer.h
innxmit.o: innxmit.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/inn/history.h ../include/inn/macros.h \
  ../include/inn/portable-stdbool.h ../include/inn/innconf.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/nntp.h ../include/inn/paths.h \
  ../include/inn/qio.h ../include/inn/storage.h ../include/inn/options.h \
  ../include/inn/timer.h ../include/inn/wire.h
ninpaths.o: ninpaths.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h
nntpget.o: nntpget.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/inn/history.h ../include/inn/macros.h \
  ../include/inn/portable-stdbool.h ../include/inn/innconf.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/nntp.h ../include/inn/paths.h
overchan.o: overchan.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/overview.h \
  ../include/inn/storage.h ../include/inn/options.h ../include/inn/paths.h \
  ../include/inn/qio.h
shlock.o: shlock.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/messages.h
shrinkfile.o: shrinkfile.c ../include/portable/system.h \
  ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/portable/macros.h ../include/portable/stdbool.h \
  ../include/inn/innconf.h ../include/inn/macros.h \
  ../include/inn/portable-stdbool.h ../include/inn/libinn.h \
  ../include/inn/concat.h ../include/inn/xmalloc.h ../include/inn/system.h \
  ../include/inn/xwrite.h ../include/inn/messages.h

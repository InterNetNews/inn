##  $Id$

include ../Makefile.global

top	      = ..
CFLAGS        = $(GCFLAGS)

ALL           = actmerge actsync actsyncd archive batcher buffchan \
		cvtbatch filechan innbind inndf innxmit innxbatch mod-active \
		news2mail ninpaths nntpget nntpsend overchan send-ihave \
		send-nntp send-uucp sendinpaths sendxbatches shlock \
		shrinkfile

MAN	      = ../doc/man/send-uucp.8

SOURCES       = actsync.c archive.c batcher.c buffchan.c cvtbatch.c \
		filechan.c innbind.c inndf.c innxbatch.c innxmit.c map.c \
		ninpaths.c nntpget.c overchan.c shlock.c shrinkfile.c

all: $(ALL)

warnings:
	$(MAKE) COPT='$(WARNINGS)' all

install: all
	for F in actmerge actsyncd news2mail nntpsend send-ihave send-nntp \
	         send-uucp sendinpaths sendxbatches ; do \
	    $(CP_XPUB) $$F $D$(PATHBIN)/$$F ; \
	done
	$(CP_XPRI) mod-active $D$(PATHBIN)/mod-active
	$(LI_XPRI) overchan $D$(PATHBIN)/overchan
	for F in actsync archive batcher buffchan cvtbatch filechan inndf \
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

clean:
	rm -f *.o $(ALL)
	rm -rf .libs

clobber distclean: clean
	rm -f tags

tags ctags: $(SOURCES)
	$(CTAGS) $(SOURCES)

profiled:
	$(MAKEPROFILING) all

##  Compilation rules.

BOTH		= $(LIBSTORAGE) $(LIBHIST) $(LIBSTORAGE) $(LIBINN)

LINK 		= $(LIBLD) $(LDFLAGS) -o $@
INNLIBS		= $(LIBINN) $(LIBS)
STORELIBS	= $(BOTH) $(STORAGE_LIBS) $(LIBS)

FIX		= $(FIXSCRIPT)

$(FIXSCRIPT):
	@echo Run configure before running make.  See INSTALL for details.
	@exit 1

actsync:	actsync.o    $(LIBINN)	; $(LINK) actsync.o    $(INNLIBS)
archive:	archive.o    $(BOTH)	; $(LINK) archive.o    $(STORELIBS)
batcher:	batcher.o    $(BOTH)	; $(LINK) batcher.o    $(STORELIBS)
cvtbatch:	cvtbatch.o   $(BOTH)	; $(LINK) cvtbatch.o   $(STORELIBS)
innbind:	innbind.o    $(LIBINN)	; $(LINK) innbind.o    $(INNLIBS)
inndf:		inndf.o      $(BOTH)	; $(LINK) inndf.o      $(STORELIBS)
innxbatch:	innxbatch.o  $(LIBINN)	; $(LINK) innxbatch.o  $(INNLIBS)
innxmit:	innxmit.o    $(BOTH)	; $(LINK) innxmit.o    $(STORELIBS)
ninpaths:	ninpaths.o		; $(LINK) ninpaths.o
nntpget:	nntpget.o    $(BOTH)	; $(LINK) nntpget.o    $(STORELIBS)
overchan:	overchan.o   $(BOTH)	; $(LINK) overchan.o   $(STORELIBS)
shlock:		shlock.o     $(LIBINN)	; $(LINK) shlock.o     $(INNLIBS)
shrinkfile:	shrinkfile.o $(LIBINN)	; $(LINK) shrinkfile.o $(INNLIBS)

buffchan:	buffchan.o map.o $(LIBINN)
	$(LINK) buffchan.o map.o $(LIBINN) $(LIBS)

filechan:	filechan.o map.o $(LIBINN)
	$(LINK) filechan.o map.o $(LIBINN) $(LIBS)

actmerge:	actmerge.in      $(FIX)	; $(FIX) actmerge.in
actsyncd:	actsyncd.in      $(FIX)	; $(FIX) actsyncd.in
mod-active:	mod-active.in    $(FIX)	; $(FIX) mod-active.in
news2mail:	news2mail.in     $(FIX)	; $(FIX) news2mail.in
nntpsend:	nntpsend.in      $(FIX)	; $(FIX) nntpsend.in
send-ihave:	send-ihave.in    $(FIX)	; $(FIX) send-ihave.in
send-nntp:	send-nntp.in     $(FIX)	; $(FIX) send-nntp.in
send-uucp:	send-uucp.in     $(FIX)	; $(FIX) send-uucp.in
sendinpaths:	sendinpaths.in   $(FIX) ; $(FIX) sendinpaths.in
sendxbatches:	sendxbatches.in  $(FIX)	; $(FIX) sendxbatches.in

$(LIBINN):	; (cd ../lib ; $(MAKE))
$(LIBSTORAGE):	; (cd ../storage ; $(MAKE))
$(LIBHIST):	; (cd ../history ; $(MAKE))

../doc/man/send-uucp.8: send-uucp
	$(POD2MAN) -s 8 $? > $@


##  Dependencies.  Default list, below, is probably good enough.

depend:	Makefile $(SOURCES)
	$(MAKEDEPEND) '$(CFLAGS)' $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
actsync.o: actsync.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/portable/wait.h \
 ../include/inn/innconf.h ../include/inn/messages.h \
 ../include/inn/qio.h ../include/libinn.h ../include/paths.h
archive.o: archive.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/innconf.h \
 ../include/inn/messages.h ../include/inn/wire.h ../include/libinn.h \
 ../include/paths.h ../include/storage.h
batcher.o: batcher.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/innconf.h \
 ../include/inn/messages.h ../include/libinn.h ../include/paths.h \
 ../include/storage.h
buffchan.o: buffchan.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/innconf.h \
 ../include/inn/messages.h ../include/inn/qio.h ../include/libinn.h \
 ../include/paths.h map.h
cvtbatch.o: cvtbatch.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/innconf.h \
 ../include/inn/messages.h ../include/inn/qio.h ../include/inn/wire.h \
 ../include/libinn.h ../include/paths.h ../include/storage.h
filechan.o: filechan.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/innconf.h \
 ../include/inn/messages.h ../include/libinn.h ../include/paths.h \
 map.h
inndf.o: inndf.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/innconf.h \
 ../include/inn/messages.h ../include/inn/qio.h ../include/libinn.h \
 ../include/ov.h ../include/storage.h ../include/inn/history.h \
 ../include/paths.h
innxbatch.o: innxbatch.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/portable/socket.h \
 ../include/portable/time.h ../include/inn/innconf.h \
 ../include/inn/messages.h ../include/libinn.h ../include/nntp.h
innxmit.o: innxmit.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/portable/socket.h \
 ../include/portable/time.h ../include/inn/history.h \
 ../include/inn/innconf.h ../include/inn/messages.h \
 ../include/inn/qio.h ../include/inn/timer.h ../include/inn/wire.h \
 ../include/libinn.h ../include/nntp.h ../include/paths.h \
 ../include/storage.h
map.o: map.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h ../include/paths.h map.h
ninpaths.o: ninpaths.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h
nntpget.o: nntpget.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/portable/socket.h \
 ../include/portable/time.h ../include/inn/history.h \
 ../include/inn/innconf.h ../include/inn/messages.h \
 ../include/libinn.h ../include/nntp.h ../include/paths.h
overchan.o: overchan.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/portable/time.h \
 ../include/inn/innconf.h ../include/inn/messages.h \
 ../include/inn/qio.h ../include/libinn.h ../include/ov.h \
 ../include/storage.h ../include/inn/history.h ../include/paths.h
shlock.o: shlock.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/messages.h
shrinkfile.o: shrinkfile.c ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h \
 ../include/inn/innconf.h ../include/inn/messages.h \
 ../include/libinn.h

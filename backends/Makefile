##  $Id$

include ../Makefile.global

top	      = ..
CFLAGS        = $(GCFLAGS)

ALL           = actmerge actsync actsyncd archive batcher buffchan \
		cvtbatch filechan inndf innxmit innxbatch mod-active \
		news2mail ninpaths nntpget nntpsend overchan send-ihave \
		send-nntp send-uucp sendinpaths sendxbatches shlock \
		shrinkfile

MAN	      = ../doc/man/send-uucp.8

SOURCES       = actsync.c archive.c batcher.c buffchan.c cvtbatch.c \
		filechan.c inndf.c innxbatch.c innxmit.c map.c ninpaths.c \
		nntpget.c overchan.c shlock.c shrinkfile.c

INSTALLED     = $(D)$(PATHBIN)/actmerge		\
		$(D)$(PATHBIN)/actsync		\
		$(D)$(PATHBIN)/actsyncd		\
		$(D)$(PATHBIN)/archive		\
		$(D)$(PATHBIN)/batcher		\
		$(D)$(PATHBIN)/buffchan		\
		$(D)$(PATHBIN)/cvtbatch		\
		$(D)$(PATHBIN)/filechan		\
		$(D)$(PATHBIN)/inndf		\
		$(D)$(PATHBIN)/innxmit		\
		$(D)$(PATHBIN)/innxbatch	\
		$(D)$(PATHBIN)/mod-active	\
		$(D)$(PATHBIN)/news2mail	\
		$(D)$(PATHBIN)/ninpaths		\
		$(D)$(PATHBIN)/nntpget		\
		$(D)$(PATHBIN)/nntpsend		\
		$(D)$(PATHBIN)/overchan		\
		$(D)$(PATHBIN)/send-ihave	\
		$(D)$(PATHBIN)/send-nntp	\
		$(D)$(PATHBIN)/send-uucp	\
		$(D)$(PATHBIN)/sendinpaths	\
		$(D)$(PATHBIN)/sendxbatches	\
		$(D)$(PATHBIN)/shlock		\
		$(D)$(PATHBIN)/shrinkfile

all: $(ALL)

warnings:
	$(MAKE) COPT='$(WARNINGS)' all

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
STORELIBS	= $(BOTH) $(EXTSTORAGELIBS) $(LIBS)

FIX		= $(FIXSCRIPT)

actsync:	actsync.o    $(LIBINN)	; $(LINK) actsync.o    $(INNLIBS)
archive:	archive.o    $(BOTH)	; $(LINK) archive.o    $(STORELIBS)
batcher:	batcher.o    $(BOTH)	; $(LINK) batcher.o    $(STORELIBS)
cvtbatch:	cvtbatch.o   $(BOTH)	; $(LINK) cvtbatch.o   $(STORELIBS)
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


##  Installation rules.  Installation commands set in Makefile.global.

install: $(INSTALLED)

$(D)$(PATHBIN)/actmerge:            actmerge            ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/actsync:             actsync             ; $(LI_XPUB) $? $@
$(D)$(PATHBIN)/actsyncd:            actsyncd            ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/archive:             archive             ; $(LI_XPUB) $? $@
$(D)$(PATHBIN)/batcher:             batcher             ; $(LI_XPUB) $? $@
$(D)$(PATHBIN)/buffchan:            buffchan            ; $(LI_XPUB) $? $@
$(D)$(PATHBIN)/cvtbatch:            cvtbatch            ; $(LI_XPUB) $? $@
$(D)$(PATHBIN)/filechan:            filechan            ; $(LI_XPUB) $? $@
$(D)$(PATHBIN)/inndf:               inndf               ; $(LI_XPUB) $? $@
$(D)$(PATHBIN)/innxbatch:           innxbatch           ; $(LI_XPUB) $? $@
$(D)$(PATHBIN)/innxmit:             innxmit             ; $(LI_XPUB) $? $@
$(D)$(PATHBIN)/mod-active:          mod-active          ; $(CP_XPRI) $? $@
$(D)$(PATHBIN)/news2mail:           news2mail           ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/ninpaths:            ninpaths		; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/nntpget:             nntpget             ; $(LI_XPUB) $? $@
$(D)$(PATHBIN)/nntpsend:            nntpsend            ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/overchan:            overchan            ; $(LI_XPRI) $? $@
$(D)$(PATHBIN)/send-ihave:          send-ihave          ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/send-nntp:           send-nntp           ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/send-uucp:           send-uucp           ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/sendinpaths:         sendinpaths		; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/sendxbatches:        sendxbatches        ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/shlock:              shlock              ; $(LI_XPUB) $? $@
$(D)$(PATHBIN)/shrinkfile:          shrinkfile          ; $(LI_XPUB) $? $@


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
 ../include/inn/qio.h ../include/inn/wire.h ../include/libinn.h \
 ../include/nntp.h ../include/paths.h ../include/storage.h
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

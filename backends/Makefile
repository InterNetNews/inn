##  $Id$

include ../Makefile.global

CFLAGS        = $(GCFLAGS)

ALL           = actmerge actsync actsyncd archive batcher buffchan \
		controlbatch cvtbatch filechan inndf innxmit innxbatch \
		mod-active news2mail nntpget nntpsend overchan send-ihave \
		send-nntp send-uucp sendbatch sendxbatches shlock shrinkfile

EXTRA         = controlchan pgpverify

SOURCES       = actsync.c archive.c batcher.c buffchan.c cvtbatch.c \
		filechan.c inndf.c innxbatch.c innxmit.c nntpget.c \
		overchan.c shlock.c shrinkfile.c

INSTALLED     = $(D)$(PATHBIN)/actmerge		\
		$(D)$(PATHBIN)/actsync		\
		$(D)$(PATHBIN)/actsyncd		\
		$(D)$(PATHBIN)/archive		\
		$(D)$(PATHBIN)/batcher		\
		$(D)$(PATHBIN)/buffchan		\
		$(D)$(PATHBIN)/controlbatch	\
		$(D)$(PATHBIN)/controlchan	\
		$(D)$(PATHBIN)/cvtbatch		\
		$(D)$(PATHBIN)/filechan		\
		$(D)$(PATHBIN)/inndf		\
		$(D)$(PATHBIN)/innxmit		\
		$(D)$(PATHBIN)/innxbatch	\
		$(D)$(PATHBIN)/mod-active	\
		$(D)$(PATHBIN)/news2mail	\
		$(D)$(PATHBIN)/nntpget		\
		$(D)$(PATHBIN)/nntpsend		\
		$(D)$(PATHBIN)/overchan		\
		$(D)$(PATHBIN)/pgpverify	\
		$(D)$(PATHBIN)/send-ihave	\
		$(D)$(PATHBIN)/send-nntp	\
		$(D)$(PATHBIN)/send-uucp	\
		$(D)$(PATHBIN)/sendbatch	\
		$(D)$(PATHBIN)/sendxbatches	\
		$(D)$(PATHBIN)/shlock		\
		$(D)$(PATHBIN)/shrinkfile

all: $(ALL)

clean:
	rm -f *.o $(ALL)
	rm -rf .libs

clobber: clean
	rm -f $(EXTRA) tags

tags ctags: $(SOURCES)
	$(CTAGS) $(SOURCES)


##  Compilation rules.

LINK 		= $(LIBTOOL) $(CC) $(LDFLAGS) -o $@
NEWSLIBS	= $(LIBNEWS) $(LIBS)
FIX		= $(FIXSCRIPT)

actsync:	actsync.o    $(LIBNEWS)	; $(LINK) actsync.o    $(NEWSLIBS)
archive:	archive.o    $(LIBNEWS)	; $(LINK) archive.o    $(NEWSLIBS)
batcher:	batcher.o    $(LIBNEWS)	; $(LINK) batcher.o    $(NEWSLIBS)
cvtbatch:	cvtbatch.o   $(LIBNEWS)	; $(LINK) cvtbatch.o   $(NEWSLIBS)
inndf:		inndf.o      $(LIBNEWS)	; $(LINK) inndf.o      $(NEWSLIBS)
innxbatch:	innxbatch.o  $(LIBNEWS)	; $(LINK) innxbatch.o  $(NEWSLIBS)
innxmit:	innxmit.o    $(LIBNEWS)	; $(LINK) innxmit.o    $(NEWSLIBS)
nntpget:	nntpget.o    $(LIBNEWS)	; $(LINK) nntpget.o    $(NEWSLIBS)
overchan:	overchan.o   $(LIBNEWS)	; $(LINK) overchan.o   $(NEWSLIBS)
shlock:		shlock.o		; $(LINK) shlock.o
shrinkfile:	shrinkfile.o $(LIBNEWS)	; $(LINK) shrinkfile.o $(NEWSLIBS)

buffchan:	buffchan.o map.o $(LIBNEWS)
	$(LINK) buffchan.o map.o $(NEWSLIBS)

filechan:	filechan.o map.o $(LIBNEWS)
	$(LINK) filechan.o map.o $(NEWSLIBS)

actmerge:	actmerge.in      $(FIX)	; $(FIX) actmerge.in
actsyncd:	actsyncd.in      $(FIX)	; $(FIX) actsyncd.in
controlbatch:	controlbatch.in  $(FIX)	; $(FIX) controlbatch.in
mod-active:	mod-active.in    $(FIX)	; $(FIX) mod-active.in
news2mail:	news2mail.in     $(FIX)	; $(FIX) news2mail.in
nntpsend:	nntpsend.in      $(FIX)	; $(FIX) nntpsend.in
send-ihave:	send-ihave.in    $(FIX)	; $(FIX) send-ihave.in
send-nntp:	send-nntp.in     $(FIX)	; $(FIX) send-nntp.in
send-uucp:	send-uucp.in     $(FIX)	; $(FIX) send-uucp.in
sendbatch:	sendbatch.in     $(FIX)	; $(FIX) sendbatch.in
sendxbatches:	sendxbatches.in  $(FIX)	; $(FIX) sendxbatches.in

$(LIBINN):
	(cd ../lib ; $(MAKE))
$(LIBSTORAGE):
	(cd ../storage ; $(MAKE))


##  Installation rules.  Installation commands set in Makefile.global.

install: $(INSTALLED)

$(D)$(PATHBIN)/actmerge:            actmerge            ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/actsync:             actsync             ; $(LI_XPUB) $? $@
$(D)$(PATHBIN)/actsyncd:            actsyncd            ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/archive:             archive             ; $(LI_XPUB) $? $@
$(D)$(PATHBIN)/batcher:             batcher             ; $(LI_XPUB) $? $@
$(D)$(PATHBIN)/buffchan:            buffchan            ; $(LI_XPUB) $? $@
$(D)$(PATHBIN)/controlbatch:        controlbatch        ; $(CP_XPRI) $? $@
$(D)$(PATHBIN)/controlchan:         controlchan         ; $(CP_XPRI) $? $@
$(D)$(PATHBIN)/cvtbatch:            cvtbatch            ; $(LI_XPUB) $? $@
$(D)$(PATHBIN)/filechan:            filechan            ; $(LI_XPUB) $? $@
$(D)$(PATHBIN)/inndf:               inndf               ; $(LI_XPUB) $? $@
$(D)$(PATHBIN)/innxbatch:           innxbatch           ; $(LI_XPUB) $? $@
$(D)$(PATHBIN)/innxmit:             innxmit             ; $(LI_XPUB) $? $@
$(D)$(PATHBIN)/mod-active:          mod-active          ; $(CP_XPRI) $? $@
$(D)$(PATHBIN)/news2mail:           news2mail           ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/nntpget:             nntpget             ; $(LI_XPUB) $? $@
$(D)$(PATHBIN)/nntpsend:            nntpsend            ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/overchan:            overchan            ; $(LI_XPRI) $? $@
$(D)$(PATHBIN)/pgpverify:           pgpverify           ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/send-ihave:          send-ihave          ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/send-nntp:           send-nntp           ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/send-uucp:           send-uucp           ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/sendbatch:           sendbatch           ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/sendxbatches:        sendxbatches        ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/shlock:              shlock              ; $(LI_XPUB) $? $@
$(D)$(PATHBIN)/shrinkfile:          shrinkfile          ; $(LI_XPUB) $? $@


##  Dependencies.  Default list, below, is probably good enough.
depend:		Makefile $(SOURCES)
	makedepend $(DEFS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
actsync.o:	../include/clibrary.h
actsync.o:	../include/configdata.h
actsync.o:	../include/libinn.h
actsync.o:	../include/macros.h
actsync.o:	../include/paths.h
actsync.o:	../include/qio.h
archive.o:	../include/clibrary.h
archive.o:	../include/configdata.h
archive.o:	../include/libinn.h
archive.o:	../include/macros.h
archive.o:	../include/paths.h
archive.o:	../include/qio.h
batcher.o:	../include/clibrary.h
batcher.o:	../include/configdata.h
batcher.o:	../include/libinn.h
batcher.o:	../include/macros.h
batcher.o:	../include/paths.h
buffchan.o:	../include/clibrary.h
buffchan.o:	../include/configdata.h
buffchan.o:	../include/libinn.h
buffchan.o:	../include/macros.h
buffchan.o:	../include/paths.h
buffchan.o:	../include/qio.h
cvtbatch.o:	../include/clibrary.h
cvtbatch.o:	../include/configdata.h
cvtbatch.o:	../include/libinn.h
cvtbatch.o:	../include/macros.h
cvtbatch.o:	../include/paths.h
cvtbatch.o:	../include/qio.h
filechan.o:	../include/clibrary.h
filechan.o:	../include/configdata.h
filechan.o:	../include/libinn.h
filechan.o:	../include/macros.h
filechan.o:	../include/paths.h
innxbatch.o:	../include/clibrary.h
innxbatch.o:	../include/configdata.h
innxbatch.o:	../include/libinn.h
innxbatch.o:	../include/macros.h
innxbatch.o:	../include/nntp.h 
innxmit.o:	../include/clibrary.h
innxmit.o:	../include/configdata.h
innxmit.o:	../include/dbz.h
innxmit.o:	../include/libinn.h
innxmit.o:	../include/macros.h
innxmit.o:	../include/nntp.h
innxmit.o:	../include/paths.h
innxmit.o:	../include/qio.h
nntpget.o:	../include/clibrary.h
nntpget.o:	../include/configdata.h
nntpget.o:	../include/dbz.h
nntpget.o:	../include/libinn.h
nntpget.o:	../include/macros.h
nntpget.o:	../include/nntp.h
nntpget.o:	../include/paths.h
shlock.o:	../include/clibrary.h
shlock.o:	../include/configdata.h
shrinkfile.o:	../include/clibrary.h
shrinkfile.o:	../include/configdata.h
shrinkfile.o:	../include/libinn.h
shrinkfile.o:	../include/macros.h
inndf.o:	../include/config.h

##  $Id$

include ../Makefile.global

CFLAGS	= $(GCFLAGS)

SOURCES	=	archive.c batcher.c buffchan.c cvtbatch.c filechan.c \
		innxmit.c innxbatch.c nntpget.c shlock.c \
		shrinkfile.c actsync.c inndf.c overchan.c

ALL	=	archive batcher buffchan cvtbatch filechan \
		innxmit innxbatch nntpget shlock \
		shrinkfile actsync actsyncd actmerge \
		sendxbatches inndf overchan

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

all:		$(ALL)

clobber clean:
	rm -f *.o $(ALL)
	rm -rf .libs
	rm -f all install profiled

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES) ../lib/*.c ../include/*.h

profiled:
	cp /dev/null profiled

archive:	$(P) archive.o $(LIBNEWS) 
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ archive.o $(LIBNEWS) $(LIBS)

batcher:	$(P) batcher.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ batcher.o $(LIBNEWS) $(LIBS)

buffchan:	$(P) buffchan.o map.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ buffchan.o map.o $(LIBNEWS) $(LIBS)

cvtbatch:	$(P) cvtbatch.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ cvtbatch.o $(LIBNEWS) $(LIBS)

filechan:	$(P) filechan.o map.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ filechan.o map.o $(LIBNEWS) $(LIBS)

nntpget:	$(P) nntpget.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ nntpget.o $(LIBNEWS) $(LIBS)

innxmit:	$(P) innxmit.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ innxmit.o $(LIBNEWS) $(LIBS)

innxbatch:	$(P) innxbatch.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ innxbatch.o $(LIBNEWS) $(LIBS)

actsync:	$(P) actsync.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ actsync.o $(LIBNEWS) $(LIBS)

shlock:		$(P) shlock.o
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ shlock.o

shrinkfile:	$(P) shrinkfile.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ shrinkfile.o $(LIBNEWS) $(LIBS)

overchan:	$(P) overchan.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ overchan.o $(LIBNEWS) $(LIBS)

inndf:	$(P) inndf.o
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ inndf.o $(LIBNEWS) $(LIBS)


$(LIBNEWS):
	(cd ../lib ; $(MAKE) )


##  Installation rules.

install: $(INSTALLED)

LT_XPRI = $(LIBTOOL) ../installit.sh $(OWNER) -m 0550 -b .OLD
LT_XPUB = $(LIBTOOL) ../installit.sh $(OWNER) -m 0555 -b .OLD
CP_XPRI = ../installit.sh $(OWNER) -m 0550 -b .OLD
CP_XPUB = ../installit.sh $(OWNER) -m 0555 -b .OLD

$(D)$(PATHBIN)/actmerge:            actmerge            ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/actsync:             actsync             ; $(LT_XPUB) $? $@
$(D)$(PATHBIN)/actsyncd:            actsyncd            ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/archive:             archive             ; $(LT_XPUB) $? $@
$(D)$(PATHBIN)/batcher:             batcher             ; $(LT_XPUB) $? $@
$(D)$(PATHBIN)/buffchan:            buffchan            ; $(LT_XPUB) $? $@
$(D)$(PATHBIN)/controlbatch:        controlbatch        ; $(CP_XPRI) $? $@
$(D)$(PATHBIN)/controlchan:         controlchan         ; $(CP_XPRI) $? $@
$(D)$(PATHBIN)/cvtbatch:            cvtbatch            ; $(LT_XPUB) $? $@
$(D)$(PATHBIN)/filechan:            filechan            ; $(LT_XPUB) $? $@
$(D)$(PATHBIN)/inndf:               inndf               ; $(LT_XPUB) $? $@
$(D)$(PATHBIN)/innxbatch:           innxbatch           ; $(LT_XPUB) $? $@
$(D)$(PATHBIN)/innxmit:             innxmit             ; $(LT_XPUB) $? $@
$(D)$(PATHBIN)/mod-active:          mod-active          ; $(CP_XPRI) $? $@
$(D)$(PATHBIN)/news2mail:           news2mail           ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/nntpget:             nntpget             ; $(LT_XPUB) $? $@
$(D)$(PATHBIN)/nntpsend:            nntpsend            ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/overchan:            overchan            ; $(LT_XPRI) $? $@
$(D)$(PATHBIN)/pgpverify:           pgpverify           ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/send-ihave:          send-ihave          ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/send-nntp:           send-nntp           ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/send-uucp:           send-uucp           ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/sendbatch:           sendbatch           ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/sendxbatches:        sendxbatches        ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/shlock:              shlock              ; $(LT_XPUB) $? $@
$(D)$(PATHBIN)/shrinkfile:          shrinkfile          ; $(LT_XPUB) $? $@


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

##  $Id$

include ../Makefile.global

CFLAGS        = $(GCFLAGS)

ALL	      = c7unbatch cnfsheadconf cnfsstat ctlinnd decode encode \
		getlist gunbatch inews innconfval mailpost pullnews \
		rnews scanspool signcontrol sm

SOURCES	      = ctlinnd.c decode.c encode.c getlist.c inews.c innconfval.c \
		rnews.c sm.c

PATHRNEWS     = $(PATHBIN)/rnews.libexec

INSTALLED     = $(D)$(PATHBIN)/cnfsheadconf	\
		$(D)$(PATHBIN)/cnfsstat		\
		$(D)$(PATHBIN)/ctlinnd		\
		$(D)$(PATHBIN)/getlist		\
		$(D)$(PATHBIN)/inews		\
		$(D)$(PATHBIN)/innconfval	\
		$(D)$(PATHBIN)/mailpost		\
		$(D)$(PATHBIN)/pullnews		\
		$(D)$(PATHBIN)/rnews		\
		$(D)$(PATHBIN)/scanspool	\
		$(D)$(PATHBIN)/signcontrol	\
		$(D)$(PATHBIN)/sm		\
		$(D)$(PATHRNEWS)/c7unbatch	\
		$(D)$(PATHRNEWS)/decode		\
		$(D)$(PATHRNEWS)/encode		\
		$(D)$(PATHRNEWS)/gunbatch

all: $(ALL)

clean:
	rm -f *.o $(ALL)
	rm -rf .libs

clobber: clean
	rm -f tags

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES)


##  Compilation rules.

LINK 		= $(LIBTOOL) $(CC) $(LDFLAGS) -o $@
NEWSLIBS	= $(LIBNEWS) $(LIBS)
FIX		= $(FIXSCRIPT)

ctlinnd:	ctlinnd.o    $(LIBNEWS)	; $(LINK) ctlinnd.o    $(NEWSLIBS)
decode:		decode.o		; $(LINK) decode.o
encode:		encode.o		; $(LINK) encode.o
getlist:	getlist.o    $(LIBNEWS)	; $(LINK) getlist.o    $(NEWSLIBS)
inews:		inews.o      $(LIBNEWS)	; $(LINK) inews.o      $(NEWSLIBS)
innconfval:	innconfval.o $(LIBNEWS)	; $(LINK) innconfval.o $(NEWSLIBS)
rnews:		rnews.o      $(LIBNEWS)	; $(LINK) rnews.o      $(NEWSLIBS)
sm:		sm.o         $(LIBNEWS)	; $(LINK) sm.o         $(NEWSLIBS)

cnfsheadconf:	cnfsheadconf.in  $(FIX)	; $(FIX) cnfsheadconf.in
cnfsstat:	cnfsstat.in      $(FIX)	; $(FIX) cnfsstat.in
mailpost:	mailpost.in      $(FIX)	; $(FIX) mailpost.in
pullnews:	pullnews.in      $(FIX)	; $(FIX) -i pullnews.in
scanspool:	scanspool.in     $(FIX)	; $(FIX) scanspool.in
signcontrol:	signcontrol.in   $(FIX)	; $(FIX) -i signcontrol.in

c7unbatch:	../Makefile.global
	echo '#! $(SHELL)' > c7unbatch
	echo 'decode | $(COMPRESS) -d' >> c7unbatch

gunbatch:	../Makefile.global
	echo '#! $(SHELL)' > gunbatch
	echo 'exec $(GZIP) -d -c' >> gunbatch

##  Not normally built.
feedone:	feedone.o    $(LIBNEWS)	; $(LINK) feedone.o    $(NEWSLIBS)
sys2nf:		sys2nf.o     $(LIBNEWS)	; $(LINK) sys2nf.o     $(NEWSLIBS)

$(LIBINN):
	(cd ../lib ; $(MAKE))
$(LIBSTORAGE):
	(cd ../storage ; $(MAKE))


##  Installation rules.  Installation commands set in Makefile.global.

install: $(INSTALLED)

$(D)$(PATHBIN)/inews:               inews               ; $(LI_INEWS) $? $@
$(D)$(PATHBIN)/rnews:               rnews               ; $(LI_RNEWS) $? $@

$(D)$(PATHBIN)/cnfsheadconf:        cnfsheadconf        ; $(CP_XPRI) $? $@
$(D)$(PATHBIN)/cnfsstat:            cnfsstat            ; $(CP_XPRI) $? $@
$(D)$(PATHBIN)/ctlinnd:             ctlinnd             ; $(LI_XPRI) $? $@
$(D)$(PATHBIN)/getlist:             getlist             ; $(LI_XPUB) $? $@
$(D)$(PATHBIN)/innconfval:          innconfval          ; $(LI_XPUB) $? $@
$(D)$(PATHBIN)/mailpost:            mailpost            ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/pullnews:            pullnews            ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/scanspool:           scanspool           ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/signcontrol:         signcontrol         ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/sm:                  sm                  ; $(LI_XPUB) $? $@
$(D)$(PATHRNEWS)/c7unbatch:         c7unbatch           ; $(CP_XPUB) $? $@
$(D)$(PATHRNEWS)/decode:            decode              ; $(LI_XPUB) $? $@
$(D)$(PATHRNEWS)/encode:            encode              ; $(LI_XPUB) $? $@
$(D)$(PATHRNEWS)/gunbatch:          gunbatch            ; $(CP_XPUB) $? $@


##  Dependencies.  Default list, below, is probably good enough.
depend:		Makefile $(SOURCES)
	makedepend $(DEFS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
ctlinnd.o:	../include/clibrary.h
ctlinnd.o:	../include/configdata.h
ctlinnd.o:	../include/inndcomm.h
ctlinnd.o:	../include/libinn.h
ctlinnd.o:	../include/macros.h
ctlinnd.o:	../include/paths.h
sm.o:		../include/configdata.h
sm.o:		../include/clibrary.h
sm.o:		../include/libinn.h
sm.o:		../include/macros.h
decode.o:	../include/clibrary.h
decode.o:	../include/configdata.h
encode.o:	../include/clibrary.h
encode.o:	../include/configdata.h
getlist:	../include/clibrary.h
getlist:	../include/configdata.h
getlist:	../include/libinn.h
getlist:	../include/macros.h
getlist:	../include/paths.h
getlist:	../include/qio.h
inews.o:	../include/clibrary.h
inews.o:	../include/configdata.h
inews.o:	../include/libinn.h
inews.o:	../include/macros.h
inews.o:	../include/nntp.h
inews.o:	../include/paths.h
innconfval.o:	../include/clibrary.h
innconfval.o:	../include/configdata.h
innconfval.o:	../include/libinn.h
innconfval.o:	../include/macros.h
rnews.o:	../include/clibrary.h
rnews.o:	../include/configdata.h
rnews.o:	../include/libinn.h
rnews.o:	../include/macros.h
rnews.o:	../include/nntp.h
rnews.o:	../include/paths.h

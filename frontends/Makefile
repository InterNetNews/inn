##  $Id$

include ../Makefile.global

CFLAGS        = $(GCFLAGS)

ALL	      = c7unbatch cnfsheadconf cnfsstat ctlinnd decode encode \
		getlist gunbatch inews innconfval mailpost pullnews \
		ovdb_recover rnews scanspool signcontrol sm

SOURCES	      = ctlinnd.c decode.c encode.c getlist.c inews.c innconfval.c \
		ovdb_recover.c rnews.c sm.c

PATHRNEWS     = $(PATHBIN)/rnews.libexec

INSTALLED     = $(D)$(PATHBIN)/cnfsheadconf	\
		$(D)$(PATHBIN)/cnfsstat		\
		$(D)$(PATHBIN)/ctlinnd		\
		$(D)$(PATHBIN)/getlist		\
		$(D)$(PATHBIN)/inews		\
		$(D)$(PATHBIN)/innconfval	\
		$(D)$(PATHBIN)/mailpost		\
		$(D)$(PATHBIN)/ovdb_recover	\
		$(D)$(PATHBIN)/ovdb_upgrade	\
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

tags ctags: $(SOURCES)
	$(CTAGS) $(SOURCES)


##  Compilation rules.

BOTH		= $(LIBSTORAGE) $(LIBINN)

LINK 		= $(LIBTOOL) $(CC) $(LDFLAGS) -o $@
INNLIBS		= $(LIBINN) $(LIBS)
STORELIBS	= $(BOTH) $(EXTSTORAGELIBS) $(LIBS)

FIX		= $(FIXSCRIPT)

ctlinnd:	ctlinnd.o      $(LIBINN) ; $(LINK) ctlinnd.o      $(INNLIBS)
decode:		decode.o                 ; $(LINK) decode.o
encode:		encode.o                 ; $(LINK) encode.o
getlist:	getlist.o      $(LIBINN) ; $(LINK) getlist.o      $(INNLIBS)
inews:		inews.o        $(LIBINN) ; $(LINK) inews.o        $(INNLIBS)
innconfval:	innconfval.o   $(LIBINN) ; $(LINK) innconfval.o   $(INNLIBS)
ovdb_recover:	ovdb_recover.o $(BOTH)   ; $(LINK) ovdb_recover.o $(STORELIBS)
rnews:		rnews.o        $(LIBINN) ; $(LINK) rnews.o        $(INNLIBS)
sm:		sm.o           $(BOTH)   ; $(LINK) sm.o           $(STORELIBS)

ovdb_recover.o: ovdb_recover.c
	$(CC) $(CFLAGS) $(BERKELEY_DB_CFLAGS) -c $<

cnfsheadconf:	cnfsheadconf.in  $(FIX)	; $(FIX) cnfsheadconf.in
cnfsstat:	cnfsstat.in      $(FIX)	; $(FIX) cnfsstat.in
mailpost:	mailpost.in      $(FIX)	; $(FIX) mailpost.in
pullnews:	pullnews.in      $(FIX)	; $(FIX) -i pullnews.in
scanspool:	scanspool.in     $(FIX)	; $(FIX) scanspool.in
signcontrol:	signcontrol.in   $(FIX)	; $(FIX) -i signcontrol.in

c7unbatch: Makefile ../Makefile.global
	( echo '#! $(SHELL)' ; echo 'decode | $(UNCOMPRESS)' ) > $@

gunbatch: Makefile ../Makefile.global
	( echo '#! $(SHELL)' ; echo 'exec $(GZIP) -d -c' ) > $@

##  Not normally built.
feedone:	feedone.o     $(LIBINN)	; $(LINK) feedone.o $(INNLIBS)
sys2nf:		sys2nf.o      $(LIBINN)	; $(LINK) sys2nf.o  $(INNLIBS)

$(LIBINN):	; (cd ../lib ; $(MAKE))
$(LIBSTORAGE):	; (cd ../storage ; $(MAKE))


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
$(D)$(PATHBIN)/ovdb_recover:        ovdb_recover	; $(LI_XPRI) $? $@
$(D)$(PATHBIN)/pullnews:            pullnews            ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/scanspool:           scanspool           ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/signcontrol:         signcontrol         ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/sm:                  sm                  ; $(LI_XPUB) $? $@
$(D)$(PATHRNEWS)/c7unbatch:         c7unbatch           ; $(CP_XPUB) $? $@
$(D)$(PATHRNEWS)/decode:            decode              ; $(LI_XPUB) $? $@
$(D)$(PATHRNEWS)/encode:            encode              ; $(LI_XPUB) $? $@
$(D)$(PATHRNEWS)/gunbatch:          gunbatch            ; $(CP_XPUB) $? $@

$(D)$(PATHBIN)/ovdb_upgrade: Makefile
	rm -f $@
	ln -s ovdb_recover $@


##  Dependencies.  Default list, below, is probably good enough.

depend:	Makefile $(SOURCES)
	$(MAKEDEPEND) -DUSE_BERKELEY_DB $(CFLAGS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
ctlinnd.o: ctlinnd.c ../include/configdata.h ../include/config.h \
 ../include/clibrary.h ../include/libinn.h ../include/inndcomm.h \
 ../include/paths.h ../include/macros.h
decode.o: decode.c ../include/configdata.h ../include/config.h \
 ../include/clibrary.h
encode.o: encode.c ../include/configdata.h ../include/config.h \
 ../include/clibrary.h
getlist.o: getlist.c ../include/config.h ../include/clibrary.h \
 ../include/macros.h ../include/libinn.h ../include/qio.h \
 ../include/paths.h
inews.o: inews.c ../include/config.h ../include/clibrary.h \
 ../include/libinn.h ../include/macros.h ../include/nntp.h \
 ../include/paths.h
innconfval.o: innconfval.c ../include/config.h ../include/clibrary.h \
 ../include/libinn.h ../include/macros.h ../include/paths.h
ovdb_recover.o: ovdb_recover.c ../include/config.h \
 ../include/clibrary.h ../include/libinn.h ../include/ov.h \
 ../include/storage.h ../storage/ovdb/ovdb.h
rnews.o: rnews.c ../include/config.h ../include/clibrary.h \
 ../include/libinn.h ../include/macros.h ../include/nntp.h \
 ../include/paths.h
sm.o: sm.c ../include/config.h ../include/clibrary.h \
 ../include/libinn.h ../include/macros.h ../include/paths.h \
 ../include/qio.h

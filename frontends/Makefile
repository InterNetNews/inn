##  $Id$

include ../Makefile.global

top           = ..
CFLAGS        = $(GCFLAGS)

ALL	      = c7unbatch cnfsheadconf cnfsstat ctlinnd decode encode \
		getlist gunbatch inews innconfval mailpost pullnews \
		ovdb_init ovdb_monitor ovdb_server ovdb_stat rnews \
		scanspool sm

SOURCES	      = ctlinnd.c decode.c encode.c getlist.c inews.c innconfval.c \
		ovdb_init.c ovdb_monitor.c ovdb_server.c ovdb_stat.c rnews.c \
		sm.c

PATHRNEWS     = $(PATHBIN)/rnews.libexec

INSTALLED     = $(D)$(PATHBIN)/cnfsheadconf	\
		$(D)$(PATHBIN)/cnfsstat		\
		$(D)$(PATHBIN)/ctlinnd		\
		$(D)$(PATHBIN)/getlist		\
		$(D)$(PATHBIN)/inews		\
		$(D)$(PATHBIN)/innconfval	\
		$(D)$(PATHBIN)/mailpost		\
		$(D)$(PATHBIN)/ovdb_init	\
		$(D)$(PATHBIN)/ovdb_monitor	\
		$(D)$(PATHBIN)/ovdb_server	\
		$(D)$(PATHBIN)/ovdb_stat	\
		$(D)$(PATHBIN)/pullnews		\
		$(D)$(PATHBIN)/rnews		\
		$(D)$(PATHBIN)/scanspool	\
		$(D)$(PATHBIN)/sm		\
		$(D)$(PATHRNEWS)/c7unbatch	\
		$(D)$(PATHRNEWS)/decode		\
		$(D)$(PATHRNEWS)/encode		\
		$(D)$(PATHRNEWS)/gunbatch

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

BOTH		= $(LIBSTORAGE) $(LIBHIST) $(LIBINN)

LINK 		= $(LIBLD) $(LDFLAGS) -o $@
INNLIBS		= $(LIBINN) $(LIBS)
STORELIBS	= $(BOTH) $(EXTSTORAGELIBS) $(LIBS)

FIX		= $(FIXSCRIPT)

ctlinnd:	ctlinnd.o      $(LIBINN) ; $(LINK) ctlinnd.o      $(INNLIBS)
decode:		decode.o       $(LIBINN) ; $(LINK) decode.o       $(INNLIBS)
encode:		encode.o                 ; $(LINK) encode.o
getlist:	getlist.o      $(LIBINN) ; $(LINK) getlist.o      $(INNLIBS)
inews:		inews.o        $(LIBINN) ; $(LINK) inews.o        $(INNLIBS)
innconfval:	innconfval.o   $(LIBINN) ; $(LINK) innconfval.o   $(INNLIBS)
ovdb_init:	ovdb_init.o    $(BOTH)   ; $(LINK) ovdb_init.o    $(STORELIBS)
ovdb_monitor:	ovdb_monitor.o $(BOTH)   ; $(LINK) ovdb_monitor.o $(STORELIBS)
ovdb_server:	ovdb_server.o  $(BOTH)   ; $(LINK) ovdb_server.o  $(STORELIBS)
ovdb_stat:	ovdb_stat.o    $(BOTH)   ; $(LINK) ovdb_stat.o    $(STORELIBS)
rnews:		rnews.o        $(LIBINN) ; $(LINK) rnews.o        $(INNLIBS)
sm:		sm.o           $(BOTH)   ; $(LINK) sm.o           $(STORELIBS)

ovdb_init.o: ovdb_init.c
	$(CC) $(CFLAGS) $(BERKELEY_DB_CFLAGS) -c $<

ovdb_monitor.o: ovdb_monitor.c
	$(CC) $(CFLAGS) $(BERKELEY_DB_CFLAGS) -c $<

ovdb_server.o: ovdb_server.c
	$(CC) $(CFLAGS) $(BERKELEY_DB_CFLAGS) -c $<

ovdb_stat.o: ovdb_stat.c
	$(CC) $(CFLAGS) $(BERKELEY_DB_CFLAGS) -c $<

cnfsheadconf:	cnfsheadconf.in  $(FIX)	; $(FIX) cnfsheadconf.in
cnfsstat:	cnfsstat.in      $(FIX)	; $(FIX) cnfsstat.in
mailpost:	mailpost.in      $(FIX)	; $(FIX) mailpost.in
pullnews:	pullnews.in      $(FIX)	; $(FIX) -i pullnews.in
scanspool:	scanspool.in     $(FIX)	; $(FIX) scanspool.in

c7unbatch: Makefile ../Makefile.global
	( echo '#! $(SHELL)' ; echo 'decode | $(UNCOMPRESS)' ) > $@
	chmod 755 c7unbatch

gunbatch: Makefile ../Makefile.global
	( echo '#! $(SHELL)' ; echo 'exec $(GZIP) -d -c' ) > $@
	chmod 755 gunbatch

##  Not normally built.
feedone:	feedone.o     $(LIBINN)	; $(LINK) feedone.o $(INNLIBS)
sys2nf:		sys2nf.o      $(LIBINN)	; $(LINK) sys2nf.o  $(INNLIBS)

$(LIBINN):	; (cd ../lib ; $(MAKE))
$(LIBSTORAGE):	; (cd ../storage ; $(MAKE))
$(LIBHIST):	; (cd ../history ; $(MAKE))


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
$(D)$(PATHBIN)/ovdb_init:           ovdb_init		; $(LI_XPRI) $? $@
$(D)$(PATHBIN)/ovdb_monitor:        ovdb_monitor	; $(LI_XPRI) $? $@
$(D)$(PATHBIN)/ovdb_server:         ovdb_server		; $(LI_XPRI) $? $@
$(D)$(PATHBIN)/ovdb_stat:           ovdb_stat		; $(LI_XPRI) $? $@
$(D)$(PATHBIN)/pullnews:            pullnews            ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/scanspool:           scanspool           ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/sm:                  sm                  ; $(LI_XPUB) $? $@
$(D)$(PATHRNEWS)/c7unbatch:         c7unbatch           ; $(CP_XPUB) $? $@
$(D)$(PATHRNEWS)/decode:            decode              ; $(LI_XPUB) $? $@
$(D)$(PATHRNEWS)/encode:            encode              ; $(LI_XPUB) $? $@
$(D)$(PATHRNEWS)/gunbatch:          gunbatch            ; $(CP_XPUB) $? $@


##  Dependencies.  Default list, below, is probably good enough.

depend:	Makefile $(SOURCES)
	$(MAKEDEPEND) '$(CFLAGS)' $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
ctlinnd.o: ctlinnd.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inndcomm.h ../include/libinn.h \
 ../include/macros.h ../include/paths.h
decode.o: decode.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h
encode.o: encode.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h
getlist.o: getlist.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/qio.h ../include/macros.h \
 ../include/libinn.h ../include/paths.h
inews.o: inews.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/portable/time.h ../include/libinn.h \
 ../include/macros.h ../include/nntp.h ../include/paths.h
innconfval.o: innconfval.c ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h ../include/libinn.h \
 ../include/macros.h ../include/paths.h
ovdb_init.o: ovdb_init.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h ../include/ov.h \
 ../include/storage.h ../include/inn/history.h ../storage/ovdb/ovdb.h \
 ../storage/ovdb/ovdb-private.h
ovdb_monitor.o: ovdb_monitor.c ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h ../include/libinn.h \
 ../include/ov.h ../include/storage.h ../include/inn/history.h \
 ../storage/ovdb/ovdb.h ../storage/ovdb/ovdb-private.h
ovdb_server.o: ovdb_server.c ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h ../include/libinn.h \
 ../include/macros.h ../include/paths.h ../include/storage.h \
 ../include/ov.h ../include/inn/history.h ../storage/ovdb/ovdb.h \
 ../storage/ovdb/ovdb-private.h ../include/inn/version.h
ovdb_stat.o: ovdb_stat.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h ../include/macros.h \
 ../include/paths.h ../include/storage.h ../include/ov.h \
 ../include/inn/history.h ../storage/ovdb/ovdb.h \
 ../storage/ovdb/ovdb-private.h
rnews.o: rnews.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/portable/wait.h ../include/libinn.h \
 ../include/macros.h ../include/nntp.h ../include/paths.h
sm.o: sm.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h ../include/macros.h \
 ../include/paths.h ../include/storage.h

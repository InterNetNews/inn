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

all: $(ALL)

warnings:
	$(MAKE) COPT='$(WARNINGS)' all

install: all
	$(LI_INEWS) inews $D$(PATHBIN)/inews
	$(LI_RNEWS) rnews $D$(PATHBIN)/rnews
	$(CP_XPRI) cnfsheadconf $D$(PATHBIN)/cnfsheadconf
	for F in cnfsstat mailpost pullnews scanspool ; do \
	    $(CP_XPUB) $$F $D$(PATHBIN)/$$F ; \
	done
	for F in ctlinnd ovdb_init ovdb_monitor ovdb_server ovdb_stat ; do \
	    $(LI_XPRI) $$F $D$(PATHBIN)/$$F ; \
	done
	for F in getlist innconfval sm ; do \
	    $(LI_XPUB) $$F $D$(PATHBIN)/$$F ; \
	done
	$(CP_XPUB) c7unbatch $D$(PATHBIN)/rnews.libexec/c7unbatch
	$(LI_XPUB) decode $D$(PATHBIN)/rnews.libexec/decode
	$(LI_XPUB) encode $D$(PATHBIN)/rnews.libexec/encode
	$(CP_XPUB) gunbatch $D$(PATHBIN)/rnews.libexec/gunbatch

clean:
	rm -f *.o $(ALL)
	rm -rf .libs

clobber distclean: clean
	rm -f tags

tags ctags: $(SOURCES)
	$(CTAGS) $(SOURCES)

profiled:
	$(MAKEPROFILING) all

$(FIXSCRIPT):
	@echo Run configure before running make.  See INSTALL for details.
	@exit 1


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
rnews:		rnews.o        $(LIBINN) ; $(LINK) rnews.o        $(STORELIBS)
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


##  Dependencies.  Default list, below, is probably good enough.

depend:	Makefile $(SOURCES)
	$(MAKEDEPEND) '$(CFLAGS)' $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
ctlinnd.o: ctlinnd.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/innconf.h \
 ../include/inn/messages.h ../include/inndcomm.h ../include/libinn.h \
 ../include/paths.h
decode.o: decode.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/messages.h
encode.o: encode.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h
getlist.o: getlist.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/innconf.h \
 ../include/inn/messages.h ../include/inn/qio.h ../include/libinn.h \
 ../include/paths.h
inews.o: inews.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/portable/time.h \
 ../include/inn/innconf.h ../include/inn/messages.h \
 ../include/libinn.h ../include/nntp.h ../include/paths.h
innconfval.o: innconfval.c ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h \
 ../include/inn/innconf.h ../include/inn/messages.h \
 ../include/libinn.h
ovdb_init.o: ovdb_init.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h ../include/inn/innconf.h \
 ../include/inn/messages.h ../include/ov.h ../include/storage.h \
 ../include/inn/history.h ../storage/ovdb/ovdb.h \
 ../storage/ovdb/ovdb-private.h
ovdb_monitor.o: ovdb_monitor.c ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h \
 ../include/portable/setproctitle.h ../include/portable/wait.h \
 ../include/inn/innconf.h ../include/inn/messages.h \
 ../include/libinn.h ../include/ov.h ../include/storage.h \
 ../include/inn/history.h ../storage/ovdb/ovdb.h \
 ../storage/ovdb/ovdb-private.h
ovdb_server.o: ovdb_server.c ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h \
 ../include/portable/mmap.h ../include/portable/time.h \
 ../include/portable/setproctitle.h ../include/portable/socket.h \
 ../include/portable/wait.h ../include/inn/innconf.h \
 ../include/inn/messages.h ../include/libinn.h ../include/paths.h \
 ../include/storage.h ../include/ov.h ../include/inn/history.h \
 ../storage/ovdb/ovdb.h ../storage/ovdb/ovdb-private.h
ovdb_stat.o: ovdb_stat.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/innconf.h \
 ../include/inn/messages.h ../include/libinn.h ../include/ov.h \
 ../include/storage.h ../include/inn/history.h ../include/paths.h \
 ../storage/ovdb/ovdb.h ../storage/ovdb/ovdb-private.h
rnews.o: rnews.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/portable/wait.h \
 ../include/inn/innconf.h ../include/inn/messages.h \
 ../include/inn/wire.h ../include/libinn.h ../include/nntp.h \
 ../include/paths.h ../include/storage.h
sm.o: sm.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/innconf.h \
 ../include/inn/messages.h ../include/inn/qio.h ../include/storage.h

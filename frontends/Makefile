include ../Makefile.global

top           = ..
CFLAGS        = $(GCFLAGS)

ALL	      = bunbatch c7unbatch cnfsheadconf cnfsstat ctlinnd decode encode \
		getlist gunbatch inews innconfval mailpost pullnews \
		ovdb_init ovdb_monitor ovdb_server ovdb_stat rnews \
		scanspool sm

OPTIONAL      = feedone sys2nf

MAN	      = ../doc/man/mailpost.8

SOURCES	      = ctlinnd.c decode.c encode.c getlist.c inews.c innconfval.c \
		ovdb_init.c ovdb_monitor.c ovdb_server.c ovdb_stat.c rnews.c \
		sm.c

EXTRA_SOURCES = feedone.c sys2nf.c

PATHRNEWS     = $(PATHBIN)/rnews.libexec

all: $(ALL) $(MAN)

optional:
	$(MAKE) COPT="$(COPT) $(WARNINGS)" $(OPTIONAL)

warnings:
	$(MAKE) COPT="$(COPT) $(WARNINGS)" all

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
	$(CP_XPUB) bunbatch $D$(PATHBIN)/rnews.libexec/bunbatch
	$(CP_XPUB) c7unbatch $D$(PATHBIN)/rnews.libexec/c7unbatch
	$(LI_XPUB) decode $D$(PATHBIN)/rnews.libexec/decode
	$(LI_XPUB) encode $D$(PATHBIN)/rnews.libexec/encode
	$(CP_XPUB) gunbatch $D$(PATHBIN)/rnews.libexec/gunbatch

bootstrap: $(MAN)

clean clobber distclean:
	rm -f *.o $(ALL) $(OPTIONAL)
	rm -rf .libs

maintclean: distclean
	rm -f $(MAN)

profiled:
	$(MAKEPROFILING) all

$(FIXSCRIPT):
	@echo Run configure before running make.  See INSTALL for details.
	@exit 1


##  Compilation rules.

# $(LIBSTORAGE) must be listed twice to resolve circular dependencies
# when doing static linking.
BOTH		= $(LIBSTORAGE) $(LIBHIST) $(LIBSTORAGE) $(LIBINN)

LINK 		= $(LIBLD) $(LDFLAGS) -o $@
LINKDEPS	= $(LIBLDDEPS) $(LDFLAGS) -o $@
INNLIBS		= $(LIBINN) $(LIBS)
STORELIBS	= $(BOTH) $(STORAGE_LIBS) $(LIBS)

FIX		= $(SHELL) $(FIXSCRIPT)

ctlinnd:	ctlinnd.o      $(LIBINN) ; $(LINK) ctlinnd.o      $(INNLIBS)
decode:		decode.o       $(LIBINN) ; $(LINK) decode.o       $(INNLIBS)
encode:		encode.o                 ; $(LINK) encode.o
getlist:	getlist.o      $(LIBINN) ; $(LINK) getlist.o      $(INNLIBS)
inews:		inews.o        $(LIBINN) ; $(LINK) inews.o        $(INNLIBS)
innconfval:	innconfval.o   $(LIBINN) ; $(LINK) innconfval.o   $(INNLIBS)
ovdb_init:	ovdb_init.o    $(BOTH)
	$(LINKDEPS) ovdb_init.o    $(STORELIBS)
ovdb_monitor:	ovdb_monitor.o $(BOTH)
	$(LINKDEPS) ovdb_monitor.o $(STORELIBS)
ovdb_server:	ovdb_server.o  $(BOTH)
	$(LINKDEPS) ovdb_server.o  $(STORELIBS)
ovdb_stat:	ovdb_stat.o    $(BOTH)
	$(LINKDEPS) ovdb_stat.o    $(STORELIBS)
rnews:		rnews.o        $(BOTH)   ; $(LINK) rnews.o        $(STORELIBS)
sm:		sm.o           $(BOTH)
	$(LINKDEPS) sm.o           $(STORELIBS)

ovdb_init.o: ovdb_init.c
	$(CC) $(CFLAGS) $(BDB_CPPFLAGS) -c $<

ovdb_monitor.o: ovdb_monitor.c
	$(CC) $(CFLAGS) $(BDB_CPPFLAGS) -c $<

ovdb_server.o: ovdb_server.c
	$(CC) $(CFLAGS) $(BDB_CPPFLAGS) -c $<

ovdb_stat.o: ovdb_stat.c
	$(CC) $(CFLAGS) $(BDB_CPPFLAGS) -c $<

cnfsheadconf:	cnfsheadconf.in  $(FIXSCRIPT)	; $(FIX) cnfsheadconf.in
cnfsstat:	cnfsstat.in      $(FIXSCRIPT)	; $(FIX) cnfsstat.in
mailpost:	mailpost.in      $(FIXSCRIPT)	; $(FIX) mailpost.in
pullnews:	pullnews.in      $(FIXSCRIPT)	; $(FIX) pullnews.in
scanspool:	scanspool.in     $(FIXSCRIPT)	; $(FIX) scanspool.in

bunbatch: Makefile ../Makefile.global
	( echo '#! $(SHELL)' ; echo 'exec $(BZIP2) -d -c' ) > $@
	chmod 755 bunbatch

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
$(LIBSTORAGE):	; (cd ../storage ; $(MAKE) library)
$(LIBHIST):	; (cd ../history ; $(MAKE))


../doc/man/mailpost.8: mailpost.in
	$(POD2MAN) -s 8 -n "MAILPOST" $? > $@


##  Dependencies.  Default list, below, is probably good enough.

depend:	Makefile $(SOURCES) $(EXTRA_SOURCES)
	$(MAKEDEPEND) '$(CFLAGS)' $(SOURCES) $(EXTRA_SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
ctlinnd.o: ctlinnd.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/inndcomm.h ../include/inn/system.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/xwrite.h ../include/inn/messages.h ../include/inn/paths.h
decode.o: decode.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/messages.h
encode.o: encode.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h
getlist.o: getlist.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/nntp.h ../include/inn/paths.h \
  ../include/inn/qio.h ../include/inn/vector.h
inews.o: inews.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/newsuser.h \
  ../include/inn/nntp.h ../include/inn/paths.h
innconfval.o: innconfval.c ../include/portable/system.h \
  ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/portable/macros.h ../include/portable/stdbool.h \
  ../include/inn/confparse.h ../include/inn/macros.h \
  ../include/inn/portable-stdbool.h ../include/inn/innconf.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/secrets.h \
  ../include/inn/version.h
ovdb_init.o: ovdb_init.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/libinn.h \
  ../include/inn/concat.h ../include/inn/macros.h \
  ../include/inn/portable-stdbool.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h ../include/inn/ov.h \
  ../include/inn/history.h ../include/inn/storage.h \
  ../include/inn/options.h ../storage/ovdb/ovdb-private.h \
  ../storage/ovdb/ovdb.h ../include/inn/innconf.h \
  ../include/inn/messages.h
ovdb_monitor.o: ovdb_monitor.c ../include/portable/system.h \
  ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/portable/macros.h ../include/portable/stdbool.h \
  ../include/portable/setproctitle.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/ov.h ../include/inn/history.h \
  ../include/inn/storage.h ../include/inn/options.h \
  ../storage/ovdb/ovdb-private.h ../storage/ovdb/ovdb.h
ovdb_server.o: ovdb_server.c ../include/portable/system.h \
  ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/portable/macros.h ../include/portable/stdbool.h \
  ../include/portable/mmap.h ../include/portable/setproctitle.h \
  ../include/portable/socket.h ../include/portable/getaddrinfo.h \
  ../include/portable/getnameinfo.h ../include/portable/socket-unix.h \
  ../include/inn/fdflag.h ../include/inn/portable-socket.h \
  ../include/inn/portable-stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/libinn.h ../include/inn/concat.h \
  ../include/inn/xmalloc.h ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/ov.h ../include/inn/history.h \
  ../include/inn/storage.h ../include/inn/options.h ../include/inn/paths.h \
  ../storage/ovdb/ovdb-private.h ../storage/ovdb/ovdb.h
ovdb_stat.o: ovdb_stat.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/ov.h ../include/inn/history.h \
  ../include/inn/storage.h ../include/inn/options.h ../include/inn/paths.h \
  ../storage/ovdb/ovdb-private.h ../storage/ovdb/ovdb.h
rnews.o: rnews.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/fdflag.h \
  ../include/inn/portable-socket.h ../include/inn/system.h \
  ../include/inn/portable-getaddrinfo.h \
  ../include/inn/portable-getnameinfo.h ../include/inn/portable-stdbool.h \
  ../include/inn/innconf.h ../include/inn/macros.h ../include/inn/libinn.h \
  ../include/inn/concat.h ../include/inn/xmalloc.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/newsuser.h \
  ../include/inn/nntp.h ../include/inn/paths.h ../include/inn/storage.h \
  ../include/inn/options.h ../include/inn/wire.h
sm.o: sm.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/buffer.h \
  ../include/inn/portable-stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/libinn.h ../include/inn/concat.h \
  ../include/inn/xmalloc.h ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/qio.h ../include/inn/storage.h \
  ../include/inn/options.h ../include/inn/wire.h
feedone.o: feedone.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/libinn.h \
  ../include/inn/concat.h ../include/inn/macros.h \
  ../include/inn/portable-stdbool.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/nntp.h
sys2nf.o: sys2nf.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h ../include/inn/nntp.h \
  ../include/inn/paths.h

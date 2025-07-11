include ../Makefile.global

top		= ..
CFLAGS		= $(GCFLAGS) $(SSL_CPPFLAGS) $(SASL_CPPFLAGS) $(ZLIB_CPPFLAGS) \
		  $(BLACKLIST_CPPFLAGS)

ALL		= nnrpd

SOURCES		= article.c auth-ext.c cache.c commands.c group.c line.c \
		  list.c misc.c newnews.c nnrpd.c perl.c perm.c post.c \
		  python.c sasl.c tls.c track.c zlib.c

INCLUDES	= cache.h nnrpd.h post.h tls.h

OBJECTS		= $(SOURCES:.c=.o)

all: $(ALL)

warnings:
	$(MAKE) COPT="$(COPT) $(WARNINGS)" all

install: all
	$(LI_XPUB) nnrpd $D$(PATHBIN)/nnrpd

bootstrap:

clean clobber distclean maintclean:
	rm -f *.o $(ALL) nnrpdp profiled
	rm -rf .libs


##  Compilation rules.

NNRPDLIBS	= $(LIBSTORAGE) $(LIBHIST) $(LIBINN) $(STORAGE_LIBS) \
		  $(CANLOCK_LDFLAGS) $(CANLOCK_LIBS) \
		  $(SSL_LDFLAGS) $(SSL_LIBS) \
		  $(CRYPTO_LIBS) $(SASL_LDFLAGS) $(SASL_LIBS) \
		  $(ZLIB_LDFLAGS) $(ZLIB_LIBS) \
		  $(BLACKLIST_LDFLAGS) $(BLACKLIST_LIBS) \
		  $(PERL_LIBS) $(PYTHON_LIBS) \
		  $(LIBS)

.c.o:
	$(CC) $(CFLAGS) -c $<

perl.o:		perl.c   ; $(CC) $(CFLAGS) $(PERL_CPPFLAGS) -c perl.c
python.o:	python.c ; $(CC) $(CFLAGS) $(PYTHON_CPPFLAGS) -c python.c

nnrpd: $(OBJECTS) $(LIBHIST) $(LIBSTORAGE) $(LIBINN)
	$(LIBLD) $(LDFLAGS) -o $@ $(OBJECTS) $(NNRPDLIBS)

$(LIBINN):	; (cd ../lib ; $(MAKE))
$(LIBSTORAGE):	; (cd ../storage ; $(MAKE) library)
$(LIBHIST):	; (cd ../history ; $(MAKE))


##  Profiling.  These rules have not been checked for a while and may need
##  some work.

profiled: nnrpdp

nnrpdp: $(SOURCES)
	rm -f $(OBJECTS)
	$(MAKEPROFILING) nnrpd
	mv nnrpd nnrpdp
	rm -f $(OBJECTS)


##  Dependencies.  Default list, below, is probably good enough.

depend:	$(SOURCES)
	$(MAKEDEPEND) '$(CFLAGS) $(PERL_CPPFLAGS) $(PYTHON_CPPFLAGS)' \
	    $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
article.o: article.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h cache.h ../include/inn/libinn.h \
  ../include/inn/concat.h ../include/inn/macros.h \
  ../include/inn/portable-stdbool.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h ../include/inn/storage.h \
  ../include/inn/options.h ../include/inn/innconf.h \
  ../include/inn/messages.h ../include/inn/ov.h ../include/inn/history.h \
  ../include/inn/overview.h ../include/inn/wire.h nnrpd.h \
  ../include/portable/socket.h ../include/portable/getaddrinfo.h \
  ../include/portable/getnameinfo.h ../include/inn/nntp.h \
  ../include/inn/paths.h ../include/inn/qio.h ../include/inn/timer.h \
  ../include/inn/vector.h tls.h
auth-ext.o: auth-ext.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/buffer.h \
  ../include/inn/portable-stdbool.h ../include/inn/messages.h \
  ../include/inn/vector.h nnrpd.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/macros.h \
  ../include/inn/xmalloc.h ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/nntp.h ../include/inn/paths.h ../include/inn/qio.h \
  ../include/inn/storage.h ../include/inn/options.h ../include/inn/timer.h
cache.o: cache.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h ../include/inn/list.h \
  ../include/inn/storage.h ../include/inn/options.h ../include/inn/tst.h \
  cache.h
commands.o: commands.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/fdflag.h \
  ../include/inn/portable-socket.h ../include/inn/system.h \
  ../include/inn/portable-getaddrinfo.h \
  ../include/inn/portable-getnameinfo.h ../include/inn/portable-stdbool.h \
  ../include/inn/innconf.h ../include/inn/macros.h \
  ../include/inn/messages.h ../include/inn/ov.h ../include/inn/history.h \
  ../include/inn/storage.h ../include/inn/options.h \
  ../include/inn/version.h nnrpd.h ../include/portable/socket.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/xwrite.h ../include/inn/nntp.h ../include/inn/paths.h \
  ../include/inn/qio.h ../include/inn/timer.h ../include/inn/vector.h \
  tls.h
group.o: group.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/ov.h ../include/inn/history.h ../include/inn/storage.h \
  ../include/inn/options.h nnrpd.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h ../include/inn/nntp.h \
  ../include/inn/paths.h ../include/inn/qio.h ../include/inn/timer.h \
  ../include/inn/vector.h
line.o: line.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/messages.h nnrpd.h \
  ../include/portable/socket.h ../include/portable/getaddrinfo.h \
  ../include/portable/getnameinfo.h ../include/inn/libinn.h \
  ../include/inn/concat.h ../include/inn/macros.h \
  ../include/inn/portable-stdbool.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h ../include/inn/nntp.h \
  ../include/inn/paths.h ../include/inn/qio.h ../include/inn/storage.h \
  ../include/inn/options.h ../include/inn/timer.h ../include/inn/vector.h \
  tls.h
list.o: list.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/messages.h ../include/inn/ov.h ../include/inn/history.h \
  ../include/inn/storage.h ../include/inn/options.h \
  ../include/inn/overview.h nnrpd.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h ../include/inn/nntp.h \
  ../include/inn/paths.h ../include/inn/qio.h ../include/inn/timer.h \
  ../include/inn/vector.h
misc.o: misc.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h nnrpd.h \
  ../include/portable/socket.h ../include/portable/getaddrinfo.h \
  ../include/portable/getnameinfo.h ../include/inn/libinn.h \
  ../include/inn/concat.h ../include/inn/xmalloc.h ../include/inn/system.h \
  ../include/inn/xwrite.h ../include/inn/nntp.h ../include/inn/paths.h \
  ../include/inn/qio.h ../include/inn/storage.h ../include/inn/options.h \
  ../include/inn/timer.h ../include/inn/vector.h tls.h ../include/inn/ov.h \
  ../include/inn/history.h
newnews.o: newnews.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h cache.h ../include/inn/libinn.h \
  ../include/inn/concat.h ../include/inn/macros.h \
  ../include/inn/portable-stdbool.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h ../include/inn/storage.h \
  ../include/inn/options.h ../include/inn/innconf.h \
  ../include/inn/messages.h ../include/inn/ov.h ../include/inn/history.h \
  ../include/inn/overview.h ../include/inn/wire.h nnrpd.h \
  ../include/portable/socket.h ../include/portable/getaddrinfo.h \
  ../include/portable/getnameinfo.h ../include/inn/nntp.h \
  ../include/inn/paths.h ../include/inn/qio.h ../include/inn/timer.h \
  ../include/inn/vector.h
nnrpd.o: nnrpd.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/portable/setproctitle.h \
  ../include/portable/socket.h ../include/portable/getaddrinfo.h \
  ../include/portable/getnameinfo.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/network-innbind.h \
  ../include/inn/portable-socket.h ../include/inn/network.h \
  ../include/inn/newsuser.h ../include/inn/ov.h ../include/inn/history.h \
  ../include/inn/storage.h ../include/inn/options.h \
  ../include/inn/overview.h ../include/inn/secrets.h \
  ../include/inn/version.h nnrpd.h ../include/inn/nntp.h \
  ../include/inn/paths.h ../include/inn/qio.h ../include/inn/timer.h \
  ../include/inn/vector.h tls.h
perl.o: perl.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/nntp.h ../include/inn/paths.h nnrpd.h \
  ../include/portable/socket.h ../include/portable/getaddrinfo.h \
  ../include/portable/getnameinfo.h ../include/inn/libinn.h \
  ../include/inn/concat.h ../include/inn/xmalloc.h ../include/inn/system.h \
  ../include/inn/xwrite.h ../include/inn/qio.h ../include/inn/storage.h \
  ../include/inn/options.h ../include/inn/timer.h ../include/inn/vector.h \
  post.h ../include/ppport.h ../include/innperl.h ../include/config.h
perm.o: perm.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/conffile.h \
  ../include/portable/macros.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/network.h ../include/inn/portable-socket.h \
  ../include/inn/system.h ../include/inn/portable-getaddrinfo.h \
  ../include/inn/portable-getnameinfo.h ../include/innperl.h \
  ../include/config.h nnrpd.h ../include/portable/socket.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/xwrite.h ../include/inn/nntp.h ../include/inn/paths.h \
  ../include/inn/qio.h ../include/inn/storage.h ../include/inn/options.h \
  ../include/inn/timer.h ../include/inn/vector.h tls.h
post.o: post.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/ov.h ../include/inn/history.h ../include/inn/storage.h \
  ../include/inn/options.h nnrpd.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h ../include/inn/nntp.h \
  ../include/inn/paths.h ../include/inn/qio.h ../include/inn/timer.h \
  ../include/inn/vector.h post.h
python.o: python.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/hashtab.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/innconf.h nnrpd.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h ../include/inn/nntp.h \
  ../include/inn/paths.h ../include/inn/qio.h ../include/inn/storage.h \
  ../include/inn/options.h ../include/inn/timer.h ../include/inn/vector.h
sasl.o: sasl.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/messages.h nnrpd.h \
  ../include/portable/socket.h ../include/portable/getaddrinfo.h \
  ../include/portable/getnameinfo.h ../include/inn/libinn.h \
  ../include/inn/concat.h ../include/inn/macros.h \
  ../include/inn/portable-stdbool.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h ../include/inn/nntp.h \
  ../include/inn/paths.h ../include/inn/qio.h ../include/inn/storage.h \
  ../include/inn/options.h ../include/inn/timer.h ../include/inn/vector.h \
  ../include/inn/ov.h ../include/inn/history.h tls.h
tls.o: tls.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h nnrpd.h \
  ../include/portable/socket.h ../include/portable/getaddrinfo.h \
  ../include/portable/getnameinfo.h ../include/inn/libinn.h \
  ../include/inn/concat.h ../include/inn/xmalloc.h ../include/inn/system.h \
  ../include/inn/xwrite.h ../include/inn/nntp.h ../include/inn/paths.h \
  ../include/inn/qio.h ../include/inn/storage.h ../include/inn/options.h \
  ../include/inn/timer.h ../include/inn/vector.h tls.h
track.o: track.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h nnrpd.h \
  ../include/portable/socket.h ../include/portable/getaddrinfo.h \
  ../include/portable/getnameinfo.h ../include/inn/libinn.h \
  ../include/inn/concat.h ../include/inn/xmalloc.h ../include/inn/system.h \
  ../include/inn/xwrite.h ../include/inn/nntp.h ../include/inn/paths.h \
  ../include/inn/qio.h ../include/inn/storage.h ../include/inn/options.h \
  ../include/inn/timer.h ../include/inn/vector.h
zlib.o: zlib.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/messages.h nnrpd.h \
  ../include/portable/socket.h ../include/portable/getaddrinfo.h \
  ../include/portable/getnameinfo.h ../include/inn/libinn.h \
  ../include/inn/concat.h ../include/inn/macros.h \
  ../include/inn/portable-stdbool.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h ../include/inn/nntp.h \
  ../include/inn/paths.h ../include/inn/qio.h ../include/inn/storage.h \
  ../include/inn/options.h ../include/inn/timer.h ../include/inn/vector.h

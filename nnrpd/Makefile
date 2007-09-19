##  $Id$

include ../Makefile.global

top		= ..
CFLAGS		= $(GCFLAGS) $(SSL_CPPFLAGS) $(SASL_CPPFLAGS)

ALL		= nnrpd

SOURCES		= article.c auth-ext.c cache.c group.c commands.c line.c \
		  list.c misc.c newnews.c nnrpd.c perl.c perm.c post.c \
		  python.c sasl.c tls.c track.c

INCLUDES	= cache.h nnrpd.h post.h tls.h

OBJECTS		= $(SOURCES:.c=.o)

all: $(ALL)

warnings:
	$(MAKE) COPT='$(WARNINGS)' all

install: all
	$(LI_XPUB) nnrpd $D$(PATHBIN)/nnrpd

bootstrap:

clean:
	rm -f *.o $(ALL) nnrpdp profiled
	rm -rf .libs

clobber distclean maintclean: clean
	rm -f tags

tags ctags: $(SOURCES) $(INCLUDES)
	$(CTAGS) $(SOURCES) $(INCLUDES) ../lib/*.c ../include/*.h


##  Compilation rules.

NNRPDLIBS	= $(LIBHIST) $(LIBSTORAGE) $(LIBINN) $(STORAGE_LIBS) \
		  $(PERL_LIBS) $(PYTHON_LIBS) $(SSL_LDFLAGS) $(SSL_LIBS) \
		  $(SASL_LDFLAGS) $(SASL_LIBS) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(SSL_CPPFLAGS) $(SASL_CPPFLAGS) -c $<

perl.o:		perl.c   ; $(CC) $(CFLAGS) $(PERL_CPPFLAGS) -c perl.c
python.o:	python.c ; $(CC) $(CFLAGS) $(PYTHON_CPPFLAGS) -c python.c

nnrpd: $(OBJECTS) $(LIBHIST) $(LIBSTORAGE) $(LIBINN)
	$(LIBLD) $(LDFLAGS) -o $@ $(OBJECTS) $(NNRPDLIBS)

$(LIBINN):	; (cd ../lib ; $(MAKE))
$(LIBSTORAGE):	; (cd ../storage ; $(MAKE))
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
article.o: article.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/inn/innconf.h ../include/inn/defines.h \
  ../include/inn/messages.h ../include/inn/wire.h nnrpd.h \
  ../include/portable/socket.h ../include/config.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/portable/time.h ../include/inn/qio.h ../include/inn/libinn.h \
  ../include/nntp.h ../include/inn/nntp.h ../include/inn/paths.h \
  ../include/inn/storage.h ../include/inn/vector.h ../include/inn/timer.h \
  ../include/inn/ov.h ../include/inn/storage.h ../include/inn/history.h \
  tls.h cache.h
auth-ext.o: auth-ext.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/portable/wait.h ../include/config.h \
  ../include/inn/buffer.h ../include/inn/defines.h \
  ../include/inn/messages.h ../include/inn/vector.h nnrpd.h \
  ../include/portable/socket.h ../include/portable/getaddrinfo.h \
  ../include/portable/getnameinfo.h ../include/portable/time.h \
  ../include/inn/qio.h ../include/inn/libinn.h ../include/nntp.h \
  ../include/inn/nntp.h ../include/inn/paths.h ../include/inn/storage.h \
  ../include/inn/timer.h
cache.o: cache.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/inn/innconf.h ../include/inn/defines.h \
  ../include/inn/tst.h ../include/inn/list.h ../include/inn/libinn.h \
  ../include/inn/storage.h cache.h
group.o: group.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/inn/innconf.h ../include/inn/defines.h \
  nnrpd.h ../include/portable/socket.h ../include/config.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/portable/time.h ../include/inn/qio.h ../include/inn/libinn.h \
  ../include/nntp.h ../include/inn/nntp.h ../include/inn/paths.h \
  ../include/inn/storage.h ../include/inn/vector.h ../include/inn/timer.h \
  ../include/inn/ov.h ../include/inn/storage.h ../include/inn/history.h
commands.o: commands.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/portable/wait.h ../include/config.h \
  nnrpd.h ../include/portable/socket.h ../include/portable/getaddrinfo.h \
  ../include/portable/getnameinfo.h ../include/portable/time.h \
  ../include/inn/qio.h ../include/inn/defines.h ../include/inn/libinn.h \
  ../include/nntp.h ../include/inn/nntp.h ../include/inn/paths.h \
  ../include/inn/storage.h ../include/inn/vector.h ../include/inn/timer.h \
  ../include/inn/ov.h ../include/inn/storage.h ../include/inn/history.h \
  ../include/inn/innconf.h ../include/inn/messages.h \
  ../include/inn/version.h tls.h
line.o: line.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/inn/messages.h ../include/inn/defines.h \
  nnrpd.h ../include/portable/socket.h ../include/config.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/portable/time.h ../include/inn/qio.h ../include/inn/libinn.h \
  ../include/nntp.h ../include/inn/nntp.h ../include/inn/paths.h \
  ../include/inn/storage.h ../include/inn/vector.h ../include/inn/timer.h \
  tls.h
list.o: list.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h nnrpd.h ../include/portable/socket.h \
  ../include/config.h ../include/portable/getaddrinfo.h \
  ../include/portable/getnameinfo.h ../include/portable/time.h \
  ../include/inn/qio.h ../include/inn/defines.h ../include/inn/libinn.h \
  ../include/nntp.h ../include/inn/nntp.h ../include/inn/paths.h \
  ../include/inn/storage.h ../include/inn/vector.h ../include/inn/timer.h \
  ../include/inn/ov.h ../include/inn/storage.h ../include/inn/history.h \
  ../include/inn/innconf.h ../include/inn/messages.h
misc.o: misc.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/inn/innconf.h ../include/inn/defines.h \
  nnrpd.h ../include/portable/socket.h ../include/config.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/portable/time.h ../include/inn/qio.h ../include/inn/libinn.h \
  ../include/nntp.h ../include/inn/nntp.h ../include/inn/paths.h \
  ../include/inn/storage.h ../include/inn/vector.h ../include/inn/timer.h \
  tls.h
newnews.o: newnews.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/inn/innconf.h ../include/inn/defines.h \
  ../include/inn/messages.h ../include/inn/wire.h nnrpd.h \
  ../include/portable/socket.h ../include/config.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/portable/time.h ../include/inn/qio.h ../include/inn/libinn.h \
  ../include/nntp.h ../include/inn/nntp.h ../include/inn/paths.h \
  ../include/inn/storage.h ../include/inn/vector.h ../include/inn/timer.h \
  ../include/inn/ov.h ../include/inn/storage.h ../include/inn/history.h \
  cache.h
nnrpd.o: nnrpd.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h \
  ../include/portable/alloca.h ../include/config.h ../include/clibrary.h \
  ../include/config.h ../include/portable/setproctitle.h \
  ../include/portable/socket.h ../include/portable/getaddrinfo.h \
  ../include/portable/getnameinfo.h ../include/portable/wait.h \
  ../include/inn/innconf.h ../include/inn/defines.h \
  ../include/inn/messages.h ../include/inn/network.h \
  ../include/inn/version.h ../include/inn/libinn.h ../include/inn/ov.h \
  ../include/inn/storage.h ../include/inn/history.h nnrpd.h \
  ../include/portable/time.h ../include/inn/qio.h ../include/nntp.h \
  ../include/inn/nntp.h ../include/inn/paths.h ../include/inn/storage.h \
  ../include/inn/vector.h ../include/inn/timer.h tls.h
perl.o: perl.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/inn/innconf.h ../include/inn/defines.h \
  nnrpd.h ../include/portable/socket.h ../include/config.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/portable/time.h ../include/inn/qio.h ../include/inn/libinn.h \
  ../include/nntp.h ../include/inn/nntp.h ../include/inn/paths.h \
  ../include/inn/storage.h ../include/inn/vector.h ../include/inn/timer.h \
  post.h \
  ../include/ppport.h \
  ../include/innperl.h
perm.o: perm.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/portable/wait.h ../include/config.h \
  ../include/conffile.h ../include/inn/network.h ../include/inn/defines.h \
  ../include/inn/innconf.h ../include/innperl.h nnrpd.h \
  ../include/portable/socket.h ../include/portable/getaddrinfo.h \
  ../include/portable/getnameinfo.h ../include/portable/time.h \
  ../include/inn/qio.h ../include/inn/libinn.h ../include/nntp.h \
  ../include/inn/nntp.h ../include/inn/paths.h ../include/inn/storage.h \
  ../include/inn/vector.h ../include/inn/timer.h
post.o: post.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/inn/innconf.h ../include/inn/defines.h \
  nnrpd.h ../include/portable/socket.h ../include/config.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/portable/time.h ../include/inn/qio.h ../include/inn/libinn.h \
  ../include/nntp.h ../include/inn/nntp.h ../include/inn/paths.h \
  ../include/inn/storage.h ../include/inn/vector.h ../include/inn/timer.h \
  ../include/inn/ov.h ../include/inn/storage.h ../include/inn/history.h \
  post.h
python.o: python.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/inn/innconf.h ../include/inn/defines.h \
  nnrpd.h ../include/portable/socket.h ../include/config.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/portable/time.h ../include/inn/qio.h ../include/inn/libinn.h \
  ../include/nntp.h ../include/inn/nntp.h ../include/inn/paths.h \
  ../include/inn/storage.h ../include/inn/vector.h ../include/inn/timer.h \
  ../include/inn/hashtab.h
sasl.o: sasl.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/inn/messages.h ../include/inn/defines.h \
  nnrpd.h ../include/portable/socket.h ../include/config.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/portable/time.h ../include/inn/qio.h ../include/inn/libinn.h \
  ../include/nntp.h ../include/inn/nntp.h ../include/inn/paths.h \
  ../include/inn/storage.h ../include/inn/vector.h ../include/inn/timer.h
tls.o: tls.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h \
  ../include/portable/alloca.h ../include/config.h ../include/clibrary.h \
  ../include/config.h nnrpd.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/portable/time.h ../include/inn/qio.h \
  ../include/inn/defines.h ../include/inn/libinn.h ../include/nntp.h \
  ../include/inn/nntp.h ../include/inn/paths.h ../include/inn/storage.h \
  ../include/inn/vector.h ../include/inn/timer.h ../include/inn/innconf.h \
  tls.h
track.o: track.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/inn/innconf.h ../include/inn/defines.h \
  nnrpd.h ../include/portable/socket.h ../include/config.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/portable/time.h ../include/inn/qio.h ../include/inn/libinn.h \
  ../include/nntp.h ../include/inn/nntp.h ../include/inn/paths.h \
  ../include/inn/storage.h ../include/inn/vector.h ../include/inn/timer.h

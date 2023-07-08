include ../Makefile.global

top	        = ..
CFLAGS		= $(GCFLAGS) $(SYSTEMD_CFLAGS)

ALL		= innd tinyleaf

SOURCES		= art.c cc.c chan.c icd.c innd.c keywords.c lc.c nc.c \
		  newsfeeds.c ng.c perl.c proc.c python.c rc.c site.c \
		  status.c util.c wip.c

EXTRASOURCES	= tinyleaf.c

OBJECTS		= $(SOURCES:.c=.o)

all: $(ALL)

warnings:
	$(MAKE) COPT="$(COPT) $(WARNINGS)" all

install: all
	$(LI_XPRI) innd $D$(PATHBIN)/innd
	$(LI_XPUB) tinyleaf $D$(PATHBIN)/tinyleaf

bootstrap:

clean clobber distclean maintclean:
	rm -f *.o $(ALL) inndp
	rm -rf .libs


##  Compilation rules.

INNDLIBS 	= $(LIBSTORAGE) $(LIBHIST) $(LIBINN) $(STORAGE_LIBS) \
		  $(SYSTEMD_LIBS) $(CANLOCK_LDFLAGS) $(CANLOCK_LIBS) \
		  $(PERL_LIBS) $(PYTHON_LIBS) $(REGEX_LIBS) $(LIBS)

perl.o:		perl.c   ; $(CC) $(CFLAGS) $(PERL_CPPFLAGS) -c perl.c
python.o:	python.c ; $(CC) $(CFLAGS) $(PYTHON_CPPFLAGS) -c python.c

innd: $(OBJECTS) $(LIBSTORAGE) $(LIBHIST) $(LIBINN)
	$(LIBLD) $(LDFLAGS) -o $@ $(OBJECTS) $(INNDLIBS)

tinyleaf: tinyleaf.o $(LIBINN)
	$(LIBLD) $(LDFLAGS) -o $@ tinyleaf.o $(LIBINN) $(LIBS)

$(LIBINN):	; (cd ../lib ; $(MAKE))
$(LIBSTORAGE):	; (cd ../storage ; $(MAKE) library)
$(LIBHIST):	; (cd ../history ; $(MAKE))


##  Profiling.  These rules have not been checked for a while and may need
##  some work.

profiled: inndp

inndp: $(SOURCES)
	rm -f $(OBJECTS)
	$(MAKEPROFILING) innd
	mv innd inndp
	rm -f $(OBJECTS)


##  Dependencies.  Default list, below, is probably good enough.

depend:	Makefile $(SOURCES) $(EXTRASOURCES)
	$(MAKEDEPEND) '$(CFLAGS) $(PERL_CPPFLAGS) $(PYTHON_CPPFLAGS)' \
	    $(SOURCES) $(EXTRASOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
art.o: art.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/md5.h ../include/inn/system.h ../include/inn/ov.h \
  ../include/inn/history.h ../include/inn/storage.h \
  ../include/inn/options.h ../include/inn/overview.h \
  ../include/inn/vector.h ../include/inn/wire.h innd.h \
  ../include/portable/sd-daemon.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/inn/buffer.h ../include/inn/libinn.h ../include/inn/concat.h \
  ../include/inn/xmalloc.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/nntp.h ../include/inn/paths.h \
  ../include/inn/timer.h
cc.o: cc.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/portable/socket-unix.h \
  ../include/inn/fdflag.h ../include/inn/portable-socket.h \
  ../include/inn/system.h ../include/inn/portable-getaddrinfo.h \
  ../include/inn/portable-getnameinfo.h ../include/inn/portable-stdbool.h \
  ../include/inn/innconf.h ../include/inn/macros.h \
  ../include/inn/inndcomm.h ../include/inn/qio.h innd.h \
  ../include/portable/sd-daemon.h ../include/portable/socket.h \
  ../include/inn/buffer.h ../include/inn/history.h ../include/inn/libinn.h \
  ../include/inn/concat.h ../include/inn/xmalloc.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/nntp.h ../include/inn/paths.h \
  ../include/inn/storage.h ../include/inn/options.h ../include/inn/timer.h \
  ../include/inn/vector.h ../include/innperl.h ../include/config.h
chan.o: chan.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/fdflag.h \
  ../include/inn/portable-socket.h ../include/inn/system.h \
  ../include/inn/portable-getaddrinfo.h \
  ../include/inn/portable-getnameinfo.h ../include/inn/portable-stdbool.h \
  ../include/inn/innconf.h ../include/inn/macros.h \
  ../include/inn/network.h innd.h ../include/portable/sd-daemon.h \
  ../include/portable/socket.h ../include/inn/buffer.h \
  ../include/inn/history.h ../include/inn/libinn.h ../include/inn/concat.h \
  ../include/inn/xmalloc.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/nntp.h ../include/inn/paths.h \
  ../include/inn/storage.h ../include/inn/options.h ../include/inn/timer.h \
  ../include/inn/vector.h
icd.o: icd.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/portable/mmap.h \
  ../include/inn/fdflag.h ../include/inn/portable-socket.h \
  ../include/inn/system.h ../include/inn/portable-getaddrinfo.h \
  ../include/inn/portable-getnameinfo.h ../include/inn/portable-stdbool.h \
  ../include/inn/innconf.h ../include/inn/macros.h ../include/inn/mmap.h \
  ../include/inn/ov.h ../include/inn/history.h ../include/inn/storage.h \
  ../include/inn/options.h innd.h ../include/portable/sd-daemon.h \
  ../include/portable/socket.h ../include/inn/buffer.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/xwrite.h ../include/inn/messages.h ../include/inn/nntp.h \
  ../include/inn/paths.h ../include/inn/timer.h ../include/inn/vector.h
innd.o: innd.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/messages.h ../include/inn/newsuser.h ../include/innperl.h \
  ../include/config.h ../include/inn/ov.h ../include/inn/history.h \
  ../include/inn/storage.h ../include/inn/options.h innd.h \
  ../include/portable/sd-daemon.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/inn/buffer.h ../include/inn/libinn.h ../include/inn/concat.h \
  ../include/inn/xmalloc.h ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/nntp.h ../include/inn/paths.h ../include/inn/timer.h \
  ../include/inn/vector.h
keywords.o: keywords.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/libinn.h \
  ../include/inn/concat.h ../include/inn/macros.h \
  ../include/inn/portable-stdbool.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h ../include/inn/innconf.h \
  innd.h ../include/portable/sd-daemon.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/inn/buffer.h ../include/inn/history.h \
  ../include/inn/messages.h ../include/inn/nntp.h ../include/inn/paths.h \
  ../include/inn/storage.h ../include/inn/options.h ../include/inn/timer.h \
  ../include/inn/vector.h
lc.o: lc.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h innd.h \
  ../include/portable/sd-daemon.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/inn/buffer.h ../include/inn/history.h ../include/inn/libinn.h \
  ../include/inn/concat.h ../include/inn/xmalloc.h ../include/inn/system.h \
  ../include/inn/xwrite.h ../include/inn/messages.h ../include/inn/nntp.h \
  ../include/inn/paths.h ../include/inn/storage.h ../include/inn/options.h \
  ../include/inn/timer.h ../include/inn/vector.h \
  ../include/portable/socket-unix.h
nc.o: nc.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/qio.h ../include/inn/version.h innd.h \
  ../include/portable/sd-daemon.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/inn/buffer.h ../include/inn/history.h ../include/inn/libinn.h \
  ../include/inn/concat.h ../include/inn/xmalloc.h ../include/inn/system.h \
  ../include/inn/xwrite.h ../include/inn/messages.h ../include/inn/nntp.h \
  ../include/inn/paths.h ../include/inn/storage.h ../include/inn/options.h \
  ../include/inn/timer.h ../include/inn/vector.h
newsfeeds.o: newsfeeds.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h innd.h \
  ../include/portable/sd-daemon.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/inn/buffer.h ../include/inn/history.h ../include/inn/libinn.h \
  ../include/inn/concat.h ../include/inn/xmalloc.h ../include/inn/system.h \
  ../include/inn/xwrite.h ../include/inn/messages.h ../include/inn/nntp.h \
  ../include/inn/paths.h ../include/inn/storage.h ../include/inn/options.h \
  ../include/inn/timer.h ../include/inn/vector.h
ng.o: ng.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/ov.h ../include/inn/history.h ../include/inn/storage.h \
  ../include/inn/options.h innd.h ../include/portable/sd-daemon.h \
  ../include/portable/socket.h ../include/portable/getaddrinfo.h \
  ../include/portable/getnameinfo.h ../include/inn/buffer.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/nntp.h ../include/inn/paths.h \
  ../include/inn/timer.h ../include/inn/vector.h
perl.o: perl.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/wire.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h innd.h \
  ../include/portable/sd-daemon.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/inn/buffer.h ../include/inn/history.h ../include/inn/libinn.h \
  ../include/inn/concat.h ../include/inn/xmalloc.h ../include/inn/system.h \
  ../include/inn/xwrite.h ../include/inn/messages.h ../include/inn/nntp.h \
  ../include/inn/paths.h ../include/inn/storage.h ../include/inn/options.h \
  ../include/inn/timer.h ../include/inn/vector.h \
  ../include/ppport.h \
  ../include/innperl.h \
  ../include/config.h
proc.o: proc.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h innd.h ../include/portable/sd-daemon.h \
  ../include/portable/socket.h ../include/portable/getaddrinfo.h \
  ../include/portable/getnameinfo.h ../include/inn/buffer.h \
  ../include/inn/portable-stdbool.h ../include/inn/history.h \
  ../include/inn/macros.h ../include/inn/libinn.h ../include/inn/concat.h \
  ../include/inn/xmalloc.h ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/nntp.h ../include/inn/paths.h \
  ../include/inn/storage.h ../include/inn/options.h ../include/inn/timer.h \
  ../include/inn/vector.h
python.o: python.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h \
  ../include/inn/innconf.h ../include/inn/macros.h \
  ../include/inn/portable-stdbool.h ../include/inn/wire.h innd.h \
  ../include/portable/sd-daemon.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/inn/buffer.h ../include/inn/history.h ../include/inn/libinn.h \
  ../include/inn/concat.h ../include/inn/xmalloc.h ../include/inn/system.h \
  ../include/inn/xwrite.h ../include/inn/messages.h ../include/inn/nntp.h \
  ../include/inn/paths.h ../include/inn/storage.h ../include/inn/options.h \
  ../include/inn/timer.h ../include/inn/vector.h
rc.o: rc.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/inn/fdflag.h ../include/inn/portable-socket.h \
  ../include/inn/portable-stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/network-innbind.h \
  ../include/inn/network.h ../include/inn/vector.h innd.h \
  ../include/portable/sd-daemon.h ../include/inn/buffer.h \
  ../include/inn/history.h ../include/inn/libinn.h ../include/inn/concat.h \
  ../include/inn/xmalloc.h ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/nntp.h ../include/inn/paths.h \
  ../include/inn/storage.h ../include/inn/options.h ../include/inn/timer.h
site.o: site.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/fdflag.h \
  ../include/inn/portable-socket.h ../include/inn/system.h \
  ../include/inn/portable-getaddrinfo.h \
  ../include/inn/portable-getnameinfo.h ../include/inn/portable-stdbool.h \
  ../include/inn/innconf.h ../include/inn/macros.h innd.h \
  ../include/portable/sd-daemon.h ../include/portable/socket.h \
  ../include/inn/buffer.h ../include/inn/history.h ../include/inn/libinn.h \
  ../include/inn/concat.h ../include/inn/xmalloc.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/nntp.h ../include/inn/paths.h \
  ../include/inn/storage.h ../include/inn/options.h ../include/inn/timer.h \
  ../include/inn/vector.h
status.o: status.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/inn/innconf.h ../include/inn/macros.h \
  ../include/inn/portable-stdbool.h ../include/inn/network.h \
  ../include/inn/portable-socket.h ../include/inn/version.h innd.h \
  ../include/portable/sd-daemon.h ../include/inn/buffer.h \
  ../include/inn/history.h ../include/inn/libinn.h ../include/inn/concat.h \
  ../include/inn/xmalloc.h ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/nntp.h ../include/inn/paths.h \
  ../include/inn/storage.h ../include/inn/options.h ../include/inn/timer.h \
  ../include/inn/vector.h ../include/innperl.h ../include/config.h
util.o: util.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/fdflag.h \
  ../include/inn/portable-socket.h ../include/inn/system.h \
  ../include/inn/portable-getaddrinfo.h \
  ../include/inn/portable-getnameinfo.h ../include/inn/portable-stdbool.h \
  ../include/inn/innconf.h ../include/inn/macros.h ../include/inn/libinn.h \
  ../include/inn/concat.h ../include/inn/xmalloc.h ../include/inn/xwrite.h \
  innd.h ../include/portable/sd-daemon.h ../include/portable/socket.h \
  ../include/inn/buffer.h ../include/inn/history.h \
  ../include/inn/messages.h ../include/inn/nntp.h ../include/inn/paths.h \
  ../include/inn/storage.h ../include/inn/options.h ../include/inn/timer.h \
  ../include/inn/vector.h
wip.o: wip.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h innd.h \
  ../include/portable/sd-daemon.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/inn/buffer.h ../include/inn/history.h ../include/inn/libinn.h \
  ../include/inn/concat.h ../include/inn/xmalloc.h ../include/inn/system.h \
  ../include/inn/xwrite.h ../include/inn/messages.h ../include/inn/nntp.h \
  ../include/inn/paths.h ../include/inn/storage.h ../include/inn/options.h \
  ../include/inn/timer.h ../include/inn/vector.h
tinyleaf.o: tinyleaf.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/dispatch.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/macros.h \
  ../include/inn/portable-stdbool.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h ../include/inn/md5.h \
  ../include/inn/messages.h ../include/inn/nntp.h ../include/inn/utility.h \
  ../include/inn/vector.h ../include/inn/version.h

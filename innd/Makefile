##  $Id$

include ../Makefile.global

top	        = ..
CFLAGS		= $(GCFLAGS) $(TCLINC)

ALL		= innd tinyleaf

SOURCES		= art.c cc.c chan.c icd.c innd.c keywords.c lc.c nc.c \
		  newsfeeds.c ng.c perl.c proc.c python.c rc.c site.c \
		  status.c util.c wip.c

OBJECTS		= $(SOURCES:.c=.o)

all: $(ALL)

warnings:
	$(MAKE) COPT='$(WARNINGS)' all

install: all
	$(LI_XPRI) innd $D$(PATHBIN)/innd
	$(LI_XPUB) tinyleaf $D$(PATHBIN)/tinyleaf

bootstrap:

clean:
	rm -f *.o $(ALL) inndp
	rm -rf .libs

clobber distclean maintclean: clean
	rm -f tags

tags ctags: $(SOURCES)
	$(CTAGS) $(SOURCES) ../lib/*.c innd.h ../include/*.h


##  Compilation rules.

INNDLIBS 	= $(LIBSTORAGE) $(LIBHIST) $(LIBINN) $(STORAGE_LIBS) \
		  $(PERL_LIBS) $(PYTHON_LIBS) $(REGEX_LIBS) $(LIBS)

perl.o:		perl.c   ; $(CC) $(CFLAGS) $(PERL_CPPFLAGS) -c perl.c
python.o:	python.c ; $(CC) $(CFLAGS) $(PYTHON_CPPFLAGS) -c python.c

innd: $(OBJECTS) $(LIBSTORAGE) $(LIBHIST) $(LIBINN)
	$(LIBLD) $(LDFLAGS) -o $@ $(OBJECTS) $(INNDLIBS)

tinyleaf: tinyleaf.o $(LIBINN)
	$(LIBLD) $(LDFLAGS) -o $@ tinyleaf.o $(LIBINN)

$(LIBINN):	; (cd ../lib ; $(MAKE))
$(LIBSTORAGE):	; (cd ../storage ; $(MAKE))
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

depend:	Makefile $(SOURCES)
	$(MAKEDEPEND) '$(CFLAGS) $(PERL_CPPFLAGS) $(PYTHON_CPPFLAGS)' \
	    $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
art.o: art.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/inn/innconf.h ../include/inn/defines.h \
  ../include/inn/wire.h innd.h ../include/portable/time.h \
  ../include/config.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/inn/buffer.h ../include/inn/history.h \
  ../include/inn/messages.h ../include/inn/timer.h \
  ../include/inn/libinn.h ../include/nntp.h ../include/inn/nntp.h \
  ../include/inn/paths.h ../include/inn/storage.h ../include/inn/ov.h \
  ../include/inn/storage.h ../include/inn/history.h
cc.o: cc.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/inn/innconf.h ../include/inn/defines.h \
  ../include/inn/qio.h innd.h ../include/portable/time.h \
  ../include/config.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/inn/buffer.h ../include/inn/history.h \
  ../include/inn/messages.h ../include/inn/timer.h \
  ../include/inn/libinn.h ../include/nntp.h ../include/inn/nntp.h \
  ../include/inn/paths.h ../include/inn/storage.h \
  ../include/inn/inndcomm.h ../include/innperl.h
chan.o: chan.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/inn/innconf.h ../include/inn/defines.h \
  ../include/inn/network.h innd.h ../include/portable/time.h \
  ../include/config.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/inn/buffer.h ../include/inn/history.h \
  ../include/inn/messages.h ../include/inn/timer.h \
  ../include/inn/libinn.h ../include/nntp.h ../include/inn/nntp.h \
  ../include/inn/paths.h ../include/inn/storage.h
icd.o: icd.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/portable/mmap.h ../include/config.h \
  ../include/inn/innconf.h ../include/inn/defines.h ../include/inn/mmap.h \
  innd.h ../include/portable/time.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/inn/buffer.h ../include/inn/history.h \
  ../include/inn/messages.h ../include/inn/timer.h \
  ../include/inn/libinn.h ../include/nntp.h ../include/inn/nntp.h \
  ../include/inn/paths.h ../include/inn/storage.h ../include/inn/ov.h \
  ../include/inn/storage.h ../include/inn/history.h
innd.o: innd.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/inn/innconf.h ../include/inn/defines.h \
  ../include/inn/messages.h ../include/innperl.h innd.h \
  ../include/portable/time.h ../include/config.h \
  ../include/portable/socket.h ../include/portable/getaddrinfo.h \
  ../include/portable/getnameinfo.h ../include/inn/buffer.h \
  ../include/inn/history.h ../include/inn/timer.h ../include/inn/libinn.h \
  ../include/nntp.h ../include/inn/nntp.h ../include/inn/paths.h \
  ../include/inn/storage.h ../include/inn/ov.h ../include/inn/storage.h \
  ../include/inn/history.h
keywords.o: keywords.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/inn/libinn.h ../include/inn/defines.h \
  ../include/inn/innconf.h innd.h ../include/portable/time.h \
  ../include/config.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/inn/buffer.h ../include/inn/history.h \
  ../include/inn/messages.h ../include/inn/timer.h ../include/nntp.h \
  ../include/inn/nntp.h ../include/inn/paths.h ../include/inn/storage.h
lc.o: lc.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/inn/innconf.h ../include/inn/defines.h \
  innd.h ../include/portable/time.h ../include/config.h \
  ../include/portable/socket.h ../include/portable/getaddrinfo.h \
  ../include/portable/getnameinfo.h ../include/inn/buffer.h \
  ../include/inn/history.h ../include/inn/messages.h \
  ../include/inn/timer.h ../include/inn/libinn.h ../include/nntp.h \
  ../include/inn/nntp.h ../include/inn/paths.h ../include/inn/storage.h
nc.o: nc.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/inn/innconf.h ../include/inn/defines.h \
  ../include/inn/version.h innd.h ../include/portable/time.h \
  ../include/config.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/inn/buffer.h ../include/inn/history.h \
  ../include/inn/messages.h ../include/inn/timer.h \
  ../include/inn/libinn.h ../include/nntp.h ../include/inn/nntp.h \
  ../include/inn/paths.h ../include/inn/storage.h
newsfeeds.o: newsfeeds.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/inn/innconf.h ../include/inn/defines.h \
  innd.h ../include/portable/time.h ../include/config.h \
  ../include/portable/socket.h ../include/portable/getaddrinfo.h \
  ../include/portable/getnameinfo.h ../include/inn/buffer.h \
  ../include/inn/history.h ../include/inn/messages.h \
  ../include/inn/timer.h ../include/inn/libinn.h ../include/nntp.h \
  ../include/inn/nntp.h ../include/inn/paths.h ../include/inn/storage.h
ng.o: ng.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/inn/innconf.h ../include/inn/defines.h \
  innd.h ../include/portable/time.h ../include/config.h \
  ../include/portable/socket.h ../include/portable/getaddrinfo.h \
  ../include/portable/getnameinfo.h ../include/inn/buffer.h \
  ../include/inn/history.h ../include/inn/messages.h \
  ../include/inn/timer.h ../include/inn/libinn.h ../include/nntp.h \
  ../include/inn/nntp.h ../include/inn/paths.h ../include/inn/storage.h \
  ../include/inn/ov.h ../include/inn/storage.h ../include/inn/history.h
perl.o: perl.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/inn/wire.h ../include/inn/defines.h \
  innd.h ../include/portable/time.h ../include/config.h \
  ../include/portable/socket.h ../include/portable/getaddrinfo.h \
  ../include/portable/getnameinfo.h ../include/inn/buffer.h \
  ../include/inn/history.h ../include/inn/messages.h \
  ../include/inn/timer.h ../include/inn/libinn.h ../include/nntp.h \
  ../include/inn/nntp.h ../include/inn/paths.h ../include/inn/storage.h \
   ../include/ppport.h \
  ../include/innperl.h
proc.o: proc.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/portable/wait.h ../include/config.h \
  innd.h ../include/portable/time.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/inn/buffer.h ../include/inn/defines.h \
  ../include/inn/history.h ../include/inn/messages.h \
  ../include/inn/timer.h ../include/inn/libinn.h ../include/nntp.h \
  ../include/inn/nntp.h ../include/inn/paths.h ../include/inn/storage.h
python.o: python.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/inn/innconf.h ../include/inn/defines.h \
  ../include/inn/wire.h innd.h ../include/portable/time.h \
  ../include/config.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/inn/buffer.h ../include/inn/history.h \
  ../include/inn/messages.h ../include/inn/timer.h \
  ../include/inn/libinn.h ../include/nntp.h ../include/inn/nntp.h \
  ../include/inn/paths.h ../include/inn/storage.h
rc.o: rc.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/portable/socket.h ../include/config.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/inn/innconf.h ../include/inn/defines.h \
  ../include/inn/network.h innd.h ../include/portable/time.h \
  ../include/inn/buffer.h ../include/inn/history.h \
  ../include/inn/messages.h ../include/inn/timer.h \
  ../include/inn/libinn.h ../include/nntp.h ../include/inn/nntp.h \
  ../include/inn/paths.h ../include/inn/storage.h
site.o: site.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/inn/innconf.h ../include/inn/defines.h \
  innd.h ../include/portable/time.h ../include/config.h \
  ../include/portable/socket.h ../include/portable/getaddrinfo.h \
  ../include/portable/getnameinfo.h ../include/inn/buffer.h \
  ../include/inn/history.h ../include/inn/messages.h \
  ../include/inn/timer.h ../include/inn/libinn.h ../include/nntp.h \
  ../include/inn/nntp.h ../include/inn/paths.h ../include/inn/storage.h
status.o: status.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/portable/socket.h ../include/config.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/inn/network.h ../include/inn/defines.h \
  ../include/inn/innconf.h ../include/inn/version.h innd.h \
  ../include/portable/time.h ../include/inn/buffer.h \
  ../include/inn/history.h ../include/inn/messages.h \
  ../include/inn/timer.h ../include/inn/libinn.h ../include/nntp.h \
  ../include/inn/nntp.h ../include/inn/paths.h ../include/inn/storage.h \
  ../include/innperl.h
util.o: util.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/inn/innconf.h ../include/inn/defines.h \
  ../include/inn/libinn.h innd.h ../include/portable/time.h \
  ../include/config.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/inn/buffer.h ../include/inn/history.h \
  ../include/inn/messages.h ../include/inn/timer.h ../include/nntp.h \
  ../include/inn/nntp.h ../include/inn/paths.h ../include/inn/storage.h
wip.o: wip.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/inn/innconf.h ../include/inn/defines.h \
  innd.h ../include/portable/time.h ../include/config.h \
  ../include/portable/socket.h ../include/portable/getaddrinfo.h \
  ../include/portable/getnameinfo.h ../include/inn/buffer.h \
  ../include/inn/history.h ../include/inn/messages.h \
  ../include/inn/timer.h ../include/inn/libinn.h ../include/nntp.h \
  ../include/inn/nntp.h ../include/inn/paths.h ../include/inn/storage.h

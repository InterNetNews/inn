include ../Makefile.global

# This version number should be increased with every change to the library
# source following the rules laid out in the libtool manual.  This will also
# force the file name of the shared library to change so that one can
# recover from make update.  We can't use .OLD extensions for the shared
# library since ldconfig will think .OLD sorts after the regular library and
# will point the binaries at the old library.
LTVERSION     = 6:1:0

top	= ..
CFLAGS  = $(GCFLAGS)

# The base library files that are always compiled and included.
SOURCES       = argparse.c artnumber.c buffer.c cleanfrom.c clientactive.c \
	        clientlib.c                                                \
	      	commands.c concat.c conffile.c confparse.c daemonize.c	   \
	      	date.c dbz.c defdist.c dispatch.c fdflag.c fdlimit.c	   \
	      	getfqdn.c getmodaddr.c hash.c hashtab.c headers.c hex.c	   \
	      	innconf.c inndcomm.c list.c localopen.c lockfile.c	   \
	      	makedir.c md5.c messageid.c messages.c mmap.c network.c	   \
	        network-innbind.c newsuser.c nntp.c qio.c                  \
		radix32.c readin.c					   \
	      	remopen.c reservedfd.c resource.c sendarticle.c sendpass.c \
	      	sequence.c timer.c tst.c uwildmat.c vector.c wire.c	   \
	      	xfopena.c xmalloc.c xsignal.c xwrite.c

# Sources for additional functions only built to replace missing system ones.
EXTRA_SOURCES = asprintf.c fseeko.c ftello.c getaddrinfo.c		   \
		getnameinfo.c getpagesize.c inet_aton.c inet_ntoa.c	   \
		inet_ntop.c mkstemp.c pread.c pwrite.c                     \
	        reallocarray.c sd-daemon.c setenv.c                        \
		seteuid.c setproctitle.c snprintf.c strcasecmp.c strlcat.c \
		strlcpy.c strspn.c strtok.c symlink.c

OBJECTS       = $(LIBOBJS) $(SOURCES:.c=.o)
LOBJECTS      = $(OBJECTS:.o=.lo)

.SUFFIXES: .lo

all: libinn.$(EXTLIB) perl.o

warnings:
	$(MAKE) COPT='$(WARNINGS)' all

install: all
	$(LI_LPUB) libinn.$(EXTLIB) $D$(PATHLIB)/libinn.$(EXTLIB)

bootstrap:

clean clobber distclean maintclean:
	rm -f *.o *.lo libinn.la libinn.a
	rm -f profiled perl$(PROFSUFFIX).o libinn$(PROFSUFFIX).a
	rm -f libinn_pure_*.a .pure
	rm -rf .libs

libinn.la: $(OBJECTS) $(LOBJECTS)
	$(LIBLD) $(LDFLAGS) -o $@ $(LOBJECTS) $(LIBS) \
	    -rpath $(PATHLIB) -version-info $(LTVERSION)

libinn.a: $(OBJECTS)
	ar r $@ $(OBJECTS)
	$(RANLIB) libinn.a

.c.o .c.lo:
	$(LIBCC) $(CFLAGS) -c $*.c

perl.o: perl.c
	$(CC) $(CFLAGS) $(PERL_CPPFLAGS) -c perl.c

../include/inn/system.h:
	(cd ../include && $(MAKE))

##  Profiling.  The rules are a bit brute-force, but good enough.
profiled: libinn$(PROFSUFFIX).a perl$(PROFSUFFIX).o
	date >$@

libinn$(PROFSUFFIX).a perl$(PROFSUFFIX).o: $(OBJECTS) perl.o
	rm -f $(OBJECTS)
	$(MAKEPROFILING) libinn.a
	$(MAKEPROFILING) perl.o
	mv libinn.a libinn$(PROFSUFFIX).a
	mv perl.o perl$(PROFSUFFIX).o
	$(RANLIB) libinn$(PROFSUFFIX).a
	rm -f $(OBJECTS)

##  Dependencies.  Default list, below, is probably good enough.

depend: Makefile $(SOURCES) $(EXTRA_SOURCES) perl.c ../include/inn/system.h
	$(MAKEDEPEND) '$(CFLAGS) $(PERL_CPPFLAGS)' $(SOURCES) \
	    $(EXTRA_SOURCES) perl.c

# DO NOT DELETE THIS LINE -- make depend depends on it.
argparse.o: argparse.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h ../include/inn/libinn.h ../include/inn/concat.h \
  ../include/inn/xmalloc.h ../include/inn/xwrite.h
artnumber.o: artnumber.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/libinn.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/xwrite.h
buffer.o: buffer.c ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/portable/system.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/buffer.h \
  ../include/inn/portable-stdbool.h ../include/inn/system.h \
  ../include/inn/xmalloc.h
cleanfrom.o: cleanfrom.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/libinn.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/xwrite.h
clientactive.o: clientactive.c ../include/portable/system.h \
  ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/portable/macros.h ../include/portable/stdbool.h \
  ../include/inn/innconf.h ../include/inn/macros.h \
  ../include/inn/portable-stdbool.h ../include/inn/system.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/xwrite.h ../include/inn/nntp.h ../include/inn/paths.h
clientlib.o: clientlib.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h ../include/inn/libinn.h ../include/inn/concat.h \
  ../include/inn/xmalloc.h ../include/inn/xwrite.h ../include/inn/nntp.h
commands.o: commands.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/libinn.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/xwrite.h
concat.o: concat.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/concat.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h ../include/inn/messages.h \
  ../include/inn/xmalloc.h
conffile.o: conffile.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/conffile.h \
  ../include/portable/macros.h ../include/inn/libinn.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/xwrite.h
confparse.o: confparse.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/confparse.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h ../include/inn/hashtab.h ../include/inn/libinn.h \
  ../include/inn/concat.h ../include/inn/xmalloc.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/vector.h
daemonize.o: daemonize.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/libinn.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/xwrite.h ../include/inn/messages.h
date.o: date.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/libinn.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/xwrite.h
dbz.o: dbz.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/portable/mmap.h \
  ../include/inn/dbz.h ../include/inn/libinn.h ../include/inn/macros.h \
  ../include/inn/portable-stdbool.h ../include/inn/system.h \
  ../include/inn/concat.h ../include/inn/xmalloc.h ../include/inn/xwrite.h \
  ../include/inn/fdflag.h ../include/inn/portable-socket.h \
  ../include/inn/portable-getaddrinfo.h \
  ../include/inn/portable-getnameinfo.h ../include/inn/innconf.h \
  ../include/inn/messages.h ../include/inn/mmap.h
defdist.o: defdist.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h ../include/inn/libinn.h ../include/inn/concat.h \
  ../include/inn/xmalloc.h ../include/inn/xwrite.h ../include/inn/paths.h
dispatch.o: dispatch.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/dispatch.h \
  ../include/inn/vector.h ../include/inn/system.h
fdflag.o: fdflag.c ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/portable/system.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/fdflag.h \
  ../include/inn/portable-socket.h ../include/inn/portable-getaddrinfo.h \
  ../include/inn/portable-getnameinfo.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h ../include/inn/macros.h
fdlimit.o: fdlimit.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/libinn.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/xwrite.h
getfqdn.o: getfqdn.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/inn/libinn.h ../include/inn/macros.h \
  ../include/inn/portable-stdbool.h ../include/inn/system.h \
  ../include/inn/concat.h ../include/inn/xmalloc.h ../include/inn/xwrite.h
getmodaddr.o: getmodaddr.c ../include/portable/system.h \
  ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/portable/macros.h ../include/portable/stdbool.h \
  ../include/inn/innconf.h ../include/inn/macros.h \
  ../include/inn/portable-stdbool.h ../include/inn/system.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/xwrite.h ../include/inn/nntp.h ../include/inn/paths.h
hash.o: hash.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/libinn.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/xwrite.h ../include/inn/md5.h ../include/inn/utility.h
hashtab.o: hashtab.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/hashtab.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h ../include/inn/libinn.h ../include/inn/concat.h \
  ../include/inn/xmalloc.h ../include/inn/xwrite.h
headers.o: headers.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/libinn.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/xwrite.h
hex.o: hex.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/utility.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h
innconf.o: innconf.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/confparse.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h ../include/inn/innconf.h ../include/inn/libinn.h \
  ../include/inn/concat.h ../include/inn/xmalloc.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/paths.h ../include/inn/vector.h
inndcomm.o: inndcomm.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/portable/socket-unix.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h ../include/inn/inndcomm.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/xwrite.h ../include/inn/paths.h
list.o: list.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/list.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h
localopen.o: localopen.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h ../include/inn/libinn.h ../include/inn/concat.h \
  ../include/inn/xmalloc.h ../include/inn/xwrite.h ../include/inn/nntp.h \
  ../include/inn/paths.h ../include/portable/socket-unix.h
lockfile.o: lockfile.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/libinn.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/xwrite.h
makedir.o: makedir.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/libinn.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/xwrite.h
md5.o: md5.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/md5.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h
messageid.o: messageid.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h ../include/inn/libinn.h ../include/inn/concat.h \
  ../include/inn/xmalloc.h ../include/inn/xwrite.h ../include/inn/nntp.h
messages.o: messages.c ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/portable/system.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/macros.h \
  ../include/inn/messages.h ../include/inn/system.h \
  ../include/inn/xmalloc.h
mmap.o: mmap.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/portable/mmap.h \
  ../include/inn/messages.h ../include/inn/system.h ../include/inn/mmap.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h
network.o: network.c ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/portable/socket.h ../include/portable/macros.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/portable/system.h ../include/portable/stdbool.h \
  ../include/inn/fdflag.h ../include/inn/portable-socket.h \
  ../include/inn/portable-stdbool.h ../include/inn/system.h \
  ../include/inn/innconf.h ../include/inn/macros.h \
  ../include/inn/messages.h ../include/inn/network.h \
  ../include/inn/xmalloc.h ../include/inn/xwrite.h
network-innbind.o: network-innbind.c ../include/portable/system.h \
  ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/portable/macros.h ../include/portable/stdbool.h \
  ../include/portable/socket.h ../include/portable/getaddrinfo.h \
  ../include/portable/getnameinfo.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h ../include/inn/libinn.h ../include/inn/concat.h \
  ../include/inn/xmalloc.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/network-innbind.h \
  ../include/inn/portable-socket.h ../include/inn/network.h
newsuser.o: newsuser.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h ../include/inn/messages.h \
  ../include/inn/newsuser.h
nntp.o: nntp.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/inn/buffer.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h ../include/inn/innconf.h ../include/inn/macros.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/xwrite.h ../include/inn/network.h \
  ../include/inn/portable-socket.h ../include/inn/nntp.h \
  ../include/inn/vector.h
qio.o: qio.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/libinn.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/xwrite.h ../include/inn/qio.h
radix32.o: radix32.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/libinn.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/xwrite.h
readin.o: readin.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/libinn.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/xwrite.h
remopen.o: remopen.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/inn/innconf.h ../include/inn/macros.h \
  ../include/inn/portable-stdbool.h ../include/inn/system.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/xwrite.h ../include/inn/network.h \
  ../include/inn/portable-socket.h ../include/inn/nntp.h
reservedfd.o: reservedfd.c ../include/portable/system.h \
  ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/portable/macros.h ../include/portable/stdbool.h \
  ../include/inn/libinn.h ../include/inn/macros.h \
  ../include/inn/portable-stdbool.h ../include/inn/system.h \
  ../include/inn/concat.h ../include/inn/xmalloc.h ../include/inn/xwrite.h
resource.o: resource.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/libinn.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/xwrite.h
sendarticle.o: sendarticle.c ../include/portable/system.h \
  ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/portable/macros.h ../include/portable/stdbool.h \
  ../include/inn/libinn.h ../include/inn/macros.h \
  ../include/inn/portable-stdbool.h ../include/inn/system.h \
  ../include/inn/concat.h ../include/inn/xmalloc.h ../include/inn/xwrite.h \
  ../include/inn/nntp.h
sendpass.o: sendpass.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h ../include/inn/libinn.h ../include/inn/concat.h \
  ../include/inn/xmalloc.h ../include/inn/xwrite.h ../include/inn/nntp.h \
  ../include/inn/paths.h
sequence.o: sequence.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/sequence.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h
timer.o: timer.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/libinn.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/xwrite.h ../include/inn/messages.h ../include/inn/timer.h
tst.o: tst.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/libinn.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/xwrite.h ../include/inn/tst.h
uwildmat.o: uwildmat.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/libinn.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/xwrite.h
vector.o: vector.c ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/portable/system.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/vector.h \
  ../include/inn/system.h ../include/inn/xmalloc.h
wire.o: wire.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/libinn.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/xwrite.h ../include/inn/wire.h
xfopena.o: xfopena.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/libinn.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/xwrite.h
xmalloc.o: xmalloc.c ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/portable/system.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/messages.h \
  ../include/inn/system.h ../include/inn/xmalloc.h
xsignal.o: xsignal.c ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/inn/libinn.h ../include/inn/macros.h \
  ../include/inn/portable-stdbool.h ../include/inn/system.h \
  ../include/inn/concat.h ../include/inn/xmalloc.h ../include/inn/xwrite.h
xwrite.o: xwrite.c ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/portable/system.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/portable/uio.h \
  ../include/inn/xwrite.h ../include/inn/system.h
asprintf.o: asprintf.c ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/portable/macros.h ../include/portable/system.h \
  ../include/portable/stdbool.h
fseeko.o: fseeko.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h
ftello.o: ftello.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h
getaddrinfo.o: getaddrinfo.c ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/portable/socket.h ../include/portable/macros.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/portable/system.h ../include/portable/stdbool.h
getnameinfo.o: getnameinfo.c ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/portable/macros.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/portable/system.h ../include/portable/stdbool.h
getpagesize.o: getpagesize.c ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h
inet_aton.o: inet_aton.c ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/portable/socket.h ../include/portable/macros.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/portable/system.h ../include/portable/stdbool.h
inet_ntoa.o: inet_ntoa.c ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/portable/socket.h ../include/portable/macros.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/portable/system.h ../include/portable/stdbool.h
inet_ntop.o: inet_ntop.c ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/portable/socket.h ../include/portable/macros.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/portable/system.h ../include/portable/stdbool.h
mkstemp.o: mkstemp.c ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/portable/system.h ../include/portable/macros.h \
  ../include/portable/stdbool.h
pread.o: pread.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h
pwrite.o: pwrite.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h
reallocarray.o: reallocarray.c ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/system.h \
  ../include/portable/macros.h ../include/portable/stdbool.h
sd-daemon.o: sd-daemon.c ../include/portable/sd-daemon.h \
  ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/portable/macros.h
setenv.o: setenv.c ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/portable/system.h ../include/portable/macros.h \
  ../include/portable/stdbool.h
seteuid.o: seteuid.c ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h
setproctitle.o: setproctitle.c ../include/portable/system.h \
  ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/portable/macros.h ../include/portable/stdbool.h \
  ../include/portable/setproctitle.h ../include/inn/messages.h \
  ../include/inn/system.h
snprintf.o: snprintf.c ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h
strcasecmp.o: strcasecmp.c ../include/portable/system.h \
  ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/portable/macros.h ../include/portable/stdbool.h
strlcat.o: strlcat.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h
strlcpy.o: strlcpy.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h
strspn.o: strspn.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h
strtok.o: strtok.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h
symlink.o: symlink.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h
perl.o: perl.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/libinn.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/system.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/xwrite.h \
  ../include/ppport.h \
  ../include/innperl.h \
  ../include/config.h

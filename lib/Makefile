##  $Id$

include ../Makefile.global

top	= ..
CFLAGS  = $(GCFLAGS)

# The base library files that are always compiled and included.
SOURCES       = buffer.c cleanfrom.c clientactive.c clientlib.c concat.c \
		conffile.c confparse.c daemonize.c date.c dbz.c defdist.c \
		fdflags.c fdlimit.c genid.c getfqdn.c getmodaddr.c gettime.c \
		hash.c hashtab.c innconf.c inndcomm.c localopen.c lockfile.c \
		makedir.c md5.c messages.c mmap.c parsedate.c qio.c \
		radix32.c readin.c remopen.c reservedfd.c resource.c \
		sendarticle.c sendpass.c sequence.c sockaddr.c timer.c tst.c \
		uwildmat.c vector.c version.c wire.c xfopena.c xmalloc.c \
		xsignal.c xwrite.c

# Sources for additional functions only built to replace missing system ones.
EXTRA_SOURCES = fseeko.c ftello.c getpagesize.c hstrerror.c inet_aton.c \
		inet_ntoa.c memcmp.c mkstemp.c pread.c pwrite.c setenv.c \
		setproctitle.c strcasecmp.c strerror.c strlcat.c strlcpy.c \
		strspn.c strtok.c

OBJECTS       = $(LIBOBJS) $(SOURCES:.c=.o)
LOBJECTS      = $(OBJECTS:.o=.lo)

.SUFFIXES: .lo

all: libinn.$(EXTLIB) perl.o

warnings:
	$(MAKE) COPT='$(WARNINGS)' all

install: all $(D)$(PATHLIB)/libinn.$(EXTLIB)

clobber clean distclean:
	rm -f *.o *.lo libinn.la libinn.a parsedate.c parsedate
	rm -f profiled perl$(PROFSUFFIX).o libinn$(PROFSUFFIX).a
	rm -f libinn_pure_*.a .pure
	rm -rf .libs

tags ctags: $(SOURCES)
	$(CTAGS) $(SOURCES) ../include/*.h

libinn.la: $(OBJECTS) $(LOBJECTS)
	$(LIBLD) $(LDFLAGS) -o $@ $(LOBJECTS) $(LIBS) \
	    -rpath $(PATHLIB) -version-info 2:0:0

libinn.a: $(OBJECTS)
	ar r $@ $(OBJECTS)
	$(RANLIB) libinn.a

.c.o .c.lo:
	$(LIBCC) $(CFLAGS) -c $*.c

perl.o: perl.c
	$(CC) $(CFLAGS) $(PERLINC) $(LDFLAGS) -c perl.c

../include/inn/system.h:
	(cd ../include && $(MAKE))

parsedate.c: parsedate.y
	@echo Expect 6 shift/reduce conflicts
	$(YACC) parsedate.y
	@mv y.tab.c parsedate.c

parsedate: parsedate.c gettime.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ -DTEST -DYYDEBUG parsedate.c gettime.o

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

##  Low-level install actions.
$(D)$(PATHLIB)/libinn.$(EXTLIB): libinn.$(EXTLIB)
	$(LI_XPUB) libinn.$(EXTLIB) $@

##  Dependencies.  Default list, below, is probably good enough.

depend: Makefile $(SOURCES) $(EXTRA_SOURCES) perl.c ../include/inn/system.h
	$(MAKEDEPEND) '$(CFLAGS) $(PERLINC)' $(SOURCES) $(EXTRA_SOURCES) perl.c

# Special dependency to teach make to build the include directory properly.
../include/inn/defines.h: ../include/inn/system.h

# DO NOT DELETE THIS LINE -- make depend depends on it.
buffer.o: buffer.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/buffer.h ../include/libinn.h
cleanfrom.o: cleanfrom.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h
clientactive.o: clientactive.c ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h \
 ../include/inn/innconf.h ../include/libinn.h ../include/nntp.h \
 ../include/paths.h
clientlib.o: clientlib.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/innconf.h ../include/libinn.h \
 ../include/nntp.h
concat.o: concat.c ../include/config.h ../include/inn/defines.h \
 ../include/libinn.h
conffile.o: conffile.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/conffile.h ../include/libinn.h
confparse.o: confparse.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/confparse.h \
 ../include/inn/hashtab.h ../include/inn/messages.h \
 ../include/inn/vector.h ../include/libinn.h
daemonize.o: daemonize.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/messages.h ../include/libinn.h
date.o: date.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h
dbz.o: dbz.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/dbz.h ../include/libinn.h \
 ../include/inn/innconf.h ../include/inn/mmap.h
defdist.o: defdist.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/innconf.h ../include/libinn.h \
 ../include/paths.h
fdflags.o: fdflags.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h
fdlimit.o: fdlimit.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h
genid.o: genid.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/innconf.h ../include/libinn.h
getfqdn.o: getfqdn.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h ../include/paths.h
getmodaddr.o: getmodaddr.c ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h \
 ../include/inn/innconf.h ../include/libinn.h ../include/nntp.h \
 ../include/paths.h
gettime.o: gettime.c ../include/config.h ../include/inn/defines.h \
 ../include/libinn.h
hash.o: hash.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/md5.h ../include/libinn.h
hashtab.o: hashtab.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/hashtab.h ../include/libinn.h
innconf.o: innconf.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/confparse.h \
 ../include/inn/innconf.h ../include/inn/messages.h \
 ../include/inn/vector.h ../include/libinn.h ../include/paths.h
inndcomm.o: inndcomm.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/portable/time.h \
 ../include/portable/socket.h ../include/inn/innconf.h \
 ../include/inndcomm.h ../include/libinn.h ../include/paths.h
localopen.o: localopen.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/innconf.h ../include/libinn.h \
 ../include/nntp.h ../include/paths.h
lockfile.o: lockfile.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h
makedir.o: makedir.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h
md5.o: md5.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/md5.h
messages.o: messages.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/messages.h ../include/libinn.h
mmap.o: mmap.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/portable/mmap.h \
 ../include/inn/messages.h ../include/inn/mmap.h
parsedate.o: parsedate.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h
qio.o: qio.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/qio.h ../include/libinn.h
radix32.o: radix32.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h
readin.o: readin.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h
remopen.o: remopen.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/portable/socket.h \
 ../include/inn/innconf.h ../include/libinn.h ../include/nntp.h \
 ../include/paths.h
reservedfd.o: reservedfd.c ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h ../include/libinn.h
resource.o: resource.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h
sendarticle.o: sendarticle.c ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h ../include/libinn.h \
 ../include/nntp.h
sendpass.o: sendpass.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/innconf.h ../include/libinn.h \
 ../include/nntp.h ../include/paths.h
sequence.o: sequence.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/sequence.h
sockaddr.o: sockaddr.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/portable/socket.h \
 ../include/libinn.h
timer.o: timer.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/portable/time.h \
 ../include/inn/messages.h ../include/inn/timer.h ../include/libinn.h
tst.o: tst.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/tst.h ../include/libinn.h
uwildmat.o: uwildmat.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h
vector.o: vector.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/vector.h ../include/libinn.h
version.o: version.c ../include/config.h ../include/inn/defines.h \
 ../include/inn/version.h
wire.o: wire.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/wire.h ../include/libinn.h
xfopena.o: xfopena.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h
xmalloc.o: xmalloc.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/messages.h ../include/libinn.h
xsignal.o: xsignal.c ../include/config.h ../include/inn/defines.h \
 ../include/libinn.h
xwrite.o: xwrite.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h
fseeko.o: fseeko.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h
ftello.o: ftello.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h
getpagesize.o: getpagesize.c ../include/config.h \
 ../include/inn/defines.h
hstrerror.o: hstrerror.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h
inet_aton.o: inet_aton.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h
inet_ntoa.o: inet_ntoa.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h
memcmp.o: memcmp.c ../include/config.h ../include/inn/defines.h
mkstemp.o: mkstemp.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/portable/time.h
pread.o: pread.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h
pwrite.o: pwrite.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h
setenv.o: setenv.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h
setproctitle.o: setproctitle.c ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h \
 ../include/portable/setproctitle.h ../include/inn/messages.h
strcasecmp.o: strcasecmp.c ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h
strerror.o: strerror.c ../include/config.h ../include/inn/defines.h
strlcat.o: strlcat.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h
strlcpy.o: strlcpy.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h
strspn.o: strspn.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h
strtok.o: strtok.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h
perl.o: perl.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h ../include/ppport.h \
 ../include/innperl.h

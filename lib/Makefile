##  $Id$

include ../Makefile.global

top	= ..
CFLAGS  = $(GCFLAGS)

SOURCES = argparse.c cleanfrom.c clientactive.c clientlib.c concat.c \
	conffile.c confparse.c daemonize.c date.c dbz.c defdist.c error.c \
	fdflags.c fdlimit.c findheader.c fseeko.c ftello.c genid.c \
	getconfig.c getfqdn.c getmodaddr.c gettime.c grpalias.c hash.c \
	hashtab.c hstrerror.c inet_aton.c inet_ntoa.c inndcomm.c localopen.c \
	lockfile.c makedir.c md5.c memcmp.c parsedate.c perl.c pread.c \
	pwrite.c qio.c radix32.c readin.c remopen.c reservedfd.c resource.c \
	rwlock.c sendarticle.c sendpass.c sequence.c setenv.c sockaddr.c \
	strcasecmp.c strerror.c strspn.c strtok.c tempname.c timer.c \
	wildmat.c vector.c version.c xfopena.c xmalloc.c xsignal.c xwrite.c

OBJECTS = $(LIBOBJS) \
	argparse.o cleanfrom.o clientactive.o clientlib.o concat.o \
	conffile.o confparse.o daemonize.o date.o dbz.o defdist.o error.o \
	fdflags.o fdlimit.o findheader.o genid.o getconfig.o getfqdn.o \
	getmodaddr.o gettime.o grpalias.o hash.o hashtab.o inndcomm.o \
	localopen.o lockfile.o makedir.o md5.o parsedate.o qio.o radix32.o \
	readin.o remopen.o reservedfd.o resource.o rwlock.o sendarticle.o \
	sendpass.o sequence.o sockaddr.o tempname.o timer.o wildmat.o \
	vector.o version.o xfopena.o xmalloc.o xsignal.o xwrite.o

LOBJECTS= $(OBJECTS:.o=.lo)

.SUFFIXES: .lo

all:		libinn.$(EXTLIB) perl.o

warnings:
	$(MAKE) COPT='$(WARNINGS)' all

install:	all $(D)$(PATHLIB)/libinn.$(EXTLIB)

clobber clean distclean:
	rm -f *.o *.lo libinn.la libinn.a parsedate.c parsedate
	rm -f profiled libinn_p.a
	rm -rf .libs

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES) ../include/*.h

libinn.la:	$(OBJECTS) $(LOBJECTS)
	$(LIBLD) $(LDFLAGS) -o $@ $(LOBJECTS) $(LIBS) \
	    -rpath $(PATHLIB) -version-info 2:0:0

libinn.a:       $(OBJECTS)
	ar r $@ $(OBJECTS)
	$(RANLIB) libinn.a

.c.o .c.lo:
	$(LIBCC) $(CFLAGS) -c $*.c

perl.o: perl.c
	$(CC) $(CFLAGS) $(PERLINC) $(LDFLAGS) -c perl.c

../include/inn/version.h:
	(cd ../include && $(MAKE))

parsedate.c:	parsedate.y
	@echo Expect 6 shift/reduce conflicts
	$(YACC) parsedate.y
	@mv y.tab.c parsedate.c

parsedate:	parsedate.c gettime.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ -DTEST -DYYDEBUG parsedate.c gettime.o

##  Profiling.  The rules are a bit brute-force, but good enough.
profiled:	../libinn_p.a
	date >$@

../libinn_p.a:	$(SOURCES)
	rm -f $(OBJECTS)
	$(MAKE) libinn.a CFLAGS="$(CFLAGS) $(PROF)"
	mv libinn.a ../libinn_p.a
	$(RANLIB) ../libinn_p.a
	rm -f $(OBJECTS)

##  Low-level install actions.
$(D)$(PATHLIB)/libinn.$(EXTLIB): libinn.$(EXTLIB)
	$(LI_XPUB) libinn.$(EXTLIB) $@

##  Dependencies.  Default list, below, is probably good enough.

depend: Makefile $(SOURCES) ../include/inn/version.h
	$(MAKEDEPEND) $(CFLAGS) $(PERLINC) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
argparse.o: argparse.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h ../include/macros.h
cleanfrom.o: cleanfrom.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/macros.h
clientactive.o: clientactive.c ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h ../include/libinn.h \
 ../include/macros.h ../include/nntp.h ../include/paths.h
clientlib.o: clientlib.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h ../include/nntp.h
concat.o: concat.c ../include/config.h ../include/inn/defines.h \
 ../include/libinn.h
conffile.o: conffile.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/conffile.h ../include/libinn.h \
 ../include/macros.h
confparse.o: confparse.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/confparse.h \
 ../include/inn/hashtab.h ../include/inn/vector.h ../include/libinn.h \
 ../include/macros.h
daemonize.o: daemonize.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h
date.o: date.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h
dbz.o: dbz.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/dbz.h ../include/libinn.h \
 ../include/macros.h
defdist.o: defdist.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h ../include/macros.h \
 ../include/paths.h
error.o: error.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h
fdflags.o: fdflags.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h
fdlimit.o: fdlimit.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h
findheader.o: findheader.c ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h ../include/inn/qio.h \
 ../include/libinn.h ../include/macros.h
fseeko.o: fseeko.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h
ftello.o: ftello.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h
genid.o: genid.c ../include/config.h ../include/inn/defines.h \
 ../include/libinn.h ../include/macros.h
getconfig.o: getconfig.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/innconf.h ../include/libinn.h \
 ../include/macros.h ../include/paths.h
getfqdn.o: getfqdn.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h ../include/paths.h
getmodaddr.o: getmodaddr.c ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h ../include/libinn.h \
 ../include/macros.h ../include/nntp.h ../include/paths.h
gettime.o: gettime.c ../include/config.h ../include/inn/defines.h \
 ../include/libinn.h
grpalias.o: grpalias.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/qio.h ../include/libinn.h \
 ../include/macros.h ../include/paths.h
hash.o: hash.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/md5.h ../include/libinn.h \
 ../include/macros.h
hashtab.o: hashtab.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/hashtab.h ../include/libinn.h \
 ../include/macros.h
hstrerror.o: hstrerror.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h
inet_aton.o: inet_aton.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h
inet_ntoa.o: inet_ntoa.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h
inndcomm.o: inndcomm.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/portable/time.h \
 ../include/portable/socket.h ../include/inndcomm.h \
 ../include/libinn.h ../include/macros.h ../include/paths.h
localopen.o: localopen.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h ../include/macros.h \
 ../include/nntp.h ../include/paths.h
lockfile.o: lockfile.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h
makedir.o: makedir.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h
md5.o: md5.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/md5.h
memcmp.o: memcmp.c ../include/config.h ../include/inn/defines.h
parsedate.o: parsedate.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h ../include/macros.h
perl.o: perl.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h ../include/macros.h \
 ../include/ppport.h ../include/innperl.h
pread.o: pread.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h
pwrite.o: pwrite.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h
qio.o: qio.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/qio.h ../include/libinn.h \
 ../include/macros.h
radix32.o: radix32.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h
readin.o: readin.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h ../include/macros.h
remopen.o: remopen.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/portable/socket.h \
 ../include/libinn.h ../include/nntp.h ../include/paths.h
reservedfd.o: reservedfd.c ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h ../include/libinn.h \
 ../include/macros.h
resource.o: resource.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/macros.h
rwlock.o: rwlock.c ../include/rwlock.h
sendarticle.o: sendarticle.c ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h ../include/libinn.h \
 ../include/nntp.h
sendpass.o: sendpass.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h ../include/macros.h \
 ../include/nntp.h ../include/paths.h
sequence.o: sequence.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/sequence.h
setenv.o: setenv.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h
sockaddr.o: sockaddr.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/portable/socket.h
strcasecmp.o: strcasecmp.c ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h
strerror.o: strerror.c ../include/config.h ../include/inn/defines.h
strspn.o: strspn.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h
strtok.o: strtok.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h
tempname.o: tempname.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h
timer.o: timer.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h ../include/inn/timer.h \
 ../include/portable/time.h
wildmat.o: wildmat.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h
vector.o: vector.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/vector.h ../include/libinn.h
version.o: version.c ../include/config.h ../include/inn/defines.h \
 ../include/inn/version.h
xfopena.o: xfopena.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h
xmalloc.o: xmalloc.c ../include/config.h ../include/inn/defines.h \
 ../include/libinn.h
xsignal.o: xsignal.c ../include/config.h ../include/inn/defines.h \
 ../include/libinn.h
xwrite.o: xwrite.c ../include/config.h ../include/inn/defines.h \
 ../include/libinn.h

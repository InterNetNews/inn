##  $Id$

include ../Makefile.global

CFLAGS  = $(GCFLAGS)

SOURCES = argparse.c cleanfrom.c clientactive.c clientlib.c closeonexec.c \
	concat.c conffile.c date.c dbz.c defdist.c endian.c error.c \
	fdlimit.c findheader.c fseeko.c ftello.c genid.c getconfig.c \
	getfqdn.c getmodaddr.c getopt.c gettime.c grpalias.c hash.c \
	hstrerror.c inet_aton.c inet_ntoa.c inndcomm.c localopen.c \
	lockfile.c makedir.c md5.c memchr.c memcmp.c memmove.c memset.c \
	mkfifo.c nonblocking.c parsedate.c perl.c pread.c pwrite.c qio.c \
	radix32.c readin.c remopen.c reservedfd.c resource.c rwlock.c \
	sendarticle.c sendpass.c setenv.c strcasecmp.c strerror.c strspn.c \
	strtok.c tempname.c waitnb.c wildmat.c version.c xfopena.c \
	xmalloc.c xsignal.c xwrite.c

OBJECTS = $(LIBOBJS) \
	argparse.o cleanfrom.o clientactive.o clientlib.o closeonexec.o \
	concat.o conffile.o date.o dbz.o defdist.o error.o fdlimit.o \
	findheader.o genid.o getconfig.o getfqdn.o getmodaddr.o gettime.o \
	grpalias.o hash.o inndcomm.o localopen.o lockfile.o makedir.o md5.o \
	nonblocking.o parsedate.o qio.o radix32.o readin.o remopen.o \
	reservedfd.o resource.o rwlock.o sendarticle.o sendpass.o \
	tempname.o waitnb.o wildmat.o version.o xfopena.o xmalloc.o \
	xsignal.o xwrite.o

LOBJECTS= $(OBJECTS:.o=.lo)

all:		libinn.$(EXTLIB) perl.o ../include/autoconfig.h

install:	all $(D)$(PATHLIB)/libinn.$(EXTLIB)

clobber clean:
	rm -f *.o *.lo libinn.la libinn.a parsedate.c parsedate
	rm -f profiled libinn_p.a endian version.c
	rm -rf .libs

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES) ../include/*.h

libinn.la:	$(OBJECTS) $(LOBJECTS)
	$(LIBCC) $(LDFLAGS) -o $@ $(LOBJECTS) \
	    -rpath $(PATHLIB) -version-info 2:0:0

libinn.a:       $(OBJECTS)
	ar r $@ $(OBJECTS)
	$(RANLIB) libinn.a

.c.o:
	$(LIBCC) $(CFLAGS) -c $*.c

endian: endian.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ endian.c

perl.o: perl.c
	$(CC) $(CFLAGS) $(PERLINC) $(LDFLAGS) -c perl.c

../include/autoconfig.h: endian
	./endian >../include/autoconfig.h

version.c:	mkversion ../Makefile.global
	./mkversion '$(VERSION)' '$(VERSION_EXTRA)'

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

depend: Makefile $(SOURCES) ../include/autoconfig.h
	$(MAKEDEPEND) $(CFLAGS) $(PERLINC) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
argparse.o: argparse.c ../include/configdata.h ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h ../include/libinn.h \
 ../include/macros.h
cleanfrom.o: cleanfrom.c ../include/configdata.h ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h ../include/macros.h
clientactive.o: clientactive.c ../include/configdata.h \
 ../include/config.h ../include/inn/defines.h ../include/clibrary.h \
 ../include/paths.h ../include/libinn.h ../include/nntp.h \
 ../include/macros.h
clientlib.o: clientlib.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h ../include/nntp.h
closeonexec.o: closeonexec.c ../include/config.h \
 ../include/inn/defines.h ../include/libinn.h
concat.o: concat.c ../include/config.h ../include/inn/defines.h \
 ../include/libinn.h
conffile.o: conffile.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/conffile.h ../include/libinn.h \
 ../include/macros.h
date.o: date.c ../include/config.h ../include/inn/defines.h \
 ../include/libinn.h
dbz.o: dbz.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/dbz.h ../include/libinn.h \
 ../include/macros.h ../include/md5.h
defdist.o: defdist.c ../include/configdata.h ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h ../include/libinn.h \
 ../include/paths.h ../include/macros.h
endian.o: endian.c
error.o: error.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h
findheader.o: findheader.c ../include/configdata.h ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h ../include/libinn.h \
 ../include/macros.h ../include/inn/qio.h
fseeko.o: fseeko.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h
ftello.o: ftello.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h
genid.o: genid.c ../include/config.h ../include/inn/defines.h \
 ../include/libinn.h
getconfig.o: getconfig.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/innconf.h ../include/libinn.h \
 ../include/macros.h ../include/paths.h
getdtab.o: getdtab.c ../include/config.h ../include/inn/defines.h
getfqdn.o: getfqdn.c ../include/configdata.h ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h ../include/paths.h \
 ../include/libinn.h
getmodaddr.o: getmodaddr.c ../include/configdata.h ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h ../include/paths.h \
 ../include/libinn.h ../include/macros.h ../include/nntp.h
getopt.o: getopt.c ../include/configdata.h ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h
gettime.o: gettime.c ../include/config.h ../include/inn/defines.h \
 ../include/libinn.h
grpalias.o: grpalias.c ../include/configdata.h ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h ../include/macros.h \
 ../include/libinn.h ../include/paths.h ../include/inn/qio.h
hash.o: hash.c ../include/configdata.h ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h ../include/libinn.h \
 ../include/macros.h ../include/md5.h
hstrerror.o: hstrerror.c ../include/configdata.h ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h
inet_aton.o: inet_aton.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h
inet_ntoa.o: inet_ntoa.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h
inndcomm.o: inndcomm.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inndcomm.h ../include/libinn.h \
 ../include/macros.h ../include/paths.h
localopen.o: localopen.c ../include/configdata.h ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h ../include/paths.h \
 ../include/nntp.h ../include/macros.h ../include/libinn.h
lockfile.o: lockfile.c ../include/config.h ../include/inn/defines.h \
 ../include/libinn.h
makedir.o: makedir.c ../include/configdata.h ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h ../include/libinn.h
md5.o: md5.c ../include/configdata.h ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h ../include/md5.h \
 ../include/autoconfig.h
memchr.o: memchr.c ../include/configdata.h ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h
memcmp.o: memcmp.c ../include/config.h ../include/inn/defines.h
memmove.o: memmove.c ../include/configdata.h ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h
memset.o: memset.c ../include/configdata.h ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h
mkfifo.o: mkfifo.c ../include/configdata.h ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h
nonblocking.o: nonblocking.c ../include/config.h \
 ../include/inn/defines.h
parsedate.o: parsedate.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h ../include/macros.h
perl.o: perl.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h ../include/macros.h \
 ../include/ppport.h
pread.o: pread.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h
pwrite.o: pwrite.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h
qio.o: qio.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/qio.h ../include/libinn.h \
 ../include/macros.h
radix32.o: radix32.c ../include/configdata.h ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h
readin.o: readin.c ../include/configdata.h ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h ../include/libinn.h \
 ../include/macros.h
remopen.o: remopen.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h ../include/nntp.h \
 ../include/paths.h
reservedfd.o: reservedfd.c ../include/configdata.h ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h ../include/macros.h \
 ../include/libinn.h
resource.o: resource.c ../include/configdata.h ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h ../include/macros.h
rwlock.o: rwlock.c ../include/rwlock.h
sendarticle.o: sendarticle.c ../include/configdata.h \
 ../include/config.h ../include/inn/defines.h ../include/clibrary.h \
 ../include/libinn.h ../include/nntp.h
sendpass.o: sendpass.c ../include/configdata.h ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h ../include/nntp.h \
 ../include/paths.h ../include/libinn.h ../include/macros.h
setenv.o: setenv.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h
strcasecmp.o: strcasecmp.c ../include/configdata.h ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h
strerror.o: strerror.c ../include/config.h ../include/inn/defines.h
strspn.o: strspn.c ../include/configdata.h ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h
strtok.o: strtok.c ../include/configdata.h ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h
tempname.o: tempname.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h
waitnb.o: waitnb.c ../include/config.h ../include/inn/defines.h
wildmat.o: wildmat.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h
version.o: version.c
xfopena.o: xfopena.c ../include/configdata.h ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h
xmalloc.o: xmalloc.c ../include/config.h ../include/inn/defines.h \
 ../include/libinn.h
xsignal.o: xsignal.c ../include/config.h ../include/inn/defines.h \
 ../include/libinn.h
xwrite.o: xwrite.c ../include/config.h ../include/inn/defines.h \
 ../include/libinn.h

##  $Id$

include ../Makefile.global

CFLAGS  = $(GCFLAGS)

##  MISSING_SRC and MISSING_OBJ fill in standard library functions the
##  system doesn't have, as detected by configure and recorded in
##  Makefile.global.

SOURCES = $(MISSING_SRC) \
	argparse.c checkart.c cleanfrom.c clientactive.c clientlib.c \
	closeonexec.c concat.c conffile.c date.c dbz.c defdist.c findheader.c \
	genid.c getconfig.c getdtab.c getfqdn.c getmodaddr.c gettime.c \
	grpalias.c hash.c inndcomm.c localopen.c lockfile.c makedir.c \
	md5.c nonblocking.c parsedate.c perl.c qio.c radix32.c readin.c \
	remopen.c reservedfd.c resource.c rwlock.c sendarticle.c \
	sendpass.c tempname.c waitnb.c wildmat.c version.c xfopena.c \
	xmalloc.c xsignal.c xwrite.c

OBJECTS = $(MISSING_OBJ) \
	argparse.o checkart.o cleanfrom.o clientactive.o clientlib.o \
	closeonexec.o concat.o conffile.o date.o dbz.o defdist.o findheader.o \
	genid.o getconfig.o getdtab.o getfqdn.o getmodaddr.o gettime.o \
	grpalias.o hash.o inndcomm.o localopen.o lockfile.o makedir.o \
	md5.o nonblocking.o parsedate.o qio.o radix32.o readin.o \
	remopen.o reservedfd.o resource.o rwlock.o sendarticle.o \
	sendpass.o tempname.o waitnb.o wildmat.o version.o xfopena.o \
	xmalloc.o xsignal.o xwrite.o

LOBJECTS= $(OBJECTS:.o=.lo)

all:		../include/inn/version.h libinn.$(EXTLIB) perl.o ../include/autoconfig.h

install:	all $(D)$(PATHLIB)/libinn.$(EXTLIB)

clobber clean:
	rm -f *.o *.lo libinn.la libinn.a parsedate.c parsedate qiotest
	rm -f profiled libinn_p.a endian ../include/inn/version.h
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

../include/inn/version.h:	mkversion ../Makefile.global
	-mkdir ../include/inn > /dev/null 2>&1
	./mkversion '$(VERSION)' '$(VERSION_EXTRA)' $@

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
setenv.o: setenv.c ../include/configdata.h ../include/config.h \
 ../include/clibrary.h ../include/libinn.h ../include/macros.h
argparse.o: argparse.c ../include/configdata.h ../include/config.h \
 ../include/clibrary.h ../include/libinn.h ../include/macros.h
checkart.o: checkart.c ../include/configdata.h ../include/config.h \
 ../include/clibrary.h ../include/nntp.h
cleanfrom.o: cleanfrom.c ../include/configdata.h ../include/config.h \
 ../include/clibrary.h ../include/macros.h
clientactive.o: clientactive.c ../include/configdata.h \
 ../include/config.h ../include/clibrary.h ../include/paths.h \
 ../include/libinn.h ../include/nntp.h ../include/macros.h
clientlib.o: clientlib.c ../include/config.h ../include/clibrary.h \
 ../include/libinn.h ../include/nntp.h
closeonexec.o: closeonexec.c ../include/config.h ../include/libinn.h
concat.o: concat.c ../include/config.h ../include/libinn.h \
 ../include/macros.h
conffile.o: conffile.c ../include/config.h ../include/clibrary.h \
 ../include/conffile.h ../include/libinn.h ../include/macros.h
date.o: date.c ../include/config.h ../include/clibrary.h \
 ../include/libinn.h
dbz.o: dbz.c ../include/config.h ../include/clibrary.h \
 ../include/md5.h ../include/libinn.h ../include/macros.h \
 ../include/dbz.h
defdist.o: defdist.c ../include/configdata.h ../include/config.h \
 ../include/clibrary.h ../include/libinn.h ../include/paths.h \
 ../include/macros.h
findheader.o: findheader.c ../include/configdata.h ../include/config.h \
 ../include/clibrary.h ../include/libinn.h ../include/macros.h \
 ../include/qio.h
genid.o: genid.c ../include/config.h ../include/libinn.h \
 ../include/macros.h
getconfig.o: getconfig.c ../include/config.h ../include/clibrary.h \
 ../include/innconf.h ../include/libinn.h ../include/macros.h \
 ../include/paths.h
getdtab.o: getdtab.c ../include/config.h
getfqdn.o: getfqdn.c ../include/configdata.h ../include/config.h \
 ../include/clibrary.h ../include/paths.h ../include/libinn.h
getmodaddr.o: getmodaddr.c ../include/configdata.h ../include/config.h \
 ../include/clibrary.h ../include/paths.h ../include/libinn.h \
 ../include/macros.h ../include/nntp.h
gettime.o: gettime.c ../include/config.h ../include/libinn.h
grpalias.o: grpalias.c ../include/configdata.h ../include/config.h \
 ../include/clibrary.h ../include/macros.h ../include/libinn.h \
 ../include/paths.h ../include/qio.h
hash.o: hash.c ../include/configdata.h ../include/config.h \
 ../include/clibrary.h ../include/libinn.h ../include/macros.h \
 ../include/md5.h
inndcomm.o: inndcomm.c ../include/config.h ../include/clibrary.h \
 ../include/inndcomm.h ../include/libinn.h ../include/macros.h \
 ../include/paths.h
localopen.o: localopen.c ../include/configdata.h ../include/config.h \
 ../include/clibrary.h ../include/paths.h ../include/nntp.h \
 ../include/macros.h ../include/libinn.h
lockfile.o: lockfile.c ../include/config.h ../include/libinn.h
makedir.o: makedir.c ../include/configdata.h ../include/config.h \
 ../include/clibrary.h ../include/libinn.h
md5.o: md5.c ../include/configdata.h ../include/config.h \
 ../include/clibrary.h ../include/md5.h ../include/autoconfig.h
nonblocking.o: nonblocking.c ../include/config.h
parsedate.o: parsedate.c ../include/config.h ../include/clibrary.h \
 ../include/libinn.h ../include/macros.h
perl.o: perl.c ../include/configdata.h ../include/config.h \
 ../include/clibrary.h ../include/libinn.h ../include/macros.h \
 ../include/ppport.h
qio.o: qio.c ../include/configdata.h ../include/config.h \
 ../include/clibrary.h ../include/libinn.h ../include/qio.h \
 ../include/macros.h
radix32.o: radix32.c ../include/configdata.h ../include/config.h \
 ../include/clibrary.h
readin.o: readin.c ../include/configdata.h ../include/config.h \
 ../include/clibrary.h ../include/libinn.h ../include/macros.h
remopen.o: remopen.c ../include/config.h ../include/clibrary.h \
 ../include/libinn.h ../include/nntp.h ../include/paths.h
reservedfd.o: reservedfd.c ../include/configdata.h ../include/config.h \
 ../include/clibrary.h ../include/macros.h ../include/libinn.h
resource.o: resource.c ../include/configdata.h ../include/config.h \
 ../include/clibrary.h ../include/macros.h
rwlock.o: rwlock.c ../include/rwlock.h
sendarticle.o: sendarticle.c ../include/configdata.h \
 ../include/config.h ../include/clibrary.h ../include/libinn.h \
 ../include/nntp.h
sendpass.o: sendpass.c ../include/configdata.h ../include/config.h \
 ../include/clibrary.h ../include/nntp.h ../include/paths.h \
 ../include/libinn.h ../include/macros.h
tempname.o: tempname.c ../include/config.h ../include/clibrary.h
waitnb.o: waitnb.c ../include/config.h
wildmat.o: wildmat.c ../include/configdata.h ../include/config.h \
 ../include/clibrary.h
version.o: version.c ../include/inn/version.h
xfopena.o: xfopena.c ../include/configdata.h ../include/config.h \
 ../include/clibrary.h
xmalloc.o: xmalloc.c ../include/config.h ../include/libinn.h
xsignal.o: xsignal.c ../include/config.h ../include/libinn.h
xwrite.o: xwrite.c ../include/config.h ../include/clibrary.h \
 ../include/libinn.h ../include/macros.h

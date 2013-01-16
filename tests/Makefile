##  $Id$

include ../Makefile.global

top		= ..
RUNTESTS_CFLAGS	= -DSOURCE='"$(abs_builddir)/tests"' -DBUILD='"$(abs_builddir)/tests"'
CFLAGS		= $(GCFLAGS) $(DB_CPPFLAGS) $(DBM_CPPFLAGS) $(PERL_CPPFLAGS) $(PYTHON_CPPFLAGS) $(SSL_CPPFLAGS) $(SASL_CPPFLAGS) $(KRB5_CPPFLAGS) $(RUNTESTS_CFLAGS) -I.

##  The tests that need to be built.  Tests in the form of shell scripts
##  or some other form that doesn't require compiling shouldn't be in this
##  list.  If they need other things compiled, those other things should be
##  added to EXTRA.

TESTS	= authprogs/ident.t innd/artparse.t innd/chan.t lib/asprintf.t \
	lib/buffer.t lib/concat.t lib/conffile.t lib/confparse.t lib/date.t \
	lib/dispatch.t lib/getaddrinfo.t lib/getnameinfo.t lib/hash.t \
	lib/hashtab.t lib/hex.t lib/inet_aton.t \
	lib/inet_ntoa.t lib/inet_ntop.t lib/innconf.t lib/list.t lib/md5.t \
	lib/memcmp.t lib/messages.t lib/mkstemp.t lib/network.t lib/pread.t \
	lib/pwrite.t lib/qio.t lib/setenv.t lib/snprintf.t lib/strlcat.t \
	lib/strlcpy.t lib/tst.t lib/uwildmat.t lib/vector.t lib/wire.t \
	lib/xwrite.t nnrpd/auth-ext.t overview/api.t overview/buffindexed.t \
	overview/tradindexed.t overview/xref.t util/innbind.t

##  Extra stuff that needs to be built before tests can be run.

EXTRA	= runtests clients/server-list lib/xmalloc

all check test tests: $(TESTS) $(EXTRA)
	./runtests TESTS

build: $(TESTS) $(EXTRA)

warnings:
	$(MAKE) COPT='$(WARNINGS)' build

clean clobber distclean maintclean:
	rm -f *.o *.lo */*.o */*.lo .pure */.pure $(TESTS) $(EXTRA)
	rm -rf .libs */.libs

$(FIXSCRIPT):
	@echo Run configure before running make.  See INSTALL for details.
	@exit 1

.c.o: $*.c
	$(CC) $(CFLAGS) -c -o $@ $*.c

LINK		= $(LIBTOOLLD) $(CC) $(LDFLAGS) $(DB_LDFLAGS) $(SSL_LDFLAGS) $(SASL_LDFLAGS) $(KRB5_LDFLAGS) -o $@
STORAGEDEPS	= $(LIBSTORAGE) $(LIBHIST) $(LIBINN)
STORAGELIBS	= $(STORAGEDEPS) $(STORAGE_LIBS)

# All of the innd object files other than innd.o, for INN unit testing.
INNOBJS		= ../innd/art.o ../innd/cc.o ../innd/chan.o ../innd/icd.o \
		../innd/keywords.o ../innd/lc.o ../innd/nc.o \
		../innd/newsfeeds.o ../innd/ng.o ../innd/perl.o \
		../innd/proc.o ../innd/python.o ../innd/rc.o ../innd/site.o \
		../innd/status.o ../innd/util.o ../innd/wip.o

# The libraries innd needs to link.
INNDLIBS        = $(LIBSTORAGE) $(LIBHIST) $(LIBINN) $(STORAGE_LIBS) \
		$(PYTHON_LIBS) $(REGEX_LIBS) $(LIBS) $(PERL_LIBS)

runtests: runtests.o
	$(LINK) runtests.o

clients/server-list: clients/server-list.in $(FIXSCRIPT)
	$(FIXSCRIPT) -i clients/server-list.in

authprogs/ident.t: authprogs/ident-t.o libtest.o $(LIBINN)
	$(LINK) authprogs/ident-t.o libtest.o $(LIBINN) $(LIBS)

innd/artparse.t: innd/artparse-t.o innd/fakeinnd.o libtest.o $(INNOBJS)
	$(LINK) innd/artparse-t.o innd/fakeinnd.o libtest.o $(INNOBJS) \
	    $(INNDLIBS)

innd/chan.t: innd/chan-t.o innd/fakeinnd.o libtest.o $(INNOBJS)
	$(LINK) innd/chan-t.o innd/fakeinnd.o libtest.o $(INNOBJS) $(INNDLIBS)

lib/asprintf.o: ../lib/asprintf.c
	$(CC) $(CFLAGS) -DTESTING -c -o $@ ../lib/asprintf.c

lib/asprintf.t: lib/asprintf.o lib/asprintf-t.o libtest.o $(LIBINN)
	$(LINK) lib/asprintf.o lib/asprintf-t.o libtest.o $(LIBINN) $(LIBS)

lib/buffer.t: lib/buffer-t.o libtest.o $(LIBINN)
	$(LINK) lib/buffer-t.o libtest.o $(LIBINN)

lib/concat.t: lib/concat-t.o libtest.o $(LIBINN)
	$(LINK) lib/concat-t.o libtest.o $(LIBINN)

lib/conffile.t: lib/conffile-t.o libtest.o $(LIBINN)
	$(LINK) lib/conffile-t.o libtest.o $(LIBINN)

lib/confparse.t: lib/confparse-t.o libtest.o $(LIBINN)
	$(LINK) lib/confparse-t.o libtest.o $(LIBINN)

lib/date.t: lib/date-t.o libtest.o $(LIBINN)
	$(LINK) lib/date-t.o libtest.o $(LIBINN)

lib/dispatch.t: lib/dispatch-t.o libtest.o $(LIBINN)
	$(LINK) lib/dispatch-t.o libtest.o $(LIBINN)

lib/getaddrinfo.o: ../lib/getaddrinfo.c
	$(CC) $(CFLAGS) -DTESTING -c -o $@ ../lib/getaddrinfo.c

lib/getaddrinfo.t: lib/getaddrinfo.o lib/getaddrinfo-t.o libtest.o $(LIBINN)
	$(LINK) lib/getaddrinfo.o lib/getaddrinfo-t.o libtest.o \
	    $(LIBINN) $(LIBS)

lib/getnameinfo.o: ../lib/getnameinfo.c
	$(CC) $(CFLAGS) -DTESTING -c -o $@ ../lib/getnameinfo.c

lib/getnameinfo.t: lib/getnameinfo.o lib/getnameinfo-t.o libtest.o $(LIBINN)
	$(LINK) lib/getnameinfo.o lib/getnameinfo-t.o libtest.o \
	    $(LIBINN) $(LIBS)

lib/hash.t: lib/hash-t.o libtest.o $(LIBINN)
	$(LINK) lib/hash-t.o libtest.o $(LIBINN)

lib/hashtab.t: lib/hashtab-t.o libtest.o $(LIBINN)
	$(LINK) lib/hashtab-t.o libtest.o $(LIBINN)

lib/hex.t: lib/hex-t.o libtest.o $(LIBINN)
	$(LINK) lib/hex-t.o libtest.o $(LIBINN) $(LIBS)

lib/inet_aton.o: ../lib/inet_aton.c
	$(CC) $(CFLAGS) -DTESTING -c -o $@ ../lib/inet_aton.c

lib/inet_aton.t: lib/inet_aton.o lib/inet_aton-t.o libtest.o $(LIBINN)
	$(LINK) lib/inet_aton.o lib/inet_aton-t.o libtest.o $(LIBINN)

lib/inet_ntoa.o: ../lib/inet_ntoa.c
	$(CC) $(CFLAGS) -DTESTING -c -o $@ ../lib/inet_ntoa.c

lib/inet_ntoa.t: lib/inet_ntoa.o lib/inet_ntoa-t.o libtest.o $(LIBINN)
	$(LINK) lib/inet_ntoa.o lib/inet_ntoa-t.o libtest.o $(LIBINN)

lib/inet_ntop.o: ../lib/inet_ntop.c
	$(CC) $(CFLAGS) -DTESTING -c -o $@ ../lib/inet_ntop.c

lib/inet_ntop.t: lib/inet_ntop.o lib/inet_ntop-t.o libtest.o $(LIBINN)
	$(LINK) lib/inet_ntop.o lib/inet_ntop-t.o libtest.o $(LIBINN)

lib/innconf.t: lib/innconf-t.o libtest.o $(LIBINN)
	$(LINK) lib/innconf-t.o libtest.o $(LIBINN) $(LIBS)

lib/list.t: lib/list-t.o libtest.o $(LIBINN)
	$(LINK) lib/list-t.o libtest.o $(LIBINN) $(LIBS)

lib/md5.t: lib/md5-t.o libtest.o $(LIBINN)
	$(LINK) lib/md5-t.o libtest.o $(LIBINN)

lib/memcmp.o: ../lib/memcmp.c
	$(CC) $(CFLAGS) -DTESTING -c -o $@ ../lib/memcmp.c

lib/memcmp.t: lib/memcmp.o lib/memcmp-t.o libtest.o
	$(LINK) lib/memcmp.o lib/memcmp-t.o libtest.o $(LIBINN)

lib/messages.o: ../lib/messages.c
	$(CC) $(CFLAGS) -DDEBUG -c -o $@ ../lib/messages.c

lib/messages-t.o: lib/messages-t.c
	$(CC) $(CFLAGS) -DDEBUG -c -o $@ lib/messages-t.c

lib/messages.t: lib/messages.o lib/messages-t.o libtest.o $(LIBINN)
	$(LINK) lib/messages-t.o lib/messages.o libtest.o $(LIBINN)

lib/mkstemp.o: ../lib/mkstemp.c
	$(CC) $(CFLAGS) -DTESTING -c -o $@ ../lib/mkstemp.c

lib/mkstemp.t: lib/mkstemp.o lib/mkstemp-t.o libtest.o
	$(LINK) lib/mkstemp.o lib/mkstemp-t.o libtest.o $(LIBINN)

lib/network.t: lib/network-t.o libtest.o $(LIBINN)
	$(LINK) lib/network-t.o libtest.o $(LIBINN) $(LIBS)

lib/pread.o: ../lib/pread.c
	$(CC) $(CFLAGS) -DTESTING -c -o $@ ../lib/pread.c

lib/pread.t: lib/pread.o lib/pread-t.o libtest.o $(LIBINN)
	$(LINK) lib/pread.o lib/pread-t.o libtest.o $(LIBINN)

lib/pwrite.o: ../lib/pwrite.c
	$(CC) $(CFLAGS) -DTESTING -c -o $@ ../lib/pwrite.c

lib/pwrite.t: lib/pwrite.o lib/pwrite-t.o libtest.o $(LIBINN)
	$(LINK) lib/pwrite.o lib/pwrite-t.o libtest.o $(LIBINN)

lib/qio.t: lib/qio-t.o libtest.o $(LIBINN)
	$(LINK) lib/qio-t.o libtest.o $(LIBINN)

lib/setenv.o: ../lib/setenv.c
	$(CC) $(CFLAGS) -DTESTING -c -o $@ ../lib/setenv.c

lib/setenv.t: lib/setenv.o lib/setenv-t.o libtest.o $(LIBINN)
	$(LINK) lib/setenv.o lib/setenv-t.o libtest.o $(LIBINN)

lib/snprintf.o: ../lib/snprintf.c
	$(CC) $(CFLAGS) -DTESTING -c -o $@ ../lib/snprintf.c

lib/snprintf.t: lib/snprintf.o lib/snprintf-t.o libtest.o $(LIBINN)
	$(LINK) lib/snprintf.o lib/snprintf-t.o libtest.o $(LIBINN)

lib/strlcat.o: ../lib/strlcat.c
	$(CC) $(CFLAGS) -DTESTING -c -o $@ ../lib/strlcat.c

lib/strlcat.t: lib/strlcat.o lib/strlcat-t.o libtest.o $(LIBINN)
	$(LINK) lib/strlcat.o lib/strlcat-t.o libtest.o $(LIBINN)

lib/strlcpy.o: ../lib/strlcpy.c
	$(CC) $(CFLAGS) -DTESTING -c -o $@ ../lib/strlcpy.c

lib/strlcpy.t: lib/strlcpy.o lib/strlcpy-t.o libtest.o $(LIBINN)
	$(LINK) lib/strlcpy.o lib/strlcpy-t.o libtest.o $(LIBINN)

lib/tst.t: lib/tst-t.o libtest.o $(LIBINN)
	$(LINK) lib/tst-t.o libtest.o $(LIBINN)

lib/uwildmat.t: lib/uwildmat-t.o libtest.o $(LIBINN)
	$(LINK) lib/uwildmat-t.o libtest.o $(LIBINN)

lib/vector.t: lib/vector-t.o libtest.o $(LIBINN)
	$(LINK) lib/vector-t.o libtest.o $(LIBINN)

lib/wire.t: lib/wire-t.o libtest.o $(LIBINN)
	$(LINK) lib/wire-t.o libtest.o $(LIBINN)

lib/xmalloc: lib/xmalloc.o $(LIBINN)
	$(LINK) lib/xmalloc.o $(LIBINN)

lib/xwrite.o: ../lib/xwrite.c
	$(CC) $(CFLAGS) -DTESTING -c -o $@ ../lib/xwrite.c

lib/xwrite.t: lib/xwrite-t.o lib/xwrite.o lib/fakewrite.o libtest.o $(LIBINN)
	$(LINK) lib/xwrite-t.o lib/xwrite.o lib/fakewrite.o libtest.o $(LIBINN)

nnrpd/auth-ext.t: nnrpd/auth-ext-t.o ../nnrpd/auth-ext.o $(LIBINN)
	$(LINK) nnrpd/auth-ext-t.o ../nnrpd/auth-ext.o libtest.o $(LIBINN)

overview/api.t: overview/api-t.o libtest.o $(STORAGEDEPS)
	$(LINK) overview/api-t.o libtest.o $(STORAGELIBS) $(LIBS)

overview/buffindexed-t.o: overview/overview-t.c
	$(CC) $(CFLAGS) -DOVTYPE=buffindexed -c -o $@ overview/overview-t.c

overview/buffindexed.t: overview/buffindexed-t.o libtest.o $(STORAGEDEPS)
	$(LINK) overview/buffindexed-t.o libtest.o $(STORAGELIBS) $(LIBS)

overview/tradindexed-t.o: overview/overview-t.c
	$(CC) $(CFLAGS) -DOVTYPE=tradindexed -c -o $@ overview/overview-t.c

overview/tradindexed.t: overview/tradindexed-t.o libtest.o $(STORAGEDEPS)
	$(LINK) overview/tradindexed-t.o libtest.o $(STORAGELIBS) $(LIBS)

overview/xref.t: overview/xref-t.o libtest.o $(STORAGEDEPS)
	$(LINK) overview/xref-t.o libtest.o $(STORAGELIBS) $(LIBS)

util/innbind.t: util/innbind-t.o libtest.o $(LIBINN)
	$(LINK) util/innbind-t.o libtest.o $(LIBINN) $(LIBS)

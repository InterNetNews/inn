##  $Id$

include ../Makefile.global

top	= ..
CFLAGS	= $(GCFLAGS) -I.

##  The tests that need to be built.  Tests in the form of shell scripts
##  or some other form that doesn't require compiling shouldn't be in this
##  list.  If they need other things compiled, those other things should be
##  added to EXTRA.

TESTS	= lib/buffer.t lib/concat.t lib/confparse.t lib/date.t lib/hash.t \
	lib/hashtab.t lib/hstrerror.t lib/inet_aton.t lib/inet_ntoa.t \
	lib/innconf.t lib/md5.t lib/memcmp.t lib/messages.t lib/mkstemp.t \
	lib/pread.t lib/pwrite.t lib/qio.t lib/snprintf.t lib/strerror.t \
	lib/strlcat.t lib/strlcpy.t lib/uwildmat.t lib/vector.t lib/wire.t \
	lib/xwrite.t overview/tradindexed.t

##  Extra stuff that needs to be built before tests can be run.

EXTRA	= runtests lib/setenv.tr lib/xmalloc

all check test: $(TESTS) $(EXTRA)
	./runtests TESTS

build: $(TESTS) $(EXTRA)

warnings:
	$(MAKE) COPT='$(WARNINGS)' build

clean clobber distclean:
	rm -f *.o *.lo */*.o */*.lo .pure */.pure $(TESTS) $(EXTRA)
	rm -rf .libs */.libs

.c.o: $*.c
	$(CC) $(CFLAGS) -c -o $@ $*.c

LINK		= $(LIBTOOL) $(CC) $(LDFLAGS) -o $@
STORAGEDEPS	= $(LIBSTORAGE) $(LIBHIST) $(LIBINN)
STORAGELIBS	= $(STORAGEDEPS) $(EXTSTORAGELIBS)

runtests: runtests.o
	$(LINK) runtests.o

lib/buffer.t: lib/buffer-t.o libtest.o $(LIBINN)
	$(LINK) lib/buffer-t.o libtest.o $(LIBINN)

lib/concat.t: lib/concat-t.o libtest.o $(LIBINN)
	$(LINK) lib/concat-t.o libtest.o $(LIBINN)

lib/confparse.t: lib/confparse-t.o libtest.o $(LIBINN)
	$(LINK) lib/confparse-t.o libtest.o $(LIBINN)

lib/date.t: lib/date-t.o libtest.o $(LIBINN)
	$(LINK) lib/date-t.o libtest.o $(LIBINN)

lib/hash.t: lib/hash-t.o libtest.o $(LIBINN)
	$(LINK) lib/hash-t.o libtest.o $(LIBINN)

lib/hashtab.t: lib/hashtab-t.o libtest.o $(LIBINN)
	$(LINK) lib/hashtab-t.o libtest.o $(LIBINN)

lib/hstrerror.o: ../lib/hstrerror.o
	$(CC) $(CFLAGS) -DTESTING -c -o $@ ../lib/hstrerror.c

lib/hstrerror.t: lib/hstrerror.o lib/hstrerror-t.o libtest.o
	$(LINK) lib/hstrerror.o lib/hstrerror-t.o libtest.o $(LIBINN)

lib/inet_aton.o: ../lib/inet_aton.c
	$(CC) $(CFLAGS) -DTESTING -c -o $@ ../lib/inet_aton.c

lib/inet_aton.t: lib/inet_aton.o lib/inet_aton-t.o
	$(LINK) lib/inet_aton.o lib/inet_aton-t.o

lib/inet_ntoa.o: ../lib/inet_ntoa.c
	$(CC) $(CFLAGS) -DTESTING -c -o $@ ../lib/inet_ntoa.c

lib/inet_ntoa.t: lib/inet_ntoa.o lib/inet_ntoa-t.o libtest.o
	$(LINK) lib/inet_ntoa.o lib/inet_ntoa-t.o libtest.o $(LIBINN)

lib/innconf.t: lib/innconf-t.o libtest.o $(LIBINN)
	$(LINK) lib/innconf-t.o libtest.o $(LIBINN) $(LIBS)

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

lib/messages.t: lib/messages.o lib/messages-t.o $(LIBINN)
	$(LINK) lib/messages-t.o lib/messages.o $(LIBINN)

lib/mkstemp.o: ../lib/mkstemp.c
	$(CC) $(CFLAGS) -DTESTING -c -o $@ ../lib/mkstemp.c

lib/mkstemp.t: lib/mkstemp.o lib/mkstemp-t.o libtest.o
	$(LINK) lib/mkstemp.o lib/mkstemp-t.o libtest.o $(LIBINN)

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

lib/setenv.tr: lib/setenv.o lib/setenv-t.o libtest.o $(LIBINN)
	$(LINK) lib/setenv.o lib/setenv-t.o libtest.o $(LIBINN)

lib/snprintf.o: ../lib/snprintf.c
	$(CC) $(CFLAGS) -DTESTING -c -o $@ ../lib/snprintf.c

lib/snprintf.t: lib/snprintf.o lib/snprintf-t.o libtest.o
	$(LINK) lib/snprintf.o lib/snprintf-t.o libtest.o $(LIBINN)

lib/strerror.o: ../lib/strerror.c
	$(CC) $(CFLAGS) -DTESTING -c -o $@ ../lib/strerror.c

lib/strerror.t: lib/strerror.o lib/strerror-t.o libtest.o
	$(LINK) lib/strerror.o lib/strerror-t.o libtest.o $(LIBINN)

lib/strlcat.o: ../lib/strlcat.c
	$(CC) $(CFLAGS) -DTESTING -c -o $@ ../lib/strlcat.c

lib/strlcat.t: lib/strlcat.o lib/strlcat-t.o libtest.o
	$(LINK) lib/strlcat.o lib/strlcat-t.o libtest.o $(LIBINN)

lib/strlcpy.o: ../lib/strlcpy.c
	$(CC) $(CFLAGS) -DTESTING -c -o $@ ../lib/strlcpy.c

lib/strlcpy.t: lib/strlcpy.o lib/strlcpy-t.o libtest.o
	$(LINK) lib/strlcpy.o lib/strlcpy-t.o libtest.o $(LIBINN)

lib/uwildmat.t: lib/uwildmat-t.o $(LIBINN)
	$(LINK) lib/uwildmat-t.o $(LIBINN)

lib/vector.t: lib/vector-t.o libtest.o $(LIBINN)
	$(LINK) lib/vector-t.o libtest.o $(LIBINN)

lib/wire.t: lib/wire-t.o libtest.o $(LIBINN)
	$(LINK) lib/wire-t.o libtest.o $(LIBINN)

lib/xmalloc: lib/xmalloc.o $(LIBINN)
	$(LINK) lib/xmalloc.o $(LIBINN)

lib/xwrite.o: ../lib/xwrite.c
	$(CC) $(CFLAGS) -DTESTING -c -o $@ ../lib/xwrite.c

lib/xwrite.t: lib/xwrite-t.o lib/xwrite.o lib/fakewrite.o $(LIBINN)
	$(LINK) lib/xwrite-t.o lib/xwrite.o lib/fakewrite.o $(LIBINN)

overview/tradindexed.t: overview/tradindexed-t.o libtest.o $(STORAGEDEPS)
	$(LINK) overview/tradindexed-t.o libtest.o $(STORAGELIBS) $(LIBS)

##  $Id$

include ../Makefile.global

CFLAGS	= $(GCFLAGS)

##  The tests that need to be built.  Tests in the form of shell scripts
##  or some other form that doesn't require compiling shouldn't be in this
##  list.  If they need other things compiled, those other things should be
##  added to EXTRA.

TESTS	= lib/concat.t lib/date.t lib/error.t lib/inet_aton.t \
	lib/inet_ntoa.t lib/memcmp.t lib/pread.t lib/pwrite.t \
	lib/qio.t lib/strerror.t lib/xwrite.t

##  Extra stuff that needs to be built before tests can be run.

EXTRA	= runtests lib/setenv.tr lib/xmalloc

all check test: $(TESTS) $(EXTRA)
	./runtests TESTS

clean clobber:
	rm -f *.o */*.o $(TESTS) $(EXTRA)

.c.o: $*.c
	$(CC) $(CFLAGS) -c -o $@ $*.c

LINK = $(LIBTOOL) $(CC) $(LDFLAGS) -o $@

runtests: runtests.o $(LIBINN)
	$(LINK) runtests.o $(LIBINN)

lib/concat.t: lib/concat-t.o $(LIBINN)
	$(LINK) lib/concat-t.o $(LIBINN)

lib/date.t: lib/date-t.o $(LIBINN)
	$(LINK) lib/date-t.o $(LIBINN)

lib/error.t: lib/error-t.o $(LIBINN)
	$(LINK) lib/error-t.o $(LIBINN)

lib/inet_aton.o: ../lib/inet_aton.c
	$(CC) $(CFLAGS) -DTESTING -c -o $@ ../lib/inet_aton.c

lib/inet_aton.t: lib/inet_aton.o lib/inet_aton-t.o
	$(LINK) lib/inet_aton.o lib/inet_aton-t.o

lib/inet_ntoa.o: ../lib/inet_ntoa.c
	$(CC) $(CFLAGS) -DTESTING -c -o $@ ../lib/inet_ntoa.c

lib/inet_ntoa.t: lib/inet_ntoa.o lib/inet_ntoa-t.o
	$(LINK) lib/inet_ntoa.o lib/inet_ntoa-t.o

lib/memcmp.o: ../lib/memcmp.c
	$(CC) $(CFLAGS) -DTESTING -c -o $@ ../lib/memcmp.c

lib/memcmp.t: lib/memcmp.o lib/memcmp-t.o
	$(LINK) lib/memcmp.o lib/memcmp-t.o

lib/pread.o: ../lib/pread.c
	$(CC) $(CFLAGS) -DTESTING -c -o $@ ../lib/pread.c

lib/pread.t: lib/pread.o lib/pread-t.o
	$(LINK) lib/pread.o lib/pread-t.o $(LIBINN)

lib/pwrite.o: ../lib/pwrite.c
	$(CC) $(CFLAGS) -DTESTING -c -o $@ ../lib/pwrite.c

lib/pwrite.t: lib/pwrite.o lib/pwrite-t.o
	$(LINK) lib/pwrite.o lib/pwrite-t.o $(LIBINN)

lib/qio.t: lib/qio-t.o $(LIBINN)
	$(LINK) lib/qio-t.o $(LIBINN)

lib/setenv.o: ../lib/setenv.c
	$(CC) $(CFLAGS) -DTESTING -c -o $@ ../lib/setenv.c

lib/setenv.tr: lib/setenv.o lib/setenv-t.o
	$(LINK) lib/setenv.o lib/setenv-t.o $(LIBINN)

lib/strerror.o: ../lib/strerror.c
	$(CC) $(CFLAGS) -DTESTING -c -o $@ ../lib/strerror.c

lib/strerror.t: lib/strerror.o lib/strerror-t.o
	$(LINK) lib/strerror.o lib/strerror-t.o

lib/xmalloc: lib/xmalloc.o $(LIBINN)
	$(LINK) lib/xmalloc.o $(LIBINN)

lib/xwrite.t: lib/xwrite-t.o lib/fakewrite.o $(LIBINN)
	$(LINK) lib/xwrite-t.o lib/fakewrite.o $(LIBINN)

##  $Id$

include ../Makefile.global

CFLAGS	= $(GCFLAGS)

##  The tests that need to be built.  Tests in the form of shell scripts
##  or some other form that doesn't require compiling shouldn't be in this
##  list.  If they need other things compiled, they should be added to
##  EXTRA.

TESTS	= lib/concat.t lib/date.t lib/error.t lib/strerror.t lib/xwrite.t

##  Extra stuff that needs to be built before tests can be run.

EXTRA	= runtests lib/xmalloc

all check test: $(TESTS) $(EXTRA)
	./runtests TESTS

clean clobber:
	rm -f *.o */*.o $(TESTS) $(EXTRA)

.c.o: $*.c
	$(CC) $(CFLAGS) -c -o $@ $*.c

LINK = $(LIBTOOL) $(CC) $(LDFLAGS) -o $@

runtests: runtests.o $(LIBINN)
	$(LINK) runtests.o $(LIBINN)

lib/xmalloc: lib/xmalloc.o $(LIBINN)
	$(LINK) lib/xmalloc.o $(LIBINN)

lib/strerror.o: ../lib/strerror.c
	$(CC) $(CFLAGS) -DTESTING -c -o $@ ../lib/strerror.c

lib/strerror.t: lib/strerror.o lib/strerror-t.o
	$(LINK) lib/strerror.o lib/strerror-t.o

lib/concat.t: lib/concat-t.o $(LIBINN)
	$(LINK) lib/concat-t.o $(LIBINN)

lib/date.t: lib/date-t.o $(LIBINN)
	$(LINK) lib/date-t.o $(LIBINN)

lib/error.t: lib/error-t.o $(LIBINN)
	$(LINK) lib/error-t.o $(LIBINN)

lib/xwrite.t: lib/xwrite-t.o lib/fakewrite.o $(LIBINN)
	$(LINK) lib/xwrite-t.o lib/fakewrite.o $(LIBINN)

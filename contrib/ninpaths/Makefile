include ../../Makefile.global

VERSION=3.1.1
DEFS=-DVERSION=\"$(VERSION)\"

CFLAGS=$(GCFLAGS) $(DEFS)

all: ninpaths sendinpaths

ninpaths: ninpaths.o
	$(CC) $(LDFLAGS) -o $@ ninpaths.o

ninpaths.o: ninpaths.c
	$(CC) $(CFLAGS) -o $@ -c ninpaths.c

sendinpaths: sendinpaths.in
	$(UPFIXSCRIPT) sendinpaths

clean:
	rm -f ninpaths *.o *.out core

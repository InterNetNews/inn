include ../../Makefile.global

VERSION=3.1.1
DEFS=-DVERSION=\"$(VERSION)\"

top=../..
CFLAGS=$(GCFLAGS) $(DEFS)

all: ninpaths sendinpaths

ninpaths: ninpaths.o
	$(CC) $(LDFLAGS) -o $@ ninpaths.o

ninpaths.o: ninpaths.c
	$(CC) $(CFLAGS) -o $@ -c ninpaths.c

sendinpaths: sendinpaths.in
	$(FIXSCRIPT) sendinpaths.in

clean:
	rm -f ninpaths sendinpaths *.o *.out core

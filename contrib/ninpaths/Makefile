include ../../Makefile.global

CFLAGS=$(GCFLAGS)

VERSION=3.1.1
DEFS=-DVERSION=\"$(VERSION)\"

all: ninpaths sendinpaths

ninpaths: ninpaths.o
	$(CC) $(LDFLAGS) -o $@ $<

ninpaths.o: ninpaths.c
	$(CC) $(CFLAGS) -o $@ -c $<

sendinpaths: sendinpaths.in
	$(UPFIXSCRIPT) sendinpaths

clean:
	rm -f ninpaths *.o *.out core

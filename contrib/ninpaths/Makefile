include ../../Makefile.global

CFLAGS=$(GCFLAGS)

VERSION=3.1.1
DEFS=-DVERSION=\"$(VERSION)\"

all: ninpaths sendinpaths

ninpaths: ninpaths.o
	$(CC) $(CFLAGS) -o $@ $<

ninpaths.o: ninpaths.c
	$(CC) $(LDLAGS) -o $@ -c $<

sendinpaths: sendinpaths.in
	$(UPFIXSCRIPT) sendinpaths

clean:
	rm -f ninpaths *.o *.out core

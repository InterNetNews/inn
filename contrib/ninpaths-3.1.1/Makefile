VERSION=3.1.1

CC=gcc
DEFS=-DVERSION=\"$(VERSION)\"
CFLAGS=-pipe -Wall -W -Wstrict-prototypes -pedantic -g -O3 $(DEFS)

all: ninpaths

ninpaths: ninpaths.o
	$(CC) $(CFLAGS) -o $@ $<

ninpaths.o: ninpaths.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -f ninpaths *.o *.out core

FILES = ninpaths.README ninpaths.c sendinpaths Makefile
AFILES = $(FILES) Checksums

Checksums: $(FILES)
	makechecksums $(FILES) > Checksums

shar: $(AFILES)
	shar $(AFILES) | cat Blurb - > ninpaths.shar

tgz: $(AFILES)
	(cd ..; ln -sf ninpaths ninpaths-$(VERSION); \
	 tar czf ninpaths-$(VERSION).tar.gz $(AFILES:%=ninpaths-$(VERSION)/%))

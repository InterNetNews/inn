##  $Revision$

include ../Makefile.global

CFLAGS	= $(GCFLAGS)

SOURCES	=	ckpasswd.c ident.c pwcheck.c radius.c

ALLPASS	=	ckpasswd pwcheck radius
ALLRES	=	ident
ALL	=	$(ALLPASS) $(ALLRES)

all:		$(ALL)

install:	all
	for i in ${ALLPASS}; \
		do $(LIBTOOL) ../installit.sh $(OWNER) -m 0555 \
			-b .OLD $${i} $D${PATHBIN}/auth/passwd/$${i}; \
	done
	for i in ${ALLRES}; \
		do $(LIBTOOL) ../installit.sh $(OWNER) -m 0555 \
			-b .OLD $${i} $D${PATHBIN}/auth/resolv/$${i}; \
	done

clobber clean:
	rm -f *.o $(ALL)
	rm -rf .libs
	rm -f all install profiled

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES) ../lib/*.c ../include/*.h

pwcheck:	$(P) pwcheck.o
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ pwcheck.o $(LIBS)

radius:	$(P) radius.o
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ radius.o $(LIBS) $(LIBNEWS)

ckpasswd:	$(P) ckpasswd.o
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ ckpasswd.o $(LIBS)

ident:	$(P) ident.o
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ ident.o $(LIBS)


$(LIBNEWS):
	(cd ../lib ; $(MAKE) )

##  Dependencies.  Default list, below, is probably good enough.
depend:		Makefile $(SOURCES)
	makedepend $(DEFS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.

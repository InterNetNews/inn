##  $Revision$
include ../Makefile.global

CFLAGS = $(GCFLAGS) $(PERLINC)

NNRPD	= ${PATHBIN}/nnrpd
ACTIVED	= ${PATHBIN}/actived

SOURCES	= \
	article.c group.c commands.c misc.c newnews.c nnrpd.c \
	perl.c perm.c post.c loadave.c track.c udp.c

OBJECTS	= \
	article.o group.o commands.o misc.o newnews.o \
	perl.o perm.o post.o loadave.o track.o udp.o

ALL	= nnrpd actived

all:		$(ALL)

install:	$D$(NNRPD) $D$(ACTIVED)

##  Low-level install actions.
$D$(NNRPD):	nnrpd
	$(LIBTOOL) ../installit.sh $(OWNER) -m 0555 -b .OLD $? $@

##  Low-level install actions.
$D$(ACTIVED):	actived
	$(LIBTOOL) ../installit.sh $(OWNER) -m 0555 -b .OLD $? $@

clobber clean:
	rm -f *.o $(ALL)
	rm -rf .libs
	rm -f nnrpd nnrpdp actived profiled
	rm -f all install 

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES) ../lib/*.c nnrpd.h ../include/*.h

nnrpd:		$(P) nnrpd.o $(OBJECTS) $(LIBNEWS)
	@rm -f $@
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ $@.o $(OBJECTS) $(LIBNEWS) $(PERLLIB) $(LIBS) 

actived:	$(P) actived.o activedstats.o $(OBJECTS) $(LIBNEWS)
	@rm -f $@
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ $@.o activedstats.o $(OBJECTS) $(LIBNEWS) $(PERLLIB) $(LIBS) 

../include/dbz.h:
	(cd ../lib ; $(MAKE) ../include/dbz.h)
$(LIBNEWS):
	(cd ../lib ; $(MAKE) )

##  Profiling.  The rules are a bit brute-force, but good enough.
profiled:	nnrpdp
	date >$@

nnrpdp:		$(SOURCES)
	rm -f $(OBJECTS)
	$(MAKE) nnrpd CFLAGS="$(CFLAGS) $(PROF)" LIBNEWS="../libinn_p.a ../storage/libstorage.a ../libinn_p.a ../storage/libstorage.a"
	mv nnrpd nnrpdp
	rm -f $(OBJECTS)

ccenter:	$(SOURCES)
	#load $(CFLAGS) $(SOURCES) $(LIBNEWS)

##  Dependencies.  Default list, below, is probably good enough.
depend:		Makefile $(SOURCES)
	makedepend $(DEFS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
$(OBJECTS):	nnrpd.h protocol.h \
		../include/clibrary.h ../include/configdata.h \
		../include/libinn.h \
		../include/macros.h ../include/nntp.h \
		../include/paths.h ../include/qio.h
group.o:	../include/mydir.h
misc.o:		../include/dbz.h

##  $Revision$
include ../Makefile.global

CFLAGS = $(GCFLAGS)

NNRPD	= ${PATHBIN}/nnrpd

SOURCES	= \
	article.c group.c commands.c misc.c newnews.c nnrpd.c \
	perl.c perm.c post.c loadave.c track.c python.c

OBJECTS	= \
	article.o group.o commands.o misc.o newnews.o \
	perl.o perm.o post.o loadave.o track.o python.o

ALL	= nnrpd

all:		$(ALL)

perl.o: perl.c
	$(CC) $(CFLAGS) $(PERLINC) -c perl.c

python.o: python.c
	$(CC) $(CFLAGS) $(PYTHONINC) -c python.c -o $@

install:	$D$(NNRPD)

##  Low-level install actions.
$D$(NNRPD):	nnrpd
	$(LI_XPUB) $? $@

clobber clean:
	rm -f *.o $(ALL)
	rm -rf .libs
	rm -f nnrpd nnrpdp profiled
	rm -f all install 

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES) ../lib/*.c nnrpd.h ../include/*.h

BIGLIST = $(OBJECTS) $(LIBSTORAGE) $(LIBINN) $(EXTSTORAGELIBS) $(PERLLIB) $(PYTHONLIB) $(LIBS)

nnrpd:		$(P) nnrpd.o $(OBJECTS) $(LIBSTORAGE) $(LIBINN)
	@rm -f $@
#	$(LIBTOOL) ./quantify $(CC) $(LDFLAGS) -o $@ $@.o $(BIGLIST)
#	$(LIBTOOL) purify $(CC) $(LDFLAGS) -o $@ $@.o $(BIGLIST)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ $@.o $(BIGLIST)

$(LIBINN):
	(cd ../lib ; $(MAKE) )

##  Profiling.  The rules are a bit brute-force, but good enough.
profiled:	nnrpdp
	date >$@

nnrpdp:		$(SOURCES)
	rm -f $(OBJECTS)
	$(MAKE) nnrpd CFLAGS="$(CFLAGS) $(PROF)" LIBINN="../libinn_p.a"
	mv nnrpd nnrpdp
	rm -f $(OBJECTS)

ccenter:	$(SOURCES)
	#load $(CFLAGS) $(SOURCES) $(LIBINN)

##  Dependencies.  Default list, below, is probably good enough.
depend:		Makefile $(SOURCES)
	makedepend $(DEFS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
$(OBJECTS):	nnrpd.h \
		../include/clibrary.h ../include/configdata.h \
		../include/libinn.h \
		../include/macros.h ../include/nntp.h \
		../include/paths.h ../include/qio.h
misc.o:		../include/dbz.h

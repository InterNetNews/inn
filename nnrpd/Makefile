##  $Revision$
include ../Makefile.global

CFLAGS += $(PERLINC)

NNRPD	= ${PATHBIN}/nnrpd

SOURCES	= \
	article.c group.c commands.c misc.c newnews.c nnrpd.c \
	perl.c post.c loadave.c track.c

OBJECTS	= \
	article.o group.o commands.o misc.o newnews.o nnrpd.o \
	perl.o post.o loadave.o track.o

ALL	= nnrpd

all:		$(ALL)

install:	$D$(NNRPD)

##  Low-level install actions.
$D$(NNRPD):	nnrpd
	$(SHELL) ../installit.sh $(OWNER) -m 0555 -b .OLD $? $@

clobber clean:
	rm -f *.o $(ALL)
	rm -f nnrpd nnrpdp profiled
	rm -f all install 

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES) ../lib/*.c nnrpd.h ../include/*.h

nnrpd:		$(P) $(OBJECTS) $(LIBNEWS)
	@rm -f $@
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(LIBNEWS) $(LIBS) $(PERLLIB)

../include/dbz.h:
	(cd ../lib ; $(MAKE) ../include/dbz.h)
$(LIBNEWS):
	(cd ../lib ; $(MAKE) install )

##  Profiling.  The rules are a bit brute-force, but good enough.
profiled:	nnrpdp
	date >$@

nnrpdp:		$(SOURCES)
	rm -f $(OBJECTS)
	$(MAKE) nnrpd CFLAGS="$(CFLAGS) $(PROF)" LIBNEWS=../libinn_p.a
	mv nnrpd nnrpdp
	rm -f $(OBJECTS)

ccenter:	$(SOURCES)
	#load $(CFLAGS) $(SOURCES) $(LIBNEWS)

##  Dependencies.  Default list, below, is probably good enough.
depend:		Makefile $(SOURCES)
	makedepend $(DEFS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
$(OBJECTS):	nnrpd.h \
		../include/clibrary.h ../include/configdata.h \
		../include/libinn.h ../include/logging.h \
		../include/macros.h ../include/nntp.h \
		../include/paths.h ../include/qio.h
group.o:	../include/mydir.h
misc.o:		../include/dbz.h

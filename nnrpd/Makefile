##  $Revision$
include ../Makefile.global

##  =()<P	= @<P>@>()=
P	= 

##  =()<PERLLIB = @<PERL_LIB>@>()=
PERLLIB = 
##  =()<PERLINC = @<PERL_INC>@>()=
PERLINC = 
##  =()<LINTFLAGS	= @<LINTFLAGS>@>()=
LINTFLAGS	= -b -h -z  -I../include $(DEFS)
##  =()<LINTFILTER	= @<LINTFILTER>@>()=
LINTFILTER	= | sed -n -f ../sedf.sun
##  =()<CTAGS		= @<CTAGS>@>()=
CTAGS		= /usr/bin/ctags -t -w
##  =()<PROF	= @<PROF>@>()=
PROF	= -pg

CFLAGS += $(PERLINC)

NNRPD	= ${NEWSBIN}/nnrpd

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
	rm -f all install lint

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES) ../lib/*.c nnrpd.h ../include/*.h

nnrpd:		$(P) $(OBJECTS) $(LIBNEWS)
	@rm -f $@
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(LIBNEWS) $(LIBS) $(PERLLIB)

lint:		$(ALL)
	@rm -f lint
	-lint $(LINTFLAGS) $(SOURCES) $(LINTLIB) $(LINTFILTER) >lint

../include/dbz.h:
	(cd ../lib ; $(MAKE) ../include/dbz.h)
$(LIBNEWS) $(LINTLIB):
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

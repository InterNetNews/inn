##  $Revision$
include ../Makefile.global

##  =()<P	= @<P>@>()=
P	= 

RNEWSPROGS	= ${NEWSBIN}/rnews.libexec

##  =()<LINTFLAGS	= @<LINTFLAGS>@>()=
LINTFLAGS	= -b -h -z  -I../include $(DEFS)
##  =()<LINTFILTER	= @<LINTFILTER>@>()=
LINTFILTER	= | sed -n -f ../sedf.sun
##  =()<CTAGS		= @<CTAGS>@>()=
CTAGS		= /usr/bin/ctags -t -w

##  =()<NNTPLIB	= @<NNTPLIB>@>()=
NNTPLIB	= 

SOURCES	= ctlinnd.c inews.c rnews.c sm.c

ALL	= inews rnews ctlinnd getlist innconfval decode encode sm

all:		$(ALL)
	date >$@

install:	$(NEWSBIN)/inews $(NEWSBIN)/rnews \
		$(NEWSBIN)/ctlinnd $(NEWSBIN)/getlist $(NEWSBIN)/innconfval \
		$(RNEWSPROGS)/c7unbatch \
		$(RNEWSPROGS)/decode $(RNEWSPROGS)/encode \
		$(NEWSBIN)/sm

clobber clean:
	rm -f *.o $(ALL)
	rm -f all install lint profiled

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES) ../lib/*.c ../include/*.h

inews:		$(P) inews.o $(LIBNEWS)
	$(CC) $(LDFLAGS) -o $@ inews.o $(NNTPLIB) $(LIBNEWS) $(LIBS)
rnews:		$(P) rnews.o $(LIBNEWS)
	$(CC) $(LDFLAGS) -o $@ rnews.o $(NNTPLIB) $(LIBNEWS) $(LIBNEWS) $(LIBS)
ctlinnd:	$(P) ctlinnd.o $(LIBNEWS)
	$(CC) $(LDFLAGS) -o $@ ctlinnd.o $(LIBNEWS) $(LIBS)
sm:		$(P) sm.o $(LIBNEWS)
	$(CC) $(LDFLAGS) -o $@ sm.o $(LIBNEWS) $(LIBNEWS) $(LIBS)
getlist:	$(P) getlist.o $(LIBNEWS)
	$(CC) $(LDFLAGS) -o $@ getlist.o $(LIBNEWS) $(LIBS)
innconfval:	$(P) innconfval.o $(LIBNEWS)
	$(CC) $(LDFLAGS) -o $@ innconfval.o $(LIBNEWS) $(LIBS)
decode:		$(P) decode.o $(LIBNEWS)
	$(CC) $(LDFLAGS) -o $@ decode.o $(LIBNEWS) $(LIBS)
encode:		$(P) encode.o $(LIBNEWS)
	$(CC) $(LDFLAGS) -o $@ encode.o $(LIBNEWS) $(LIBS)

##  Not normally built.
feedone:		feedone.o $(LIBNEWS)
	$(CC) $(LDFLAGS) -o $@ feedone.o $(LIBNEWS) $(LIBS)
sys2nf:		sys2nf.o $(LIBNEWS)
	$(CC) $(LDFLAGS) -o $@ sys2nf.o $(LIBNEWS) $(LIBS)


lint:		$(ALL)
	@rm -f lint
	-lint $(LINTFLAGS) inews.c      $(LINTLIB) $(LINTFILTER)  >lint
	-lint $(LINTFLAGS) rnews.c      $(LINTLIB) $(LINTFILTER) >>lint
	-lint $(LINTFLAGS) ctlinnd.c    $(LINTLIB) $(LINTFILTER) >>lint
	-lint $(LINTFLAGS) sm.c		$(LINTLIB) $(LINTFILTER) >>lint
	-lint $(LINTFLAGS) innconfval.c $(LINTLIB) $(LINTFILTER) >>lint
	-lint $(LINTFLAGS) getlist.c    $(LINTLIB) $(LINTFILTER) >>lint
	-lint $(LINTFLAGS) decode.c     $(LINTLIB) $(LINTFILTER) >>lint
	-lint $(LINTFLAGS) encode.c     $(LINTLIB) $(LINTFILTER) >>lint

$(LIBNEWS) $(LINTLIB):
	(cd ../lib ; $(MAKE) install)

profiled:
	cp /dev/null profiled

##
.SUFFIXES: .c .src
.c.src:
	#load $(CFLAGS) $<

##  Low-level install actions.
$(NEWSBIN)/inews:		inews
	$(SHELL) ../installit.sh $(OWNER) -m 02555 -b .OLD $? $@
$(NEWSBIN)/rnews:		rnews
	$(SHELL) ../installit.sh $(ROWNER) -m 04555 -b .OLD $? $@
$(NEWSBIN)/ctlinnd:	ctlinnd
	$(SHELL) ../installit.sh $(OWNER) -m  0550 -b .OLD $? $@
$(NEWSBIN)/sm:	sm
	$(SHELL) ../installit.sh $(OWNER) -m  0550 -b .OLD $? $@
$(NEWSBIN)/getlist:	getlist
	$(SHELL) ../installit.sh $(OWNER) -m  0555 -b .OLD $? $@
$(NEWSBIN)/innconfval:	innconfval
	$(SHELL) ../installit.sh $(OWNER) -m  0555 -b .OLD $? $@
$(RNEWSPROGS)/c7unbatch:	c7unbatch.sh
	$(SHELL) ../installit.sh $(OWNER) -m  0555 -b .OLD $? $@
$(RNEWSPROGS)/decode:		decode
	$(SHELL) ../installit.sh $(OWNER) -m  0555 -b .OLD $? $@
$(RNEWSPROGS)/encode:		encode
	$(SHELL) ../installit.sh $(OWNER) -m  0555 -b .OLD $? $@

##  Dependencies.  Default list, below, is probably good enough.
depend:		Makefile $(SOURCES)
	makedepend $(DEFS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
ctlinnd.o:	../include/clibrary.h
ctlinnd.o:	../include/configdata.h
ctlinnd.o:	../include/inndcomm.h
ctlinnd.o:	../include/libinn.h
ctlinnd.o:	../include/macros.h
ctlinnd.o:	../include/paths.h
sm.o:		../include/configdata.h
sm.o:		../include/clibrary.h
sm.o:		../include/libinn.h
sm.o:		../include/macros.h
decode.o:	../include/clibrary.h
decode.o:	../include/configdata.h
encode.o:	../include/clibrary.h
encode.o:	../include/configdata.h
getlist:	../include/clibrary.h
getlist:	../include/configdata.h
getlist:	../include/libinn.h
getlist:	../include/macros.h
getlist:	../include/paths.h
getlist:	../include/qio.h
inews.o:	../include/clibrary.h
inews.o:	../include/configdata.h
inews.o:	../include/libinn.h
inews.o:	../include/macros.h
inews.o:	../include/nntp.h
inews.o:	../include/paths.h
innconfval.o:	../include/clibrary.h
innconfval.o:	../include/configdata.h
innconfval.o:	../include/libinn.h
innconfval.o:	../include/macros.h
rnews.o:	../include/clibrary.h
rnews.o:	../include/configdata.h
rnews.o:	../include/libinn.h
rnews.o:	../include/logging.h
rnews.o:	../include/macros.h
rnews.o:	../include/mydir.h
rnews.o:	../include/nntp.h
rnews.o:	../include/paths.h

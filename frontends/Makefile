##  $Id$

include ../Makefile.global

CFLAGS	= $(GCFLAGS)

SOURCES	= ctlinnd.c inews.c rnews.c sm.c

ALL	= inews rnews ctlinnd getlist innconfval decode encode sm

all:		$(ALL)
	date >$@

PATHRNP       = $(PATHBIN)/rnews.libexec

INSTALLED     = $(D)$(PATHBIN)/cnfsheadconf	\
		$(D)$(PATHBIN)/cnfsstat		\
		$(D)$(PATHBIN)/ctlinnd		\
		$(D)$(PATHBIN)/getlist		\
		$(D)$(PATHBIN)/inews		\
		$(D)$(PATHBIN)/innconfval	\
		$(D)$(PATHBIN)/mailpost		\
		$(D)$(PATHBIN)/pullnews		\
		$(D)$(PATHBIN)/rnews		\
		$(D)$(PATHBIN)/scanspool	\
		$(D)$(PATHBIN)/signcontrol	\
		$(D)$(PATHBIN)/sm		\
		$(D)$(PATHRNP)/c7unbatch	\
		$(D)$(PATHRNP)/decode		\
		$(D)$(PATHRNP)/encode		\
		$(D)$(PATHRNP)/gunbatch

clobber clean:
	rm -f *.o $(ALL)
	rm -rf .libs
	rm -f all install profiled

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES) ../lib/*.c ../include/*.h

inews:		$(P) inews.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ inews.o $(NNTPLIB) $(LIBNEWS) $(LIBS)
rnews:		$(P) rnews.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ rnews.o $(NNTPLIB) $(LIBNEWS) $(LIBNEWS) $(LIBS)
ctlinnd:	$(P) ctlinnd.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ ctlinnd.o $(LIBNEWS) $(LIBS)
sm:		$(P) sm.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ sm.o $(LIBNEWS) $(LIBNEWS) $(LIBS)
getlist:	$(P) getlist.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ getlist.o $(LIBNEWS) $(LIBS)
innconfval:	$(P) innconfval.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ innconfval.o $(LIBNEWS) $(LIBS)
decode:		$(P) decode.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ decode.o $(LIBNEWS) $(LIBS)
encode:		$(P) encode.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ encode.o $(LIBNEWS) $(LIBS)

##  Not normally built.
feedone:		feedone.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ feedone.o $(LIBNEWS) $(LIBS)
sys2nf:		sys2nf.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ sys2nf.o $(LIBNEWS) $(LIBS)


$(LIBNEWS):
	(cd ../lib ; $(MAKE) )

profiled:
	cp /dev/null profiled

##
.SUFFIXES: .c .src
.c.src:
	#load $(CFLAGS) $<


##  Installation rules.

install: $(INSTALLED)

LT_INEW = $(LIBTOOL) ../installit.sh $(OWNER) -m $(INEWSMODE) -b .OLD
LT_RNEW = $(LIBTOOL) ../installit.sh $(ROWNER) -m $(RNEWSMODE) -b .OLD
LT_XPRI = $(LIBTOOL) ../installit.sh $(OWNER) -m 0550 -b .OLD
LT_XPUB = $(LIBTOOL) ../installit.sh $(OWNER) -m 0555 -b .OLD
CP_XPRI = ../installit.sh $(OWNER) -m 0550 -b .OLD
CP_XPUB = ../installit.sh $(OWNER) -m 0555 -b .OLD

$(D)$(PATHBIN)/cnfsheadconf:        cnfsheadconf        ; $(CP_XPRI) $? $@
$(D)$(PATHBIN)/cnfsstat:            cnfsstat            ; $(CP_XPRI) $? $@
$(D)$(PATHBIN)/ctlinnd:             ctlinnd             ; $(LT_XPRI) $? $@
$(D)$(PATHBIN)/getlist:             getlist             ; $(LT_XPUB) $? $@
$(D)$(PATHBIN)/inews:               inews               ; $(LT_INEW) $? $@
$(D)$(PATHBIN)/innconfval:          innconfval          ; $(LT_XPUB) $? $@
$(D)$(PATHBIN)/mailpost:            mailpost            ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/pullnews:            pullnews            ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/rnews:               rnews               ; $(LT_RNEW) $? $@
$(D)$(PATHBIN)/scanspool:           scanspool           ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/signcontrol:         signcontrol         ; $(CP_XPUB) $? $@
$(D)$(PATHBIN)/sm:                  sm                  ; $(LT_XPUB) $? $@
$(D)$(PATHRNP)/c7unbatch:           c7unbatch           ; $(CP_XPUB) $? $@
$(D)$(PATHRNP)/decode:              decode              ; $(LT_XPUB) $? $@
$(D)$(PATHRNP)/encode:              encode              ; $(LT_XPUB) $? $@
$(D)$(PATHRNP)/gunbatch:            gunbatch            ; $(CP_XPUB) $? $@


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
rnews.o:	../include/macros.h
rnews.o:	../include/nntp.h
rnews.o:	../include/paths.h

##  $Id$

include ../Makefile.global

CFLAGS	= $(GCFLAGS)

SOURCES	=	convdate.c expire.c expireover.c fastrm.c grephistory.c \
		makehistory.c newsrequeue.c prunehistory.c makedbz.c

INSTALLED     = $(D)$(PATHBIN)/convdate		\
		$(D)$(PATHBIN)/expire		\
		$(D)$(PATHBIN)/expireover	\
		$(D)$(PATHBIN)/expirerm		\
		$(D)$(PATHBIN)/fastrm		\
		$(D)$(PATHBIN)/grephistory	\
		$(D)$(PATHBIN)/makedbz		\
		$(D)$(PATHBIN)/makehistory	\
		$(D)$(PATHBIN)/newsrequeue	\
		$(D)$(PATHBIN)/prunehistory

ALL	=	convdate expire expireover fastrm grephistory \
		makehistory newsrequeue prunehistory makedbz

all:		$(ALL)

clobber clean:
	rm -f *.o $(ALL)
	rm -rf .libs
	rm -f expirep profiled
	rm -f all install 

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES) ../lib/*.c ../include/*.h

convdate:	$(P) convdate.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ convdate.o $(LIBNEWS) $(LIBS)

expire:		$(P) expire.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ expire.o $(LIBNEWS) $(LIBS)

expireover:	$(P) expireover.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ expireover.o $(LIBNEWS) $(LIBS)

fastrm:	$(P) fastrm.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ fastrm.o $(LIBNEWS) $(LIBNEWS) $(LIBS)

grephistory:	$(P) grephistory.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ grephistory.o $(LIBNEWS) $(LIBNEWS) $(LIBS)

makehistory:	$(P) makehistory.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ makehistory.o $(LIBNEWS) $(LIBS)

newsrequeue:	$(P) newsrequeue.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ newsrequeue.o $(LIBNEWS) $(LIBS)

prunehistory:	$(P) prunehistory.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ prunehistory.o $(LIBNEWS) $(LIBS)

makedbz:	$(P) makedbz.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ makedbz.o $(LIBNEWS) $(LIBS)

$(LIBNEWS):
	(cd ../lib ; $(MAKE) )

##  Profiling.  The rules are a bit brute-force, but good enough.
profiled:	expirep 
	date >$@

NEWFLAGS	= CFLAGS="$(CFLAGS) $(PROF)" LDFLAGS="$(LDFLAGS) $(PROF)"

expirep:	expire.c
	rm -f expire.o
	$(MAKE) expire $(NEWFLAGS) LIBNEWS=../libinn_p.a
	mv expire expirep
	rm -f expire.o

##
.SUFFIXES: .c .src
.c.src:
	#load $(CFLAGS) $<


##  Installation rules.

install: $(INSTALLED)

LT_XPRI = $(LIBTOOL) ../installit.sh $(OWNER) -m 0550 -b .OLD
LT_XPUB = $(LIBTOOL) ../installit.sh $(OWNER) -m 0555 -b .OLD
CP_XPRI = ../installit.sh $(OWNER) -m 0550 -b .OLD
CP_XPUB = ../installit.sh $(OWNER) -m 0555 -b .OLD

$(D)$(PATHBIN)/convdate:            convdate            ; $(LT_XPUB) $? $@
$(D)$(PATHBIN)/expire:              expire              ; $(LT_XPRI) $? $@
$(D)$(PATHBIN)/expireover:          expireover          ; $(LT_XPRI) $? $@
$(D)$(PATHBIN)/expirerm:            expirerm            ; $(CP_XPRI) $? $@
$(D)$(PATHBIN)/fastrm:              fastrm              ; $(LT_XPUB) $? $@
$(D)$(PATHBIN)/grephistory:         grephistory         ; $(LT_XPUB) $? $@
$(D)$(PATHBIN)/makedbz:             makedbz             ; $(LT_XPRI) $? $@
$(D)$(PATHBIN)/makehistory:         makehistory         ; $(LT_XPRI) $? $@
$(D)$(PATHBIN)/newsrequeue:         newsrequeue         ; $(LT_XPRI) $? $@
$(D)$(PATHBIN)/prunehistory:        prunehistory        ; $(LT_XPRI) $? $@


##  Dependencies.  Default list, below, is probably good enough.

depend:		Makefile $(SOURCES)
	makedepend $(DEFS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
convdate.o:	../include/clibrary.h
convdate.o:	../include/configdata.h
convdate.o:	../include/libinn.h
convdate.o:	../include/macros.h
expire.o:	../include/clibrary.h
expire.o:	../include/configdata.h
expire.o:	../include/dbz.h
expire.o:	../include/inndcomm.h
expire.o:	../include/libinn.h
expire.o:	../include/macros.h
expire.o:	../include/paths.h
expire.o:	../include/qio.h
expireover.o:	../include/clibrary.h
expireover.o:	../include/configdata.h
expireover.o:	../include/libinn.h
expireover.o:	../include/macros.h
expireover.o:	../include/paths.h
expireover.o:	../include/qio.h
fastrm.o:	../include/clibrary.h
fastrm.o:	../include/configdata.h
fastrm.o:	../include/libinn.h
fastrm.o:	../include/macros.h
grephistory.o:	../include/clibrary.h
grephistory.o:	../include/configdata.h
grephistory.o:	../include/dbz.h
grephistory.o:	../include/libinn.h
grephistory.o:	../include/macros.h
grephistory.o:	../include/paths.h
makehistory.o:	../include/clibrary.h
makehistory.o:	../include/configdata.h
makehistory.o:	../include/dbz.h
makehistory.o:	../include/libinn.h
makehistory.o:	../include/macros.h
makehistory.o:	../include/paths.h
makehistory.o:	../include/qio.h
newsrequeue.o:	../include/clibrary.h
newsrequeue.o:	../include/configdata.h
newsrequeue.o:	../include/dbz.h
newsrequeue.o:	../include/libinn.h
newsrequeue.o:	../include/macros.h
newsrequeue.o:	../include/paths.h
newsrequeue.o:	../include/qio.h
prunehistory.o:	../include/clibrary.h
prunehistory.o:	../include/configdata.h
prunehistory.o:	../include/dbz.h
prunehistory.o:	../include/libinn.h
prunehistory.o:	../include/macros.h
prunehistory.o:	../include/paths.h
prunehistory.o:	../include/paths.h
makedbz.o:	../include/clibrary.h
makedbz.o:	../include/configdata.h
makedbz.o:	../include/dbz.h
makedbz.o:	../include/libinn.h
makedbz.o:	../include/macros.h
makedbz.o:	../include/paths.h
makedbz.o:	../include/qio.h

##  $Id$

include ../Makefile.global

CFLAGS        = $(GCFLAGS)

ALL           = convdate expire expireover expirerm fastrm grephistory \
		makedbz makehistory newsrequeue prunehistory

SOURCES       = convdate.c expire.c expireover.c fastrm.c grephistory.c \
		makedbz.c makehistory.c newsrequeue.c prunehistory.c

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

all: $(ALL)

clean:
	rm -f *.o $(ALL)
	rm -rf .libs

clobber: clean
	rm -f tags

tags ctags: $(SOURCES)
	$(CTAGS) $(SOURCES)


##  Compilation rules.

LINK            = $(LIBTOOL) $(CC) $(LDFLAGS) -o $@
LIB_S_I		= $(LIBSTORAGE) $(LIBINN)
ES		= $(EXTSTORAGELIBS)
FIX             = $(FIXSCRIPT)

convdate:	convdate.o     $(LIBINN)  ; $(LINK) convdate.o     $(LIBINN) $(LIBS)
expire:		expire.o       $(LIB_S_I) ; $(LINK) expire.o       $(LIB_S_I) $(ES) $(LIBS)
expireover:	expireover.o   $(LIB_S_I) ; $(LINK) expireover.o   $(LIB_S_I) $(ES) $(LIBS)
fastrm:		fastrm.o       $(LIB_S_I) ; $(LINK) fastrm.o       $(LIB_S_I) $(ES) $(LIBS)
grephistory:	grephistory.o  $(LIB_S_I) ; $(LINK) grephistory.o  $(LIB_S_I) $(ES) $(LIBS)
makedbz:	makedbz.o      $(LIBINN)  ; $(LINK) makedbz.o      $(LIBINN) $(LIBS)
makehistory:	makehistory.o  $(LIB_S_I) ; $(LINK) makehistory.o  $(LIB_S_I) $(ES) $(LIBS)
newsrequeue:	newsrequeue.o  $(LIBINN)  ; $(LINK) newsrequeue.o  $(LIBINN) $(LIBS)
prunehistory:	prunehistory.o $(LIBINN)  ; $(LINK) prunehistory.o $(LIBINN) $(LIBS)

expirerm:	expirerm.in        $(FIX) ; $(FIX) expirerm.in

$(LIBINN):
	(cd ../lib ; $(MAKE))
$(LIBSTORAGE):
	(cd ../storage ; $(MAKE))


##  Profiling.  The rules are a bit brute-force, but good enough.
profiled:	expirep 
	date >$@

NEWFLAGS	= CFLAGS="$(CFLAGS) $(PROF)" LDFLAGS="$(LDFLAGS) $(PROF)"

expirep:	expire.c
	rm -f expire.o
	$(MAKE) expire $(NEWFLAGS) LIBINN=../libinn_p.a
	mv expire expirep
	rm -f expire.o


##  Installation rules.  Installation commands set in Makefile.global.

install: $(INSTALLED)

$(D)$(PATHBIN)/convdate:            convdate            ; $(LI_XPUB) $? $@
$(D)$(PATHBIN)/expire:              expire              ; $(LI_XPRI) $? $@
$(D)$(PATHBIN)/expireover:          expireover          ; $(LI_XPRI) $? $@
$(D)$(PATHBIN)/expirerm:            expirerm            ; $(CP_XPRI) $? $@
$(D)$(PATHBIN)/fastrm:              fastrm              ; $(LI_XPUB) $? $@
$(D)$(PATHBIN)/grephistory:         grephistory         ; $(LI_XPUB) $? $@
$(D)$(PATHBIN)/makedbz:             makedbz             ; $(LI_XPRI) $? $@
$(D)$(PATHBIN)/makehistory:         makehistory         ; $(LI_XPRI) $? $@
$(D)$(PATHBIN)/newsrequeue:         newsrequeue         ; $(LI_XPRI) $? $@
$(D)$(PATHBIN)/prunehistory:        prunehistory        ; $(LI_XPRI) $? $@


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

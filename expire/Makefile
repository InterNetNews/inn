##  $Id$

include ../Makefile.global

top           = ..
CFLAGS        = $(GCFLAGS)

ALL           = convdate expire expireover expirerm fastrm grephistory \
		makedbz makehistory prunehistory

SOURCES       = convdate.c expire.c expireover.c fastrm.c grephistory.c \
		makedbz.c makehistory.c prunehistory.c

INSTALLED     = $(D)$(PATHBIN)/convdate		\
		$(D)$(PATHBIN)/expire		\
		$(D)$(PATHBIN)/expireover	\
		$(D)$(PATHBIN)/expirerm		\
		$(D)$(PATHBIN)/fastrm		\
		$(D)$(PATHBIN)/grephistory	\
		$(D)$(PATHBIN)/makedbz		\
		$(D)$(PATHBIN)/makehistory	\
		$(D)$(PATHBIN)/prunehistory

all: $(ALL)

warnings:
	$(MAKE) COPT='$(WARNINGS)' all

clean:
	rm -f *.o $(ALL)
	rm -f profiled expirep
	rm -rf .libs

clobber distclean: clean
	rm -f tags

tags ctags: $(SOURCES)
	$(CTAGS) $(SOURCES)


##  Compilation rules.

BOTH		= $(LIBHIST) $(LIBSTORAGE) $(LIBINN)

LINK            = $(LIBLD) $(LDFLAGS) -o $@
INNLIBS		= $(LIBINN) $(LIBS)
STORELIBS	= $(BOTH) $(EXTSTORAGELIBS) $(LIBS)

FIX             = $(FIXSCRIPT)

convdate:	convdate.o     $(LIBINN) ; $(LINK) convdate.o     $(INNLIBS)
expire:		expire.o       $(BOTH)   ; $(LINK) expire.o       $(STORELIBS)
expireover:	expireover.o   $(BOTH)   ; $(LINK) expireover.o   $(STORELIBS)
fastrm:		fastrm.o       $(BOTH)   ; $(LINK) fastrm.o       $(STORELIBS)
grephistory:	grephistory.o  $(BOTH)   ; $(LINK) grephistory.o  $(STORELIBS)
makedbz:	makedbz.o      $(LIBINN) ; $(LINK) makedbz.o      $(INNLIBS)
makehistory:	makehistory.o  $(BOTH)   ; $(LINK) makehistory.o  $(STORELIBS)
prunehistory:	prunehistory.o $(BOTH)   ; $(LINK) prunehistory.o $(STORELIBS)

expirerm:	expirerm.in    $(FIX)    ; $(FIX) expirerm.in

$(LIBINN):	; (cd ../lib ; $(MAKE))
$(LIBSTORAGE):	; (cd ../storage ; $(MAKE))
$(LIBHIST):	; (cd ../history ; $(MAKE))


##  Profiling.  These rules have not been checked for a while and may need
##  some work.

profiled:	expirep 
	date >$@

expirep:	expire.c
	rm -f expire.o
	$(MAKEPROFILING) expire
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
$(D)$(PATHBIN)/prunehistory:        prunehistory        ; $(LI_XPRI) $? $@


##  Dependencies.  Default list, below, is probably good enough.

depend:	Makefile $(SOURCES)
	$(MAKEDEPEND) '$(CFLAGS)' $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
convdate.o: convdate.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/messages.h ../include/libinn.h
expire.o: expire.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inndcomm.h ../include/libinn.h \
 ../include/macros.h ../include/paths.h ../include/storage.h \
 ../include/inn/history.h
expireover.o: expireover.c ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h \
 ../include/inn/messages.h ../include/inn/qio.h ../include/libinn.h \
 ../include/macros.h ../include/ov.h ../include/storage.h \
 ../include/inn/history.h ../include/paths.h
fastrm.o: fastrm.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/messages.h ../include/inn/qio.h \
 ../include/libinn.h ../include/macros.h ../include/storage.h
grephistory.o: grephistory.c ../include/clibrary.h ../include/config.h \
 ../include/inn/defines.h ../include/libinn.h ../include/macros.h \
 ../include/paths.h ../include/storage.h ../include/inn/history.h
makedbz.o: makedbz.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/dbz.h ../include/libinn.h \
 ../include/inn/qio.h ../include/macros.h ../include/paths.h \
 ../include/storage.h
makehistory.o: makehistory.c ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h \
 ../include/portable/wait.h ../include/inn/qio.h ../include/libinn.h \
 ../include/macros.h ../include/ov.h ../include/storage.h \
 ../include/inn/history.h ../include/paths.h
prunehistory.o: prunehistory.c ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h ../include/libinn.h \
 ../include/paths.h ../include/inn/history.h

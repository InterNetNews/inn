##  $Id$

include ../Makefile.global

top	        = ..
CFLAGS		= $(GCFLAGS) $(TCLINC)

ALL		= innd

SOURCES		= art.c cc.c chan.c icd.c innd.c keywords.c lc.c nc.c \
		  newsfeeds.c ng.c perl.c proc.c python.c rc.c site.c \
		  status.c tcl.c util.c wip.c

OBJECTS		= $(SOURCES:.c=.o)

INSTALLED	= $(D)$(PATHBIN)/innd

all: $(ALL)

warnings:
	$(MAKE) COPT='$(WARNINGS)' all

clean:
	rm -f *.o $(ALL) inndp
	rm -rf .libs

clobber distclean: clean
	rm -f tags

tags ctags: $(SOURCES)
	$(CTAGS) $(SOURCES) ../lib/*.c innd.h ../include/*.h


##  Compilation rules.

INNDLIBS 	= $(LIBSTORAGE) $(LIBHIST) $(LIBINN) $(STORAGE_LIBS) \
		  $(PYTHON_LIBS) $(REGEX_LIBS) $(LIBS) $(PERL_LIBS)

perl.o:		perl.c   ; $(CC) $(CFLAGS) $(PERL_CPPFLAGS) -c perl.c
python.o:	python.c ; $(CC) $(CFLAGS) $(PYTHON_CPPFLAGS) -c python.c

innd: $(OBJECTS) $(LIBSTORAGE) $(LIBHIST) $(LIBINN)
	$(LIBLD) $(LDFLAGS) -o $@ $(OBJECTS) $(INNDLIBS)


##  Installation rules.  Installation commands set in Makefile.global.

install: $(INSTALLED)

$(D)$(PATHBIN)/innd:	innd	; $(LI_XPRI) $? $@

$(LIBINN):	; (cd ../lib ; $(MAKE))
$(LIBSTORAGE):	; (cd ../storage ; $(MAKE))
$(LIBHIST):	; (cd ../history ; $(MAKE))


##  Profiling.  These rules have not been checked for a while and may need
##  some work.

profiled: inndp

inndp: $(SOURCES)
	rm -f $(OBJECTS)
	$(MAKEPROFILING) innd
	mv innd inndp
	rm -f $(OBJECTS)


##  Dependencies.  Default list, below, is probably good enough.

depend: Makefile $(SOURCES)
	$(MAKEDEPEND) '$(CFLAGS) $(PERLINC) $(PYTHONINC) $(TCLINC)' $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
art.o: art.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/innconf.h ../include/inn/wire.h \
 innd.h ../include/portable/time.h ../include/portable/socket.h \
 ../include/inn/buffer.h ../include/inn/history.h \
 ../include/inn/messages.h ../include/inn/timer.h ../include/libinn.h \
 ../include/nntp.h ../include/paths.h ../include/storage.h \
 ../include/ov.h
cc.o: cc.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/innconf.h ../include/inn/qio.h \
 innd.h ../include/portable/time.h ../include/portable/socket.h \
 ../include/inn/buffer.h ../include/inn/history.h \
 ../include/inn/messages.h ../include/inn/timer.h ../include/libinn.h \
 ../include/nntp.h ../include/paths.h ../include/storage.h \
 ../include/inndcomm.h ../include/innperl.h
chan.o: chan.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/innconf.h innd.h \
 ../include/portable/time.h ../include/portable/socket.h \
 ../include/inn/buffer.h ../include/inn/history.h \
 ../include/inn/messages.h ../include/inn/timer.h ../include/libinn.h \
 ../include/nntp.h ../include/paths.h ../include/storage.h
icd.o: icd.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/portable/mmap.h \
 ../include/inn/innconf.h innd.h ../include/portable/time.h \
 ../include/portable/socket.h ../include/inn/buffer.h \
 ../include/inn/history.h ../include/inn/messages.h \
 ../include/inn/timer.h ../include/libinn.h ../include/nntp.h \
 ../include/paths.h ../include/storage.h ../include/ov.h
innd.o: innd.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/innconf.h \
 ../include/inn/messages.h ../include/innperl.h innd.h \
 ../include/portable/time.h ../include/portable/socket.h \
 ../include/inn/buffer.h ../include/inn/history.h \
 ../include/inn/timer.h ../include/libinn.h ../include/nntp.h \
 ../include/paths.h ../include/storage.h ../include/ov.h
keywords.o: keywords.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h ../include/inn/innconf.h \
 innd.h ../include/portable/time.h ../include/portable/socket.h \
 ../include/inn/buffer.h ../include/inn/history.h \
 ../include/inn/messages.h ../include/inn/timer.h ../include/nntp.h \
 ../include/paths.h ../include/storage.h
lc.o: lc.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/innconf.h innd.h \
 ../include/portable/time.h ../include/portable/socket.h \
 ../include/inn/buffer.h ../include/inn/history.h \
 ../include/inn/messages.h ../include/inn/timer.h ../include/libinn.h \
 ../include/nntp.h ../include/paths.h ../include/storage.h
nc.o: nc.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/innconf.h innd.h \
 ../include/portable/time.h ../include/portable/socket.h \
 ../include/inn/buffer.h ../include/inn/history.h \
 ../include/inn/messages.h ../include/inn/timer.h ../include/libinn.h \
 ../include/nntp.h ../include/paths.h ../include/storage.h
newsfeeds.o: newsfeeds.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/innconf.h innd.h \
 ../include/portable/time.h ../include/portable/socket.h \
 ../include/inn/buffer.h ../include/inn/history.h \
 ../include/inn/messages.h ../include/inn/timer.h ../include/libinn.h \
 ../include/nntp.h ../include/paths.h ../include/storage.h
ng.o: ng.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/innconf.h innd.h \
 ../include/portable/time.h ../include/portable/socket.h \
 ../include/inn/buffer.h ../include/inn/history.h \
 ../include/inn/messages.h ../include/inn/timer.h ../include/libinn.h \
 ../include/nntp.h ../include/paths.h ../include/storage.h \
 ../include/ov.h
perl.o: perl.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h innd.h ../include/portable/time.h \
 ../include/portable/socket.h ../include/inn/buffer.h \
 ../include/inn/history.h ../include/inn/messages.h \
 ../include/inn/timer.h ../include/libinn.h ../include/nntp.h \
 ../include/paths.h ../include/storage.h ../include/ppport.h \
 ../include/innperl.h
proc.o: proc.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/portable/wait.h innd.h \
 ../include/portable/time.h ../include/portable/socket.h \
 ../include/inn/buffer.h ../include/inn/history.h \
 ../include/inn/messages.h ../include/inn/timer.h ../include/libinn.h \
 ../include/nntp.h ../include/paths.h ../include/storage.h
python.o: python.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/innconf.h innd.h \
 ../include/portable/time.h ../include/portable/socket.h \
 ../include/inn/buffer.h ../include/inn/history.h \
 ../include/inn/messages.h ../include/inn/timer.h ../include/libinn.h \
 ../include/nntp.h ../include/paths.h ../include/storage.h
rc.o: rc.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/innconf.h innd.h \
 ../include/portable/time.h ../include/portable/socket.h \
 ../include/inn/buffer.h ../include/inn/history.h \
 ../include/inn/messages.h ../include/inn/timer.h ../include/libinn.h \
 ../include/nntp.h ../include/paths.h ../include/storage.h
site.o: site.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/innconf.h innd.h \
 ../include/portable/time.h ../include/portable/socket.h \
 ../include/inn/buffer.h ../include/inn/history.h \
 ../include/inn/messages.h ../include/inn/timer.h ../include/libinn.h \
 ../include/nntp.h ../include/paths.h ../include/storage.h
status.o: status.c ../include/clibrary.h ../include/config.h \
 ../include/inn/defines.h ../include/inn/innconf.h innd.h \
 ../include/portable/time.h ../include/portable/socket.h \
 ../include/inn/buffer.h ../include/inn/history.h \
 ../include/inn/messages.h ../include/inn/timer.h ../include/libinn.h \
 ../include/nntp.h ../include/paths.h ../include/storage.h \
 ../include/innperl.h
tcl.o: tcl.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/innconf.h innd.h \
 ../include/portable/time.h ../include/portable/socket.h \
 ../include/inn/buffer.h ../include/inn/history.h \
 ../include/inn/messages.h ../include/inn/timer.h ../include/libinn.h \
 ../include/nntp.h ../include/paths.h ../include/storage.h
util.o: util.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/innconf.h ../include/libinn.h \
 innd.h ../include/portable/time.h ../include/portable/socket.h \
 ../include/inn/buffer.h ../include/inn/history.h \
 ../include/inn/messages.h ../include/inn/timer.h ../include/nntp.h \
 ../include/paths.h ../include/storage.h
wip.o: wip.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/innconf.h innd.h \
 ../include/portable/time.h ../include/portable/socket.h \
 ../include/inn/buffer.h ../include/inn/history.h \
 ../include/inn/messages.h ../include/inn/timer.h ../include/libinn.h \
 ../include/nntp.h ../include/paths.h ../include/storage.h

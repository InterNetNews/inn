##  $Id$

include ../Makefile.global

top	        = ..
CFLAGS		= $(GCFLAGS) $(TCLINC)

ALL		= innd inndstart

SOURCES		= art.c cc.c chan.c icd.c innd.c inndstart.c keywords.c lc.c \
		  nc.c newsfeeds.c ng.c perl.c proc.c python.c rc.c site.c \
		  status.c tcl.c util.c wip.c

# The objects that are linked into innd.  All SOURCES except inndstart.
OBJECTS		= art.o cc.o chan.o icd.o innd.o keywords.o lc.o nc.o \
		  newsfeeds.o ng.o perl.o proc.o python.o rc.o site.o \
		  status.o tcl.o util.o wip.o

all: $(ALL)

warnings:
	$(MAKE) COPT='$(WARNINGS)' all

install: all
	$(LI_XPRI) innd $D$(PATHBIN)/innd
	@ME=`$(WHOAMI)` ; \
	if [ x"$$ME" = xroot ] ; then \
	    echo $(LI_SPRI) inndstart $D$(PATHBIN)/inndstart ; \
	    $(LI_SPRI) inndstart $D$(PATHBIN)/inndstart ; \
	else \
	    echo $(LI_XPRI) inndstart $D$(PATHBIN)/inndstart ; \
	    $(LI_XPRI) inndstart $D$(PATHBIN)/inndstart ; \
	    echo '' ; \
	    echo '========================' ; \
	    echo 'NOTE NOTE NOTE NOTE NOTE' ; \
	    ls -l $D$(PATHBIN)/inndstart ; \
	    echo '$D$(PATHBIN)/inndstart needs to be installed setuid root' ; \
	    echo '' ; echo ; \
	fi

clean:
	rm -f *.o $(ALL) inndp profiled
	rm -rf .libs

clobber distclean: clean
	rm -f tags

tags ctags: $(SOURCES)
	$(CTAGS) $(SOURCES) ../lib/*.c innd.h ../include/*.h


##  Compilation rules.

INNDLIBS 	= $(LIBSTORAGE) $(LIBHIST) $(LIBINN) $(EXTSTORAGELIBS) \
		  $(PERLLIB) $(TCLLIB) $(PYTHONLIB) $(REGEXLIB) $(LIBS)

perl.o:		perl.c		; $(CC) $(CFLAGS) $(PERLINC) -c perl.c
python.o:	python.c	; $(CC) $(CFLAGS) $(PYTHONINC) -c python.c

innd: $(OBJECTS) $(LIBSTORAGE) $(LIBHIST) $(LIBINN)
	$(LIBLD) $(LDFLAGS) -o $@ $(OBJECTS) $(INNDLIBS)

inndstart: inndstart.o $(LIBINN)
	$(LIBLD) $(LDFLAGS) -o $@ inndstart.o $(LIBINN) $(LIBS)

$(LIBINN):	; (cd ../lib ; $(MAKE))
$(LIBSTORAGE):	; (cd ../storage ; $(MAKE))
$(LIBHIST):	; (cd ../history ; $(MAKE))


##  Profiling.  These rules have not been checked for a while and may need
##  some work.

profiled:	inndp
	date >$@

inndp:		$(SOURCES)
	rm -f $(OBJECTS)
	$(MAKEPROFILING) innd
	mv innd inndp
	rm -f $(OBJECTS)


##  Dependencies.  Default list, below, is probably good enough.

depend: Makefile $(SOURCES)
	$(MAKEDEPEND) '$(CFLAGS) $(PERLINC) $(PYTHONINC) $(TCLINC)' $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
art.o: art.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/clibrary.h ../include/config.h \
  ../include/inn/innconf.h ../include/inn/defines.h ../include/inn/wire.h \
  ../include/inn/md5.h innd.h ../include/portable/time.h \
  ../include/config.h ../include/portable/socket.h \
  ../include/inn/buffer.h ../include/inn/history.h \
  ../include/inn/messages.h ../include/inn/timer.h ../include/libinn.h \
  ../include/nntp.h ../include/paths.h ../include/storage.h \
  ../include/ov.h ../include/storage.h ../include/inn/history.h
cc.o: cc.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/clibrary.h ../include/config.h \
  ../include/inn/innconf.h ../include/inn/defines.h ../include/inn/qio.h \
  innd.h ../include/portable/time.h ../include/config.h \
  ../include/portable/socket.h ../include/inn/buffer.h \
  ../include/inn/history.h ../include/inn/messages.h \
  ../include/inn/timer.h ../include/libinn.h ../include/nntp.h \
  ../include/paths.h ../include/storage.h ../include/inndcomm.h \
  ../include/innperl.h
chan.o: chan.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/clibrary.h ../include/config.h \
  ../include/inn/innconf.h ../include/inn/defines.h innd.h \
  ../include/portable/time.h ../include/config.h \
  ../include/portable/socket.h ../include/inn/buffer.h \
  ../include/inn/history.h ../include/inn/messages.h \
  ../include/inn/timer.h ../include/libinn.h ../include/nntp.h \
  ../include/paths.h ../include/storage.h
icd.o: icd.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/clibrary.h ../include/config.h \
  ../include/portable/mmap.h ../include/config.h ../include/inn/innconf.h \
  ../include/inn/defines.h innd.h ../include/portable/time.h \
  ../include/portable/socket.h ../include/inn/buffer.h \
  ../include/inn/history.h ../include/inn/messages.h \
  ../include/inn/timer.h ../include/libinn.h ../include/nntp.h \
  ../include/paths.h ../include/storage.h ../include/ov.h \
  ../include/storage.h ../include/inn/history.h
innd.o: innd.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/clibrary.h ../include/config.h \
  ../include/inn/innconf.h ../include/inn/defines.h \
  ../include/inn/messages.h ../include/innperl.h innd.h \
  ../include/portable/time.h ../include/config.h \
  ../include/portable/socket.h ../include/inn/buffer.h \
  ../include/inn/history.h ../include/inn/timer.h ../include/libinn.h \
  ../include/nntp.h ../include/paths.h ../include/storage.h \
  ../include/ov.h ../include/storage.h ../include/inn/history.h
inndstart.o: inndstart.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/clibrary.h ../include/config.h \
  ../include/portable/socket.h ../include/config.h \
  ../include/inn/innconf.h ../include/inn/defines.h \
  ../include/inn/messages.h ../include/libinn.h ../include/paths.h
keywords.o: keywords.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/clibrary.h ../include/config.h \
  ../include/libinn.h ../include/inn/innconf.h ../include/inn/defines.h \
  innd.h ../include/portable/time.h ../include/config.h \
  ../include/portable/socket.h ../include/inn/buffer.h \
  ../include/inn/history.h ../include/inn/messages.h \
  ../include/inn/timer.h ../include/nntp.h ../include/paths.h \
  ../include/storage.h
lc.o: lc.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/clibrary.h ../include/config.h \
  ../include/inn/innconf.h ../include/inn/defines.h innd.h \
  ../include/portable/time.h ../include/config.h \
  ../include/portable/socket.h ../include/inn/buffer.h \
  ../include/inn/history.h ../include/inn/messages.h \
  ../include/inn/timer.h ../include/libinn.h ../include/nntp.h \
  ../include/paths.h ../include/storage.h
nc.o: nc.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/clibrary.h ../include/config.h \
  ../include/inn/innconf.h ../include/inn/defines.h innd.h \
  ../include/portable/time.h ../include/config.h \
  ../include/portable/socket.h ../include/inn/buffer.h \
  ../include/inn/history.h ../include/inn/messages.h \
  ../include/inn/timer.h ../include/libinn.h ../include/nntp.h \
  ../include/paths.h ../include/storage.h
newsfeeds.o: newsfeeds.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/clibrary.h ../include/config.h \
  ../include/inn/innconf.h ../include/inn/defines.h innd.h \
  ../include/portable/time.h ../include/config.h \
  ../include/portable/socket.h ../include/inn/buffer.h \
  ../include/inn/history.h ../include/inn/messages.h \
  ../include/inn/timer.h ../include/libinn.h ../include/nntp.h \
  ../include/paths.h ../include/storage.h
ng.o: ng.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/clibrary.h ../include/config.h \
  ../include/inn/innconf.h ../include/inn/defines.h innd.h \
  ../include/portable/time.h ../include/config.h \
  ../include/portable/socket.h ../include/inn/buffer.h \
  ../include/inn/history.h ../include/inn/messages.h \
  ../include/inn/timer.h ../include/libinn.h ../include/nntp.h \
  ../include/paths.h ../include/storage.h ../include/ov.h \
  ../include/storage.h ../include/inn/history.h
perl.o: perl.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h
proc.o: proc.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/clibrary.h ../include/config.h \
  ../include/portable/wait.h ../include/config.h innd.h \
  ../include/portable/time.h ../include/portable/socket.h \
  ../include/inn/buffer.h ../include/inn/defines.h \
  ../include/inn/history.h ../include/inn/messages.h \
  ../include/inn/timer.h ../include/libinn.h ../include/nntp.h \
  ../include/paths.h ../include/storage.h
python.o: python.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/clibrary.h ../include/config.h \
  ../include/inn/innconf.h ../include/inn/defines.h innd.h \
  ../include/portable/time.h ../include/config.h \
  ../include/portable/socket.h ../include/inn/buffer.h \
  ../include/inn/history.h ../include/inn/messages.h \
  ../include/inn/timer.h ../include/libinn.h ../include/nntp.h \
  ../include/paths.h ../include/storage.h
rc.o: rc.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/clibrary.h ../include/config.h \
  ../include/portable/socket.h ../include/config.h \
  ../include/inn/innconf.h ../include/inn/defines.h innd.h \
  ../include/portable/time.h ../include/inn/buffer.h \
  ../include/inn/history.h ../include/inn/messages.h \
  ../include/inn/timer.h ../include/libinn.h ../include/nntp.h \
  ../include/paths.h ../include/storage.h
site.o: site.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/clibrary.h ../include/config.h \
  ../include/inn/innconf.h ../include/inn/defines.h innd.h \
  ../include/portable/time.h ../include/config.h \
  ../include/portable/socket.h ../include/inn/buffer.h \
  ../include/inn/history.h ../include/inn/messages.h \
  ../include/inn/timer.h ../include/libinn.h ../include/nntp.h \
  ../include/paths.h ../include/storage.h
status.o: status.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/clibrary.h ../include/config.h \
  ../include/portable/socket.h ../include/config.h \
  ../include/inn/innconf.h ../include/inn/defines.h innd.h \
  ../include/portable/time.h ../include/inn/buffer.h \
  ../include/inn/history.h ../include/inn/messages.h \
  ../include/inn/timer.h ../include/libinn.h ../include/nntp.h \
  ../include/paths.h ../include/storage.h ../include/innperl.h
tcl.o: tcl.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/clibrary.h ../include/config.h \
  ../include/inn/innconf.h ../include/inn/defines.h innd.h \
  ../include/portable/time.h ../include/config.h \
  ../include/portable/socket.h ../include/inn/buffer.h \
  ../include/inn/history.h ../include/inn/messages.h \
  ../include/inn/timer.h ../include/libinn.h ../include/nntp.h \
  ../include/paths.h ../include/storage.h
util.o: util.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/clibrary.h ../include/config.h \
  ../include/inn/innconf.h ../include/inn/defines.h ../include/libinn.h \
  innd.h ../include/portable/time.h ../include/config.h \
  ../include/portable/socket.h ../include/inn/buffer.h \
  ../include/inn/history.h ../include/inn/messages.h \
  ../include/inn/timer.h ../include/nntp.h ../include/paths.h \
  ../include/storage.h
wip.o: wip.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/clibrary.h ../include/config.h \
  ../include/inn/innconf.h ../include/inn/defines.h innd.h \
  ../include/portable/time.h ../include/config.h \
  ../include/portable/socket.h ../include/inn/buffer.h \
  ../include/inn/history.h ../include/inn/messages.h \
  ../include/inn/timer.h ../include/libinn.h ../include/nntp.h \
  ../include/paths.h ../include/storage.h

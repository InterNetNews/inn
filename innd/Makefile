##  $Id$

include ../Makefile.global

CFLAGS  = $(GCFLAGS)

SOURCES	= \
	art.c cc.c chan.c his.c icd.c innd.c lc.c nc.c newsfeeds.c ng.c \
	perl.c proc.c python.c rc.c site.c status.c tcl.c timer.c wip.c

OBJECTS	= \
	art.o cc.o chan.o his.o icd.o innd.o lc.o nc.o newsfeeds.o ng.o \
	perl.o proc.o python.o rc.o site.o status.o tcl.o timer.o wip.o

ALL	= innd inndstart

all:		$(ALL)

art.o: art.c
	$(CC) $(CFLAGS) $(TCLINC) -c art.c -o $@

cc.o: cc.c
	$(CC) $(CFLAGS) $(PERLINC) -c cc.c -o $@

perl.o: perl.c
	$(CC) $(CFLAGS) $(PERLINC) -c perl.c -o $@

python.o: python.c
	$(CC) $(CFLAGS) $(PYTHONINC) -c python.c -o $@

tcl.o: tcl.c
	$(CC) $(CFLAGS) $(TCLINC) -c tcl.c -o $@

install: $(D)$(PATHBIN)/innd $(D)$(PATHBIN)/inndstart

$(D)${PATHBIN}/innd: innd
	$(LI_XPRI) innd $(D)$(PATHBIN)/innd

$(D)$(PATHBIN)/inndstart: inndstart
	@ME=`$(WHOAMI)` ; \
	if [ x"$$ME" = xroot ] ; then \
	    echo $(LI_SPRI) inndstart $(D)$(PATHBIN)/inndstart ; \
	    $(LI_SPRI) inndstart $(D)$(PATHBIN)/inndstart ; \
	else \
	    echo $(LI_XPRI) inndstart $(D)$(PATHBIN)/inndstart ; \
	    $(LI_XPRI) inndstart $(D)$(PATHBIN)/inndstart ; \
	    echo '' ; \
	    echo '========================' ; \
	    echo 'NOTE NOTE NOTE NOTE NOTE' ; \
	    ls -l $@ ; \
	    echo '$@ needs to be installed setuid root' ; \
	    echo '' ; echo ; \
	fi

clobber clean:
	rm -f *.o $(ALL)
	rm -rf .libs
	rm -f inndp profiled
	rm -f all install 

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES) ../lib/*.c innd.h ../include/*.h

innd:		$(P) $(OBJECTS) $(LIBSTORAGE) $(LIBINN)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(LIBSTORAGE) $(LIBINN) $(EXTSTORAGELIBS) $(TCLLIB) $(PERLLIB) $(PYTHONLIB) $(LIBS)

inndstart:	$(P) inndstart.o $(LIBINN)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ inndstart.o $(LIBINN) $(LIBS)

$(LIBINN):
	(cd ../lib ; $(MAKE))
$(LIBSTORAGE):
	(cd ../storage ; $(MAKE))

##  Profiling.  The rules are a bit brute-force, but good enough.
profiled:	inndp
	date >$@

inndp:		$(SOURCES)
	rm -f $(OBJECTS)
	$(MAKE) innd CFLAGS="$(CFLAGS) $(PROF)" LIBINN=../libinn_p.a
	mv innd inndp
	rm -f $(OBJECTS)

##  Dependencies.  Default list, below, is probably good enough.
depend:		Makefile $(SOURCES) ../include/dbz.h
	makedepend $(DEFS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
$(OBJECTS):	innd.h ../include/clibrary.h ../include/configdata.h \
		../include/libinn.h \
		../include/macros.h ../include/nntp.h ../include/paths.h
inndstart.o:	../include/clibrary.h ../include/configdata.h \
		../include/libinn.h \
		../include/macros.h ../include/paths.h

##  $Revision$
include ../Makefile.global

CFLAGS = $(GCFLAGS) $(PERLINC) $(TCLINC) $(PYTHONINC)

SOURCES	= \
	art.c cc.c chan.c his.c icd.c innd.c lc.c nc.c newsfeeds.c ng.c \
	perl.c proc.c python.c rc.c site.c status.c tcl.c timer.c wip.c

OBJECTS	= \
	art.o cc.o chan.o his.o icd.o innd.o lc.o nc.o newsfeeds.o ng.o \
	perl.o proc.o python.o rc.o site.o status.o tcl.o timer.o wip.o

ALL	= innd inndstart

all:		$(ALL)

install: $D${PATHBIN}/innd $D${PATHBIN}/inndstart

$D${PATHBIN}/innd: innd
	$(LIBTOOL) ../installit.sh $(OWNER) -m 0550 -b .OLD innd $D${PATHBIN}/innd

$D${PATHBIN}/inndstart: inndstart
	-$(LIBTOOL) ../installit.sh -O root -G $(NEWSGROUP) -m 4550 -b .OLD inndstart $D${PATHBIN}/inndstart
	@ME=`(whoami || /usr/ucb/whoami) 2> /dev/null` ;\
	if [ "X$$ME" = Xroot ]; then \
		echo $@ has been installed setuid root. ;\
	else\
		echo "" ; echo "===============================" ;\
		echo  "NOTE NOTE NOTE NOTE NOTE" ;\
		ls -l $@ ;\
		echo $@ needs to be installed setuid root ;\
		echo "" ; echo "" ;\
	fi


clobber clean:
	rm -f *.o $(ALL)
	rm -rf .libs
	rm -f inndp profiled
	rm -f all install 

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES) ../lib/*.c innd.h ../include/*.h

innd:		$(P) $(OBJECTS) $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(LIBNEWS) $(TCLLIB) $(PERLLIB) $(PYTHONLIB) $(LIBS)

inndstart:	$(P) inndstart.o $(LIBNEWS)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ inndstart.o $(LIBNEWS) $(LIBS)

../include/dbz.h:
	(cd ../lib ; $(MAKE) ../include/dbz.h)

$(LIBNEWS):
	(cd ../lib ; $(MAKE) )

##  Profiling.  The rules are a bit brute-force, but good enough.
profiled:	inndp
	date >$@

inndp:		$(SOURCES)
	rm -f $(OBJECTS)
	$(MAKE) innd CFLAGS="$(CFLAGS) $(PROF)" LIBNEWS=../libinn_p.a
	mv innd inndp
	rm -f $(OBJECTS)

##
ccenter:	$(SOURCES)
	#load $(CFLAGS) $(SOURCES) $(LIBNEWS)

##  Dependencies.  Default list, below, is probably good enough.
depend:		Makefile $(SOURCES) ../include/dbz.h
	makedepend $(DEFS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
$(OBJECTS):	innd.h ../include/clibrary.h ../include/configdata.h \
		../include/libinn.h \
		../include/macros.h ../include/nntp.h ../include/paths.h
ng.o:		../include/mydir.h
inndstart.o:	../include/clibrary.h ../include/configdata.h \
		../include/libinn.h \
		../include/macros.h ../include/paths.h

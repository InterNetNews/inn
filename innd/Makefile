##  $Revision$
include ../Makefile.global

##  =()<TCLINC	= @<TCL_INC>@>()=
TCLINC	= 

##  =()<LINTFLAGS	= @<LINTFLAGS>@ $(PERLINC) $(TCLINC)>()=
LINTFLAGS	= -b -h -z  -I../include $(DEFS) $(PERLINC) $(TCLINC)
##  =()<LINTFILTER	= @<LINTFILTER>@>()=
LINTFILTER	= | sed -n -f ../sedf.sun
##  =()<CTAGS		= @<CTAGS>@>()=
CTAGS		= /usr/bin/ctags -t -w
##  =()<PROF	= @<PROF>@>()=
PROF	= -pg

CFLAGS += $(PERLINC)

## =()<TCL_LIB	= @<TCL_LIB>@>()=
TCL_LIB	= 

SOURCES	= \
	art.c cc.c chan.c his.c icd.c innd.c lc.c nc.c newsfeeds.c ng.c \
	perl.c proc.c rc.c site.c status.c tcl.c timer.c wip.c

OBJECTS	= \
	art.o cc.o chan.o his.o icd.o innd.o lc.o nc.o newsfeeds.o ng.o \
	perl.o proc.o rc.o site.o status.o tcl.o timer.o wip.o

ALL	= innd inndstart

all:		$(ALL)

install: ${PATHBIN}/innd ${PATHBIN}/inndstart

${PATHBIN}/innd: innd
	$(SHELL) ../installit.sh $(OWNER) -m 0550 -b .OLD innd ${PATHBIN}/innd

${PATHBIN}/inndstart: inndstart
	-$(SHELL) ../installit.sh -O root -G $(NEWSGROUP) -m 4550 -b .OLD inndstart ${PATHBIN}/inndstart
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
	rm -f inndp profiled
	rm -f all install lint

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES) ../lib/*.c innd.h ../include/*.h

innd:		$(P) $(OBJECTS) $(LIBNEWS)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(LIBNEWS) $(TCL_LIB) $(PERLLIB) $(LIBS)

inndstart:	$(P) inndstart.o $(LIBNEWS)
	$(CC) $(LDFLAGS) -o $@ inndstart.o $(LIBNEWS) $(LIBS)

lint:		$(ALL)
	@rm -f lint
	-lint $(LINTFLAGS) $(SOURCES)  $(LINTLIB) $(LINTFILTER)  >lint
	-lint $(LINTFLAGS) inndstart.c $(LINTLIB) $(LINTFILTER) >>lint

../include/dbz.h:
	(cd ../lib ; $(MAKE) ../include/dbz.h)
$(LIBNEWS) $(LINTLIB):
	(cd ../lib ; $(MAKE) install)

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
		../include/libinn.h ../include/logging.h \
		../include/macros.h ../include/nntp.h ../include/paths.h
ng.o:		../include/mydir.h
inndstart.o:	../include/clibrary.h ../include/configdata.h \
		../include/libinn.h ../include/logging.h \
		../include/macros.h ../include/paths.h

##  $Revision$
SHELL	= /bin/sh
MAKE	= make
IFTRUE	= @$(SHELL) ../iftrue.sh

##  =()<P	= @<P>@>()=
P	= 

##  =()<CC	= @<CC>@>()=
CC	= gcc
##  =()<DEFS	= @<DEFS>@>()=
DEFS	= -I../include
##  =()<PERLINC = @<PERL_INC>@>()=
PERLINC = 
##  =()<CFLAGS	= @<CFLAGS>@ $(PERLINC)>()=
CFLAGS	= $(DEFS) -g $(PERLINC)
##  =()<DBZCFLAGS	= @<DBZCFLAGS>@>()=
DBZCFLAGS	= $(CFLAGS)
##  =()<LDFLAGS	= @<LDFLAGS>@>()=
LDFLAGS	= 
##  =()<LINTLIBSTYLE	= @<LINTLIBSTYLE>@>()=
LINTLIBSTYLE	= NONE
##  =()<LINTFLAGS	= @<LINTFLAGS>@ $(PERLINC)>()=
LINTFLAGS	= -b -h -z $(DEFS) $(PERLINC)
##  =()<LINTFILTER	= @<LINTFILTER>@>()=
LINTFILTER	= | sed -n -f ../sedf.sun
##  =()<YACC	= @<YACC>@>()=
YACC	= yacc
##  =()<CTAGS		= @<CTAGS>@>()=
CTAGS		= ctags -t -w
##  =()<PROF	= @<PROF>@>()=
PROF	= -pg

##  =()<RANLIB	= @<RANLIB>@>()=
RANLIB	= ranlib

##  For OSx systems, get these from the ATT Universe libc.
##  See the rule for version.o, below.
OSXATTOBJ= version.o memchr.o memcmp.o memcpy.o memset.o strchr.o strrchr.o

##  =()<MISSING_SRC = @<MISSING_SRC>@>()=
MISSING_SRC = 
##  =()<MISSING_OBJ = @<MISSING_OBJ>@>()=
MISSING_OBJ = 

SOURCES = $(MISSING_SRC) \
	checkart.c cleanfrom.c clientactive.c clientlib.c closeonexec.c \
	dbz.c defdist.c findheader.c genid.c getconfig.c getdtab.c \
	getfqdn.c getmodaddr.c gettime.c inndcomm.c innvers.c localopen.c \
	lockfile.c md5.c nonblocking.c parsedate.c perl.c qio.c radix32.c \
	readin.c remopen.c resource.c sendarticle.c sendpass.c tempname.c \
	waitnb.c wildmat.c xfopena.c xmalloc.c xmemerr.c xrealloc.c xwrite.c \
	xwritev.c
OBJECTS = $(MISSING_OBJ) \
	checkart.o cleanfrom.o clientactive.o clientlib.o closeonexec.o \
	dbz.o defdist.o findheader.o genid.o getconfig.o getdtab.o \
	getfqdn.o getmodaddr.o gettime.o inndcomm.o innvers.o localopen.o \
	lockfile.o md5.o nonblocking.o parsedate.o perl.o qio.o radix32.o \
	readin.o remopen.o resource.o sendarticle.o sendpass.o tempname.o \
	waitnb.o wildmat.o xfopena.o xmalloc.o xmemerr.o xrealloc.o xwrite.o \
	xwritev.o

all:			libinn.a

install:		../libinn.a ../llib-linn.ln

clobber clean:
	rm -f *.o libinn.a llib-linn.ln
	rm -f parsedate parsedate.c
	rm -f profiled libinn_p.a
	rm -f all install lint lint.all

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES) ../include/*.h

libinn.a:		$(P) $(OBJECTS)
	ar r $@ $(OBJECTS)
	$(RANLIB) libinn.a

llib-linn.ln:		lint
	-$(SHELL) ./makellib.sh $(LINTLIBSTYLE) "$(LINTFLAGS)" $(SOURCES)

lint:			$(P) $(OBJECTS)
	@rm -f lint
	-lint -u $(LINTFLAGS) $(SOURCES) $(LINTFILTER) >lint.all
	-grep -v yaccpar <lint.all >lint

##  Profiling.  The rules are a bit brute-force, but good enough.
profiled:		../libinn_p.a
	date >$@

../libinn_p.a:		$(SOURCES)
	rm -f $(OBJECTS)
	$(MAKE) libinn.a CFLAGS="$(CFLAGS) $(PROF)"
	mv libinn.a ../libinn_p.a
	$(RANLIB) ../libinn_p.a
	rm -f $(OBJECTS)

parsedate.c:		parsedate.y
	@echo Expect 6 shift/reduce conflicts
	$(YACC) parsedate.y
	@mv y.tab.c parsedate.c

parsedate:		$(P) parsedate.c gettime.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ -DTEST -DYYDEBUG parsedate.c gettime.o

##  These rules will only be triggered if syslog appears in the MISSING_xxx
##  macros above.
syslog.o:		syslog.c
syslog.c:		../syslog/syslog.c
	(cd .. ; $(MAKE) syslogfix )

## This rule will only be triggered if $(OSXATTOBJ) is in MISSING_OBJ.
version.o:
	@rm -f $(OSXATTOBJ)
	/.attbin/ar vx /usr/.attlib/libc.a $(OSXATTOBJ)

##
ccenter:	$(SOURCES)
	#load $(CFLAGS) $(SOURCES)

##  Low-level install actions.
../libinn.a:		libinn.a
	@rm -f $@
	cp libinn.a $@
	$(RANLIB) $@
../llib-linn.ln:	llib-linn.ln
	@rm -f $@
	cp llib-linn.ln $@

##  Dependencies.  Default list, below, is probably good enough.
depend:		Makefile $(SOURCES) ../include/dbz.h
	makedepend $(DEFS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
checkart.o:	../include/nntp.h
checkart.o:	../include/nntp.h
cleanfrom.o:	../include/macros.h
clientactive.o:	../include/paths.h
clientlib.o:	../include/myserver.h
clientlib.o:	../include/nntp.h
clientlib.o:	../include/macros.h
clientlib.o:	../include/paths.h
dbz.o:		../include/dbz.h
defdist.o:	../include/paths.h
defdist.o:	../include/macros.h
findheader.o:	../include/macros.h
genid.o:	../include/configdata.h
genid.o:	../include/clibrary.h
genid.o:	../include/libinn.h
getconfig.o:	../include/macros.h
getconfig.o:	../include/paths.h
getfqdn.o:	../include/paths.h
getmodaddr.o:	../include/macros.h
getmodaddr.o:	../include/paths.h
inndcomm.o:	../include/inndcomm.h
inndcomm.o:	../include/macros.h
inndcomm.o:	../include/nntp.h
inndcomm.o:	../include/paths.h
innvers.o:	../include/patchlevel.h
localopen.o:	../include/macros.h
localopen.o:	../include/nntp.h
localopen.o:	../include/paths.h
md5.o:		../include/md5.h
parsedate.o:	../include/macros.h
qio.o:		../include/macros.h
qio.o:		../include/qio.h
readin.o:	../include/macros.h
remopen.o:	../include/nntp.h
remopen.o:	../include/paths.h
resource.o:	../include/macros.h
sendarticle.o:	../include/nntp.h
sendpass.o:	../include/macros.h
sendpass.o:	../include/nntp.h
sendpass.o:	../include/paths.h
xmalloc.o:	../include/macros.h
xrealloc.o:	../include/macros.h

$(OBJECTS):	../include/configdata.h \
		../include/clibrary.h \
		../include/libinn.h

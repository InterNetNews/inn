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
##  =()<CFLAGS	= -I. @<CFLAGS>@ $(PERLINC)>()=
CFLAGS	= -I. $(DEFS) -g -Wall $(PERLINC)
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
##  =()<LIBS	= @<LIBS>@>()=
LIBS	= 

all:			autoconfig methods libstorage.a

include Make.methods

SOURCES = interface.c methods.c qio.c
OBJECTS = interface.o methods.o qio.o


methods: 
	@if [ ! -d objs ] ; then \
		mkdir objs; \
	fi ;
	@for D in $(SUBDIR); do \
	cd $$D; $(MAKE) $(FLAGS) DESTDIR=$(DESTDIR) || exit 1; cd ..; \
	done

install:		all ../libstorage.a 

clobber clean:
	rm -f *.o objs/* libstorage.a *~
	rm -f methods.c methods.h qiotest
	rm -f profiled libstorage_p.a
	rm -f all install lint lint.all
	@for D in $(SUBDIR); do \
		cd $$D; $(MAKE) clean; cd .. ;\
	done

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES) ../include/*.h

libstorage.a:		$(P) $(OBJECTS)
	ar r $@ $(OBJECTS) objs/*
	$(RANLIB) libstorage.a

qiotest: qio.c ../libstorage.a ../libinn.a
	$(CC) $(CFLAGS) -DTEST -o qiotest qio.c ../libstorage.a ../libinn.a

autoconfig:
	@./buildconfig `find . -name method.config -print`

##  Profiling.  The rules are a bit brute-force, but good enough.
profiled:		../libstorage_p.a
	date >$@

../libstorage_p.a:	$(SOURCES)
	rm -f $(OBJECTS)
	$(MAKE) libstorage.a CFLAGS="$(CFLAGS) $(PROF)"
	mv libstorage.a ../libstorage_p.a
	$(RANLIB) ../libstorage_p.a
	rm -f $(OBJECTS)

##  Low-level install actions.
../libstorage.a:	libstorage.a
	@rm -f $@
	cp libstorage.a $@
	$(RANLIB) $@

##  Dependencies.  Default list, below, is probably good enough.
depend:		Makefile $(SOURCES)
	makedepend $(DEFS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.

interface.o: methods.h interface.h
methods.o: interface.h

##  $Revision$
include ../Makefile.global

CC	= $(LIBCC)
CP	= $(LIBTOOL) cp

CFLAGS	= $(GCFLAGS) -I.

IFTRUE	= @$(SHELL) ../iftrue.sh

all:			autoconfig
	@$(MAKE) $(FLAGS) DESTDIR=$(DESTDIR) methods
	@$(MAKE) $(FLAGS) DESTDIR=$(DESTDIR) libstorage.$(EXTLIB)

include Make.methods

SOURCES = interface.c methods.c qio.c overview.c
OBJECTS = interface.o methods.o qio.o overview.o
LOBJECTS = interface.lo methods.lo qio.lo overview.lo


methods: 
	@if [ ! -d objs ] ; then \
		mkdir objs; \
	fi ;
	@for D in $(SUBDIR); do \
	cd $$D; $(MAKE) $(FLAGS) DESTDIR=$(DESTDIR) || exit 1; cd ..; \
	done

install:		all $(D)$(PATHLIB)/libstorage.$(EXTLIB) 

clobber clean:
	rm -f *.o *.lo libstorage.la libstorage.a objs/*
	rm -rf .libs
	rm -f methods.c methods.h qiotest
	rm -f profiled libstorage_p.a
	rm -f all install 
	@for D in $(SUBDIR); do \
		cd $$D; $(MAKE) clean; cd .. ;\
	done

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES) ../include/*.h

libstorage.la:	$(P) $(OBJECTS) $(LOBJECTS)
	$(CC) $(LDFLAGS) -o libstorage.la $(LOBJECTS) objs/*.lo \
		-rpath $(PATHLIB) -version-info 2:0:0

libstorage.a:         $(P) $(OBJECTS)
	ar r $@ $(OBJECTS) objs/*
	$(RANLIB) libstorage.a

qiotest: qio.c libstorage.$(EXTLIB) ../lib/libinn.$(EXTLIB)
	$(CC) $(CFLAGS) -DTEST -o qiotest qio.c libstorage.$(EXTLIB) ../lib/libinn.$(EXTLIB)

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
$(D)$(PATHLIB)/libstorage.$(EXTLIB):	libstorage.$(EXTLIB)
	@rm -f $@
	$(LIBTOOL) cp -p libstorage.$(EXTLIB) $@

##  Dependencies.  Default list, below, is probably good enough.
depend:		Makefile $(SOURCES)
	makedepend $(DEFS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.

interface.o: methods.h interface.h
methods.o: interface.h
overview.o: overview.h

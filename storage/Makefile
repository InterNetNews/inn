##  $Id$

include ../Makefile.global

CP	= $(LIBTOOL) cp
CFLAGS	= $(GCFLAGS) -I.

all:			autoconfig
	@$(MAKE) $(FLAGS) DESTDIR=$(DESTDIR) methods
	@$(MAKE) $(FLAGS) DESTDIR=$(DESTDIR) libstorage.$(EXTLIB)

include Make.methods
include Make.ovmethods

SOURCES = interface.c methods.c ovmethods.c ov.c
OBJECTS = interface.o methods.o ovmethods.o ov.o
LOBJECTS = interface.lo methods.lo ovmethods.lo ov.lo

methods: 
	@[ -d objs ] || mkdir objs
	@for D in $(SUBDIR) $(OVSUBDIR); do \
	cd $$D; $(MAKE) $(FLAGS) DESTDIR=$(DESTDIR) || exit 1; cd ..; \
	done

install:		all $(D)$(PATHLIB)/libstorage.$(EXTLIB) 

clobber clean:
	rm -f *.o *.lo libstorage.la libstorage.a
	rm -rf objs .libs
	rm -f methods.c methods.h
	rm -f ovmethods.c ovmethods.h
	rm -f profiled libstorage_p.a
	rm -f buildconfig ovbuildconfig
	@for D in $(SUBDIR) $(OVSUBDIR); do \
		cd $$D; $(MAKE) clean; cd .. ;\
	done

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES) ../include/*.h

libstorage.la:	$(P) $(OBJECTS) $(LOBJECTS)
	$(LIBCC) $(LDFLAGS) -o libstorage.la $(LOBJECTS) objs/*.lo \
		-rpath $(PATHLIB) -version-info 2:0:0

libstorage.a:         $(P) $(OBJECTS)
	ar r $@ $(OBJECTS) objs/*
	$(RANLIB) libstorage.a

.c.o:
	$(LIBCC) $(CFLAGS) -c $*.c -o $@

ovtest: ov.c libstorage.$(EXTLIB) ../lib/libinn.$(EXTLIB)
	$(PURE) $(CC) $(CFLAGS) -I/shared/include -D_TEST_ -o ovtest ov.c libstorage.$(EXTLIB) ../lib/libinn.$(EXTLIB) -L/shared/lib -ldb -lnsl -lsocket

methods.h: autoconfig

autoconfig: buildconfig ovbuildconfig
	@./buildconfig `find . -name method.config -print`
	@./ovbuildconfig `find . -name ovmethod.config -print`

buildconfig: buildconfig.in $(FIXSCRIPT)
	$(FIXSCRIPT) -i buildconfig.in

ovbuildconfig: ovbuildconfig.in $(FIXSCRIPT)
	$(FIXSCRIPT) -i ovbuildconfig.in

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
$(D)$(PATHLIB)/libstorage.$(EXTLIB): libstorage.$(EXTLIB)
	$(LI_XPUB) $? $@

##  Dependencies.  Default list, below, is probably good enough.
depend:		Makefile $(SOURCES)
	makedepend $(DEFS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.

interface.o: methods.h interface.h
methods.o: interface.h
ov.c: ../include/ov.h ovmethods.h ovinterface.h

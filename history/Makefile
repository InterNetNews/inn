##  $Id$

include ../Makefile.global

CFLAGS	= $(GCFLAGS) -I.

all:			autoconfig
	@$(MAKE) $(FLAGS) DESTDIR=$(DESTDIR) methods
	@$(MAKE) $(FLAGS) DESTDIR=$(DESTDIR) libinnhist.$(EXTLIB)

warnings:
	$(MAKE) COPT='$(WARNINGS)' all

include Make.hismethods

SOURCES = his.c hismethods.c
OBJECTS = his.o hismethods.o
LOBJECTS = his.lo hismethods.lo

methods: 
	@for D in $(HISSUBDIR); do \
	cd $$D; $(MAKE) $(FLAGS) DESTDIR=$(DESTDIR) || exit 1; cd ..; \
	done

install:		all $(D)$(PATHLIB)/libinnhist.$(EXTLIB) 

clobber clean:
	rm -f *.o *.lo libinnhist.la libinnhist.a
	rm -rf .libs
	rm -f hismethods.c hismethods.h
	rm -f profiled libinnhist_p.a
	rm -f hisbuildconfig
	@for D in $(HISSUBDIR); do \
		cd $$D; $(MAKE) clean; cd .. ;\
	done

tags ctags:	$(SOURCES)
	$(CTAGS) $(SOURCES) ../include/*.h

libinnhist.la:	$(P) $(OBJECTS) $(LOBJECTS) $(LHIS_OBJECTS)
	$(LIBLD) $(LDFLAGS) -o libinnhist.la $(LOBJECTS) $(LHIS_OBJECTS) \
		-rpath $(PATHLIB) -version-info 2:0:0

libinnhist.a:         $(P) $(OBJECTS) $(HIS_OBJECTS)
	ar r $@ $(OBJECTS) $(HIS_OBJECTS)
	$(RANLIB) libinnhist.a

.c.o:
	$(LIBCC) $(CFLAGS) -c $*.c -o $@

hismethods.h hismethods.c: autoconfig

autoconfig: hisbuildconfig
	@./hisbuildconfig `find . -name hismethod.config -print`

hisbuildconfig: hisbuildconfig.in $(FIXSCRIPT)
	$(FIXSCRIPT) -i hisbuildconfig.in

##  Profiling.  The rules are a bit brute-force, but good enough.
profiled:		../libinnhist_p.a
	date >$@

../libinnhist_p.a:	$(SOURCES)
	rm -f $(OBJECTS)
	$(MAKE) libinnhist.a CFLAGS="$(CFLAGS) $(PROF)"
	mv libinnhist.a ../libinnhist_p.a
	$(RANLIB) ../libinnhist_p.a
	rm -f $(OBJECTS)

##  Low-level install actions.
$(D)$(PATHLIB)/libinnhist.$(EXTLIB): libinnhist.$(EXTLIB)
	$(LI_XPUB) $? $@

##  Dependencies.  Default list, below, is probably good enough.
depend:	$(SOURCES)
	$(MAKEDEPEND) $(CFLAGS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
his.o: his.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/portable/time.h ../include/libinn.h \
 ../include/macros.h ../include/inn/history.h ../include/inn/timer.h \
 hisinterface.h hismethods.h ../include/storage.h
hismethods.o: hismethods.c ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h hisinterface.h \
 hisv6/hisv6.h

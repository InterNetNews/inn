##  $Id$

include ../Makefile.global

CFLAGS		= $(GCFLAGS) $(SSLINC)

ALL		= nnrpd

SOURCES		= article.c group.c commands.c loadave.c misc.c newnews.c \
		  nnrpd.c perl.c perm.c post.c python.c sasl_config.c \
		  tls.c track.c

INCLUDES	= nnrpd.h post.h sasl_config.h tls.h

OBJECTS		= $(SOURCES:.c=.o)

INSTALLED	= $(D)$(PATHBIN)/nnrpd

all: $(ALL)

clean:
	rm -f *.o $(ALL) nnrpdp profiled
	rm -rf .libs

clobber: clean
	rm -f tags

tags ctags: $(SOURCES) $(INCLUDES)
	$(CTAGS) $(SOURCES) $(INCLUDES) ../lib/*.c ../include/*.h


##  Compilation rules.

NNRPDLIBS	= $(LIBSTORAGE) $(LIBINN) $(EXTSTORAGELIBS) $(PERLLIB) \
		  $(PYTHONLIB) $(SSLLIB) $(LIBS)

perl.o:		perl.c		; $(CC) $(CFLAGS) $(PERLINC) -c perl.c
python.o:	python.c	; $(CC) $(CFLAGS) $(PYTHONINC) -c python.c

nnrpd: $(OBJECTS) $(LIBSTORAGE) $(LIBINN)
	$(LIBTOOL) $(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(NNRPDLIBS)


##  Installation rules.  Installation commands set in Makefile.global.

install: $(INSTALLED)

$(D)$(PATHBIN)/nnrpd:	nnrpd	; $(LI_XPUB) $? $@

$(LIBINN):	; (cd ../lib ; $(MAKE))
$(LIBSTORAGE):	; (cd ../storage ; $(MAKE))


##  Profiling.  These rules have not been checked for a while and may need
##  some work.

profiled:	nnrpdp
	date >$@

nnrpdp:		$(SOURCES)
	rm -f $(OBJECTS)
	$(MAKE) nnrpd CFLAGS="$(CFLAGS) $(PROF)" LIBINN="../libinn_p.a"
	mv nnrpd nnrpdp
	rm -f $(OBJECTS)


##  Dependencies.  Default list, below, is probably good enough.

depend:	$(SOURCES)
	$(MAKEDEPEND) $(CFLAGS) $(PERLINC) $(PYTHONINC) $(TCLINC) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
article.o: article.c ../include/config.h ../include/clibrary.h nnrpd.h \
 ../include/libinn.h ../include/macros.h ../include/nntp.h \
 ../include/paths.h ../include/qio.h ../include/ov.h
group.o: group.c ../include/config.h ../include/clibrary.h nnrpd.h \
 ../include/libinn.h ../include/macros.h ../include/nntp.h \
 ../include/paths.h ../include/qio.h ../include/ov.h
commands.o: commands.c ../include/config.h ../include/clibrary.h \
 nnrpd.h ../include/libinn.h ../include/macros.h ../include/nntp.h \
 ../include/paths.h ../include/qio.h ../include/ov.h
misc.o: misc.c ../include/config.h ../include/clibrary.h nnrpd.h \
 ../include/libinn.h ../include/macros.h ../include/nntp.h \
 ../include/paths.h ../include/qio.h ../include/dbz.h
newnews.o: newnews.c ../include/config.h ../include/clibrary.h nnrpd.h \
 ../include/libinn.h ../include/macros.h ../include/nntp.h \
 ../include/paths.h ../include/qio.h ../include/ov.h
nnrpd.o: nnrpd.c ../include/config.h ../include/clibrary.h \
 ../include/libinn.h ../include/ov.h ../include/storage.h nnrpd.h \
 ../include/macros.h ../include/nntp.h ../include/paths.h \
 ../include/qio.h
perl.o: perl.c ../include/configdata.h ../include/config.h \
 ../include/clibrary.h ../include/paths.h post.h ../include/macros.h \
 ../include/nntp.h nnrpd.h ../include/libinn.h ../include/qio.h \
 ../include/ppport.h
perm.o: perm.c ../include/config.h ../include/clibrary.h \
 ../include/conffile.h nnrpd.h ../include/libinn.h ../include/macros.h \
 ../include/nntp.h ../include/paths.h ../include/qio.h
post.o: post.c ../include/config.h ../include/clibrary.h nnrpd.h \
 ../include/libinn.h ../include/macros.h ../include/nntp.h \
 ../include/paths.h ../include/qio.h ../include/ov.h post.h
loadave.o: loadave.c ../include/configdata.h ../include/config.h \
 ../include/clibrary.h nnrpd.h ../include/libinn.h ../include/macros.h \
 ../include/nntp.h ../include/paths.h ../include/qio.h
track.o: track.c ../include/config.h ../include/clibrary.h nnrpd.h \
 ../include/libinn.h ../include/macros.h ../include/nntp.h \
 ../include/paths.h ../include/qio.h
python.o: python.c ../include/configdata.h ../include/config.h \
 ../include/clibrary.h nnrpd.h ../include/libinn.h ../include/macros.h \
 ../include/nntp.h ../include/paths.h ../include/qio.h
tls.o: tls.c ../include/config.h
sasl_config.o: sasl_config.c ../include/config.h

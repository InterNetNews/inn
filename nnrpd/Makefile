##  $Id$

include ../Makefile.global

top		= ..
CFLAGS		= $(GCFLAGS) $(SSLINC)

ALL		= nnrpd

SOURCES		= article.c group.c commands.c loadave.c misc.c newnews.c \
		  nnrpd.c perl.c perm.c post.c python.c sasl_config.c \
		  tls.c track.c

INCLUDES	= nnrpd.h post.h sasl_config.h tls.h

OBJECTS		= $(SOURCES:.c=.o)

INSTALLED	= $(D)$(PATHBIN)/nnrpd

all: $(ALL)

warnings:
	$(MAKE) COPT='$(WARNINGS)' all

clean:
	rm -f *.o $(ALL) nnrpdp profiled
	rm -rf .libs

clobber distclean: clean
	rm -f tags

tags ctags: $(SOURCES) $(INCLUDES)
	$(CTAGS) $(SOURCES) $(INCLUDES) ../lib/*.c ../include/*.h


##  Compilation rules.

NNRPDLIBS	= $(LIBHIST) $(LIBSTORAGE) $(LIBINN) $(EXTSTORAGELIBS) \
		  $(PERLLIB) $(PYTHONLIB) $(SSLLIB) $(LIBS)

perl.o:		perl.c		; $(CC) $(CFLAGS) $(PERLINC) -c perl.c
python.o:	python.c	; $(CC) $(CFLAGS) $(PYTHONINC) -c python.c

nnrpd: $(OBJECTS) $(LIBHIST) $(LIBSTORAGE) $(LIBINN)
	$(LIBLD) $(LDFLAGS) -o $@ $(OBJECTS) $(NNRPDLIBS)


##  Installation rules.  Installation commands set in Makefile.global.

install: $(INSTALLED)

$(D)$(PATHBIN)/nnrpd:	nnrpd	; $(LI_XPUB) $? $@

$(LIBINN):	; (cd ../lib ; $(MAKE))
$(LIBSTORAGE):	; (cd ../storage ; $(MAKE))
$(LIBHIST):	; (cd ../history ; $(MAKE))


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
article.o: article.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h nnrpd.h ../include/portable/time.h \
 ../include/inn/qio.h ../include/libinn.h ../include/macros.h \
 ../include/nntp.h ../include/paths.h ../include/storage.h \
 ../include/inn/vector.h ../include/ov.h ../include/inn/history.h \
 tls.h
group.o: group.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h nnrpd.h ../include/portable/time.h \
 ../include/inn/qio.h ../include/libinn.h ../include/macros.h \
 ../include/nntp.h ../include/paths.h ../include/storage.h \
 ../include/inn/vector.h ../include/ov.h ../include/inn/history.h
commands.o: commands.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/portable/wait.h nnrpd.h \
 ../include/portable/time.h ../include/inn/qio.h ../include/libinn.h \
 ../include/macros.h ../include/nntp.h ../include/paths.h \
 ../include/storage.h ../include/inn/vector.h ../include/ov.h \
 ../include/inn/history.h
loadave.o: loadave.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h nnrpd.h ../include/portable/time.h \
 ../include/inn/qio.h ../include/libinn.h ../include/macros.h \
 ../include/nntp.h ../include/paths.h ../include/storage.h \
 ../include/inn/vector.h
misc.o: misc.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h nnrpd.h ../include/portable/time.h \
 ../include/inn/qio.h ../include/libinn.h ../include/macros.h \
 ../include/nntp.h ../include/paths.h ../include/storage.h \
 ../include/inn/vector.h tls.h sasl_config.h
newnews.o: newnews.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h nnrpd.h ../include/portable/time.h \
 ../include/inn/qio.h ../include/libinn.h ../include/macros.h \
 ../include/nntp.h ../include/paths.h ../include/storage.h \
 ../include/inn/vector.h ../include/ov.h ../include/inn/history.h
nnrpd.o: nnrpd.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/portable/wait.h ../include/libinn.h \
 ../include/ov.h ../include/storage.h ../include/inn/history.h nnrpd.h \
 ../include/portable/time.h ../include/inn/qio.h ../include/macros.h \
 ../include/nntp.h ../include/paths.h ../include/inn/vector.h tls.h \
 sasl_config.h
perl.o: perl.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/macros.h nnrpd.h \
 ../include/portable/time.h ../include/inn/qio.h ../include/libinn.h \
 ../include/nntp.h ../include/paths.h ../include/storage.h \
 ../include/inn/vector.h post.h ../include/innperl.h \
 ../include/ppport.h
perm.o: perm.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/portable/wait.h \
 ../include/conffile.h ../include/innperl.h ../include/ppport.h \
 nnrpd.h ../include/portable/time.h ../include/inn/qio.h \
 ../include/libinn.h ../include/macros.h ../include/nntp.h \
 ../include/paths.h ../include/storage.h ../include/inn/vector.h
post.o: post.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h nnrpd.h ../include/portable/time.h \
 ../include/inn/qio.h ../include/libinn.h ../include/macros.h \
 ../include/nntp.h ../include/paths.h ../include/storage.h \
 ../include/inn/vector.h ../include/ov.h ../include/inn/history.h \
 post.h
python.o: python.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h nnrpd.h ../include/portable/time.h \
 ../include/inn/qio.h ../include/libinn.h ../include/macros.h \
 ../include/nntp.h ../include/paths.h ../include/storage.h \
 ../include/inn/vector.h
sasl_config.o: sasl_config.c ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h nnrpd.h \
 ../include/portable/time.h ../include/inn/qio.h ../include/libinn.h \
 ../include/macros.h ../include/nntp.h ../include/paths.h \
 ../include/storage.h ../include/inn/vector.h sasl_config.h
tls.o: tls.c ../include/config.h ../include/inn/defines.h nnrpd.h \
 ../include/portable/time.h ../include/inn/qio.h ../include/libinn.h \
 ../include/macros.h ../include/nntp.h ../include/paths.h \
 ../include/storage.h ../include/inn/vector.h tls.h
track.o: track.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h nnrpd.h ../include/portable/time.h \
 ../include/inn/qio.h ../include/libinn.h ../include/macros.h \
 ../include/nntp.h ../include/paths.h ../include/storage.h \
 ../include/inn/vector.h

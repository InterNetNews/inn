##  $Id$

include ../Makefile.global

CFLAGS        = $(GCFLAGS)

ALL           = ckpasswd domain ident pwcheck radius auth_smb

LIBSMB	      = smbval/smbvalid.a

SOURCES       = ckpasswd.c domain.c ident.c pwcheck.c radius.c auth_smb.c

INSTALLED     = $(D)$(PATHAUTHPASSWD)/ckpasswd \
		$(D)$(PATHAUTHPASSWD)/pwcheck \
		$(D)$(PATHAUTHPASSWD)/radius \
		$(D)$(PATHAUTHPASSWD)/auth_smb \
		$(D)$(PATHAUTHRESOLV)/domain \
		$(D)$(PATHAUTHRESOLV)/ident

all: $(LIBSMB) $(ALL)

clobber clean:
	rm -f *.o $(ALL)
	rm -rf .libs
	cd smbval && $(MAKE) clean

tags ctags: $(SOURCES)
	$(CTAGS) $(SOURCES) ../lib/*.c ../include/*.h


##  Compilation rules.

LINK		= $(LIBTOOL) $(CC) $(LDFLAGS) -o $@
NEWSLIBS        = $(LIBINN) $(LIBS)

ckpasswd:	ckpasswd.o		; $(LINK) ckpasswd.o $(DBMLIB) $(LIBS)
domain:		domain.o      $(LIBINN) ; $(LINK) domain.o   $(NEWSLIBS)
ident:		ident.o			; $(LINK) ident.o    $(LIBS)
pwcheck:	pwcheck.o		; $(LINK) pwcheck.o  $(LIBS)
radius:		radius.o      $(LIBINN)	; $(LINK) radius.o   $(NEWSLIBS)
auth_smb:	auth_smb.o    $(LIBSMB) ; $(LINK) auth_smb.o $(LIBSMB) $(LIBS)

$(LIBINN):	; (cd ../lib ; $(MAKE))
$(LIBSTORAGE):	; (cd ../storage ; $(MAKE))
$(LIBSMB):	; (cd smbval ; $(MAKE))

##  Installation rules.  Installation commands set in Makefile.global.

install: $(INSTALLED)

$(D)$(PATHAUTHPASSWD)/ckpasswd:		ckpasswd	; $(LI_XPUB) $? $@
$(D)$(PATHAUTHPASSWD)/pwcheck:		pwcheck		; $(LI_XPUB) $? $@
$(D)$(PATHAUTHPASSWD)/radius:		radius		; $(LI_XPUB) $? $@
$(D)$(PATHAUTHRESOLV)/domain:		domain		; $(LI_XPUB) $? $@
$(D)$(PATHAUTHRESOLV)/ident:		ident		; $(LI_XPUB) $? $@


##  Dependencies.  Default list, below, is probably good enough.

depend: Makefile $(SOURCES)
	$(MAKEDEPEND) $(CFLAGS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
ckpasswd.o: ckpasswd.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h
domain.o: domain.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h ../include/macros.h
ident.o: ident.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h ../include/macros.h
pwcheck.o: pwcheck.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h
radius.o: radius.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/md5.h ../include/macros.h \
 ../include/libinn.h ../include/nntp.h ../include/paths.h
auth_smb.o: auth_smb.c ../include/config.h ../include/clibrary.h \
 smbval/valid.h

##  $Id$

include ../Makefile.global

CFLAGS        = $(GCFLAGS)

ALL           = ckpasswd domain ident pwcheck radius auth_smb

LIBSMB	      = smbval/smbvalid.a

LIBAUTH       = libauth.o

SOURCES       = ckpasswd.c domain.c ident.c libauth.c pwcheck.c radius.c \
		auth_smb.c

INSTALLED     = $(D)$(PATHAUTHPASSWD)/ckpasswd \
		$(D)$(PATHAUTHPASSWD)/pwcheck \
		$(D)$(PATHAUTHPASSWD)/radius \
		$(D)$(PATHAUTHPASSWD)/auth_smb \
		$(D)$(PATHAUTHRESOLV)/domain \
		$(D)$(PATHAUTHRESOLV)/ident

all: $(LIBSMB) $(LIBAUTH) $(ALL)

warnings:
	$(MAKE) COPT='$(WARNINGS)' all

clobber clean:
	rm -f *.o $(ALL)
	rm -rf .libs
	cd smbval && $(MAKE) clean

tags ctags: $(SOURCES)
	$(CTAGS) $(SOURCES) ../lib/*.c ../include/*.h


##  Compilation rules.

LINK		= $(LIBLD) $(LDFLAGS) -o $@
NEWSLIBS        = $(LIBINN) $(LIBS)
AUTHLIBS	= $(LIBAUTH) $(LIBS)
AUTH_INN	= $(LIBINN) $(LIBAUTH)

CK_LIBS		= $(DBMLIB) $(AUTHLIBS)
RAD_LIBS	= $(AUTH_INN) $(LIBS)

ckpasswd:	ckpasswd.o  $(LIBAUTH)	; $(LINK) ckpasswd.o $(CK_LIBS)
domain:		domain.o    $(LIBINN)	; $(LINK) domain.o   $(NEWSLIBS)
ident:		ident.o     $(LIBAUTH)	; $(LINK) ident.o    $(AUTHLIBS)
pwcheck:	pwcheck.o   $(LIBAUTH)	; $(LINK) pwcheck.o  $(AUTHLIBS)
radius:		radius.o    $(AUTH_INN)	; $(LINK) radius.o   $(RAD_LIBS)
auth_smb:	auth_smb.o  $(LIBSMB)	; $(LINK) auth_smb.o $(LIBSMB) $(LIBS)

$(LIBINN):	; (cd ../lib ; $(MAKE))
$(LIBSTORAGE):	; (cd ../storage ; $(MAKE))
$(LIBSMB):	; (cd smbval ; $(MAKE))
$(LIBAUTH): 	libauth.h libauth.c


##  Installation rules.  Installation commands set in Makefile.global.

install: $(INSTALLED)

$(D)$(PATHAUTHPASSWD)/ckpasswd:		ckpasswd	; $(LI_XPUB) $? $@
$(D)$(PATHAUTHPASSWD)/pwcheck:		pwcheck		; $(LI_XPUB) $? $@
$(D)$(PATHAUTHPASSWD)/radius:		radius		; $(LI_XPUB) $? $@
$(D)$(PATHAUTHPASSWD)/auth_smb:		auth_smb	; $(LI_XPUB) $? $@
$(D)$(PATHAUTHRESOLV)/domain:		domain		; $(LI_XPUB) $? $@
$(D)$(PATHAUTHRESOLV)/ident:		ident		; $(LI_XPUB) $? $@


##  Dependencies.  Default list, below, is probably good enough.

depend: Makefile $(SOURCES)
	$(MAKEDEPEND) $(CFLAGS) $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
ckpasswd.o: ckpasswd.c libauth.h ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h
domain.o: domain.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h ../include/macros.h
ident.o: ident.c libauth.h ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h ../include/libinn.h \
 ../include/macros.h
libauth.o: libauth.c libauth.h ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h
pwcheck.o: pwcheck.c libauth.h ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h
radius.o: radius.c libauth.h ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h ../include/inn/md5.h \
 ../include/macros.h ../include/libinn.h ../include/nntp.h \
 ../include/paths.h
auth_smb.o: auth_smb.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h smbval/valid.h

##  $Id$

include ../Makefile.global

top	      = ..
CFLAGS        = $(GCFLAGS)

ALL           = auth_smb ckpasswd domain ident pwcheck radius

LIBSMB	      = smbval/smbvalid.a

LIBAUTH       = libauth.o

SOURCES       = auth_smb.c ckpasswd.c domain.c ident.c libauth.c pwcheck.c \
		radius.c

INSTALLED     = $(D)$(PATHAUTHPASSWD)/auth_smb \
		$(D)$(PATHAUTHPASSWD)/ckpasswd \
		$(D)$(PATHAUTHPASSWD)/pwcheck \
		$(D)$(PATHAUTHPASSWD)/radius \
		$(D)$(PATHAUTHRESOLV)/domain \
		$(D)$(PATHAUTHRESOLV)/ident

all: $(LIBSMB) $(LIBAUTH) $(ALL)

warnings:
	$(MAKE) COPT='$(WARNINGS)' all

clobber clean distclean:
	rm -f *.o $(ALL)
	rm -rf .libs
	cd smbval && $(MAKE) clean

tags ctags: $(SOURCES)
	$(CTAGS) $(SOURCES) ../lib/*.c ../include/*.h


##  Compilation rules.

LINK		= $(LIBLD) $(LDFLAGS) -o $@
NEWSLIBS        = $(LIBINN) $(LIBS)
AUTHLIBS	= $(LIBAUTH) $(LIBINN) $(LIBS)

CK_LIBS		= $(PAMLIB) $(DBMLIB) $(AUTHLIBS)
SMB_LIBS	= $(LIBSMB) $(AUTHLIBS)

auth_smb:	auth_smb.o  $(LIBSMB)	; $(LINK) auth_smb.o $(SMB_LIBS)
ckpasswd:	ckpasswd.o  $(LIBAUTH)	; $(LINK) ckpasswd.o $(CK_LIBS)
domain:		domain.o    $(LIBINN)	; $(LINK) domain.o   $(NEWSLIBS)
ident:		ident.o     $(LIBAUTH)	; $(LINK) ident.o    $(AUTHLIBS)
pwcheck:	pwcheck.o   $(LIBAUTH)	; $(LINK) pwcheck.o  $(AUTHLIBS)
radius:		radius.o    $(AUTH_INN)	; $(LINK) radius.o   $(AUTHLIBS)

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
auth_smb.o: auth_smb.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h smbval/valid.h
ckpasswd.o: ckpasswd.c libauth.h ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h \
 ../include/portable/socket.h
domain.o: domain.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h ../include/macros.h
ident.o: ident.c libauth.h ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h \
 ../include/portable/socket.h ../include/libinn.h ../include/macros.h
libauth.o: libauth.c libauth.h ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h \
 ../include/portable/socket.h
pwcheck.o: pwcheck.c libauth.h ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h \
 ../include/portable/socket.h
radius.o: radius.c libauth.h ../include/config.h \
 ../include/inn/defines.h ../include/clibrary.h \
 ../include/portable/socket.h ../include/portable/time.h \
 ../include/inn/md5.h ../include/libinn.h ../include/macros.h \
 ../include/nntp.h ../include/paths.h ../include/conffile.h

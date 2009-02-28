##  $Id$

include ../Makefile.global

top	      = ..
CFLAGS        = $(GCFLAGS)

ALL           = ckpasswd domain ident radius $(KRB5_AUTH)

LIBSMB	      = smbval/smbvalid.a

LIBAUTH       = libauth.o

SOURCES       = auth_krb5.c ckpasswd.c domain.c ident.c libauth.c radius.c

all: $(ALL)

warnings:
	$(MAKE) COPT='$(WARNINGS)' all

install: all
	if [ x"$(KRB5_AUTH)" != x ] ; then \
	    $(LI_XPUB) auth_krb5 $(D)$(PATHAUTHPASSWD)/auth_krb5 ; \
	fi
	for F in ckpasswd radius ; do \
	    $(LI_XPUB) $$F $D$(PATHAUTHPASSWD)/$$F ; \
	done
	for F in domain ident ; do \
	    $(LI_XPUB) $$F $D$(PATHAUTHRESOLV)/$$F ; \
	done

bootstrap:

clean clobber distclean maintclean:
	rm -f *.o $(ALL)
	rm -rf .libs
	cd smbval && $(MAKE) clean

tags ctags: $(SOURCES)
	$(CTAGS) $(SOURCES) ../lib/*.c ../include/*.h

profiled:
	$(MAKEPROFILING) all


##  Compilation rules.

LINK		= $(LIBLD) $(LDFLAGS) -o $@
AUTH_LIBS       = $(LIBAUTH) $(LIBINN) $(LIBS)
CK_LIBS		= $(CRYPT_LIBS) $(SHADOW_LIBS) $(PAM_LIBS) $(DBM_LIBS)

auth_krb5: auth_krb5.o $(LIBAUTH) $(LIBINN)
	$(LINK) auth_krb5.o $(KRB5_LDFLAGS) $(KRB5_LIBS) $(AUTH_LIBS)

ckpasswd: ckpasswd.o $(LIBAUTH) $(LIBINN)
	$(LINK) ckpasswd.o $(CK_LIBS) $(AUTH_LIBS)

domain: domain.o $(LIBAUTH) $(LIBINN)
	$(LINK) domain.o $(AUTH_LIBS)

ident: ident.o $(LIBAUTH) $(LIBINN)
	$(LINK) ident.o $(AUTH_LIBS)

radius: radius.o $(LIBAUTH) $(LIBINN)
	$(LINK) radius.o $(AUTH_LIBS)

auth_krb5.o: auth_krb5.c
	$(CC) $(CFLAGS) $(KRB5_CPPFLAGS) -c auth_krb5.c

ckpasswd.o: ckpasswd.c
	$(CC) $(CFLAGS) $(DBM_CPPFLAGS) -c ckpasswd.c

$(LIBINN):	; (cd ../lib ; $(MAKE))
$(LIBSTORAGE):	; (cd ../storage ; $(MAKE))
$(LIBAUTH): 	libauth.h libauth.c


##  Dependencies.  Default list, below, is probably good enough.

depend: Makefile $(SOURCES)
	$(MAKEDEPEND) '$(CFLAGS)' $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
auth_krb5.o: auth_krb5.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h libauth.h ../include/portable/socket.h \
  ../include/config.h ../include/portable/getaddrinfo.h \
  ../include/portable/getnameinfo.h ../include/inn/defines.h \
  ../include/inn/messages.h ../include/inn/libinn.h
ckpasswd.o: ckpasswd.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/inn/messages.h ../include/inn/defines.h \
  ../include/inn/qio.h ../include/inn/vector.h ../include/inn/libinn.h \
  libauth.h ../include/portable/socket.h ../include/config.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h
domain.o: domain.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/inn/messages.h ../include/inn/defines.h \
  ../include/inn/libinn.h libauth.h ../include/portable/socket.h \
  ../include/config.h ../include/portable/getaddrinfo.h \
  ../include/portable/getnameinfo.h
ident.o: ident.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/inn/messages.h ../include/inn/defines.h \
  ../include/inn/network.h ../include/inn/libinn.h libauth.h \
  ../include/portable/socket.h ../include/config.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h
libauth.o: libauth.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/portable/socket.h ../include/config.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/inn/messages.h ../include/inn/defines.h libauth.h \
  ../include/inn/libinn.h
radius.o: radius.c ../include/config.h ../include/inn/defines.h \
  ../include/inn/system.h ../include/inn/options.h ../include/clibrary.h \
  ../include/config.h ../include/portable/time.h ../include/config.h \
  ../include/inn/innconf.h ../include/inn/defines.h ../include/inn/md5.h \
  ../include/inn/messages.h ../include/inn/libinn.h ../include/nntp.h \
  ../include/inn/nntp.h ../include/inn/paths.h ../include/conffile.h \
  libauth.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h

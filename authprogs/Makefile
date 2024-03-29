include ../Makefile.global

top	      = ..
CFLAGS        = $(GCFLAGS)

ALL           = ckpasswd domain ident radius $(KRB5_AUTH)

LIBINNAUTH    = libinnauth.o

SOURCES       = auth_krb5.c ckpasswd.c domain.c ident.c libinnauth.c radius.c

all: $(ALL)

warnings:
	$(MAKE) COPT="$(COPT) $(WARNINGS)" all

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

profiled:
	$(MAKEPROFILING) all


##  Compilation rules.

LINK		= $(LIBLD) $(LDFLAGS) -o $@
AUTH_LIBS       = $(LIBINNAUTH) $(LIBINN) $(LIBS)
CK_LIBS		= $(CRYPT_LIBS) $(SHADOW_LIBS) $(PAM_LIBS) $(DBM_LIBS)

auth_krb5: auth_krb5.o $(LIBINNAUTH) $(LIBINN)
	$(LINK) auth_krb5.o $(KRB5_LDFLAGS) $(KRB5_LIBS) $(AUTH_LIBS)

ckpasswd: ckpasswd.o $(LIBINNAUTH) $(LIBINN)
	$(LINK) ckpasswd.o $(CK_LIBS) $(AUTH_LIBS)

domain: domain.o $(LIBINNAUTH) $(LIBINN)
	$(LINK) domain.o $(AUTH_LIBS)

ident: ident.o $(LIBINNAUTH) $(LIBINN)
	$(LINK) ident.o $(AUTH_LIBS)

radius: radius.o $(LIBINNAUTH) $(LIBINN)
	$(LINK) radius.o $(AUTH_LIBS)

auth_krb5.o: auth_krb5.c
	$(CC) $(CFLAGS) $(KRB5_CPPFLAGS) -c auth_krb5.c

ckpasswd.o: ckpasswd.c
	$(CC) $(CFLAGS) $(DBM_CPPFLAGS) -c ckpasswd.c

$(LIBINN):	; (cd ../lib ; $(MAKE))
$(LIBINNAUTH): 	libinnauth.h libinnauth.c


##  Dependencies.  Default list, below, is probably good enough.

depend: Makefile $(SOURCES)
	$(MAKEDEPEND) '$(CFLAGS)' $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
auth_krb5.o: auth_krb5.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h libinnauth.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/macros.h \
  ../include/inn/portable-stdbool.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/messages.h
ckpasswd.o: ckpasswd.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/libinn.h \
  ../include/inn/concat.h ../include/inn/macros.h \
  ../include/inn/portable-stdbool.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/qio.h ../include/inn/vector.h \
  libinnauth.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h
domain.o: domain.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/libinn.h \
  ../include/inn/concat.h ../include/inn/macros.h \
  ../include/inn/portable-stdbool.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/messages.h libinnauth.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h
ident.o: ident.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/inn/libinn.h \
  ../include/inn/concat.h ../include/inn/macros.h \
  ../include/inn/portable-stdbool.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/messages.h ../include/inn/network.h \
  ../include/inn/portable-socket.h ../include/inn/portable-getaddrinfo.h \
  ../include/inn/portable-getnameinfo.h libinnauth.h \
  ../include/portable/socket.h
libinnauth.o: libinnauth.c ../include/portable/system.h \
  ../include/config.h ../include/inn/macros.h \
  ../include/inn/portable-macros.h ../include/inn/options.h \
  ../include/inn/system.h ../include/portable/stdbool.h \
  ../include/portable/macros.h ../include/portable/stdbool.h \
  ../include/portable/socket.h ../include/portable/getaddrinfo.h \
  ../include/portable/getnameinfo.h ../include/inn/libinn.h \
  ../include/inn/concat.h ../include/inn/macros.h \
  ../include/inn/portable-stdbool.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h \
  ../include/inn/messages.h libinnauth.h
radius.o: radius.c ../include/portable/system.h ../include/config.h \
  ../include/inn/macros.h ../include/inn/portable-macros.h \
  ../include/inn/options.h ../include/inn/system.h \
  ../include/portable/stdbool.h ../include/portable/macros.h \
  ../include/portable/stdbool.h ../include/conffile.h \
  ../include/portable/macros.h ../include/inn/innconf.h \
  ../include/inn/macros.h ../include/inn/portable-stdbool.h \
  ../include/inn/libinn.h ../include/inn/concat.h ../include/inn/xmalloc.h \
  ../include/inn/system.h ../include/inn/xwrite.h ../include/inn/md5.h \
  ../include/inn/messages.h ../include/inn/nntp.h ../include/inn/paths.h \
  libinnauth.h ../include/portable/socket.h \
  ../include/portable/getaddrinfo.h ../include/portable/getnameinfo.h

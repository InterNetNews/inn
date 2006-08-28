##  $Id$

include ../Makefile.global

top	      = ..
CFLAGS        = $(GCFLAGS)

ALL           = auth_smb ckpasswd domain ident radius $(KRB5_AUTH)

LIBSMB	      = smbval/smbvalid.a

LIBAUTH       = libauth.o

SOURCES       = auth_krb5.c auth_smb.c ckpasswd.c domain.c ident.c libauth.c \
		radius.c

all: $(ALL)

warnings:
	$(MAKE) COPT='$(WARNINGS)' all

install: all
	if [ x"$(KRB5_AUTH)" != x ] ; then \
	    $(LI_XPUB) auth_krb5 $(D)$(PATHAUTHPASSWD)/auth_krb5 ; \
	fi
	for F in auth_smb ckpasswd radius ; do \
	    $(LI_XPUB) $$F $D$(PATHAUTHPASSWD)/$$F ; \
	done
	for F in domain ident ; do \
	    $(LI_XPUB) $$F $D$(PATHAUTHRESOLV)/$$F ; \
	done

clobber clean distclean:
	rm -f *.o $(ALL)
	rm -rf .libs
	cd smbval && $(MAKE) clean

tags ctags: $(SOURCES)
	$(CTAGS) $(SOURCES) ../lib/*.c ../include/*.h

profiled:
	$(MAKEPROFILING) all


##  Compilation rules.

LINK		= $(LIBLD) $(LDFLAGS) -o $@
CKLIBS		= $(CRYPTLIB) $(SHADOWLIB) $(PAMLIB) $(DBMLIB)

auth_krb5: auth_krb5.o $(LIBAUTH) $(LIBINN)
	$(LINK) auth_krb5.o $(LIBAUTH) $(KRB5LIB) $(LIBINN) $(LIBS)

auth_smb: auth_smb.o $(LIBSMB) $(LIBAUTH) $(LIBINN)
	$(LINK) auth_smb.o $(LIBSMB) $(LIBAUTH) $(LIBINN) $(LIBS)

ckpasswd: ckpasswd.o $(LIBAUTH) $(LIBINN)
	$(LINK) ckpasswd.o $(LIBAUTH) $(CKLIBS) $(LIBINN) $(LIBS)

domain: domain.o $(LIBAUTH) $(LIBINN)
	$(LINK) domain.o $(LIBAUTH) $(LIBINN) $(LIBS)

ident: ident.o $(LIBAUTH) $(LIBINN)
	$(LINK) ident.o $(LIBAUTH) $(LIBINN) $(LIBS)

radius: radius.o $(LIBAUTH) $(LIBINN)
	$(LINK) radius.o $(LIBAUTH) $(LIBINN) $(LIBS)

auth_krb5.o: auth_krb5.c
	$(CC) $(CFLAGS) $(KRB5INC) -c auth_krb5.c

ckpasswd.o: ckpasswd.c
	$(CC) $(CFLAGS) $(DBMINC) -c ckpasswd.c

$(LIBINN):	; (cd ../lib ; $(MAKE))
$(LIBSMB):	; (cd smbval ; $(MAKE))
$(LIBAUTH): 	libauth.h libauth.c


##  Dependencies.  Default list, below, is probably good enough.

depend: Makefile $(SOURCES)
	$(MAKEDEPEND) '$(CFLAGS)' $(SOURCES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
auth_krb5.o: auth_krb5.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h libauth.h ../include/portable/socket.h \
 ../include/inn/messages.h ../include/libinn.h
auth_smb.o: auth_smb.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/messages.h libauth.h \
 ../include/portable/socket.h smbval/valid.h
ckpasswd.o: ckpasswd.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/messages.h ../include/inn/qio.h \
 ../include/inn/vector.h ../include/libinn.h libauth.h \
 ../include/portable/socket.h
domain.o: domain.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/messages.h ../include/libinn.h \
 libauth.h ../include/portable/socket.h
ident.o: ident.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/inn/messages.h ../include/libinn.h \
 libauth.h ../include/portable/socket.h
libauth.o: libauth.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/libinn.h libauth.h \
 ../include/portable/socket.h ../include/inn/messages.h
radius.o: radius.c ../include/config.h ../include/inn/defines.h \
 ../include/clibrary.h ../include/portable/time.h \
 ../include/inn/innconf.h ../include/inn/md5.h \
 ../include/inn/messages.h ../include/libinn.h ../include/nntp.h \
 ../include/paths.h ../include/conffile.h libauth.h \
 ../include/portable/socket.h

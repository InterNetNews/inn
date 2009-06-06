dnl ssl.m4 -- Find the path to the SSL libraries.
dnl $Id$
dnl
dnl This file provides INN_LIB_OPENSSL, which defines the --with-openssl
dnl command-line option and probes for the location of OpenSSL if that
dnl option is used without an optional path.  It looks by default in $prefix,
dnl /usr/local, and /usr.  However it finds OpenSSL, it also makes sure that
dnl it links correctly and checks to see if RSAref is used.  It exports
dnl SSL_LDFLAGS, SSL_CPPFLAGS, and SSL_LIBS.
dnl
dnl Support is only present for OpenSSL at the current time, although it would
dnl be nice to add support for GnuTLS at some point.  This will likely also
dnl require source changes and possibly a licensing change.

AC_DEFUN([INN_LIB_OPENSSL],
[SSL_LDFLAGS=
SSL_CPPFLAGS=
SSL_LIBS=
AC_ARG_WITH([openssl],
    [AS_HELP_STRING([--with-openssl@<:@=PATH@:>@],
        [Enable OpenSSL (for NNTP over TLS/SSL support)])],
    SSL_DIR=$with_openssl,
    SSL_DIR=no)
AC_MSG_CHECKING([if OpenSSL is desired])
if test x"$SSL_DIR" = xno ; then
    AC_MSG_RESULT([no])
else
    AC_MSG_RESULT([yes])
    AC_MSG_CHECKING([for OpenSSL location])
    if test x"$SSL_DIR" = xyes ; then
        for dir in $prefix /usr/local /usr ; do
            if test -f "$dir/include/openssl/ssl.h" ; then
                SSL_DIR=$dir
                break
            fi
        done
    fi
    if test x"$SSL_DIR" = xyes ; then
        AC_MSG_ERROR([cannot find OpenSSL])
    else
        AC_MSG_RESULT([$SSL_DIR])
        if test x"$SSL_DIR" != x/usr ; then
            SSL_CPPFLAGS="-I$SSL_DIR/include"
            SSL_LDFLAGS="-L$SSL_DIR/lib"
        fi
        inn_save_LDFLAGS=$LDFLAGS
        LDFLAGS="$SSL_LDFLAGS $LDFLAGS"
        AC_CHECK_LIB([rsaref], [RSAPublicEncrypt],
            [AC_CHECK_LIB([RSAglue], [RSAPublicEncrypt],
                [SSL_LIBS="-lRSAglue -lrsaref"], , [-lrsaref])])
        AC_CHECK_LIB([crypto], [BIO_new],
            [AC_CHECK_LIB([dl], [DSO_load],
                [SSL_LIBS="-lcrypto -ldl $SSL_LIBS"],
                [SSL_LIBS="-lcrypto $SSL_LIBS"], [-lcrypto $SSL_LIBS])],
            [AC_MSG_ERROR(cannot link with OpenSSL)], [$SSL_LIBS])
        AC_CHECK_LIB([ssl], [SSL_library_init],
            [SSL_LIBS="-lssl $SSL_LIBS"],
            [AC_MSG_ERROR(cannot link with OpenSSL)], [$SSL_LIBS])
        LDFLAGS=$inn_save_LDFLAGS
        AC_DEFINE([HAVE_SSL], 1, [Define if OpenSSL is available.])
    fi
fi
AC_SUBST([SSL_CPPFLAGS])
AC_SUBST([SSL_LDFLAGS])
AC_SUBST([SSL_LIBS])])

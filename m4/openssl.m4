dnl Find the compiler and linker flags for OpenSSL.
dnl $Id$
dnl
dnl Finds the compiler and linker flags for linking with both the OpenSSL SSL
dnl library and its crypto library.  Provides the --with-openssl,
dnl --with-openssl-lib, and --with-openssl-include configure options to
dnl specify non-standard paths to the OpenSSL libraries.
dnl
dnl Provides the macro INN_LIB_OPENSSL and sets the substitution variables
dnl OPENSSL_CPPFLAGS, OPENSSL_LDFLAGS, OPENSSL_LIBS, CRYPTO_CPPFLAGS,
dnl CRYPTO_LDFLAGS, and CRYPTO_LIBS.  Also provides INN_LIB_OPENSSL_SWITCH and
dnl INN_LIB_CRYPTO_SWITCH to set CPPFLAGS, LDFLAGS, and LIBS to include the
dnl SSL or crypto libraries, saving the current values first, and
dnl INN_LIB_OPENSSL_RESTORE and INN_LIB_CRYPTO_RESTORE to restore those
dnl settings to before the last INN_LIB_OPENSSL_SWITCH or
dnl INN_LIB_CRYPTO_SWITCH.
dnl
dnl Depends on the lib-helper.m4 framework.
dnl
dnl The canonical version of this file is maintained in the rra-c-util
dnl package, available at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
dnl
dnl Written by Russ Allbery <rra@stanford.edu>
dnl Copyright 2010, 2013
dnl     The Board of Trustees of the Leland Stanford Junior University
dnl
dnl This file is free software; the authors give unlimited permission to copy
dnl and/or distribute it, with or without modifications, as long as this
dnl notice is preserved.

dnl Save the current CPPFLAGS, LDFLAGS, and LIBS settings and switch to
dnl versions that include the SSL or crypto flags.  Used as a wrapper, with
dnl INN_LIB_OPENSSL_RESTORE or INN_LIB_CRYPTO_RESTORE, around tests.
AC_DEFUN([INN_LIB_OPENSSL_SWITCH], [INN_LIB_HELPER_SWITCH([OPENSSL])])
AC_DEFUN([INN_LIB_CRYPTO_SWITCH], [INN_LIB_HELPER_SWITCH([CRYPTO])])

dnl Restore CPPFLAGS, LDFLAGS, and LIBS to their previous values (before
dnl INN_LIB_OPENSSL_SWITCH or INN_LIB_CRYPTO_SWITCH were called).
AC_DEFUN([INN_LIB_OPENSSL_RESTORE], [INN_LIB_HELPER_RESTORE([OPENSSL])])
AC_DEFUN([INN_LIB_CRYPTO_RESTORE], [INN_LIB_HELPER_RESTORE([CRYPTO])])

dnl Checks if the OpenSSL and crypto libraries are present.  The single
dnl argument, if "true", says to fail if the OpenSSL SSL library could not be
dnl found.
AC_DEFUN([_INN_LIB_OPENSSL_INTERNAL],
[INN_LIB_HELPER_PATHS([OPENSSL])
 CRYPTO_CPPFLAGS="$OPENSSL_CPPFLAGS"
 CRYPTO_LDFLAGS="$OPENSSL_LDFLAGS"
 CRYPTO_LIBS=
 AC_SUBST([CRYPTO_CPPFLAGS])
 AC_SUBST([CRYPTO_LDFLAGS])
 AC_SUBST([CRYPTO_LIBS])
 INN_LIB_OPENSSL_SWITCH
 AC_CHECK_LIB([crypto], [AES_cbc_encrypt], [CRYPTO_LIBS=-lcrypto],
    [AS_IF([test x"$1" = xtrue],
        [AC_MSG_ERROR([cannot find usable OpenSSL crypto library])])])
 AS_IF([test x"$inn_reduced_depends" = xtrue],
    [AC_CHECK_LIB([ssl], [SSL_library_init],
        [OPENSSL_LIBS="-lssl $CRYPTO_LIBS"],
        [AS_IF([test x"$1" = xtrue],
            [AC_MSG_ERROR([cannot find usable OpenSSL library])])],
        [$CRYPTO_LIBS])],
    [AC_CHECK_LIB([ssl], [SSL_library_init], [OPENSSL_LIBS=-lssl],
        [AS_IF([test x"$1" = xtrue],
            [AC_MSG_ERROR([cannot find usable OpenSSL library])])])])
 INN_LIB_OPENSSL_RESTORE])

dnl The main macro for packages with mandatory OpenSSL support.
AC_DEFUN([INN_LIB_OPENSSL],
[INN_LIB_HELPER_VAR_INIT([OPENSSL])
 INN_LIB_HELPER_WITH([openssl], [OpenSSL], [OPENSSL])
 _INN_LIB_OPENSSL_INTERNAL([true])
 AC_DEFINE([HAVE_SSL], 1, [Define if libssl is available.])])

dnl The main macro for packages with optional OpenSSL support.
AC_DEFUN([INN_LIB_OPENSSL_OPTIONAL],
[INN_LIB_HELPER_VAR_INIT([OPENSSL])
 INN_LIB_HELPER_WITH_OPTIONAL([openssl], [OpenSSL], [OPENSSL])
 AS_IF([test x"$inn_use_OPENSSL" != xfalse],
    [AS_IF([test x"$inn_use_OPENSSL" = xtrue],
        [_INN_LIB_OPENSSL_INTERNAL([true])],
        [_INN_LIB_OPENSSL_INTERNAL([false])])])
 AS_IF([test x"$OPENSSL_LIBS" != x],
    [inn_use_OPENSSL=true
     AC_DEFINE([HAVE_SSL], 1, [Define if libssl is available.])])])

dnl Find the compiler and linker flags for OpenSSL.
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
dnl INN_LIB_CRYPTO_SWITCH.  Defines HAVE_OPENSSL and sets inn_use_OPENSSL to
dnl true if the library is found.
dnl
dnl Provides the INN_LIB_OPENSSL_OPTIONAL macro, which should be used if
dnl OpenSSL support is optional.  This macro will still set the substitution
dnl variables and shell variables described above, but they'll be empty unless
dnl OpenSSL libraries are detected.  HAVE_OPENSSL will be defined only if the
dnl library is found.
dnl
dnl Depends on INN_ENABLE_REDUCED_DEPENDS and the lib-helper.m4 framework.
dnl
dnl The canonical version of this file is maintained in the rra-c-util
dnl package, available at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
dnl
dnl Written by Russ Allbery <eagle@eyrie.org>
dnl Copyright 2016, 2018 Russ Allbery <eagle@eyrie.org>
dnl Copyright 2010, 2013
dnl     The Board of Trustees of the Leland Stanford Junior University
dnl
dnl This file is free software; the authors give unlimited permission to copy
dnl and/or distribute it, with or without modifications, as long as this
dnl notice is preserved.
dnl
dnl SPDX-License-Identifier: FSFULLR

dnl Save the current CPPFLAGS, LDFLAGS, and LIBS settings and switch to
dnl versions that include the SSL or crypto flags.  Used as a wrapper, with
dnl INN_LIB_OPENSSL_RESTORE or INN_LIB_CRYPTO_RESTORE, around tests.
AC_DEFUN([INN_LIB_OPENSSL_SWITCH], [INN_LIB_HELPER_SWITCH([OPENSSL])])
AC_DEFUN([INN_LIB_CRYPTO_SWITCH], [INN_LIB_HELPER_SWITCH([CRYPTO])])

dnl Restore CPPFLAGS, LDFLAGS, and LIBS to their previous values (before
dnl INN_LIB_OPENSSL_SWITCH or INN_LIB_CRYPTO_SWITCH were called).
AC_DEFUN([INN_LIB_OPENSSL_RESTORE], [INN_LIB_HELPER_RESTORE([OPENSSL])])
AC_DEFUN([INN_LIB_CRYPTO_RESTORE], [INN_LIB_HELPER_RESTORE([CRYPTO])])

dnl Check for the OpenSSL and crypto libraries and assemble OPENSSL_LIBS and
dnl CRYPTO_LIBS.  Helper function for _INN_LIB_OPENSSL_INTERNAL.  Must be
dnl called with INN_LIB_OPENSSL_SWITCH enabled.
AC_DEFUN([_INN_LIB_OPENSSL_INTERNAL_LIBS],
[inn_openssl_extra=
 LIBS=
 AS_IF([test x"$inn_reduced_depends" != xtrue],
    [AC_SEARCH_LIBS([dlopen], [dl])])
 inn_openssl_extra="$LIBS"
 LIBS="$inn_OPENSSL_save_LIBS"
 AC_CHECK_LIB([crypto], [AES_cbc_encrypt],
    [CRYPTO_LIBS="-lcrypto $inn_openssl_extra"],
    [AS_IF([test x"$1" = xtrue],
        [AC_MSG_ERROR([cannot find usable OpenSSL crypto library])])],
    [$inn_openssl_extra])
 AS_IF([test x"$inn_reduced_depends" = xtrue],
    [AC_CHECK_LIB([ssl], [SSL_accept], [OPENSSL_LIBS=-lssl],
        [AS_IF([test x"$1" = xtrue],
            [AC_MSG_ERROR([cannot find usable OpenSSL library])])])],
    [AC_CHECK_LIB([ssl], [SSL_accept],
        [OPENSSL_LIBS="-lssl $CRYPTO_LIBS"],
        [AS_IF([test x"$1" = xtrue],
            [AC_MSG_ERROR([cannot find usable OpenSSL library])])],
        [$CRYPTO_LIBS])])])

dnl Checks if the OpenSSL header and OpenSSL and crypto libraries are present.
dnl The single argument, if "true", says to fail if the OpenSSL SSL library
dnl could not be found.
AC_DEFUN([_INN_LIB_OPENSSL_INTERNAL],
[AC_REQUIRE([INN_ENABLE_REDUCED_DEPENDS])
 INN_LIB_HELPER_PATHS([OPENSSL])
 CRYPTO_CPPFLAGS="$OPENSSL_CPPFLAGS"
 CRYPTO_LDFLAGS="$OPENSSL_LDFLAGS"
 CRYPTO_LIBS=
 AC_SUBST([CRYPTO_CPPFLAGS])
 AC_SUBST([CRYPTO_LDFLAGS])
 AC_SUBST([CRYPTO_LIBS])
 INN_LIB_OPENSSL_SWITCH
 AC_CHECK_HEADER([openssl/ssl.h],
    [_INN_LIB_OPENSSL_INTERNAL_LIBS([$1])],
    [AS_IF([test x"$1" = xtrue],
        [AC_MSG_ERROR([cannot find usable OpenSSL header])])])
 INN_LIB_OPENSSL_RESTORE])

dnl The main macro for packages with mandatory OpenSSL support.
AC_DEFUN([INN_LIB_OPENSSL],
[INN_LIB_HELPER_VAR_INIT([OPENSSL])
 INN_LIB_HELPER_WITH([openssl], [OpenSSL], [OPENSSL])
 _INN_LIB_OPENSSL_INTERNAL([true])
 inn_use_OPENSSL=true
 AC_DEFINE([HAVE_OPENSSL], 1, [Define if libssl is available.])])

dnl The main macro for packages with optional OpenSSL support.
AC_DEFUN([INN_LIB_OPENSSL_OPTIONAL],
[INN_LIB_HELPER_VAR_INIT([OPENSSL])
 INN_LIB_HELPER_WITH_OPTIONAL([openssl], [OpenSSL], [OPENSSL])
 AS_IF([test x"$inn_use_OPENSSL" != xfalse],
    [AS_IF([test x"$inn_use_OPENSSL" = xtrue],
        [_INN_LIB_OPENSSL_INTERNAL([true])],
        [_INN_LIB_OPENSSL_INTERNAL([false])])])
 AS_IF([test x"$OPENSSL_LIBS" = x],
    [INN_LIB_HELPER_VAR_CLEAR([OPENSSL])
     INN_LIB_HELPER_VAR_CLEAR([CRYPTO])],
    [inn_use_OPENSSL=true
     AC_DEFINE([HAVE_OPENSSL], 1, [Define if libssl is available.])])])

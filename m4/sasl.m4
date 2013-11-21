dnl Find the compiler and linker flags for Cyrus SASL.
dnl $Id$
dnl
dnl Finds the compiler and linker flags for linking with the Cyrus SASL
dnl library.  Provides the --with-sasl, --with-sasl-lib, and
dnl --with-sasl-include configure options to specify non-standard paths to the
dnl Cyrus SASL library.
dnl
dnl Provides the macro INN_LIB_SASL and sets the substitution variables
dnl SASL_CPPFLAGS, SASL_LDFLAGS, and SASL_LIBS.  Also provides
dnl INN_LIB_SASL_SWITCH to set CPPFLAGS, LDFLAGS, and LIBS to include the
dnl Cyrus SASL v2 library, saving the current values first, and
dnl INN_LIB_SASL_RESTORE to restore those settings to before the last
dnl INN_LIB_SASL_SWITCH.  Defines HAVE_SASL and sets inn_use_SASL to true if
dnl the library is found and is version two.
dnl
dnl Provides the INN_LIB_SASL_OPTIONAL macro, which should be used if Cyrus
dnl SASL support is optional.  This macro will still always set the
dnl substitution variables, but they'll be empty unless --with-sasl is given.
dnl Defines HAVE_SASL and sets inn_use_SASL to true if the Cyrus SASL library
dnl is found and is version two.
dnl
dnl Depends on the lib-helper.m4 framework.
dnl
dnl The canonical version of this file is maintained in the rra-c-util
dnl package, available at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
dnl
dnl Written by Russ Allbery <eagle@eyrie.org>
dnl Copyright 2013
dnl     The Board of Trustees of the Leland Stanford Junior University
dnl
dnl This file is free software; the authors give unlimited permission to copy
dnl and/or distribute it, with or without modifications, as long as this
dnl notice is preserved.

dnl Save the current CPPFLAGS, LDFLAGS, and LIBS settings and switch to
dnl versions that include the Cyrus SASL flags.  Used as a wrapper, with
dnl INN_LIB_SASL_RESTORE, around tests.
AC_DEFUN([INN_LIB_SASL_SWITCH], [INN_LIB_HELPER_SWITCH([SASL])])

dnl Restore CPPFLAGS, LDFLAGS, and LIBS to their previous values (before
dnl INN_LIB_SASL_SWITCH was called).
AC_DEFUN([INN_LIB_SASL_RESTORE], [INN_LIB_HELPER_RESTORE([SASL])])

dnl Checks if the Cyrus SASL library is present.  The single argument, if
dnl "true", says to fail if the Cyrus SASL library could not be found.
AC_DEFUN([_INN_LIB_SASL_INTERNAL],
[INN_LIB_HELPER_PATHS([SASL])
 INN_LIB_SASL_SWITCH
 AC_CHECK_LIB([sasl2], [sasl_getprop],
    [SASL_LIBS="-lsasl2"],
    [AS_IF([test x"$1" = xtrue],
        [AC_MSG_ERROR([cannot find usable Cyrus SASL library])])])
 INN_LIB_SASL_RESTORE])

dnl The main macro for packages with mandatory Cyrus SASL support.
AC_DEFUN([INN_LIB_SASL],
[INN_LIB_HELPER_VAR_INIT([SASL])
 INN_LIB_HELPER_WITH([sasl], [Cyrus SASL], [SASL])
 _INN_LIB_SASL_INTERNAL([true])
 inn_use_SASL=true
 AC_DEFINE([HAVE_SASL], 1, [Define if libsasl2 is available.])])

dnl The main macro for packages with optional Cyrus SASL support.
AC_DEFUN([INN_LIB_SASL_OPTIONAL],
[INN_LIB_HELPER_VAR_INIT([SASL])
 INN_LIB_HELPER_WITH_OPTIONAL([sasl], [Cyrus SASL], [SASL])
 AS_IF([test x"$inn_use_SASL" != xfalse],
    [AS_IF([test x"$inn_use_SASL" = xtrue],
        [_INN_LIB_SASL_INTERNAL([true])],
        [_INN_LIB_SASL_INTERNAL([false])])])
 AS_IF([test x"$SASL_LIBS" != x],
    [inn_use_SASL=true
     AC_DEFINE([HAVE_SASL], 1, [Define if libsasl2 is available.])])])

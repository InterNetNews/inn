dnl Find the compiler and linker flags for the zlib library.
dnl $Id$
dnl
dnl Finds the compiler and linker flags for linking with the zlib library.
dnl Provides the --with-zlib, --with-zlib-lib, and --with-zlib-include
dnl configure options to specify non-standard paths to the zlib library.
dnl
dnl Provides the macro INN_LIB_ZLIB and sets the substitution variables
dnl ZLIB_CPPFLAGS, ZLIB_LDFLAGS, and ZLIB_LIBS.  Also provides
dnl INN_LIB_ZLIB_SWITCH to set CPPFLAGS, LDFLAGS, and LIBS to include the
dnl zlib library, saving the current values first, and INN_LIB_ZLIB_RESTORE
dnl to restore those settings to before the last INN_LIB_ZLIB_SWITCH.
dnl Defines HAVE_ZLIB and sets inn_use_ZLIB to true if the library is found.
dnl
dnl Provides the INN_LIB_ZLIB_OPTIONAL macro, which should be used if zlib
dnl support is optional.  This macro will still always set the substitution
dnl variables, but they'll be empty unless --with-zlib is given.
dnl Defines HAVE_ZLIB and sets inn_use_ZLIB to true if the zlib library
dnl is found.
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
dnl versions that include the zlib flags.  Used as a wrapper, with
dnl INN_LIB_ZLIB_RESTORE, around tests.
AC_DEFUN([INN_LIB_ZLIB_SWITCH], [INN_LIB_HELPER_SWITCH([ZLIB])])

dnl Restore CPPFLAGS, LDFLAGS, and LIBS to their previous values (before
dnl INN_LIB_ZLIB_SWITCH was called).
AC_DEFUN([INN_LIB_ZLIB_RESTORE], [INN_LIB_HELPER_RESTORE([ZLIB])])

dnl Checks if the zlib library is present.  The single argument, if "true",
dnl says to fail if the zlib library could not be found.
AC_DEFUN([_INN_LIB_ZLIB_INTERNAL],
[INN_LIB_HELPER_PATHS([ZLIB])
 INN_LIB_ZLIB_SWITCH
 AC_CHECK_LIB([z], [compress],
    [ZLIB_LIBS="-lz"],
    [AS_IF([test x"$1" = xtrue],
        [AC_MSG_ERROR([cannot find usable zlib library])])])
 INN_LIB_ZLIB_RESTORE])

dnl The main macro for packages with mandatory zlib support.
AC_DEFUN([INN_LIB_ZLIB],
[INN_LIB_HELPER_VAR_INIT([ZLIB])
 INN_LIB_HELPER_WITH([zlib], [zlib], [ZLIB])
 _INN_LIB_ZLIB_INTERNAL([true])
 inn_use_ZLIB=true
 AC_DEFINE([HAVE_ZLIB], 1, [Define if libz is available.])])

dnl The main macro for packages with optional zlib support.
AC_DEFUN([INN_LIB_ZLIB_OPTIONAL],
[INN_LIB_HELPER_VAR_INIT([ZLIB])
 INN_LIB_HELPER_WITH_OPTIONAL([zlib], [zlib], [ZLIB])
 AS_IF([test x"$inn_use_ZLIB" != xfalse],
    [AS_IF([test x"$inn_use_ZLIB" = xtrue],
        [_INN_LIB_ZLIB_INTERNAL([true])],
        [_INN_LIB_ZLIB_INTERNAL([false])])])
 AS_IF([test x"$ZLIB_LIBS" != x],
    [inn_use_ZLIB=true
     AC_DEFINE([HAVE_ZLIB], 1, [Define if libz is available.])])])

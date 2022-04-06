dnl Find the compiler and linker flags for the blacklist library on FreeBSD
dnl which integrates with the blacklistd daemon.
dnl
dnl Finds the compiler and linker flags for linking with the blacklist library.
dnl Provides the --with-blacklist, --with-blacklist-lib, and
dnl --with-blacklist-include configure options to specify non-standard
dnl paths to the blacklist library.
dnl
dnl Provides the macro INN_LIB_BLACKLIST and sets the substitution variables
dnl BLACKLIST_CPPFLAGS, BLACKLIST_LDFLAGS, and BLACKLIST_LIBS.  Also provides
dnl INN_LIB_BLACKLIST_SWITCH to set CPPFLAGS, LDFLAGS, and LIBS to include the
dnl blacklist library, saving the current values first, and
dnl INN_LIB_BLACKLIST_RESTORE to restore those settings to before the last
dnl INN_LIB_BLACKLIST_SWITCH.  Defines HAVE_BLACKLIST and sets
dnl inn_use_BLACKLIST to true.
dnl
dnl Provides the INN_LIB_BLACKLIST_OPTIONAL macro, which should be used if
dnl blacklist support is optional.  This macro will still always set the
dnl substitution variables, but they'll be empty if libblacklist is not found
dnl or if --without-blacklist is given.  Defines HAVE_BLACKLIST and sets
dnl inn_use_BLACKLIST to true if the blacklist library is found and
dnl --without-blacklist is not given.
dnl
dnl Depends on the lib-helper.m4 framework.
dnl
dnl Written in 2022 by Andreas Kempe for the InterNetNews (INN) news server
dnl
dnl This file is free software; the authors give unlimited permission to copy
dnl and/or distribute it, with or without modifications, as long as this
dnl notice is preserved.
dnl
dnl SPDX-License-Identifier: FSFULLR

dnl Save the current CPPFLAGS, LDFLAGS, and LIBS settings and switch to
dnl versions that include the blacklist flags.  Used as a wrapper, with
dnl INN_LIB_BLACKLIST_RESTORE, around tests.
AC_DEFUN([INN_LIB_BLACKLIST_SWITCH], [INN_LIB_HELPER_SWITCH([BLACKLIST])])

dnl Restore CPPFLAGS, LDFLAGS, and LIBS to their previous values (before
dnl INN_LIB_BLACKLIST_SWITCH was called).
AC_DEFUN([INN_LIB_BLACKLIST_RESTORE], [INN_LIB_HELPER_RESTORE([BLACKLIST])])

dnl Checks if the blacklist library is present.  The single argument, if
dnl "true", says to fail if the blacklist library could not be found.
AC_DEFUN([_INN_LIB_BLACKLIST_INTERNAL],
[INN_LIB_HELPER_PATHS([BLACKLIST])
 INN_LIB_BLACKLIST_SWITCH
 AC_CHECK_HEADER([blacklist.h],
    [AC_CHECK_LIB([blacklist], [blacklist_r],
        [BLACKLIST_LIBS="-lblacklist"],
        [AS_IF([test x"$1" = xtrue],
            [AC_MSG_ERROR([cannot find usable blacklist library])])])],
    [AS_IF([test x"$1" = xtrue],
        [AC_MSG_ERROR([cannot find usable blacklist header])])])
 INN_LIB_BLACKLIST_RESTORE])

dnl The main macro for packages with mandatory blacklist support.
AC_DEFUN([INN_LIB_BLACKLIST],
[INN_LIB_HELPER_VAR_INIT([BLACKLIST])
 INN_LIB_HELPER_WITH([blacklist], [blacklist], [BLACKLIST])
 _INN_LIB_BLACKLIST_INTERNAL([true])
 inn_use_BLACKLIST=true
 AC_DEFINE([HAVE_BLACKLIST], 1, [Define if libblacklist is available.])])

dnl The main macro for packages with optional blacklist support.
AC_DEFUN([INN_LIB_BLACKLIST_OPTIONAL],
[INN_LIB_HELPER_VAR_INIT([BLACKLIST])
 INN_LIB_HELPER_WITH_OPTIONAL([blacklist], [blacklist], [BLACKLIST])
 AS_IF([test x"$inn_use_BLACKLIST" != xfalse],
    [AS_IF([test x"$inn_use_BLACKLIST" = xtrue],
        [_INN_LIB_BLACKLIST_INTERNAL([true])],
        [_INN_LIB_BLACKLIST_INTERNAL([false])])])
 AS_IF([test x"$BLACKLIST_LIBS" = x],
    [INN_LIB_HELPER_VAR_CLEAR([BLACKLIST])],
    [inn_use_BLACKLIST=true
     AC_DEFINE([HAVE_BLACKLIST], 1, [Define if libblacklist is available.])])])

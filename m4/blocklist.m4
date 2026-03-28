dnl Find the compiler and linker flags for the blocklist library on FreeBSD
dnl which integrates with the blocklistd daemon.
dnl
dnl Finds the compiler and linker flags for linking with the blocklist library.
dnl Provides the --with-blocklist, --with-blocklist-lib, and
dnl --with-blocklist-include configure options to specify non-standard
dnl paths to the blocklist library.
dnl
dnl Provides the macro INN_LIB_BLOCKLIST and sets the substitution variables
dnl BLOCKLIST_CPPFLAGS, BLOCKLIST_LDFLAGS, and BLOCKLIST_LIBS.  Also provides
dnl INN_LIB_BLOCKLIST_SWITCH to set CPPFLAGS, LDFLAGS, and LIBS to include the
dnl blocklist library, saving the current values first, and
dnl INN_LIB_BLOCKLIST_RESTORE to restore those settings to before the last
dnl INN_LIB_BLOCKLIST_SWITCH.  Defines HAVE_BLOCKLIST and sets
dnl inn_use_BLOCKLIST to true.
dnl
dnl Provides the INN_LIB_BLOCKLIST_OPTIONAL macro, which should be used if
dnl blocklist support is optional.  This macro will still always set the
dnl substitution variables, but they'll be empty if libblocklist is not found
dnl or if --without-blocklist is given.  Defines HAVE_BLOCKLIST and sets
dnl inn_use_BLOCKLIST to true if the blocklist library is found and
dnl --without-blocklist is not given.
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
dnl versions that include the blocklist flags.  Used as a wrapper, with
dnl INN_LIB_BLOCKLIST_RESTORE, around tests.
AC_DEFUN([INN_LIB_BLOCKLIST_SWITCH], [INN_LIB_HELPER_SWITCH([BLOCKLIST])])

dnl Restore CPPFLAGS, LDFLAGS, and LIBS to their previous values (before
dnl INN_LIB_BLOCKLIST_SWITCH was called).
AC_DEFUN([INN_LIB_BLOCKLIST_RESTORE], [INN_LIB_HELPER_RESTORE([BLOCKLIST])])

dnl Checks if the blocklist library is present.  The single argument, if
dnl "true", says to fail if the blocklist library could not be found.
AC_DEFUN([_INN_LIB_BLOCKLIST_INTERNAL],
[INN_LIB_HELPER_PATHS([BLOCKLIST])
 INN_LIB_BLOCKLIST_SWITCH
 AC_CHECK_HEADER([blocklist.h],
    [AC_CHECK_LIB([blocklist], [blocklist_r],
        [BLOCKLIST_LIBS="-lblocklist"],
        [AS_IF([test x"$1" = xtrue],
            [AC_MSG_ERROR([cannot find usable blocklist library])])])],
    [AS_IF([test x"$1" = xtrue],
        [AC_MSG_ERROR([cannot find usable blocklist header])])])
 INN_LIB_BLOCKLIST_RESTORE])

dnl The main macro for packages with mandatory blocklist support.
AC_DEFUN([INN_LIB_BLOCKLIST],
[INN_LIB_HELPER_VAR_INIT([BLOCKLIST])
 INN_LIB_HELPER_WITH([blocklist], [blocklist], [BLOCKLIST])
 _INN_LIB_BLOCKLIST_INTERNAL([true])
 inn_use_BLOCKLIST=true
 AC_DEFINE([HAVE_BLOCKLIST], 1, [Define if libblocklist is available.])])

dnl The main macro for packages with optional blocklist support.
AC_DEFUN([INN_LIB_BLOCKLIST_OPTIONAL],
[INN_LIB_HELPER_VAR_INIT([BLOCKLIST])
 INN_LIB_HELPER_WITH_OPTIONAL([blocklist], [blocklist], [BLOCKLIST])
 AS_IF([test x"$inn_use_BLOCKLIST" != xfalse],
    [AS_IF([test x"$inn_use_BLOCKLIST" = xtrue],
        [_INN_LIB_BLOCKLIST_INTERNAL([true])],
        [_INN_LIB_BLOCKLIST_INTERNAL([false])])])
 AS_IF([test x"$BLOCKLIST_LIBS" = x],
    [INN_LIB_HELPER_VAR_CLEAR([BLOCKLIST])],
    [inn_use_BLOCKLIST=true
     AC_DEFINE([HAVE_BLOCKLIST], 1, [Define if libblocklist is available.])])])

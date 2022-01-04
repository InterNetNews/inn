dnl Find the compiler and linker flags for the canlock library.
dnl
dnl Finds the compiler and linker flags for linking with the canlock library.
dnl Provides the --with-canlock, --with-canlock-lib, and
dnl --with-canlock-include configure options to specify non-standard paths
dnl to the canlock library.
dnl
dnl Provides the macro INN_LIB_CANLOCK and sets the substitution variables
dnl CANLOCK_CPPFLAGS, CANLOCK_LDFLAGS, and CANLOCK_LIBS.  Also provides
dnl INN_LIB_CANLOCK_SWITCH to set CPPFLAGS, LDFLAGS, and LIBS to include the
dnl canlock library, saving the current values first, and
dnl INN_LIB_CANLOCK_RESTORE to restore those settings to before the last
dnl INN_LIB_CANLOCK_SWITCH.  Defines HAVE_CANLOCK and sets inn_use_CANLOCK
dnl to true if the library is found.
dnl
dnl Provides the INN_LIB_CANLOCK_OPTIONAL macro, which should be used if
dnl Cancel-Lock support is optional.  This macro will still always set the
dnl substitution variables, but they'll be empty unless libcanlock is found or
dnl --without-canlock is not given.  Defines HAVE_CANLOCK and sets
dnl inn_use_CANLOCK to true if the canlock library is found.
dnl
dnl This macro checks for a parsing feature (cl_verify_multi) introduced with
dnl version 3.3.0 of libcanlock.
dnl
dnl Depends on the lib-helper.m4 framework.
dnl
dnl Written by Julien ÉLIE for the InterNetNews (INN) news server
dnl Copyright 2021 Julien ÉLIE
dnl
dnl This file is free software; the authors give unlimited permission to copy
dnl and/or distribute it, with or without modifications, as long as this
dnl notice is preserved.
dnl
dnl SPDX-License-Identifier: FSFULLR

dnl Save the current CPPFLAGS, LDFLAGS, and LIBS settings and switch to
dnl versions that include the canlock flags.  Used as a wrapper, with
dnl INN_LIB_CANLOCK_RESTORE, around tests.
AC_DEFUN([INN_LIB_CANLOCK_SWITCH], [INN_LIB_HELPER_SWITCH([CANLOCK])])

dnl Restore CPPFLAGS, LDFLAGS, and LIBS to their previous values (before
dnl INN_LIB_CANLOCK_SWITCH was called).
AC_DEFUN([INN_LIB_CANLOCK_RESTORE], [INN_LIB_HELPER_RESTORE([CANLOCK])])

dnl Checks if the canlock library is present.  The single argument, if "true",
dnl says to fail if the canlock library could not be found.
AC_DEFUN([_INN_LIB_CANLOCK_INTERNAL],
[INN_LIB_HELPER_PATHS([CANLOCK])
 INN_LIB_CANLOCK_SWITCH
 AC_CHECK_HEADER([libcanlock-3/canlock.h],
    [AC_CHECK_LIB([canlock], [cl_verify_multi],
        [CANLOCK_LIBS="-lcanlock"],
        [AS_IF([test x"$1" = xtrue],
            [AC_MSG_ERROR([cannot find usable canlock library])])])],
    [AS_IF([test x"$1" = xtrue],
        [AC_MSG_ERROR([cannot find usable canlock header])])])
 INN_LIB_CANLOCK_RESTORE])

dnl The main macro for packages with mandatory Cancel-Lock support.
AC_DEFUN([INN_LIB_CANLOCK],
[INN_LIB_HELPER_VAR_INIT([CANLOCK])
 INN_LIB_HELPER_WITH([canlock], [canlock], [CANLOCK])
 _INN_LIB_CANLOCK_INTERNAL([true])
 inn_use_CANLOCK=true
 AC_DEFINE([HAVE_CANLOCK], 1, [Define if libcanlock is available.])])

dnl The main macro for packages with optional Cancel-Lock support.
AC_DEFUN([INN_LIB_CANLOCK_OPTIONAL],
[INN_LIB_HELPER_VAR_INIT([CANLOCK])
 INN_LIB_HELPER_WITH_OPTIONAL([canlock], [canlock], [CANLOCK])
 AS_IF([test x"$inn_use_CANLOCK" != xfalse],
    [AS_IF([test x"$inn_use_CANLOCK" = xtrue],
        [_INN_LIB_CANLOCK_INTERNAL([true])],
        [_INN_LIB_CANLOCK_INTERNAL([false])])])
 AS_IF([test x"$CANLOCK_LIBS" = x],
    [INN_LIB_HELPER_VAR_CLEAR([CANLOCK])],
    [inn_use_CANLOCK=true
     AC_DEFINE([HAVE_CANLOCK], 1, [Define if libcanlock is available.])])])

dnl Find the compiler and linker flags for SQLite v3.
dnl
dnl Finds the compiler and linker flags for linking with the SQLite library.
dnl Provides the --with-sqlite3, --with-sqlite3-lib, and
dnl --with-sqlite3-include configure options to specify non-standard paths to
dnl the SQLite v3 libraries or header files.
dnl
dnl Provides the macro INN_LIB_SQLITE3 and sets the substitution variables
dnl SQLITE3_CPPFLAGS, SQLITE3_LDFLAGS, and SQLITE3_LIBS.  Also provides
dnl INN_LIB_SQLITE3_SWITCH to set CPPFLAGS, LDFLAGS, and LIBS to include the
dnl SQLite library, saving the current values first, and
dnl INN_LIB_SQLITE3_RESTORE to restore those settings to before the last
dnl INN_LIB_SQLITE3_SWITCH.  Defines HAVE_SQLITE3 and sets inn_use_SQLITE3 to
dnl true.
dnl
dnl Provides the INN_LIB_SQLITE3_OPTIONAL macro, which should be used if
dnl SQLite support is optional.  This macro will still always set the
dnl substitution variables, but they'll be empty if the SQLite library is not
dnl found or if --without-sqlite3 is given.  Defines HAVE_SQLITE3 and sets
dnl inn_use_SQLITE3 to true if the SQLite library is found and
dnl --without-sqlite3 is not given.
dnl
dnl Depends on the lib-helper.m4 framework.
dnl
dnl The canonical version of this file is maintained in the rra-c-util
dnl package, available at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
dnl
dnl Written by Russ Allbery <eagle@eyrie.org>
dnl Copyright 2020, 2022 Russ Allbery <eagle@eyrie.org>
dnl Copyright 2014
dnl     The Board of Trustees of the Leland Stanford Junior University
dnl
dnl This file is free software; the authors give unlimited permission to copy
dnl and/or distribute it, with or without modifications, as long as this
dnl notice is preserved.
dnl
dnl SPDX-License-Identifier: FSFULLR

dnl Save the current CPPFLAGS, LDFLAGS, and LIBS settings and switch to
dnl versions that include the SQLite 3 flags.  Used as a wrapper, with
dnl INN_LIB_SQLITE3_RESTORE, around tests.
AC_DEFUN([INN_LIB_SQLITE3_SWITCH], [INN_LIB_HELPER_SWITCH([SQLITE3])])

dnl Restore CPPFLAGS, LDFLAGS, and LIBS to their previous values before
dnl INN_LIB_SQLITE3_SWITCH was called.
AC_DEFUN([INN_LIB_SQLITE3_RESTORE], [INN_LIB_HELPER_RESTORE([SQLITE3])])

dnl Ensures SQLite v3 meets our minimum version requirement.
AC_DEFUN([_INN_LIB_SQLITE3_SOURCE], [[
#include <sqlite3.h>

int main(void) {
    return sqlite3_libversion_number() < 3008002;
}
]])

dnl Checks if SQLite v3 is present.  The single argument, if "true", says to
dnl fail if the SQLite library could not be found.
AC_DEFUN([_INN_LIB_SQLITE3_INTERNAL],
[AC_CACHE_CHECK([for a sufficiently recent SQLite],
    [inn_cv_have_sqlite3],
    [INN_LIB_HELPER_PATHS([SQLITE3])
     INN_LIB_SQLITE3_SWITCH
     LIBS="-lsqlite3 $LIBS"
     AC_RUN_IFELSE([AC_LANG_SOURCE([_INN_LIB_SQLITE3_SOURCE])],
        [inn_cv_have_sqlite3=yes],
        [inn_cv_have_sqlite3=no])
     INN_LIB_SQLITE3_RESTORE])
 AS_IF([test x"$inn_cv_have_sqlite3" = xyes],
    [SQLITE3_LIBS="-lsqlite3"],
    [AS_IF([test x"$1" = xtrue],
        [AC_MSG_ERROR([cannot find usable SQLite v3 library])])])])

dnl The main macro for packages with mandatory SQLite v3 support.
AC_DEFUN([INN_LIB_SQLITE3],
[INN_LIB_HELPER_VAR_INIT([SQLITE3])
 INN_LIB_HELPER_WITH([sqlite3], [SQLite v3], [SQLITE3])
 _INN_LIB_SQLITE3_INTERNAL([true])
 inn_use_SQLITE3=true
 AC_DEFINE([HAVE_SQLITE3], 1, [Define if libsqlite3 is available.])])

dnl The main macro for packages with optional SQLite v3 support.
AC_DEFUN([INN_LIB_SQLITE3_OPTIONAL],
[INN_LIB_HELPER_VAR_INIT([SQLITE3])
 INN_LIB_HELPER_WITH_OPTIONAL([sqlite3], [SQLite v3], [SQLITE3])
 AS_IF([test x"$inn_use_SQLITE3" != xfalse],
    [AS_IF([test x"$inn_use_SQLITE3" = xtrue],
        [_INN_LIB_SQLITE3_INTERNAL([true])],
        [_INN_LIB_SQLITE3_INTERNAL([false])])])
 AS_IF([test x"$SQLITE3_LIBS" = x],
    [INN_LIB_HELPER_VAR_CLEAR([SQLITE3])],
    [inn_use_SQLITE3=true
     AC_DEFINE([HAVE_SQLITE3], 1, [Define if libsqlite3 is available.])])])

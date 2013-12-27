dnl Find the compiler and linker flags for the Berkeley DB library.
dnl $Id$
dnl
dnl Finds the compiler and linker flags for linking with the Berkeley DB
dnl library.  Provides the --with-bdb, --with-bdb-lib, and --with-bdb-include
dnl configure options to specify non-standard paths to the Berkeley DB
dnl library.
dnl
dnl Provides the macro INN_LIB_BDB and sets the substitution variables
dnl BDB_CPPFLAGS, BDB_LDFLAGS, and BDB_LIBS.  Also provides INN_LIB_BDB_SWITCH
dnl to set CPPFLAGS, LDFLAGS, and LIBS to include the Berkeley DB library,
dnl saving the current values first, and INN_LIB_BDB_RESTORE to restore those
dnl settings to before the last INN_LIB_BDB_SWITCH.  Defines HAVE_BDB and sets
dnl inn_use_BDB to true if the library is found.
dnl
dnl Provides the INN_LIB_BDB_OPTIONAL macro, which should be used if Berkeley
dnl DB support is optional.  This macro will still always set the substitution
dnl variables, but they'll be empty unless --with-bdb is given.  Defines
dnl HAVE_BDB and sets inn_use_BDB to true if the Berkeley DB library is found.
dnl
dnl This file also provides INN_LIB_BDB_NDBM, which checks whether the
dnl Berkeley DB library has ndbm support.  It then defines HAVE_BDB_NDBM and
dnl sets inn_cv_lib_bdb_ndbm to yes if ndbm compatibility layer for Berkely DB
dnl is available.  Either INN_LIB_BDB or INN_LIB_BDB_OPTIONAL must be called
dnl before calling this macro.
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
dnl versions that include the Berkeley DB flags.  Used as a wrapper, with
dnl INN_LIB_BDB_RESTORE, around tests.
AC_DEFUN([INN_LIB_BDB_SWITCH], [INN_LIB_HELPER_SWITCH([BDB])])

dnl Restore CPPFLAGS, LDFLAGS, and LIBS to their previous values (before
dnl INN_LIB_BDB_SWITCH was called).
AC_DEFUN([INN_LIB_BDB_RESTORE], [INN_LIB_HELPER_RESTORE([BDB])])

dnl Checks if the Berkeley DB library is present.  The single argument,
dnl if "true", says to fail if the Berkeley DB library could not be found.
AC_DEFUN([_INN_LIB_BDB_INTERNAL],
[INN_LIB_HELPER_PATHS([BDB])
 INN_LIB_BDB_SWITCH
 AC_CHECK_LIB([db], [db_create],
    [BDB_LIBS="-ldb"],
    [AS_IF([test x"$1" = xtrue],
        [AC_MSG_ERROR([cannot find usable Berkeley DB library])])])
 INN_LIB_BDB_RESTORE])

dnl The main macro for packages with mandatory Berkeley DB support.
AC_DEFUN([INN_LIB_BDB],
[INN_LIB_HELPER_VAR_INIT([BDB])
 INN_LIB_HELPER_WITH([bdb], [Berkeley DB], [BDB])
 _INN_LIB_BDB_INTERNAL([true])
 inn_use_BDB=true
 AC_DEFINE([HAVE_BDB], 1, [Define if libdb is available.])])

dnl The main macro for packages with optional Berkeley DB support.
AC_DEFUN([INN_LIB_BDB_OPTIONAL],
[INN_LIB_HELPER_VAR_INIT([BDB])
 INN_LIB_HELPER_WITH_OPTIONAL([bdb], [Berkeley DB], [BDB])
 AS_IF([test x"$inn_use_BDB" != xfalse],
    [AS_IF([test x"$inn_use_BDB" = xtrue],
        [_INN_LIB_BDB_INTERNAL([true])],
        [_INN_LIB_BDB_INTERNAL([false])])])
 AS_IF([test x"$BDB_LIBS" != x],
    [inn_use_BDB=true
     AC_DEFINE([HAVE_BDB], 1, [Define if libdb is available.])])])

dnl Source used by INN_LIB_BDB_NDBM.
AC_DEFUN([_INN_LIB_BDB_NDBM_SOURCE], [[
#include <stdio.h>
#define DB_DBM_HSEARCH 1
#include <db.h>

int
main(void)
{
    DBM *database;
    database = dbm_open("test", 0, 0600);
    dbm_close(database);
    return 0;
}
]])

dnl Check whether Berkeley DB was compiled with ndbm compatibility layer.  If
dnl so, set HAVE_BDB_NDBM.  Either INN_LIB_BDB or INN_LIB_BDB_OPTIONAL should
dnl be called before calling this macro.
AC_DEFUN([INN_LIB_BDB_NDBM],
[INN_LIB_BDB_SWITCH
 AC_CACHE_CHECK([for working ndbm compatibility layer with Berkeley DB],
    [inn_cv_lib_bdb_ndbm],
    [AC_LINK_IFELSE([AC_LANG_SOURCE([_INN_LIB_BDB_NDBM_SOURCE])],
        [inn_cv_lib_bdb_ndbm=yes],
        [inn_cv_lib_bdb_ndbm=no])])
 AS_IF([test x"$inn_cv_lib_bdb_ndbm" = xyes],
    [AC_DEFINE([HAVE_BDB_NDBM], 1,
        [Define if the Berkeley DB ndbm compatibility layer is available.])])
 INN_LIB_BDB_RESTORE])

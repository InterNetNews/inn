dnl berkeleydb.m4 -- Various checks for the Berkeley DB libraries.
dnl $Id$
dnl
dnl This file provides INN_LIB_BERKELEYDB, which defines the --with-berkeleydb
dnl command-line option and probes for the location of Berkeley DB if that
dnl option is used without an optional path.  It looks for Berkeley DB in $prefix,
dnl /usr/local and /usr.  It then exports DB_LDFLAGS, DB_CPPFLAGS, and DB_LIBS.
dnl
dnl This file also provides INN_LIB_BERKELEYDB_NDBM, which checks whether
dnl Berkeley DB has ndbm support.  It then defines HAVE_BDB_NDBM if ndbm
dnl compatibility layer for Berkely DB is available.

AC_DEFUN([INN_LIB_BERKELEYDB],
[DB_CPPFLAGS=
DB_LDFLAGS=
DB_LIBS=
AC_ARG_WITH([berkeleydb],
    [AS_HELP_STRING([--with-berkeleydb@<:@=PATH@:>@],
        [Enable Berkeley DB (for ovdb overview method)])],
    DB_DIR=$with_berkeleydb,
    DB_DIR=no)
AC_MSG_CHECKING([if Berkeley DB is desired])
if test x"$DB_DIR" = xno ; then
    AC_MSG_RESULT([no])
else
    AC_MSG_RESULT([yes])

    AC_MSG_CHECKING([for Berkeley DB location])
    if test x"$DB_DIR" = xyes ; then
        for dir in $prefix /usr/local /usr ; do
            if test -f "$dir/include/db.h" ; then
                DB_DIR=$dir
                break
            fi
        done
    fi
    if test x"$DB_DIR" = xyes ; then
        AC_MSG_ERROR([cannot find Berkeley DB])
    else
        if test x"$DB_DIR" != x/usr ; then
            DB_CPPFLAGS="-I$DB_DIR/include"
            DB_LDFLAGS="-L$DB_DIR/lib"
        fi
        DB_LIBS="-ldb"
        AC_MSG_RESULT([$DB_DIR])
    fi
    AC_DEFINE([USE_BERKELEY_DB], 1, [Define if Berkeley DB is available.])
fi
AC_SUBST([DB_CPPFLAGS])
AC_SUBST([DB_LDFLAGS])
AC_SUBST([DB_LIBS])])

dnl Source used by INN_LIB_BERKELEYDB_NDBM.
AC_DEFUN([_INN_LIB_BERKELEYDB_NDBM_SOURCE], [[
#include <stdio.h>
#define DB_DBM_HSEARCH 1
#include <db.h>

int
main(void) {
    DBM *database;
    database = dbm_open("test", 0, 0600);
    dbm_close(database);
    return 0;
}
]])

dnl Check whether Berkeley DB was compiled with ndbm compatibility layer.
dnl If so, set HAVE_BDB_NDBM.
AC_DEFUN([INN_LIB_BERKELEYDB_NDBM],
[inn_save_LDFLAGS="$LDFLAGS"
inn_save_CFLAGS="$CFLAGS"
LDFLAGS="$LDFLAGS $DB_LDFLAGS $DB_LIBS"
CFLAGS="$CFLAGS $DB_CPPFLAGS"
AC_CACHE_CHECK([for working nbdm compatibility layer with Berkeley DB],
    [inn_cv_lib_berkeleydb_ndbm_works],
    [AC_LINK_IFELSE([AC_LANG_SOURCE([_INN_LIB_BERKELEYDB_NDBM_SOURCE])],
        [inn_cv_lib_berkeleydb_ndbm_works=yes],
        [inn_cv_lib_berkeleydb_ndbm_works=no])])
AS_IF([test x"$inn_cv_lib_berkeleydb_ndbm_works" = xyes],
    [AC_DEFINE([HAVE_BDB_NDBM], 1,
        [Define if the Berkeley DB ndbm compatibility layer is available.])])
LDFLAGS="$inn_save_LDFLAGS"
CFLAGS="$inn_save_CFLAGS"])

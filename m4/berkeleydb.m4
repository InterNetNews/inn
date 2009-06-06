dnl berkeleydb.m4 -- Find the path to the Berkeley DB libraries.
dnl $Id$
dnl
dnl This file provides INN_LIB_BERKELEYDB, which defines the --with-berkeleydb
dnl command-line option and probes for the location of Berkeley DB if that
dnl option is used without an optional path.  It looks for Berkeley DB in $prefix,
dnl /usr/local and /usr.  It then exports DB_LDFLAGS, DB_CFLAGS, and DB_LIBS.

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
        DB_CPPFLAGS="-I$DB_DIR/include"
        DB_LDFLAGS="-L$DB_DIR/lib"
        DB_LIBS="-ldb"
        AC_MSG_RESULT([$DB_DIR])
    fi
    AC_DEFINE([USE_BERKELEY_DB], 1, [Define if Berkeley DB is available.])
fi
AC_SUBST([DB_CPPFLAGS])
AC_SUBST([DB_LDFLAGS])
AC_SUBST([DB_LIBS])])

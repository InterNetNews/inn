dnl berkeleydb.m4 -- Find the path to the BerkeleyDB libraries.
dnl $Id$
dnl
dnl This file provides INN_LIB_BERKELEYDB, which defines the --with-berkeleydb
dnl command-line option and probes for the location of BerkeleyDB if that
dnl option is used without an optional path.  It looks for BerkeleyDB in a
dnl variety of standard locations and then exports DB_LDFLAGS, DB_CFLAGS, and
dnl DB_LIBS.

AC_DEFUN([INN_LIB_BERKELEYDB],
[DB_CPPFLAGS=
DB_LDFLAGS=
DB_LIBS=
AC_ARG_WITH([berkeleydb],
    [AC_HELP_STRING([--with-berkeleydb@<:@=PATH@:>@],
        [Enable BerkeleyDB (for ovdb overview method)])],
    DB_DIR=$with_berkeleydb,
    DB_DIR=no)
AC_MSG_CHECKING([if Berkeley DB is desired])
if test x"$DB_DIR" = xno ; then
    AC_MSG_RESULT([no])
else
    AC_MSG_RESULT([yes])
    AC_MSG_CHECKING([for Berkeley DB location])

    dnl First check the default installation locations.
    if test x"$DB_DIR" = xyes ; then
        for version in BerkeleyDB.4.3 BerkeleyDB.4.2 BerkeleyDB.4.1 BerkeleyDB.4.0 \
                       BerkeleyDB.3.3 BerkeleyDB.3.2 BerkeleyDB.3.1 \
                       BerkeleyDB.3.0 BerkeleyDB ; do
            if test -d "/usr/local/$version" ; then
                DB_DIR=/usr/local/$version
                break
            fi
        done
    fi

    dnl If not found there, check the default locations for some BSD ports and
    dnl Linux distributions.  They each do things in different ways.
    if test x"$DB_DIR" = xyes ; then
        for version in db43 db42 db41 db4 db3 db2 ; do
            if test -d "/usr/local/include/$version" ; then
                DB_CPPFLAGS="-I/usr/local/include/$version"
                DB_LDFLAGS="-L/usr/local/lib"
                DB_LIBS="-l$version"
                AC_MSG_RESULT([FreeBSD locations])
                break
            fi
        done
        if test x"$DB_LIBS" = x ; then
            for version in db43 db42 db41 db4 db3 db2 ; do
                if test -d "/usr/include/$version" ; then
                    DB_CPPFLAGS="-I/usr/include/$version"
                    DB_LIBS="-l$version"
                    AC_MSG_RESULT([Red Hat locations])
                    break
                fi
            done
            if test x"$DB_LIBS" = x ; then        
                DB_LIBS=-ldb
                AC_MSG_RESULT([trying -ldb])
            fi
        fi
    else
        DB_CPPFLAGS="-I$DB_DIR/include"
        DB_LDFLAGS="-L$DB_DIR/lib"
        DB_LIBS="-ldb"
        AC_MSG_RESULT([$DB_DIR])
    fi
    AC_DEFINE([USE_BERKELEY_DB], 1, [Define if BerkeleyDB is available.])
fi
AC_SUBST([DB_CPPFLAGS])
AC_SUBST([DB_LDFLAGS])
AC_SUBST([DB_LIBS])])

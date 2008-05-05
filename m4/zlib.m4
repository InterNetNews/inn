dnl zlib.m4 -- Find the path to the zlib library.
dnl $Id$
dnl
dnl This file provides INN_LIB_ZLIB, which defines the --with-zlib
dnl command-line option and probes for the location of zlib if that
dnl option is used without an optional path.  It looks by default in $prefix,
dnl /usr/local, and /usr.  It exports ZLIB_LDFLAGS, ZLIB_CPPFLAGS, and ZLIB_LIBS.
dnl The default for --with-zlib is no unless Berkeley DB is enabled, in which
dnl case the default is yes.

AC_DEFUN([INN_LIB_ZLIB],
[ZLIB_CPPFLAGS=
ZLIB_LDFLAGS=
ZLIB_LIBS=
AC_ARG_WITH([zlib],
    [AC_HELP_STRING([--with-zlib@<:@=PATH@:>@],
        [Enable zlib (used by ovdb)])],
    ZLIB_DIR=$with_zlib,
    [if test x"$DB_LIBS" != x ; then
	ZLIB_DIR=yes
     else
        ZLIB_DIR=no
     fi])
AC_MSG_CHECKING([if zlib is desired])
if test x"$ZLIB_DIR" = xno ; then
    AC_MSG_RESULT([no])
else
    AC_MSG_RESULT([yes])
    AC_MSG_CHECKING([for zlib location])
    if test x"$ZLIB_DIR" = xyes ; then
        for dir in $prefix /usr/local /usr ; do
            if test -f "$dir/include/zlib.h" ; then
                ZLIB_DIR=$dir
                break
            fi
        done
    fi
    if test x"$ZLIB_DIR" = xyes ; then
	AC_MSG_RESULT([no])
    else
        AC_MSG_RESULT([$ZLIB_DIR])
        if test x"$ZLIB_DIR" != x/usr ; then
            ZLIB_CPPFLAGS="-I$ZLIB_DIR/include"
            ZLIB_LDFLAGS="-L$ZLIB_DIR/lib"
        fi
        inn_save_LDFLAGS=$LDFLAGS
        LDFLAGS="$ZLIB_LDFLAGS $LDFLAGS"
        AC_CHECK_LIB([z], [compress], [ZLIB_LIBS=-lz],
            [AC_MSG_ERROR([cannot find libz])])
        LDFLAGS=$inn_save_LDFLAGS
        AC_DEFINE([HAVE_ZLIB], 1, [Define if zlib is available.])
    fi
fi
AC_SUBST([ZLIB_CPPFLAGS])
AC_SUBST([ZLIB_LDFLAGS])
AC_SUBST([ZLIB_LIBS])])

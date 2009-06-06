dnl sasl.m4 -- Find the path to the Cyrus SASL libraries.
dnl $Id$
dnl
dnl This file provides INN_LIB_SASL, which defines the --with-sasl
dnl command-line option and probes for the location of Cyrus SASL v2 if that
dnl option is used without an optional path.  It looks by default in $prefix,
dnl /usr/local, and /usr.  It then makes sure that Cyrus SASL is verison two
dnl and will link, and exports SASL_LDFLAGS, SASL_CPPFLAGS, and SASL_LIBS.

AC_DEFUN([INN_LIB_SASL],
[SASL_CPPFLAGS=
SASL_LDFLAGS=
SASL_LIBS=
AC_ARG_WITH([sasl],
    [AS_HELP_STRING([--with-sasl@<:@=PATH@:>@],
        [Enable SASL (for imapfeed authentication)])],
    SASL_DIR=$with_sasl,
    SASL_DIR=no)
AC_MSG_CHECKING([if SASL is desired])
if test x"$SASL_DIR" = xno ; then
    AC_MSG_RESULT([no])
else
    AC_MSG_RESULT([yes])
    AC_MSG_CHECKING([for SASL location])
    if test x"$SASL_DIR" = xyes ; then
        for dir in $prefix /usr/local /usr ; do
            if test -f "$dir/include/sasl/sasl.h" ; then
                SASL_DIR=$dir
                break
            fi
        done
    fi
    if test x"$SASL_DIR" = xyes ; then
        AC_MSG_ERROR([cannot find SASL])
    else
        AC_MSG_RESULT([$SASL_DIR])
        if test x"$SASL_DIR" != x/usr ; then
            SASL_CPPFLAGS="-I$SASL_DIR/include"
            SASL_LDFLAGS="-L$SASL_DIR/lib"
        fi
        inn_save_LDFLAGS=$LDFLAGS
        LDFLAGS="$SASL_LDFLAGS $LDFLAGS"
        AC_CHECK_LIB([sasl2], [sasl_getprop], [SASL_LIBS=-lsasl2],
            [AC_MSG_ERROR([cannot find SASL])])
        LDFLAGS=$inn_save_LDFLAGS
        AC_DEFINE([HAVE_SASL], 1, [Define if SASL is available.])
    fi
fi
AC_SUBST([SASL_CPPFLAGS])
AC_SUBST([SASL_LDFLAGS])
AC_SUBST([SASL_LIBS])])

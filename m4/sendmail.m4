dnl sendmail.m4 -- Checks for the path to sendmail.
dnl $Id$
dnl
dnl We have a custom probe for sendmail since we want to look in non-standard
dnl locations for it, and another custom macro to allow users to override the
dnl path to sendmail picked up by the script.

dnl Allow the user to specify the path to sendmail.
AC_DEFUN([INN_ARG_SENDMAIL],
[AC_ARG_VAR([SENDMAIL], [Location of sendmail binary to use])
AC_ARG_WITH([sendmail],
    [AS_HELP_STRING([--with-sendmail=PATH], [Path to sendmail])],
    SENDMAIL=$with_sendmail)])

dnl Search for sendmail, honoring the path set by the user if they've done so
dnl and otherwise looking only in /usr/sbin and /usr/lib.
AC_DEFUN([INN_PATH_SENDMAIL],
[if test "${with_sendmail+set}" = set ; then
    AC_MSG_CHECKING([for sendmail])
    AC_MSG_RESULT([$SENDMAIL])
else
    AC_PATH_PROG([SENDMAIL], [sendmail], [], [/usr/sbin:/usr/lib])
    if test -z "$SENDMAIL" ; then
        AC_MSG_ERROR([sendmail not found, re-run with --with-sendmail])
    fi
fi])

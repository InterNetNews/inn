dnl prog-ensure.m4 -- Require that a program be found in the PATH.
dnl $Id$
dnl
dnl This is a version of AC_PATH_PROG that requires that the program being
dnl searched for is found in the user's PATH.

AC_DEFUN([INN_PATH_PROG_ENSURE],
[AC_PATH_PROG([$1], [$2])
if test x"${$1}" = x ; then
    AC_MSG_ERROR([$2 was not found in path and is required])
fi])

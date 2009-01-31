dnl large-fpos.m4 -- Check for an off_t-compatible fpos_t.
dnl $Id$
dnl
dnl Some operating systems (most notably BSDI) support large files but don't
dnl have the fseeko and ftello functions.  However, fseeko and ftello can be
dnl implemented in terms of fsetpos and fgetpos if fpos_t can be cast to and
dnl from off_t.  This checks for that capability.

dnl Source used by INN_TYPE_FPOS_T_LARGE.
define([_INN_TYPE_FPOS_T_LARGE_SOURCE],
[AC_LANG_SOURCE([[
#include <stdio.h>
#include <sys/types.h>

int
main ()
{
  fpos_t fpos = 9223372036854775807ULL;
  off_t off;
  off = fpos;
  exit(off == (off_t) 9223372036854775807ULL ? 0 : 1);
}
]])])

dnl The user-callable macro.
AC_DEFUN([INN_TYPE_FPOS_T_LARGE],
[AC_CACHE_CHECK([for off_t-compatible fpos_t], [inn_cv_type_fpos_t_large],
    [AC_RUN_IFELSE([_INN_TYPE_FPOS_T_LARGE_SOURCE],
        [inn_cv_type_fpos_t_large=yes],
        [inn_cv_type_fpos_t_large=no],
        [inn_cv_type_fpos_t_large=no])])
 if test "$inn_cv_type_fpos_t_large" = yes ; then
    AC_DEFINE([HAVE_LARGE_FPOS_T], 1,
        [Define if fpos_t is at least 64 bits and compatible with off_t.])
 fi])

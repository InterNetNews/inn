dnl vamacros.m4 -- Check for support for variadic macros.
dnl $Id$
dnl
dnl This file defines two macros for probing for compiler support for variadic
dnl macros.  Provided are INN_C_C99_VAMACROS, which checks for support for the
dnl C99 variadic macro syntax, namely:
dnl
dnl     #define macro(...) fprintf(stderr, __VA_ARGS__)
dnl
dnl and INN_C_GNU_VAMACROS, which checks for support for the older GNU
dnl variadic macro syntax, namely:
dnl
dnl    #define macro(args...) fprintf(stderr, args)
dnl
dnl They set HAVE_C99_VAMACROS or HAVE_GNU_VAMACROS as appropriate.

AC_DEFUN([INN_C_C99_VAMACROS],
[AC_CACHE_CHECK([for C99 variadic macros], [inn_cv_c_c99_vamacros],
[AC_COMPILE_IFELSE(
[AC_LANG_PROGRAM([[
#include <stdio.h>
#define error(...) fprintf(stderr, __VA_ARGS__)]],
[[error("foo"); error("foo %d", 0); return 0;]])],
[inn_cv_c_c99_vamacros=yes], [inn_cv_c_c99_vamacros=no])])
 if test $inn_cv_c_c99_vamacros = yes ; then
    AC_DEFINE([HAVE_C99_VAMACROS], 1,
              [Define if the compiler supports C99 variadic macros.])
 fi])

AC_DEFUN([INN_C_GNU_VAMACROS],
[AC_CACHE_CHECK([for GNU-style variadic macros], [inn_cv_c_gnu_vamacros],
[AC_COMPILE_IFELSE(
[AC_LANG_PROGRAM([[
#include <stdio.h>
#define error(args...) fprintf(stderr, args)]],
[[error("foo"); error("foo %d", 0); return 0;]])],
[inn_cv_c_gnu_vamacros=yes], [inn_cv_c_gnu_vamacros=no])])
 if test $inn_cv_c_gnu_vamacros = yes ; then
    AC_DEFINE([HAVE_GNU_VAMACROS], 1,
              [Define if the compiler supports GNU-style variadic macros.])
 fi])

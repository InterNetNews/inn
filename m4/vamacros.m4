dnl Check for support for variadic macros.
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
dnl
dnl The canonical version of this file is maintained in the rra-c-util
dnl package, available at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
dnl
dnl Written by Russ Allbery <eagle@eyrie.org>
dnl Copyright 2006, 2008, 2009
dnl     The Board of Trustees of the Leland Stanford Junior University
dnl
dnl This file is free software; the authors give unlimited permission to copy
dnl and/or distribute it, with or without modifications, as long as this
dnl notice is preserved.

AC_DEFUN([_INN_C_C99_VAMACROS_SOURCE], [[
#include <stdio.h>
#define error(...) fprintf(stderr, __VA_ARGS__)

int
main(void) {
    error("foo");
    error("foo %d", 0);
    return 0;
}
]])

AC_DEFUN([INN_C_C99_VAMACROS],
[AC_CACHE_CHECK([for C99 variadic macros], [inn_cv_c_c99_vamacros],
    [AC_COMPILE_IFELSE([AC_LANG_SOURCE([_INN_C_C99_VAMACROS_SOURCE])],
        [inn_cv_c_c99_vamacros=yes],
        [inn_cv_c_c99_vamacros=no])])
 AS_IF([test x"$inn_cv_c_c99_vamacros" = xyes],
    [AC_DEFINE([HAVE_C99_VAMACROS], 1,
        [Define if the compiler supports C99 variadic macros.])])])

AC_DEFUN([_INN_C_GNU_VAMACROS_SOURCE], [[
#include <stdio.h>
#define error(args...) fprintf(stderr, args)

int
main(void) {
    error("foo");
    error("foo %d", 0);
    return 0;
}
]])

AC_DEFUN([INN_C_GNU_VAMACROS],
[AC_CACHE_CHECK([for GNU-style variadic macros], [inn_cv_c_gnu_vamacros],
    [AC_COMPILE_IFELSE([AC_LANG_SOURCE([_INN_C_GNU_VAMACROS_SOURCE])],
        [inn_cv_c_gnu_vamacros=yes],
        [inn_cv_c_gnu_vamacros=no])])
 AS_IF([test x"$inn_cv_c_gnu_vamacros" = xyes],
    [AC_DEFINE([HAVE_GNU_VAMACROS], 1,
        [Define if the compiler supports GNU-style variadic macros.])])])

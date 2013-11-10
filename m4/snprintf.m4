dnl Test for a working C99 snprintf.
dnl $Id$
dnl
dnl Check for a working snprintf.  Some systems have an snprintf that doesn't
dnl nul-terminate if the buffer isn't large enough.  Others return -1 if the
dnl string doesn't fit into the buffer instead of returning the number of
dnl characters that would have been formatted.  Still others don't support
dnl NULL as the buffer argument (just to get a count of the formatted length).
dnl
dnl Provides INN_FUNC_SNPRINTF, which adds snprintf.o to LIBOBJS unless a
dnl fully working snprintf is found.
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

dnl Source used by INN_FUNC_SNPRINTF.
AC_DEFUN([_INN_FUNC_SNPRINTF_SOURCE], [[
#include <stdio.h>
#include <stdarg.h>

char buf[2];

int
test(char *format, ...)
{
    va_list args;
    int count;

    va_start(args, format);
    count = vsnprintf(buf, sizeof buf, format, args);
    va_end(args);
    return count;
}

int
main()
{
    return ((test("%s", "abcd") == 4 && buf[0] == 'a' && buf[1] == '\0'
             && snprintf(NULL, 0, "%s", "abcd") == 4) ? 0 : 1);
}
]])

dnl The user-callable test.
AC_DEFUN([INN_FUNC_SNPRINTF],
[AC_CACHE_CHECK([for working snprintf], [inn_cv_func_snprintf_works],
    [AC_RUN_IFELSE([AC_LANG_SOURCE([_INN_FUNC_SNPRINTF_SOURCE])],
        [inn_cv_func_snprintf_works=yes],
        [inn_cv_func_snprintf_works=no],
        [inn_cv_func_snprintf_works=no])])
 AS_IF([test x"$inn_cv_func_snprintf_works" = xyes],
    [AC_DEFINE([HAVE_SNPRINTF], 1,
        [Define if your system has a working snprintf function.])],
    [AC_LIBOBJ([snprintf])])])

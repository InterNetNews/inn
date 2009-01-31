dnl getaddrinfo.m4 -- Check for working getaddrinfo flags.
dnl $Id$
dnl
dnl Check whether the AI_ADDRCONFIG flag is present and working
dnl with getaddrinfo.
dnl Provides INN_FUNC_GETADDRINFO_ADDRCONFIG and defines
dnl HAVE_GETADDRINFO_ADDRCONFIG if AI_ADDRCONFIG works.

dnl Source used by INN_FUNC_GETADDRINFO_ADDRCONFIG.
define([_INN_FUNC_GETADDRINFO_ADDRCONFIG_SOURCE],
[AC_LANG_SOURCE([[
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#if STDC_HEADERS || HAVE_STRING_H
# include <string.h>
#endif

int
main(int argc, char **argv) {
    struct addrinfo hints, *ai;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_ADDRCONFIG;
    return getaddrinfo("localhost", NULL, &hints, &ai) != 0;
}
]])])

dnl The public macro.
AC_DEFUN([INN_FUNC_GETADDRINFO_ADDRCONFIG],
[AC_CACHE_CHECK([for working AI_ADDRCONFIG],
    [inn_cv_func_getaddrinfo_addrconfig_works],
    [AC_RUN_IFELSE([_INN_FUNC_GETADDRINFO_ADDRCONFIG_SOURCE],
        [inn_cv_func_getaddrinfo_addrconfig_works=yes],
        [inn_cv_func_getaddrinfo_addrconfig_works=no],
        [inn_cv_func_getaddrinfo_addrconfig_works=no])])
 if test "$inn_cv_func_getaddrinfo_addrconfig_works" = yes ; then
    AC_DEFINE([HAVE_GETADDRINFO_ADDRCONFIG], 1,
        [Define if your system has a working AI_ADDRCONFIG flag with getaddrinfo.])
 fi])

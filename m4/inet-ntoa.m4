dnl inet-ntoa.m4 -- Check for a working inet_ntoa.
dnl $Id$
dnl
dnl Check whether inet_ntoa is present and working.  Since calling inet_ntoa
dnl involves passing small structs on the stack, present and working versions
dnl may still not function with gcc on some platforms (such as IRIX).
dnl Provides INN_FUNC_INET_NTOA and defines HAVE_INET_NTOA if inet_ntoa is
dnl present and working.

dnl Source used by INN_FUNC_INET_NTOA.
define([_INN_FUNC_INET_NTOA_SOURCE],
[#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#if STDC_HEADERS || HAVE_STRING_H
# include <string.h>
#endif

int
main ()
{
  struct in_addr in;
  in.s_addr = htonl (0x7f000000L);
  return (!strcmp (inet_ntoa (in), "127.0.0.0") ? 0 : 1);
}])

dnl The public macro.
AC_DEFUN([INN_FUNC_INET_NTOA],
[AC_CACHE_CHECK(for working inet_ntoa, inn_cv_func_inet_ntoa_works,
[AC_TRY_RUN(_INN_FUNC_INET_NTOA_SOURCE(),
    [inn_cv_func_inet_ntoa_works=yes],
    [inn_cv_func_inet_ntoa_works=no],
    [inn_cv_func_inet_ntoa_works=no])])
if test "$inn_cv_func_inet_ntoa_works" = yes ; then
    AC_DEFINE([HAVE_INET_NTOA], 1,
        [Define if your system has a working inet_ntoa function.])
else
    AC_LIBOBJ([inet_ntoa])
fi])

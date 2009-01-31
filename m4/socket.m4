dnl socket.m4 -- Various checks for socket support and macros.
dnl $Id$
dnl
dnl This is a collection of various Autoconf macros for checking networking
dnl and socket properties.  The macros provided are:
dnl
dnl     INN_MACRO_IN6_ARE_ADDR_EQUAL
dnl     INN_MACRO_SA_LEN
dnl     INN_MACRO_SUN_LEN
dnl     INN_SYS_UNIX_SOCKETS
dnl
dnl Most of them use a separate internal source macro to make the code easier
dnl to read.

dnl Source used by INN_IN6_EQ_BROKEN.  Test borrowed from a bug report by
dnl tmoestl@gmx.net for glibc.
AC_DEFUN([_INN_MACRO_IN6_ARE_ADDR_EQUAL_SOURCE],
[AC_LANG_SOURCE([[
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int
main (void)
{
  struct in6_addr a;
  struct in6_addr b;

  inet_pton (AF_INET6, "fe80::1234:5678:abcd", &a);
  inet_pton (AF_INET6, "fe80::1234:5678:abcd", &b);
  return IN6_ARE_ADDR_EQUAL (&a,&b) ? 0 : 1;
}
]])])

dnl Check whether the IN6_ARE_ADDR_EQUAL macro is broken (like glibc 2.1.3) or
dnl missing.
AC_DEFUN([INN_MACRO_IN6_ARE_ADDR_EQUAL],
[AC_CACHE_CHECK([whether IN6_ARE_ADDR_EQUAL macro is broken],
    [inn_cv_in6_are_addr_equal_broken],
    [AC_RUN_IFELSE([_INN_MACRO_IN6_ARE_ADDR_EQUAL_SOURCE],
        [inn_cv_in6_are_addr_equal_broken=no],
        [inn_cv_in6_are_addr_equal_broken=yes],
        [inn_cv_in6_are_addr_equal_broken=yes])])
 if test x"$inn_cv_in6_are_addr_equal_broken" = xyes ; then
    AC_DEFINE([HAVE_BROKEN_IN6_ARE_ADDR_EQUAL], 1,
        [Define if your IN6_ARE_ADDR_EQUAL macro is broken.])
 fi])

dnl Check whether the SA_LEN macro is available.  This should give the length
dnl of a struct sockaddr regardless of type.
AC_DEFUN([INN_MACRO_SA_LEN],
[AC_CACHE_CHECK([for SA_LEN macro], [inn_cv_sa_len_macro],
[AC_LINK_IFELSE(
[AC_LANG_PROGRAM([[
#include <sys/types.h>
#include <sys/socket.h>]],
    [[struct sockaddr sa; int x = SA_LEN(&sa);]])],
    [inn_cv_sa_len_macro=yes],
    [inn_cv_sa_len_macro=no])])
 if test "$inn_cv_sa_len_macro" = yes ; then
    AC_DEFINE([HAVE_SA_LEN], 1,
        [Define if <sys/socket.h> defines the SA_LEN macro])
 fi])

dnl Check for SUN_LEN, which returns the size of a struct socket regardless of
dnl its type.  This macro is required POSIX.1g but not that widespread yet.
dnl Sets HAVE_SUN_LEN if the macro is available.
AC_DEFUN([INN_MACRO_SUN_LEN],
[AC_CACHE_CHECK([for SUN_LEN macro], [inn_cv_macro_sun_len],
[AC_LINK_IFELSE(
[AC_LANG_PROGRAM([[
#include <sys/types.h>
#include <sys/un.h>]],
    [[struct sockaddr_un s_un; int i; i = SUN_LEN(&s_un);]])],
    inn_cv_macro_sun_len=yes,
    inn_cv_macro_sun_len=no)])
 if test x"$inn_cv_macro_sun_len" = xyes ; then
    AC_DEFINE([HAVE_SUN_LEN], 1,
        [Define if <sys/un.h> defines the SUN_LEN macro.])
 fi])

dnl Check if Unix domain sockets are supported.  Assume that they are if
dnl AF_UNIX is set in <sys/socket.h>.  This loses on really old versions of
dnl Linux, where AF_UNIX is available but doesn't work, but we don't care
dnl about Linux 1.0 any more.
AC_DEFUN([INN_SYS_UNIX_SOCKETS],
[AC_CACHE_CHECK([for Unix domain sockets], [inn_cv_sys_unix_sockets],
[AC_EGREP_CPP(yes,
[#include <sys/socket.h>
#ifdef AF_UNIX
yes
#endif],
    [inn_cv_sys_unix_sockets=yes],
    [inn_cv_sys_unix_sockets=no])])
if test $inn_cv_sys_unix_sockets = yes ; then
    AC_DEFINE([HAVE_UNIX_DOMAIN_SOCKETS], 1,
        [Define if you have unix domain sockets.])
fi])

dnl socket.m4 -- Various checks for socket support and macros.
dnl $Id$
dnl
dnl This is a collection of various Autoconf macros for checking networking
dnl and socket properties.  The macros provided are:
dnl
dnl     INN_FUNC_GETADDRINFO_ADDRCONFIG
dnl     INN_MACRO_IN6_ARE_ADDR_EQUAL
dnl     INN_MACRO_SA_LEN
dnl     INN_MACRO_SUN_LEN
dnl     INN_SYS_UNIX_SOCKETS
dnl
dnl They use a separate internal source macro to make the code easier to read.
dnl
dnl Copyright 2008, 2009 Board of Trustees, Leland Stanford Jr. University
dnl Copyright (c) 2004, 2005, 2006, 2007, 2008, 2009
dnl     by Internet Systems Consortium, Inc. ("ISC")
dnl Copyright (c) 1991, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001,
dnl     2002, 2003 by The Internet Software Consortium and Rich Salz
dnl
dnl See LICENSE for licensing terms.

dnl Source used by INN_FUNC_GETADDRINFO_ADDRCONFIG.
AC_DEFUN([_INN_FUNC_GETADDRINFO_ADDRCONFIG_SOURCE], [[
#include <netdb.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>

int
main(void) {
    struct addrinfo hints, *ai;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_ADDRCONFIG;
    return (getaddrinfo("localhost", NULL, &hints, &ai) != 0);
}
]])

dnl Check whether the AI_ADDRCONFIG flag works properly with getaddrinfo.
dnl If so, set HAVE_GETADDRINFO_ADDRCONFIG.
AC_DEFUN([INN_FUNC_GETADDRINFO_ADDRCONFIG],
[AC_CACHE_CHECK([for working AI_ADDRCONFIG flag],
    [inn_cv_func_getaddrinfo_addrconfig_works],
    [AC_RUN_IFELSE([AC_LANG_SOURCE([_INN_FUNC_GETADDRINFO_ADDRCONFIG_SOURCE])],
        [inn_cv_func_getaddrinfo_addrconfig_works=yes],
        [inn_cv_func_getaddrinfo_addrconfig_works=no],
        [inn_cv_func_getaddrinfo_addrconfig_works=no])])
 AS_IF([test x"$inn_cv_func_getaddrinfo_addrconfig_works" = xyes],
    [AC_DEFINE([HAVE_GETADDRINFO_ADDRCONFIG], 1,
        [Define if the AI_ADDRCONFIG flag works with getaddrinfo.])])])

dnl Source used by INN_IN6_EQ_BROKEN.  Test borrowed from a bug report by
dnl tmoestl@gmx.net for glibc.
AC_DEFUN([_INN_MACRO_IN6_ARE_ADDR_EQUAL_SOURCE], [[
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int
main (void)
{
    struct in6_addr a;
    struct in6_addr b;

    inet_pton(AF_INET6, "fe80::1234:5678:abcd", &a);
    inet_pton(AF_INET6, "fe80::1234:5678:abcd", &b);
    return IN6_ARE_ADDR_EQUAL(&a, &b) ? 0 : 1;
}
]])

dnl Check whether the IN6_ARE_ADDR_EQUAL macro is broken (like glibc 2.1.3) or
dnl missing.
AC_DEFUN([INN_MACRO_IN6_ARE_ADDR_EQUAL],
[AC_CACHE_CHECK([whether IN6_ARE_ADDR_EQUAL macro is broken],
    [inn_cv_in6_are_addr_equal_broken],
    [AC_RUN_IFELSE([AC_LANG_SOURCE([_INN_MACRO_IN6_ARE_ADDR_EQUAL_SOURCE])],
        [inn_cv_in6_are_addr_equal_broken=no],
        [inn_cv_in6_are_addr_equal_broken=yes],
        [inn_cv_in6_are_addr_equal_broken=yes])])
 AS_IF([test x"$inn_cv_in6_are_addr_equal_broken" = xyes],
    [AC_DEFINE([HAVE_BROKEN_IN6_ARE_ADDR_EQUAL], 1,
        [Define if your IN6_ARE_ADDR_EQUAL macro is broken.])])])

dnl Source used by INN_MACRO_SA_LEN.
AC_DEFUN([_INN_MACRO_SA_LEN_SOURCE], [[
#include <sys/types.h>
#include <sys/socket.h>

int
main(void)
{
    struct sockaddr sa;
    int x = SA_LEN(&sa);
}
]])

dnl Check whether the SA_LEN macro is available.  This should give the length
dnl of a struct sockaddr regardless of type.
AC_DEFUN([INN_MACRO_SA_LEN],
[AC_CACHE_CHECK([for SA_LEN macro], [inn_cv_sa_len_macro],
    [AC_LINK_IFELSE([AC_LANG_SOURCE([_INN_MACRO_SA_LEN_SOURCE])],
        [inn_cv_sa_len_macro=yes],
        [inn_cv_sa_len_macro=no])])
 AS_IF([test x"$inn_cv_sa_len_macro" = xyes],
    [AC_DEFINE([HAVE_SA_LEN], 1,
        [Define if <sys/socket.h> defines the SA_LEN macro.])])])

dnl Source used by INN_MACRO_SUN_LEN.
AC_DEFUN([_INN_MACRO_SUN_LEN_SOURCE], [[
#include <sys/types.h>
#include <sys/un.h>

int
main(void)
{
    struct sockaddr_un s_un;
    int i = SUN_LEN(&s_un);
}
]])

dnl Check for SUN_LEN, which returns the size of a struct socket regardless of
dnl its type.  This macro is required POSIX.1g but not that widespread yet.
dnl Sets HAVE_SUN_LEN if the macro is available.
AC_DEFUN([INN_MACRO_SUN_LEN],
[AC_CACHE_CHECK([for SUN_LEN macro], [inn_cv_sun_len_macro],
    [AC_LINK_IFELSE([AC_LANG_SOURCE([_INN_MACRO_SUN_LEN_SOURCE])],
        [inn_cv_sun_len_macro=yes],
        [inn_cv_sun_len_macro=no])])
 AS_IF([test x"$inn_cv_sun_len_macro" = xyes],
    [AC_DEFINE([HAVE_SUN_LEN], 1,
        [Define if <sys/un.h> defines the SUN_LEN macro.])])])

dnl Source used by INN_SYS_UNIX_SOCKETS.
AC_DEFUN([_INN_SYS_UNIX_SOCKETS], [[
#include <sys/types.h>
#include <sys/socket.h>
#ifndef AF_UNIX
error:  No Unix domain sockets!
#endif
]])

dnl Check if Unix domain sockets are supported.  Assume that they are if
dnl AF_UNIX is set in <sys/socket.h>.  This loses on really old versions of
dnl Linux, where AF_UNIX is available but doesn't work, but we don't care
dnl about Linux 1.0 any more.
AC_DEFUN([INN_SYS_UNIX_SOCKETS],
[AC_CACHE_CHECK([for Unix domain sockets], [inn_cv_sys_unix_sockets],
    [AC_COMPILE_IFELSE([AC_LANG_SOURCE([_INN_SYS_UNIX_SOCKETS])],
        [inn_cv_sys_unix_sockets=yes],
        [inn_cv_sys_unix_sockets=no])])
 AS_IF([test x"$inn_cv_sys_unix_sockets" = xyes],
    [AC_DEFINE([HAVE_UNIX_DOMAIN_SOCKETS], 1,
        [Define if you have Unix domain sockets.])])])


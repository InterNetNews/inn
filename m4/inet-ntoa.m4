dnl inet-ntoa.m4 -- Check for a working inet_ntoa.
dnl $Id$
dnl
dnl Check whether inet_ntoa is present and working.  Since calling inet_ntoa
dnl involves passing small structs on the stack, present and working versions
dnl may still not function with gcc on some platforms (such as IRIX).
dnl Provides INN_FUNC_INET_NTOA and defines HAVE_INET_NTOA if inet_ntoa is
dnl present and working.
dnl
dnl Copyright 2008, 2009 Board of Trustees, Leland Stanford Jr. University
dnl Copyright (c) 2004, 2005, 2006, 2007
dnl     by Internet Systems Consortium, Inc. ("ISC")
dnl Copyright (c) 1991, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001,
dnl     2002, 2003 by The Internet Software Consortium and Rich Salz
dnl
dnl See LICENSE for licensing terms.

dnl Source used by INN_FUNC_INET_NTOA.
AC_DEFUN([_INN_FUNC_INET_NTOA_SOURCE], [[
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

int
main(void)
{
    struct in_addr in;
    in.s_addr = htonl(0x7f000000L);
    return (strcmp(inet_ntoa(in), "127.0.0.0") == 0) ? 0 : 1;
}
]])

dnl The public macro.
AC_DEFUN([INN_FUNC_INET_NTOA],
[AC_CACHE_CHECK(for working inet_ntoa, inn_cv_func_inet_ntoa_works,
    [AC_RUN_IFELSE([AC_LANG_SOURCE([_INN_FUNC_INET_NTOA_SOURCE])],
        [inn_cv_func_inet_ntoa_works=yes],
        [inn_cv_func_inet_ntoa_works=no],
        [inn_cv_func_inet_ntoa_works=no])])
 AS_IF([test "$inn_cv_func_inet_ntoa_works" = yes],
    [AC_DEFINE([HAVE_INET_NTOA], 1,
        [Define if your system has a working inet_ntoa function.])],
    [AC_LIBOBJ([inet_ntoa])])])

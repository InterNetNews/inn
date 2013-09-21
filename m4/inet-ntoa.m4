dnl Check for a working inet_ntoa.
dnl $Id$
dnl
dnl Check whether inet_ntoa is present and working.  Since calling inet_ntoa
dnl involves passing small structs on the stack, present and working versions
dnl may still not function with gcc on some platforms (such as IRIX).
dnl Provides INN_FUNC_INET_NTOA and defines HAVE_INET_NTOA if inet_ntoa is
dnl present and working.
dnl
dnl The canonical version of this file is maintained in the rra-c-util
dnl package, available at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
dnl
dnl Copyright 1999, 2000, 2001, 2003 Russ Allbery <rra@stanford.edu>
dnl Copyright 2008, 2009
dnl     The Board of Trustees of the Leland Stanford Junior University
dnl
dnl This file is free software; the authors give unlimited permission to copy
dnl and/or distribute it, with or without modifications, as long as this
dnl notice is preserved.

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
 AS_IF([test x"$inn_cv_func_inet_ntoa_works" = xyes],
    [AC_DEFINE([HAVE_INET_NTOA], 1,
        [Define if your system has a working inet_ntoa function.])],
    [AC_LIBOBJ([inet_ntoa])])])

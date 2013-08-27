dnl Determine whether PAM uses const in prototypes.
dnl $Id$
dnl
dnl Linux marks several PAM arguments const, including the argument to
dnl pam_get_item and some arguments to conversation functions, which Solaris
dnl doesn't.  This test tries to determine which style is in use to select
dnl whether to declare variables const in order to avoid compiler warnings.
dnl
dnl Since this is just for compiler warnings, it's not horribly important if
dnl we guess wrong.  This test is ugly, but it seems to work.
dnl
dnl Contributed by Markus Moeller.
dnl
dnl The canonical version of this file is maintained in the rra-c-util
dnl package, available at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
dnl
dnl Copyright 2007 Russ Allbery <rra@stanford.edu>
dnl Copyright 2007, 2008 Markus Moeller
dnl
dnl This file is free software; the authors give unlimited permission to copy
dnl and/or distribute it, with or without modifications, as long as this
dnl notice is preserved.

dnl Source used by INN_HEADER_PAM_CONST.
AC_DEFUN([_INN_HEADER_PAM_CONST_SOURCE],
[#ifdef HAVE_SECURITY_PAM_APPL_H
# include <security/pam_appl.h>
#else
# include <pam/pam_appl.h>
#endif
])

AC_DEFUN([INN_HEADER_PAM_CONST],
[AC_CACHE_CHECK([whether PAM prefers const], [inn_cv_header_pam_const],
    [AC_EGREP_CPP([const void \*\* *_?item], _INN_HEADER_PAM_CONST_SOURCE(),
        [inn_cv_header_pam_const=yes], [inn_cv_header_pam_const=no])])
AS_IF([test x"$inn_cv_header_pam_const" = xyes],
    [inn_header_pam_const=const], [inn_header_pam_const=])
AC_DEFINE_UNQUOTED([PAM_CONST], [$inn_header_pam_const],
    [Define to const if PAM uses const in pam_get_item, empty otherwise.])])

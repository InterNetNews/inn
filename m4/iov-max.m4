dnl iov-max.m4 -- Probe for the maximum number of iovecs accepted by writev.
dnl $Id$
dnl
dnl Check for the maximum number of elements in an iovec (IOV_MAX).  SVr4
dnl systems appear to use that name for this limit (checked Solaris 2.6, IRIX
dnl 6.5, and HP-UX 11.00).  Linux doesn't have it, but instead has UIO_MAXIOV
dnl defined in <bits/uio.h> or <iovec.h> included from <sys/uio.h>.
dnl
dnl The platforms that have IOV_MAX appear to also offer it via sysconf(3),
dnl but we don't currently handle dynamic values.
dnl
dnl If IOV_MAX is not defined by <sys/uio.h> or <limits.h>, probe for its
dnl value by checking writev calls up to 1024 members of an iovec and set it
dnl to an appropriate value.

dnl Source used by INN_MACRO_IOV_MAX.
define([_INN_MACRO_IOV_MAX_SOURCE],
[AC_LANG_SOURCE([[
#include <sys/types.h>
#include <stdio.h>
#include <sys/uio.h>
#include <errno.h>
#include <fcntl.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_LIMITS_H
# include <limits.h>
#endif

int
main ()
{
  int fd, size;
  struct iovec array[1024];
  char data;

  FILE *f = fopen ("conftestval", "w");
  if (f == NULL)
    return 1;
#ifdef IOV_MAX
  fprintf (f, "set in limits.h\n");
#else
# ifdef UIO_MAXIOV
  fprintf (f, "%d\n", UIO_MAXIOV);
# else
  fd = open ("/dev/null", O_WRONLY, 0666);
  if (fd < 0)
    return 1;
  for (size = 1; size <= 1024; size++)
    {
      array[size - 1].iov_base = &data;
      array[size - 1].iov_len = sizeof data;
      if (writev (fd, array, size) < 0)
        {
          if (errno != EINVAL)
            return 1;
          fprintf (f, "%d\n", size - 1);
          exit (0);
        }
    }
  fprintf (f, "1024\n");
# endif /* UIO_MAXIOV */
#endif /* IOV_MAX */
  return 0;
}
]])])

dnl Do the actual check.
AC_DEFUN([INN_MACRO_IOV_MAX],
[AC_CACHE_CHECK([value of IOV_MAX],
    [inn_cv_macro_iov_max],
    [AC_RUN_IFELSE([_INN_MACRO_IOV_MAX_SOURCE],
        inn_cv_macro_iov_max=`cat conftestval`,
        inn_cv_macro_iov_max=error,
        16)
     if test x"$inn_cv_macro_iov_max" = xerror ; then
         AC_MSG_WARN([probe failure, assuming 16])
         inn_cv_macro_iov_max=16
     fi])
 if test x"$inn_cv_macro_iov_max" != x"set in limits.h" ; then
    AC_DEFINE_UNQUOTED(IOV_MAX, $inn_cv_macro_iov_max,
                       [Define to the max vectors in an iovec.])
 fi])

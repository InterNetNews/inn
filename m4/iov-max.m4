dnl Probe for the maximum number of iovecs accepted by writev.
dnl
dnl Written by Russ Allbery in 2003.
dnl Various bug fixes, code and documentation improvements since then
dnl in 2009, 2019, 2021, 2022, 2025.
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
AC_DEFUN([_INN_MACRO_IOV_MAX_SOURCE], [[
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/uio.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

int
main()
{
    int fd, size;
    struct iovec array[1024];
    char data;

    FILE *f = fopen("conftestval", "w");
    if (f == NULL)
        return 1;
#ifdef UIO_MAXIOV
    fprintf(f, "%d\n", UIO_MAXIOV);
#else
    fd = open("/dev/null", O_WRONLY, 0666);
    if (fd < 0)
        return 1;
    for (size = 1; size <= 1024; size++) {
        array[size - 1].iov_base = &data;
        array[size - 1].iov_len = sizeof data;
        if (writev(fd, array, size) < 0) {
            if (errno != EINVAL)
                return 1;
            fprintf(f, "%d\n", size - 1);
            exit(0);
        }
    }
    fprintf(f, "1024\n");
#endif /* UIO_MAXIOV */
    return 0;
}
]])

dnl Headers to use for checking for an IOV_MAX definition.
AC_DEFUN([_INN_MACRO_IOV_MAX_HEADERS], [AC_INCLUDES_DEFAULT] [[
#include <limits.h>
]])

dnl Do the actual check.
AC_DEFUN([INN_MACRO_IOV_MAX],
[AC_CHECK_DECL([IOV_MAX], [],
    [AC_CACHE_CHECK([value of IOV_MAX],
        [inn_cv_macro_iov_max],
        [AC_RUN_IFELSE([AC_LANG_SOURCE([_INN_MACRO_IOV_MAX_SOURCE])],
            [inn_cv_macro_iov_max=`cat conftestval`],
            [inn_cv_macro_iov_max=error],
            [16])
         AS_IF([test x"$inn_cv_macro_iov_max" = xerror],
            [AC_MSG_WARN([probe failure, assuming 16])
             inn_cv_macro_iov_max=16])])
     AC_DEFINE_UNQUOTED([IOV_MAX], [$inn_cv_macro_iov_max],
        [Define to the max vectors in an iovec.])],
    [_INN_MACRO_IOV_MAX_HEADERS])])

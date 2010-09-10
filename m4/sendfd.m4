dnl sendfd.m4 -- Check whether I_SENDFD/I_RECVFD is supported.
dnl $Id$
dnl
dnl Check whether the system supports STREAMS and can send and receive file
dnl descriptors via the I_SENDFD and I_RECVFD ioctls.  Provides
dnl INN_SYS_STREAMS_SENDFD and defines HAVE_STREAMS_SENDFD if this facility is
dnl available.

dnl Source used by INN_SYS_STREAMS_SENDFD.
define([_INN_SYS_STREAMS_SENDFD],
[AC_LANG_SOURCE([[
#include <stdio.h>
#if HAVE_STRING_H
# include <string.h>
#endif
#include <stropts.h>
#include <sys/types.h>
#include <sys/socket.h>
#if HAVE_UNISTD_H
# include <unistd.h>
#endif

int
main ()
{
  int fd[2], parent;

  pipe(fd);
  if (!isastream (fd[0]))
    {
      fprintf (stderr, "%d is not a stream\n", fd[0]);
      return 1;
    }
  if (fork () == 0)
    {
      int child;

      close (fd[0]);
      child = socket (AF_INET, SOCK_STREAM, 0);
      if (child < 0)
        return 1;
      if (ioctl (fd[1], I_SENDFD, child) < 0)
        return 1;
      return 0;
    }
  else
    {
      struct strrecvfd fdrec;

      memset (&fdrec, 0, sizeof(fdrec));
      close (fd[1]);
      if (ioctl (fd[0], I_RECVFD, &fdrec) < 0)
        {
          perror("ioctl");
          return 1;
        }
      if (fdrec.fd < 0)
        {
          fprintf(stderr, "Bad file descriptor %d\n", fd);
          return 1;
        }
      return 0;
    }
}
]])])

dnl The public macro.
AC_DEFUN([INN_SYS_STREAMS_SENDFD],
[AC_CACHE_CHECK([whether STREAMS fd passing is supported],
    [inn_cv_sys_streams_sendfd],
    [AC_RUN_IFELSE([_INN_SYS_STREAMS_SENDFD],
        [inn_cv_sys_streams_sendfd=yes],
        [inn_cv_sys_streams_sendfd=no],
        [inn_cv_sys_streams_sendfd=no])])
 if test "$inn_cv_sys_streams_sendfd" = yes ; then
    AC_DEFINE([HAVE_STREAMS_SENDFD], 1,
        [Define if your system supports STREAMS file descriptor passing.])
 fi])

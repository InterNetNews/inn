dnl mmap.m4 -- Probe for mmap properties.
dnl $Id$
dnl
dnl The mmap macro that comes with Autoconf doesn't do anything useful.
dnl Define a new INN_FUNC_MMAP that probes for a working mmap that supports
dnl shared, non-fixed maps.  This function defines HAVE_MMAP if mmap appears
dnl to work, and takes an action if found argument that can be used to make
dnl other probes.
dnl
dnl Provide INN_FUNC_MMAP_NEEDS_MSYNC, which defines MMAP_NEEDS_MSYNC if
dnl reading from an open file doesn't see changes made to that file through
dnl mmap without an intervening msync.
dnl
dnl Provide INN_FUNC_MMAP_MISSES_WRITES, which defines MMAP_MISSES_WRITES if
dnl changes to a file made with write aren't seen in an mmaped region without
dnl an intervening msync.
dnl
dnl (The above two macros together in essence probe for whether the operating
dnl system has a unified page cache.)
dnl
dnl Finally, provide AC_FUNC_MSYNC_ARGS, which defines HAVE_MSYNC_3_ARGS if
dnl msync takes three arguments (as on Solaris and Linux) rather than two
dnl (most other operating systems).

dnl Source used by INN_FUNC_MMAP.
define([_INN_FUNC_MMAP_SOURCE],
[AC_LANG_SOURCE([[
#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>

int
main()
{
  int *data, *data2;
  int i, fd;

  /* First, make a file with some known garbage in it.  Use something
     larger than one page but still an odd page size. */
  data = malloc (20000);
  if (!data)
    return 1;
  for (i = 0; i < 20000 / sizeof (int); i++)
    data[i] = rand();
  umask (0);
  fd = creat ("conftestmmaps", 0600);
  if (fd < 0)
    return 1;
  if (write (fd, data, 20000) != 20000)
    return 1;
  close (fd);

  /* Next, try to mmap the file and make sure we see the same garbage. */
  fd = open ("conftestmmaps", O_RDWR);
  if (fd < 0)
    return 1;
  data2 = mmap (0, 20000, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (data2 == (int *) -1)
    return 1;
  for (i = 0; i < 20000 / sizeof (int); i++)
    if (data[i] != data2[i])
      return 1;

  close (fd);
  unlink ("conftestmmaps");
  return 0;
}
]])])

dnl This portion is similar to what AC_FUNC_MMAP does, only it tests shared,
dnl non-fixed mmaps.
AC_DEFUN([INN_FUNC_MMAP],
[AC_CACHE_CHECK([for working mmap], [inn_cv_func_mmap],
[AC_RUN_IFELSE([_INN_FUNC_MMAP_SOURCE],
    [inn_cv_func_mmap=yes],
    [inn_cv_func_mmap=no],
    [inn_cv_func_mmap=no])])
 if test $inn_cv_func_mmap = yes ; then
    AC_DEFINE([HAVE_MMAP], 1,
        [Define if mmap exists and works for shared, non-fixed maps.])
    $1
 else
    :
    $2
 fi])

dnl Source used by INN_FUNC_MMAP_NEEDS_MSYNC.
define([_INN_FUNC_MMAP_NEEDS_MSYNC_SOURCE],
[AC_LANG_SOURCE([[
#include <sys/types.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>

int
main()
{
  int *data, *data2;
  int i, fd;

  /* First, make a file with some known garbage in it.  Use something
     larger than one page but still an odd page size. */
  data = malloc (20000);
  if (!data)
    return 1;
  for (i = 0; i < 20000 / sizeof (int); i++)
    data[i] = rand();
  umask (0);
  fd = creat ("conftestmmaps", 0600);
  if (fd < 0)
    return 1;
  if (write (fd, data, 20000) != 20000)
    return 1;
  close (fd);

  /* Next, try to mmap the file and make sure we see the same garbage. */
  fd = open ("conftestmmaps", O_RDWR);
  if (fd < 0)
    return 1;
  data2 = mmap (0, 20000, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (data2 == (int *) -1)
    return 1;

  /* Finally, see if changes made to the mmaped region propagate back to
     the file as seen by read (meaning that msync isn't needed). */
  for (i = 0; i < 20000 / sizeof (int); i++)
    data2[i]++;
  if (read (fd, data, 20000) != 20000)
    return 1;
  for (i = 0; i < 20000 / sizeof (int); i++)
    if (data[i] != data2[i])
      return 1;

  close (fd);
  unlink ("conftestmmapm");
  return 0;
}
]])])

dnl Check whether the data read from an open file sees the changes made to an
dnl mmaped region, or if msync has to be called for other applications to see
dnl those changes.
AC_DEFUN([INN_FUNC_MMAP_NEEDS_MSYNC],
[AC_CACHE_CHECK([whether msync is needed], [inn_cv_func_mmap_need_msync],
[AC_RUN_IFELSE([_INN_FUNC_MMAP_NEEDS_MSYNC_SOURCE],
    [inn_cv_func_mmap_need_msync=no],
    [inn_cv_func_mmap_need_msync=yes],
    [inn_cv_func_mmap_need_msync=yes])])
 if test $inn_cv_func_mmap_need_msync = yes ; then
    AC_DEFINE([MMAP_NEEDS_MSYNC], 1,
       [Define if you need to call msync for calls to read to see changes.])
 fi])

dnl Source used by INN_FUNC_MMAP_SEES_WRITES.
define([_INN_FUNC_MMAP_SEES_WRITES_SOURCE],
[AC_LANG_SOURCE([[
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <sys/mman.h>

/* Fractional page is probably worst case. */
static char zbuff[1024];
static char fname[] = "conftestw";

int
main ()
{
  char *map;
  int i, fd;

  fd = open (fname, O_RDWR | O_CREAT, 0660);
  if (fd < 0)
    return 1;
  unlink (fname);
  write (fd, zbuff, sizeof (zbuff));
  lseek (fd, 0, SEEK_SET);
  map = mmap (0, sizeof (zbuff), PROT_READ, MAP_SHARED, fd, 0);
  if (map == (char *) -1)
    return 2;
  for (i = 0; fname[i]; i++)
    {
      if (write (fd, &fname[i], 1) != 1)
        return 3;
      if (map[i] != fname[i])
        return 4;
    }
  return 0;
}
]])])

dnl Check if an mmaped region will see writes made to the underlying file
dnl without an intervening msync.
AC_DEFUN([INN_FUNC_MMAP_SEES_WRITES],
[AC_CACHE_CHECK([whether mmap sees writes], [inn_cv_func_mmap_sees_writes],
[AC_RUN_IFELSE([INN_FUNC_MMAP_SEES_WRITES_SOURCE],
    [inn_cv_func_mmap_sees_writes=yes],
    [inn_cv_func_mmap_sees_writes=no],
    [inn_cv_func_mmap_sees_writes=no])])
 if test $inn_cv_func_mmap_sees_writes = no ; then
    AC_DEFINE([MMAP_MISSES_WRITES], 1,
        [Define if you need to call msync after writes.])
 fi])

dnl Check whether msync takes three arguments.  (It takes three arguments on
dnl Solaris and Linux, two arguments on BSDI.)
AC_DEFUN([INN_FUNC_MSYNC_ARGS],
[AC_CACHE_CHECK([how many arguments msync takes], [inn_cv_func_msync_args],
[AC_COMPILE_IFELSE(
[AC_LANG_PROGRAM([[
#include <sys/types.h>
#include <sys/mman.h>]],
    [[char *p; int psize; msync (p, psize, MS_ASYNC);]])],
    [inn_cv_func_msync_args=3],
    [inn_cv_func_msync_args=2])])
 if test $inn_cv_func_msync_args = 3 ; then
    AC_DEFINE([HAVE_MSYNC_3_ARG], 1,
        [Define if your msync function takes three arguments.])
 fi])

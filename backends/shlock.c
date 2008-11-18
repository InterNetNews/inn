/*  $Id$
**
**  Produce reliable locks for shell scripts, by Peter Honeyman as told
**  to Rich $alz.
**
**  Modified by Berend Reitsma to solve a race condition between ValidLock
**  and unlink.
*/

#include "config.h"
#include "clibrary.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>

#include "inn/messages.h"

static bool BinaryLock;
static bool JustChecking;

static char CANTUNLINK[] = "shlock: Can't unlink \"%s\", %s";
static char CANTOPEN[] = "shlock: Can't open \"%s\", %s";
static char CANTCHDIR[] = "shlock: Can't chdir to \"%s\", %s";
static char CANTWRITEPID[] = "shlock: Can't write PID to \"%s\", %s";
static char CANTCLOSE[] = "shlock: Can't close \"%s\", %s";
static char CANTLINK[] = "shlock: Can't link \"%s\" to \"%s\", %s";
static char STALELOCK[] = "shlock: Stale lockfile detected \"%s\", please remove";
static char CANTSTAT[] = "shlock: Can't stat \"%s\", %s";

/* After this time, we start to complain about an invalid locked lockfile. */
#define LINK_TIMEOUT 30


/*
**  Open named lockfile, print possible error, and return fd.
*/
static int
OpenLock(char *name)
{
  int fd;
  
  if ((fd = open(name, O_RDONLY)) < 0 && errno != ENOENT && !JustChecking) {
    syswarn(CANTOPEN, name, strerror(errno));
  }

  return fd;
}


/*
**  Read the pid from the lockfile.
**  If the returned pid is 0, it is invalid.
**  fd is closed after the calls to ReadLock().
*/
static int
ReadLock(int fd)
{
  int i;
  pid_t pid;
  char buff[BUFSIZ];

  lseek(fd, 0, SEEK_SET);

  /* Read the pid that is written there. */
  if (BinaryLock) {
    if (read(fd, (char *)&pid, sizeof(pid)) != sizeof(pid)) {
      return 0;
    }
  } else {
    if ((i = read(fd, buff, sizeof(buff) - 1)) <= 0) {
      return 0;
    }
    buff[i] = '\0';
    pid = (pid_t) atol(buff);
  }
  return pid;
}


/*
**  Check if the pid is valid by sending a null signal.
*/
static bool
ValidPid(int pid)
{
  return (pid > 0) && ((kill(pid, 0) >= 0) || (errno != ESRCH));
}


/*
**  Check if the lock is valid.
*/
static bool
ValidLock(int fd)
{
  return ValidPid(ReadLock(fd));
}


/*
**  Unlink a file and print a message on (some) error.
*/
static bool
Unlink(char *name)
{
  if (unlink(name) < 0 && errno != ENOENT) {
    syswarn(CANTUNLINK, name, strerror(errno));
    return false;
  }
  return true;
}


/*
**  Print a usage message and exit.
*/
static void
Usage(void)
{
  fprintf(stderr, "Usage: shlock [-b|-c|-u] -f file -p pid\n");
  exit(1);
}


int
main(int ac, char *av[])
{
  int i;
  char *p;
  int fd;
  char tmp[BUFSIZ];
  char tmp2[BUFSIZ];
  char buff[BUFSIZ];
  char *name;
  pid_t pid;
  bool ok;
  bool do_sleep;

  /* Establish our identity. */
  message_program_name = "shlock";

  /* Set defaults. */
  pid = 0;
  name = NULL;
  do_sleep = false;
  JustChecking = false;
  BinaryLock = false;

  umask(NEWSUMASK);

  /* Parse JCL. */
  while ((i = getopt(ac, av, "bcf:p:u")) != EOF) {
    switch (i) {
      default:
        Usage();
        /* NOTREACHED */
      case 'b':
      case 'u':
        BinaryLock = true;
        break;
      case 'c':
        JustChecking = true;
        break;
      case 'f':
        name = optarg;
        break;
      case 'p':
        pid = (pid_t) atol(optarg);
        break;
    }
  }

  ac -= optind;
  av += optind;
  if (ac || pid == 0 || name == NULL) {
    Usage();
  }

  /* Handle the "-c" flag. */
  if (JustChecking) {
    if ((fd = OpenLock(name)) < 0)
      return 1;
    ok = ValidLock(fd);
    close(fd);
    return ok;
  }

  /* Create the temp file in the same directory as the destination.
   * To prevent (some?) buffer overflows, chdir to the directory first. */
  if ((p = strrchr(name, '/')) != NULL) {
    *p = '\0';
    if (chdir(name) < 0) {
      sysdie(CANTCHDIR, name, strerror(errno));
    }
    name = p + 1;
  }
  snprintf(tmp, sizeof(tmp), "shlock%ld", (long)getpid());

  /* Loop until we can open the file. */
  while ((fd = open(tmp, O_RDWR | O_CREAT | O_EXCL, 0644)) < 0) {
    switch (errno) {
      default:
        /* Unknown error -- give up. */
        sysdie(CANTOPEN, tmp, strerror(errno));
      case EEXIST:
        /* If we can remove the old temporary, retry the open. */
        if (!Unlink(tmp))
          return 1;
        break;
    }
  }

  /* Write the process ID. */
  if (BinaryLock) {
    ok = write(fd, &pid, sizeof(pid)) == sizeof(pid);
  } else {
    /* Don't write exactly sizeof(pid) bytes, someone (like minicom)
     * might treat it as a binary lock.  Add spaces before the PID. */
    snprintf(buff, sizeof(buff), "%10ld\n", (long) pid);
    i = strlen(buff);
    ok = write(fd, buff, i) == i;
  }
  if (!ok) {
    syswarn(CANTWRITEPID, tmp, strerror(errno));
    goto ExitClose;
  }
  if (close(fd) < 0) {
    syswarn(CANTCLOSE, tmp, strerror(errno));
    goto ExitUnlink;
  }

  /* Try to link the temporary to the lockfile. */
  while (link(tmp, name) < 0) {
    switch (errno) {
      default:
        /* Unknown error -- give up. */
        syswarn(CANTLINK, tmp, name, strerror(errno));
        goto ExitUnlink;
      case EEXIST:
      {
        /* File exists (read: existed) at link time; if lock is valid, give up. */
        struct stat stt, st1, st2;
        
        if ((fd = OpenLock(name)) < 0)
          goto ExitUnlink;
        if (ValidLock(fd))
          goto ExitClose;

        /* Use this file to lock the lockfile. */
        snprintf(tmp2, sizeof(tmp2), "%sx", tmp);
        if (!Unlink(tmp2))
          goto ExitUnlink;
        if (stat(tmp, &stt) < 0 || fstat(fd, &st1) < 0) {
          if (errno != ENOENT)
            syswarn(CANTSTAT, tmp, strerror(errno));
          goto ExitClose;
        }

        /* Check if inode possibly changed during our lifetime. */
        if (st1.st_ctime >= stt.st_ctime)
          goto ExitClose;

        /* Check if someone else has already linked to this file. */
        if (st1.st_nlink != 1) {
          /* This only happens when a crash leaves a link or someone is messing around. */
          if (st1.st_ctime + LINK_TIMEOUT < stt.st_ctime)
            syswarn(STALELOCK, name);
          do_sleep = true;
          goto ExitClose;
        }
        if (link(name, tmp2) < 0) {
          if (errno != ENOENT)
            syswarn(CANTLINK, name, tmp2, strerror(errno));
          goto ExitClose;
        }

        /* Check if fd is still pointing to the right file. */
        if (stat(name, &st2) < 0) {
          if (errno != ENOENT)
            syswarn(CANTSTAT, name, strerror(errno));
          goto ExitUnlink2;
        }

        /* Check if someone put another lockfile in place (old race condition). */
        if (st1.st_ino != st2.st_ino)
          goto ExitUnlink2;
        if (st2.st_nlink != 2)
          goto ExitUnlink2;

        /* Still the same file and we locked access, unlink it, and retry. */
        if (!Unlink(name))
          goto ExitUnlink2;
        Unlink(tmp2);
        close(fd);
      }
    }
  }

  Unlink(tmp);
  return 0;

ExitUnlink2:
  Unlink(tmp2);
ExitClose:
  close(fd);
ExitUnlink:
  Unlink(tmp);
  if (do_sleep)
    sleep(1);
  return 1;
}

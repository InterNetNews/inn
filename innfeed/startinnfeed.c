/*
 * $Id$
 *
 * Start innfeed and pass it all the arguments given. Sets up process
 * limits for innfeed.
 */

#if ! defined (USER)
#define USER "news"
#endif

#if ! defined (INNFEED)
#define INNFEED "/usr/news/local/innfeed"
#endif

#include <pwd.h>                /* getpwent */
#include <stdio.h>              /* fprintf */
#include <errno.h>              /* errno, sys_errlist */
#include <unistd.h>             /* setgid, setuid, execve */
#include <stdlib.h>             /* exit */
#include <sys/types.h>          /* setrlimit */
#include <time.h>               /* setrlimit */
#include <sys/time.h>           /* setrlimit */
#include <sys/resource.h>       /* setrlimit */
#include <string.h>

static const char *innfeed = INNFEED;

void
main(int ac, char **av, char **ep)
{
  struct passwd *pwd;
  struct rlimit rl;
  char *progname;

  /* (try to) unlimit datasize and stacksize for us and our children */
  rl.rlim_cur = rl.rlim_max = RLIM_INFINITY;

  if (setrlimit(RLIMIT_DATA, &rl) == -1)
    (void)fprintf(stderr, "%s: setrlimit(RLIMIT_DATA, RLIM_INFINITY): %s\n",
            *av, strerror(errno));
  if (setrlimit(RLIMIT_STACK, &rl) == -1)
    (void)fprintf(stderr, "%s: setrlimit(RLIMIT_STACK, RLIM_INFINITY): %s\n",
            *av, strerror (errno));
  if (setrlimit(RLIMIT_NOFILE, &rl) == -1)
    (void)fprintf(stderr, "%s: setrlimit(RLIMIT_NOFILE, RLIM_INFINITY): %s\n",
            *av, strerror (errno));

  /* stop being root */
  pwd = getpwnam(USER);
  if (pwd == (struct passwd *)NULL)
    (void)fprintf(stderr, "%s: getpwnam(%s): %s\n", *av, USER,
                  strerror (errno));
  else if (setgid(pwd->pw_gid) == -1)
    (void)fprintf(stderr, "%s: setgid(%d): %s\n", *av, pwd->pw_gid,
                  strerror (errno));
  else if (setuid(pwd->pw_uid) == -1)
    (void)fprintf(stderr, "%s: setuid(%d): %s\n", *av, pwd->pw_uid,
                  strerror (errno));
  else 
    {
      char **evp = NULL ;
      progname = av[0];
      av[0] = (char *) innfeed;

#if defined (USE_DMALLOC)
      {
        int i ;
        
        for (i = 0 ; ep[i] != NULL ; i++)
          /* nada */ ;

        evp = (char **) malloc (sizeof (char *) * i + 2) ;
        for (i = 0 ; ep[i] != NULL ; i++)
          evp [i] = ep [i] ;
        evp [i] = "DMALLOC_OPTIONS=debug=0x4e405c3,inter=100,log=innfeed-logfile";
        evp [i+1] = NULL ;
      }
#else
      evp = ep ;
#endif
      
      if (execve(innfeed, av, evp) == -1)
        (void)fprintf(stderr, "%s: execve: %s\n",
                      progname, strerror (errno));
    }
  
  exit(1);
}

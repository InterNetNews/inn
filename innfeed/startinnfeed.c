/*
 * $Id$
 *
 * Start innfeed and pass it all the arguments given. Sets up process
 * limits for innfeed.
 */

#if ! defined (INNFEED)
#define INNFEED "innfeed"
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

#include <syslog.h> 
#include "macros.h"
#include "configdata.h"
#include "clibrary.h"
#include "libinn.h"

int
main(int ac, char **av, char **ep)
{
  struct passwd *pwd;
  struct rlimit rl;
  char *progname;
  char *innfeed;

  if ((progname = strrchr(av[0], '/')) != NULL)
	progname++;
  else
	progname = av[0];

  openlog (progname,(int)(L_OPENLOG_FLAGS|LOG_PID),LOG_INN_PROG) ;

  if (ReadInnConf() < 0) {
      syslog(LOG_ERR, "cant read inn.conf");
      exit(1);
  }

#if	defined(HAVE_RLIMIT)
  /* (try to) unlimit datasize and stacksize for us and our children */
  rl.rlim_cur = rl.rlim_max = RLIM_INFINITY;

  if (setrlimit(RLIMIT_DATA, &rl) == -1)
    syslog(LOG_WARNING, "%s: setrlimit(RLIMIT_DATA, RLIM_INFINITY): %s",
            *av, strerror(errno));
  if (setrlimit(RLIMIT_STACK, &rl) == -1)
    syslog(LOG_WARNING, "%s: setrlimit(RLIMIT_STACK, RLIM_INFINITY): %s",
            *av, strerror (errno));
# ifdef RLIMIT_NOFILE
  if (innconf->rlimitnofile >= 0) {
    getrlimit(RLIMIT_NOFILE, &rl);
    if (innconf->rlimitnofile < rl.rlim_max)
        rl.rlim_max = innconf->rlimitnofile;
    if (innconf->rlimitnofile < rl.rlim_cur)
        rl.rlim_cur = innconf->rlimitnofile;
    if (setrlimit(RLIMIT_NOFILE, &rl) == -1)
      syslog(LOG_WARNING, "%s: setrlimit(RLIMIT_NOFILE, %d): %s",
            *av, rl.rlim_cur, strerror (errno));
  }
# endif /* RLIMIT_NOFILE */
#endif /* HAVE_RLIMIT */

  /* stop being root */
  pwd = getpwnam(NEWSUSER);
  if (pwd == (struct passwd *)NULL)
    syslog(LOG_ERR, "%s: getpwnam(%s): %s", *av, NEWSUSER,
                  strerror (errno));
  else if (setgid(pwd->pw_gid) == -1)
    syslog(LOG_ERR, "%s: setgid(%d): %s", *av, pwd->pw_gid,
                  strerror (errno));
  else if (setuid(pwd->pw_uid) == -1)
    syslog(LOG_ERR, "%s: setuid(%d): %s", *av, pwd->pw_uid,
                  strerror (errno));
  else 
    {
      char **evp = NULL ;

      innfeed = NEW(char, (strlen(innconf->pathbin)+1+strlen(INNFEED)+1));
      sprintf(innfeed, "%s/%s", innconf->pathbin, INNFEED);
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
        syslog(LOG_ERR, "%s: execve: %s",
                      progname, strerror (errno));
    }
  
  exit(1);
}

/*  $Id$
**
**  Miscellaneous support routines.
*/

#include "config.h"
#include "clibrary.h"

/* Needed on AIX 4.1 to get fd_set and friends. */
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif

#include "inn/innconf.h"
#include "nnrpd.h"
#include "tls.h"
#include "sasl_config.h"

#ifdef HAVE_SSL
extern SSL *tls_conn;
extern int nnrpd_starttls_done;
#endif 


/*
**  Parse a string into a NULL-terminated array of words; return number
**  of words.  If argvp isn't NULL, it and what it points to will be freed.
*/
int
Argify(line, argvp)
    char		*line;
    char		***argvp;
{
    char	**argv;
    char	*p;

    if (*argvp != NULL) {
	free(*argvp[0]);
	free(*argvp);
    }

    /*  Copy the line, which we will split up. */
    while (ISWHITE(*line))
	line++;
    p = xstrdup(line);

    /* Allocate worst-case amount of space. */
    for (*argvp = argv = xmalloc((strlen(p) + 2) * sizeof(char *)); *p; ) {
	/* Mark start of this word, find its end. */
	for (*argv++ = p; *p && !ISWHITE(*p); )
	    p++;
	if (*p == '\0')
	    break;

	/* Nip off word, skip whitespace. */
	for (*p++ = '\0'; ISWHITE(*p); )
	    p++;
    }
    *argv = NULL;
    return argv - *argvp;
}


/*
**  Take a vector which Argify made and glue it back together with
**  spaces between each element.  Returns a pointer to dynamic space.
*/
char *
Glom(av)
    char		**av;
{
    char	**v;
    int	i;
    char		*save;

    /* Get space. */
    for (i = 0, v = av; *v; v++)
	i += strlen(*v) + 1;
    i++;

    save = xmalloc(i);
    save[0] = '\0';
    for (v = av; *v; v++) {
	if (v > av)
            strlcat(save, " ", i);
        strlcat(save, *v, i);
    }

    return save;
}


/*
**  Match a list of newsgroup specifiers against a list of newsgroups.
**  func is called to see if there is a match.
*/
bool PERMmatch(char **Pats, char **list)
{
    int	                i;
    char	        *p;
    int                 match = false;

    if (Pats == NULL || Pats[0] == NULL)
	return true;

    for ( ; *list; list++) {
	for (i = 0; (p = Pats[i]) != NULL; i++) {
	    if (p[0] == '!') {
		if (uwildmat(*list, ++p))
		    match = false;
	    }
	    else if (uwildmat(*list, p))
		match = true;
	}
	if (match)
	    /* If we can read it in one group, we can read it, period. */
	    return true;
    }

    return false;
}


/*
**  Check to see if user is allowed to see this article by matching
**  Newsgroups line.
*/
bool
PERMartok(void)
{
    static char		**grplist;
    char		*p, **grp;

    if (!PERMspecified)
	return false;

    if ((p = GetHeader("Xref")) == NULL) {
	/* in case article does not include Xref */
	if ((p = GetHeader("Newsgroups")) != NULL) {
	    if (!NGgetlist(&grplist, p))
		/* No newgroups or null entry. */
		return true;
	} else {
	    return true;
	}
    } else {
	/* skip path element */
	if ((p = strchr(p, ' ')) == NULL)
	    return true;
	for (p++ ; *p == ' ' ; p++);
	if (*p == '\0')
	    return true;
	if (!NGgetlist(&grplist, p))
	    /* No newgroups or null entry. */
	    return true;
	/* chop ':' and article number */
	for (grp = grplist ; *grp != NULL ; grp++) {
	    if ((p = strchr(*grp, ':')) == NULL)
		return true;
	    *p = '\0';
	}
    }

#ifdef DO_PYTHON
    if (PY_use_dynamic) {
        char    *reply;

	/* Authorize user at a Python authorization module */
	if (PY_dynamic(PERMuser, p, false, &reply) < 0) {
	    syslog(L_NOTICE, "PY_dynamic(): authorization skipped due to no Python dynamic method defined.");
	} else {
	    if (reply != NULL) {
	        syslog(L_TRACE, "PY_dynamic() returned a refuse string for user %s at %s who wants to read %s: %s", PERMuser, ClientHost, p, reply);
                free(reply);
		return false;
	    }
            return true;
	}
    }
#endif /* DO_PYTHON */

    return PERMmatch(PERMreadlist, grplist);
}


/*
**  Parse a newsgroups line, return true if there were any.
*/
bool
NGgetlist(argvp, list)
    char		***argvp;
    char		*list;
{
    char	*p;

    for (p = list; *p; p++)
	if (*p == ',')
	    *p = ' ';

    return Argify(list, argvp) != 0;
}


/*********************************************************************
 * POSTING RATE LIMITS - The following code implements posting rate
 * limits. News clients are indexed by IP number (or PERMuser, see
 * config file). After a relatively configurable number of posts, the nnrpd
 * process will sleep for a period of time before posting anything.
 * 
 * Each time that IP number posts a message, the time of
 * posting and the previous sleep time is stored. The new sleep time
 * is computed based on these values.
 *
 * To compute the new sleep time, the previous sleep time is, for most
 * cases multiplied by a factor (backoff_k). 
 *
 * See inn.conf(5) for how this code works
 *
 *********************************************************************/

/* Defaults are pass through, i.e. not enabled 
 * NEW for INN 1.8 - Use the inn.conf file to specify the following:
 *
 * backoff_k: <integer>
 * backoff_postfast: <integer>
 * backoff_postslow: <integer>
 * backoff_trigger: <integer>
 * backoff_db: <path>
 * backoff_auth: <on|off> 
 *
 * You may also specify posting backoffs on a per user basis. To do this
 * turn on "backoff_auth"
 *
 * Now these are runtime constants. <grin>
 */
static char postrec_dir[SMBUF];   /* Where is the post record directory? */

void
InitBackoffConstants()
{
  struct stat st;

  /* Default is not to enable this code */
  BACKOFFenabled = false;
  
  /* Read the runtime config file to get parameters */

  if ((PERMaccessconf->backoff_db == NULL) ||
    !(PERMaccessconf->backoff_k >= 0L && PERMaccessconf->backoff_postfast >= 0L && PERMaccessconf->backoff_postslow >= 1L))
    return;

  /* Need this database for backing off */
  strlcpy(postrec_dir, PERMaccessconf->backoff_db, sizeof(postrec_dir));
  if (stat(postrec_dir, &st) < 0) {
    if (ENOENT == errno) {
      if (!MakeDirectory(postrec_dir, true)) {
	syslog(L_ERROR, "%s cannot create backoff_db '%s': %s",ClientHost,postrec_dir,strerror(errno));
	return;
      }
    } else {
      syslog(L_ERROR, "%s cannot stat backoff_db '%s': %s",ClientHost,postrec_dir,strerror(errno));
      return;
    }
  }
  if (!S_ISDIR(st.st_mode)) {
    syslog(L_ERROR, "%s backoff_db '%s' is not a directory",ClientHost,postrec_dir);
    return;
  }

  BACKOFFenabled = true;

  return;
}

/*
 * PostRecs are stored in individual files. I didn't have a better
 * way offhand, don't want to touch DBZ, and the number of posters is
 * small compared to the number of readers. This is the filename corresponding
 * to an IP number.
 */
char
*PostRecFilename(ip,user) 
     char                         *ip;
     char                         *user;
{
     static char                   buff[SPOOLNAMEBUFF];
     char                          dirbuff[SPOOLNAMEBUFF];
     struct in_addr                inaddr;
     unsigned long int             addr;
     unsigned char                 quads[4];
     unsigned int                  i;

     if (PERMaccessconf->backoff_auth) {
       snprintf(buff, sizeof(buff), "%s/%s", postrec_dir, user);
       return(buff);
     }

     if (inet_aton(ip, &inaddr) < 1) {
       /* If inet_aton() fails, we'll assume it's an IPv6 address.  We'll
        * also assume for now that we're dealing with a limited number of
        * IPv6 clients so we'll place their files all in the same 
        * directory for simplicity.  Someday we'll need to change this to
        * something more scalable such as DBZ when IPv6 clients become
        * more popular. */
       snprintf(buff, sizeof(buff), "%s/%s", postrec_dir, ip);
       return(buff);
     }
     /* If it's an IPv4 address just fall through. */

     addr = ntohl(inaddr.s_addr);
     for (i=0; i<4; i++)
       quads[i] = (unsigned char) (0xff & (addr>>(i*8)));

     snprintf(dirbuff, sizeof(dirbuff), "%s/%03d%03d/%03d",
         postrec_dir, quads[3], quads[2], quads[1]);
     if (!MakeDirectory(dirbuff,true)) {
       syslog(L_ERROR, "%s Unable to create postrec directories '%s': %s",
               ClientHost, dirbuff, strerror(errno));
       return NULL;
     }
     snprintf(buff, sizeof(buff), "%s/%03d", dirbuff, quads[0]);
     return(buff);
}

/*
 * Lock the post rec file. Return 1 on lock, 0 on error
 */
int
LockPostRec(path)
     char              *path;
{
  char lockname[SPOOLNAMEBUFF];  
  char temp[SPOOLNAMEBUFF];
  int statfailed = 0;
 
  snprintf(lockname, sizeof(lockname), "%s.lock", path);

  for (;; sleep(5)) {
    int fd;
    struct stat st;
    time_t now;
 
    fd = open(lockname, O_WRONLY|O_EXCL|O_CREAT, 0600);
    if (fd >= 0) {
      /* We got the lock! */
      snprintf(temp, sizeof(temp), "pid:%ld\n", (unsigned long) getpid());
      write(fd, temp, strlen(temp));
      close(fd);
      return(1);
    }

    /* No lock. See if the file is there. */
    if (stat(lockname, &st) < 0) {
      syslog(L_ERROR, "%s cannot stat lock file %s", ClientHost, strerror(errno));
      if (statfailed++ > 5) return(0);
      continue;
    }

    /* If lockfile is older than the value of
       PERMaccessconf->backoff_postslow, remove it */
    statfailed = 0;
    time(&now);
    if (now < st.st_ctime + PERMaccessconf->backoff_postslow) continue;
    syslog(L_ERROR, "%s removing stale lock file %s", ClientHost, lockname);
    unlink(lockname);
  }
}

void
UnlockPostRec(path)
     char              *path;
{
  char lockname[SPOOLNAMEBUFF];  

  snprintf(lockname, sizeof(lockname), "%s.lock", path);
  if (unlink(lockname) < 0) {
    syslog(L_ERROR, "%s can't unlink lock file: %s", ClientHost,strerror(errno)) ;
  }
  return;
}

/* 
 * Get the stored postrecord for that IP 
 */
static int
GetPostRecord(char *path, long *lastpost, long *lastsleep, long *lastn)
{
     static char                   buff[SMBUF];
     FILE                         *fp;
     char                         *s;

     fp = fopen(path,"r");
     if (fp == NULL) { 
       if (errno == ENOENT) {
         return 1;
       }
       syslog(L_ERROR, "%s Error opening '%s': %s",
              ClientHost, path, strerror(errno));
       return 0;
     }

     if (fgets(buff,SMBUF,fp) == NULL) {
       syslog(L_ERROR, "%s Error reading '%s': %s",
              ClientHost, path, strerror(errno));
       return 0;
     }
     *lastpost = atol(buff);

     if ((s = strchr(buff,',')) == NULL) {
       syslog(L_ERROR, "%s bad data in postrec file: '%s'",
              ClientHost, buff);
       return 0;
     }
     s++; *lastsleep = atol(s);

     if ((s = strchr(s,',')) == NULL) {
       syslog(L_ERROR, "%s bad data in postrec file: '%s'",
              ClientHost, buff);
       return 0;
     }
     s++; *lastn = atol(s);

     fclose(fp);
     return 1;
}

/* 
 * Store the postrecord for that IP 
 */
static int
StorePostRecord(char *path, time_t lastpost, long lastsleep, long lastn)
{
     FILE                         *fp;

     fp = fopen(path,"w");
     if (fp == NULL)                   {
       syslog(L_ERROR, "%s Error opening '%s': %s",
              ClientHost, path, strerror(errno));
       return 0;
     }

     fprintf(fp,"%ld,%ld,%ld\n",(long) lastpost,lastsleep,lastn);
     fclose(fp);
     return 1;
}

/*
 * Return the proper sleeptime. Return false on error.
 */
int
RateLimit(sleeptime,path) 
     long                         *sleeptime;
     char                         *path;
{
     TIMEINFO                      Now;
     long                          prevpost,prevsleep,prevn,n;

     if (GetTimeInfo(&Now) < 0) 
       return 0;
     
     prevpost = 0L; prevsleep = 0L; prevn = 0L; n = 0L;
     if (!GetPostRecord(path,&prevpost,&prevsleep,&prevn)) {
       syslog(L_ERROR, "%s can't get post record: %s",
              ClientHost, strerror(errno));
       return 0;
     }
     /*
      * Just because yer paranoid doesn't mean they ain't out ta get ya
      * This is called paranoid clipping
      */
     if (prevn < 0L) prevn = 0L;
     if (prevsleep < 0L)  prevsleep = 0L;
     if (prevsleep > PERMaccessconf->backoff_postfast)  prevsleep = PERMaccessconf->backoff_postfast;
     
      /*
       * Compute the new sleep time
       */
     *sleeptime = 0L;  
     if (prevpost <= 0L) {
       prevpost = 0L;
       prevn = 1L;
     } else {
       n = Now.time - prevpost;
       if (n < 0L) {
         syslog(L_NOTICE,"%s previous post was in the future (%ld sec)",
                ClientHost,n);
         n = 0L;
       }
       if (n < PERMaccessconf->backoff_postfast) {
         if (prevn >= PERMaccessconf->backoff_trigger) {
           *sleeptime = 1 + (prevsleep * PERMaccessconf->backoff_k);
         } 
       } else if (n < PERMaccessconf->backoff_postslow) {
         if (prevn >= PERMaccessconf->backoff_trigger) {
           *sleeptime = prevsleep;
         }
       } else {
         prevn = 0L;
       } 
       prevn++;
     }

     *sleeptime = ((*sleeptime) > PERMaccessconf->backoff_postfast) ? PERMaccessconf->backoff_postfast : (*sleeptime);
     /* This ought to trap this bogon */
     if ((*sleeptime) < 0L) {
	syslog(L_ERROR,"%s Negative sleeptime detected: %ld, prevsleep: %ld, N: %ld",ClientHost,*sleeptime,prevsleep,n);
	*sleeptime = 0L;
     }
  
     /* Store the postrecord */
     if (!StorePostRecord(path,Now.time,*sleeptime,prevn)) {
       syslog(L_ERROR, "%s can't store post record: %s", ClientHost, strerror(errno));
       return 0;
     }

     return 1;
}

#ifdef HAVE_SSL
/*
**  The "STARTTLS" command.  RFC2595.
*/
/* ARGSUSED0 */

void
CMDstarttls(ac, av)
    int		ac UNUSED;
    char	*av[] UNUSED;
{
  int result;

  tls_init();
  if (nnrpd_starttls_done == 1) {
      Reply("%d Already successfully executed STARTTLS\r\n",
            NNTP_STARTTLS_DONE_VAL);
      return;
  }

  Reply("%d Begin TLS negotiation now\r\n", NNTP_STARTTLS_NEXT_VAL);
  fflush(stdout);

  /* must flush our buffers before starting tls */
  
  result=tls_start_servertls(0, /* read */
			     1); /* write */
  if (result==-1) {
    /* No reply because we have already sent NNTP_STARTTLS_NEXT_VAL. */
    return;
  }
  nnrpd_starttls_done = 1;
}
#endif /* HAVE_SSL */

/*  $Revision$
**
**  Miscellaneous support routines.
*/
#include <stdio.h>
#include <sys/types.h>
#include "configdata.h"
#include "clibrary.h"
#include "nnrpd.h"
#include "dbz.h"
#if	defined(DO_NEED_TIME)
#include <time.h>
#endif	/* defined(DO_NEED_TIME) */
#include <sys/time.h>


#define ASCtoNUM(c)		((c) - '0')
#define CHARStoINT(c1, c2)	(ASCtoNUM((c1)) * 10 + ASCtoNUM((c2)))
#define DaysInYear(y)		((y % 4 ? 365 : 366))


/*
**  Parse a string into a NULL-terminated array of words; return number
**  of words.  If argvp isn't NULL, it and what it points to will be
**  DISPOSE'd.
*/
int
Argify(line, argvp)
    char		*line;
    char		***argvp;
{
    register char	**argv;
    register char	*p;
    register int	i;

    if (*argvp != NULL) {
	DISPOSE(*argvp[0]);
	DISPOSE(*argvp);
    }

    /*  Copy the line, which we will split up. */
    while (ISWHITE(*line))
	line++;
    i = strlen(line);
    p = NEW(char, i + 1);
    (void)strcpy(p, line);

    /* Allocate worst-case amount of space. */
    for (*argvp = argv = NEW(char*, i + 2); *p; ) {
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
    register char	**v;
    register char	*p;
    register int	i;
    char		*save;

    /* Get space. */
    for (i = 0, v = av; *v; v++)
	i += strlen(*v) + 1;

    for (save = p = NEW(char, i + 1), v = av; *v; v++) {
	if (p > save)
	    *p++ = ' ';
	p += strlen(strcpy(p, *v));
    }

    return save;
}


/*
**  Match a list of newsgroup specifiers against a list of newsgroups.
**  func is called to see if there is a match.
*/
BOOL
PERMmatch(match, Pats, list)
    register BOOL	match;
    char		**Pats;
    char		**list;
{
    register int	i;
    register char	*p;

    if (Pats[0] == NULL)
	return TRUE;

    for ( ; *list; list++) {
	for (i = 0; (p = Pats[i]) != NULL; i++) {
	    if (p[0] == '!') {
		if (wildmat(*list, ++p))
		    match = FALSE;
	    }
	    else if (wildmat(*list, p))
		match = TRUE;
	}
	if (match)
	    /* If we can read it in one group, we can read it, period. */
	    return TRUE;
    }

    return FALSE;
}


/*
**  Check to see if user is allowed to see this article by matching
**  Newsgroups line.
*/
BOOL
PERMartok(qp)
{
    static char		**grplist;
    char		*p;

    if (!PERMspecified)
	return PERMdefault;

    if ((p = GetHeader("Newsgroups", FALSE)) == NULL)
        return 1;
    if (NGgetlist(&grplist, p))
	/* No newgroups or null entry. */
	return 1;

    return PERMmatch(PERMdefault, PERMlist, grplist);
}


/*
**  Parse a date like yymmddhhmmss into a long.  Return -1 on error.
*/
long
NNTPtoGMT(av1, av2)
    char		*av1;
    char		*av2;
{
    /* Note that this is origin-one! */
    static int		DaysInMonth[12] = {
	0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30
    };
    register char	*p;
    int			year;
    int			month;
    int			day;
    int			hour;
    int			mins;
    int			secs;
    register int	i;
    long		seconds;
    char		buff[6 + 6 + 1];

    if (strlen(av1) != 6 || strlen(av2) != 6)
	return -1;
    (void)sprintf(buff, "%s%s", av1, av2);
    for (p = buff; *p; p++)
	if (!CTYPE(isdigit, *p))
	    return -1;

    year  = CHARStoINT(buff[ 0], buff[ 1]);
    month = CHARStoINT(buff[ 2], buff[ 3]);
    day   = CHARStoINT(buff[ 4], buff[ 5]);
    hour  = CHARStoINT(buff[ 6], buff[ 7]);
    mins  = CHARStoINT(buff[ 8], buff[ 9]);
    secs  = CHARStoINT(buff[10], buff[11]);

    if (month < 1 || month > 12
     || day < 1 || day > 31
     || mins < 0 || mins > 59
     || secs < 0 || secs > 59)
	return -1;
    if (hour == 24) {
	hour = 0;
	day++;
    }
    else if (hour < 0 || hour > 23)
	return -1;

    if (year < 50)
      year += 100 ;
    
    for (seconds = 0, year += 1900, i = 1970; i < year; i++)
	seconds += DaysInYear(i);
    if (DaysInYear(year) == 366 && month > 2)
	seconds++;
    while (--month > 0)
	seconds += DaysInMonth[month];
    seconds += day - 1;
    seconds = 24 * seconds + hour;
    seconds = 60 * seconds + mins;
    seconds = 60 * seconds + secs;

    return seconds;
}


/*
**  Convert local time (seconds since epoch) to GMT.
*/
long
LOCALtoGMT(t)
    long	t;
{
    TIMEINFO	Now;

    (void)GetTimeInfo(&Now);
    t += Now.tzone * 60;
    return t;
}


/*
**  Return the path name of an article if it is in the history file.
**  Return a pointer to static data.
*/
char *HISgetent(char *msg_id, BOOL fulldata)
{
    static BOOL		setup = FALSE;
    static FILE		*hfp;
    static char		path[BIG_BUFFER];
    char	        *p;
    char	        *q;
    char		*save;
    char		buff[BIG_BUFFER];
    HASH		key;
    OFFSET_T		offset;
    struct stat		Sb;

    if (!setup) {
	if (!dbminit(HISTORY)) {
	    syslog(L_ERROR, "%s cant dbminit %s %m", ClientHost, HISTORY);
	    return NULL;
	}
	setup = TRUE;
    }

    /* Set the key value, fetch the entry. */
    if (StorageAPI)
	key = *(HASH *)msg_id;
    else
	key = HashMessageID(msg_id);
    if ((offset = dbzfetch(key)) < 0)
	return NULL;

    /* Open history file if we need to. */
    if (hfp == NULL) {
	if ((hfp = fopen(HISTORY, "r")) == NULL) {
	    syslog(L_ERROR, "%s cant fopen %s %m", ClientHost, HISTORY);
	    return NULL;
	}
	CloseOnExec((int)fileno(hfp), TRUE);
    }

    /* Seek and read. */
    if (fseek(hfp, offset, SEEK_SET) == -1) {
	syslog(L_ERROR, "%s cant fseek to %ld %m", ClientHost, offset);
	return NULL;
    }
    if (fgets(buff, sizeof buff, hfp) == NULL) {
	syslog(L_ERROR, "%s cant fgets from %ld %m", ClientHost, offset);
	return NULL;
    }
    if ((p = strchr(buff, '\n')) != NULL)
	*p = '\0';

    /* Skip first two fields. */
    if ((p = strchr(buff, '\t')) == NULL) {
	syslog(L_ERROR, "%s bad_history at %ld for %s", ClientHost, offset, msg_id);
	return NULL;
    }
    if ((p = strchr(p + 1, '\t')) == NULL)
	/* Article has expired. */
	return NULL;
    save = p + 1;

    if (IsToken(save)) {
	strcpy(path, save);
	return path;
    }

    /* Want the full data? */
    if (fulldata) {
	(void)strcpy(path, save);
	for (p = path; *p; p++)
	    if (*p == '.')
		*p = '/';
	return path;
    }

    /* Want something we can open; loop over all entries. */
    for ( ; ; save = q + 1) {
	if ((q = strchr(save, ' ')) != NULL)
	    *q = '\0';
	for (p = save; *p; p++)
	    if (*p == '.')
		*p = '/';
	(void)sprintf(path, "%s/%s", _PATH_SPOOL, save);
	if (stat(path, &Sb) >= 0)
	    return path;
	if (q == NULL)
	    break;
    }

    return NULL;
}


/*
**  Parse a newsgroups line, return TRUE if there were any.
*/
BOOL
NGgetlist(argvp, list)
    char		***argvp;
    char		*list;
{
    register char	*p;

    for (p = list; *p; p++)
	if (*p == ',')
	    *p = ' ';

    return Argify(list, argvp) != 0;
}


/*
**  Take an NNTP distribution list <d1,d2,...> and turn it into an array.
*/
BOOL
ParseDistlist(argvp, list)
    char		***argvp;
    char		*list;
{
    static char		**argv;
    register char	*p;

    if (list[0] != '<' || (p = strchr(&list[1], '>')) == NULL)
	return FALSE;
    *p = '\0';

    for (p = list + 1; *p; p++)
	if (*p == ',')
	    *p = ' ';
    (void)Argify(list + 1, &argv);
    *argvp = argv;
    return TRUE;
}


/*
**  Read a line of input, with timeout.
*/
READTYPE
READline(start, size, timeout)
    char		*start;
    int			size;
    int			timeout;
{
    static int		count;
    static char		buffer[BUFSIZ];
    static char		*bp;
    register char	*p;
    register char	*end;
    struct timeval	t;
    FDSET		rmask;
    int			i;
    char		c;

    for (p = start, end = &start[size - 1]; ; ) {
	if (count == 0) {
	    /* Fill the buffer. */
    Again:
	    FD_ZERO(&rmask);
	    FD_SET(STDIN, &rmask);
	    t.tv_sec = timeout;
	    t.tv_usec = 0;
	    i = select(STDIN + 1, &rmask, (FDSET *)NULL, (FDSET *)NULL, &t);
	    if (i < 0) {
		if (errno == EINTR)
		    goto Again;
		syslog(L_ERROR, "%s cant select %m", ClientHost);
		return RTtimeout;
	    }
	    if (i == 0 || !FD_ISSET(STDIN, &rmask))
		return RTtimeout;
	    count = read(STDIN, buffer, sizeof buffer);
	    if (count < 0) {
		syslog(L_TRACE, "%s cant read %m", ClientHost);
		return RTtimeout;
	    }
	    if (count == 0)
		return RTeof;
	    bp = buffer;
	}

	/* Process next character. */
	count--;
	c = *bp++;
	if (c == '\n')
	    break;
	if (p < end)
	    *p++ = c;
    }

    /* If last two characters are \r\n, kill the \r as well as the \n. */
    if (p > start && p < end && p[-1] == '\r')
	p--;
    *p = '\0';
    return p == end ? RTlong : RTok;
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
 * cases multiplied by a factor (backoff_k). backoff_k is computed based on the
 * difference between this time and the last posting time in seconds.
 *
 * If this difference is less than post_fast then backoff_k is K_INC. 
 *
 * If the difference is greater than post_slow then backoff_k is 0
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
static long backoff_k = 1L;       /* Constant for backoff */
static long post_fast = 0L;       /* Interval which posting triggers backoff */
static long post_slow = 1L;       /* Interval which undoes backoff */
static long backoff_num = 10000L; /* Number of posts before backoff is invoked */
static char postrec_dir[SMBUF];   /* Where is the post record directory? */
static BOOL backoff_auth = FALSE;

void
InitBackoffConstants()
{
  static char                   buff[SMBUF];
  long                          x,i;
  char *s;
  FILE *fp;
  struct stat st;

  /* Default is not to enable this code */
  BACKOFFenabled = FALSE;
  
  /* Read the runtime config file to get parameters */
  if ((s = GetFileConfigValue("backoff_db")) != NULL)
    (void)strncpy(postrec_dir,s,SMBUF);
  else
    return;
  backoff_k = atol(GetFileConfigValue("backoff_k"));
  post_fast = atol(GetFileConfigValue("backoff_postfast"));
  post_slow = atol(GetFileConfigValue("backoff_postslow"));
  backoff_num = atol(GetFileConfigValue("backoff_trigger"));
  backoff_auth = GetBooleanConfigValue("backoff_auth", FALSE);

  /* Need this database for backing off */
  if (stat(postrec_dir, &st) < 0) {
    syslog(L_ERROR, "%s cannot stat backoff_db '%s': %s",ClientHost,postrec_dir,strerror(errno));
    return;
  }

  /* Only enable if the constants make sense */
  if (backoff_k > 1L && post_fast > 0L && post_slow > 1L) {
    BACKOFFenabled = TRUE;
  }

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
     unsigned long                 ip;
     char                         *user;
{
     static char                   buff[SPOOLNAMEBUFF];
     char                          dirbuff[SPOOLNAMEBUFF];
     unsigned char addr[4];
     unsigned int i;

     if (backoff_auth) {
       sprintf(buff,"%s/%s",postrec_dir,user);
       return(buff);
     }

     for (i=0; i<4; i++) {
       addr[i] = (unsigned char) (0x000000ff & (ip>>(i*8)));
     }

     sprintf(dirbuff,"%s/%03d%03d/%03d",postrec_dir,addr[3],addr[2],addr[1]);
     if (!MakeDirectory(dirbuff,TRUE)) {
       return NULL;
     }
     sprintf(buff,"%s/%03d",dirbuff,addr[0]);
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
 
  sprintf(lockname, "%s.lock", path);

  for (;; sleep(5)) {
    int fd;
    struct stat st;
    time_t now;
 
    fd = open(lockname, O_WRONLY|O_EXCL|O_CREAT, 0600);
    if (fd >= 0) {
      /* We got the lock! */
      sprintf(temp, "pid:%ld\n", (unsigned long) getpid());
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

    /* If lockfile is older than the value of post_slow, remove it
     */
    statfailed = 0;
    time(&now);
    if (now < st.st_ctime + post_slow) continue;
    syslog(L_ERROR, "%s removing stale lock file %s", ClientHost, lockname);
    unlink(lockname);
  }
}

int
UnlockPostRec(path)
     char              *path;
{
  char lockname[SPOOLNAMEBUFF];  

  sprintf(lockname, "%s.lock", path);
  if (unlink(lockname) < 0) {
    syslog(L_ERROR, "%s can't unlink lock file: %s", ClientHost,strerror(errno)) ;
  }
  return;
}

/* 
 * Get the stored postrecord for that IP 
 */
STATIC int
GetPostRecord(path, lastpost, lastsleep, lastn)
     char                         *path;
     long                *lastpost;
     long                *lastsleep;
     long                *lastn;
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

     (void)fclose(fp);
     return 1;
}

/* 
 * Store the postrecord for that IP 
 */
STATIC int
StorePostRecord(path, lastpost, lastsleep, lastn)
     char                         *path;
     long                         lastpost;
     long                         lastsleep;
     long                         lastn;
{
     static char                   buff[SMBUF];
     FILE                         *fp;

     fp = fopen(path,"w");
     if (fp == NULL)                   {
       syslog(L_ERROR, "%s Error opening '%s': %s",
              ClientHost, path, strerror(errno));
       return 0;
     }

     fprintf(fp,"%ld,%ld,%ld\n",lastpost,lastsleep,lastn);
     (void)fclose(fp);
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
     
     prevpost = 0L; prevsleep = 0L; prevn = 0L;
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
     if (prevsleep > post_fast)  prevsleep = post_fast;
     
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
       if (n < post_fast) {
         if (prevn >= backoff_num) {
           *sleeptime = 1 + (prevsleep * backoff_k);
         } 
       } else if (n < post_slow) {
         if (prevn >= backoff_num) {
           *sleeptime = prevsleep;
         }
       } else {
         prevn = 0L;
       } 
       prevn++;
     }

     *sleeptime = ((*sleeptime) > post_fast) ? post_fast : (*sleeptime);
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



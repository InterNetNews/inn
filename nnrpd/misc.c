/*  $Revision$
**
**  Miscellaneous support routines.
*/
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "configdata.h"
#include "clibrary.h"
#include "nnrpd.h"
#include "dbz.h"
#if	defined(DO_NEED_TIME)
#include <time.h>
#endif	/* defined(DO_NEED_TIME) */
#include <sys/time.h>

STATIC BOOL		setup = FALSE;
STATIC FILE		*hfp = NULL;
STATIC ino_t		ino = 0;

#define ASCtoNUM(c)		((c) - '0')
#define CHARStoINT(c1, c2)	(ASCtoNUM((c1)) * 10 + ASCtoNUM((c2)))
#define DaysInYear(y) ((y) % 4 ? 365 : (y) % 100 ? 366 : (y) % 400 ? 365 : 366)


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
BOOL PERMmatch(char **Pats, char **list)
{
    int	                i;
    char	        *p;
    int                 match = FALSE;

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
	return FALSE;

    if ((p = GetHeader("Newsgroups", FALSE)) == NULL)
        return 1;
    if (NGgetlist(&grplist, p))
	/* No newgroups or null entry. */
	return 1;

    return PERMmatch(PERMlist, grplist);
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
    int			datelen;
    register int	i;
    long		seconds;
    char		buff[8 + 6 + 1];

    /* Y2K: accept YYMMDD, YYYMMDD or YYYYMMDD.
            First is strict NNTP spec,
	    Second is broken clients that do "printf %02d tm_year",
	    Third is people trying to do the right date thing
	          despite the spec. */
    datelen = strlen(av1);
    if ((datelen < 6 || datelen > 8) || strlen(av2) != 6)
	return -1;
    (void)sprintf(buff, "%s%s", av1, av2);
    for (p = buff; *p; p++)
	if (!CTYPE(isdigit, (int)*p))
	    return -1;

    p = buff + datelen - 6;

    year  = CHARStoINT(p[ 0], p[ 1]);
    month = CHARStoINT(p[ 2], p[ 3]);
    day   = CHARStoINT(p[ 4], p[ 5]);
    hour  = CHARStoINT(p[ 6], p[ 7]);
    mins  = CHARStoINT(p[ 8], p[ 9]);
    secs  = CHARStoINT(p[10], p[11]);

    if (datelen == 6) {		/* YYMMDD */
	if (year > 70)
	    year += 1900;
	else
	    year += 2000;
    } else {
        year += ASCtoNUM(*--p) * 100;
	if (datelen == 7)	/* YYYMMDD */
	    year += 1900;
	else
	    year += ASCtoNUM(*--p) * 1000; /* YYYYMMDD */
    }

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

    for (seconds = 0, i = 1970; i < year; i++)
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

void HIScheck(void)
{
    struct stat		Sb;

    if (!setup)
	/* not opened yet */
	return;
    if ((stat(HISTORY, &Sb) < 0) || (Sb.st_ino != ino)) {
	dbzclose();
	if (hfp) fclose(hfp);
	ino = 0;
	setup = FALSE;
	hfp = NULL;
    }
}

#ifndef DO_TAGGED_HASH
/*
**  Get overview offset
*/
BOOL OVERgetent(HASH *key, TOKEN *token, OFFSET_T *offset)
{
    struct timeval	stv, etv;
    idxrec		ionevalue;
    idxrecext		iextvalue;

    if (!setup) {
	if (!dbzinit(HISTORY)) {
	    syslog(L_ERROR, "%s cant dbzinit %s %m", ClientHost, HISTORY);
	    return FALSE;
	}
	setup = TRUE;
    }

    /* Set the key value, fetch the entry. */
    if (innconf->extendeddbz) {
	if (innconf->nnrpdoverstats)
	    gettimeofday(&stv, NULL);
	if (!dbzfetch(*key, &iextvalue)) {
	    if (innconf->nnrpdoverstats) {
		gettimeofday(&etv, NULL);
		OVERdbz+=(etv.tv_sec - stv.tv_sec) * 1000;
		OVERdbz+=(etv.tv_usec - stv.tv_usec) / 1000;
	    }
	    return FALSE;
	}
	if (innconf->nnrpdoverstats) {
	    gettimeofday(&etv, NULL);
	    OVERdbz+=(etv.tv_sec - stv.tv_sec) * 1000;
	    OVERdbz+=(etv.tv_usec - stv.tv_usec) / 1000;
	}
	OVERmaketoken(token, iextvalue.offset[OVEROFFSET], iextvalue.overindex, iextvalue.overlen);
	if (offset)
	    *offset = iextvalue.offset[HISTOFFSET];
	return TRUE;
    }
    return FALSE;
}
#endif

/*
**  Return the path name of an article if it is in the history file.
**  Return a pointer to static data.
*/
char *HISgetent(HASH *key, BOOL useoffset, OFFSET_T *off)
{
    static char		path[BIG_BUFFER];
    char	        *p;
    char	        *q;
    char		*save;
    char		buff[BIG_BUFFER];
    OFFSET_T		offset;
    struct stat		Sb;
    struct timeval	stv, etv;
    static int		entrysize = 0;
    int			i;
#ifndef DO_TAGGED_HASH
    idxrec		ionevalue;
    idxrecext		iextvalue;
#endif

    if (entrysize == 0) {
	if (innconf->storageapi) {
	    HASH hash;
	    time_t dummy = ~(time_t)0;
	    TOKEN token;
	    sprintf(buff, "[%s]%c%lu%c%lu%c%lu%c%s\n", HashToText(hash),
		HIS_FIELDSEP, dummy, HIS_SUBFIELDSEP,
		dummy, HIS_SUBFIELDSEP,
		dummy, HIS_FIELDSEP, TokenToText(token));
	    entrysize = strlen(buff);
	} else {
	    entrysize = sizeof(buff) - 1;
	}
    }
    if (!setup) {
	if (!dbzinit(HISTORY)) {
	    syslog(L_ERROR, "%s cant dbzinit %s %m", ClientHost, HISTORY);
	    return NULL;
	}
	setup = TRUE;
    }

    /* Set the key value, fetch the entry. */
    if (useoffset && (off != NULL)) {
	offset = *off;
	gettimeofday(&stv, NULL);
    } else {
	gettimeofday(&stv, NULL);
#ifdef	DO_TAGGED_HASH
	if ((offset = dbzfetch(*key)) < 0) {
	    if (innconf->nnrpdoverstats) {
		gettimeofday(&etv, NULL);
		OVERdbz+=(etv.tv_sec - stv.tv_sec) * 1000;
		OVERdbz+=(etv.tv_usec - stv.tv_usec) / 1000;
	    }
	    return NULL;
	}
#else
	if (innconf->extendeddbz) {
	    if (!dbzfetch(*key, &iextvalue)) {
		if (innconf->nnrpdoverstats) {
		    gettimeofday(&etv, NULL);
		    OVERdbz+=(etv.tv_sec - stv.tv_sec) * 1000;
		    OVERdbz+=(etv.tv_usec - stv.tv_usec) / 1000;
		}
		return NULL;
	    }
	    offset = iextvalue.offset[HISTOFFSET];
	} else {
	    if (!dbzfetch(*key, &ionevalue)) {
		if (innconf->nnrpdoverstats) {
		    gettimeofday(&etv, NULL);
		    OVERdbz+=(etv.tv_sec - stv.tv_sec) * 1000;
		    OVERdbz+=(etv.tv_usec - stv.tv_usec) / 1000;
		}
		return NULL;
	    }
	    offset = ionevalue.offset;
	}
#endif
	if (innconf->nnrpdoverstats) {
	    gettimeofday(&etv, NULL);
	    OVERdbz+=(etv.tv_sec - stv.tv_sec) * 1000;
	    OVERdbz+=(etv.tv_usec - stv.tv_usec) / 1000;
	}
	if (off != NULL) {
	    *off = offset;
	    /* just return dummy */
	    return path;
	}
	stv = etv;
    }

    /* Open history file if we need to. */
    if (hfp == NULL) {
	if ((hfp = fopen(HISTORY, "r")) == NULL) {
	    syslog(L_ERROR, "%s cant fopen %s %m", ClientHost, HISTORY);
	    return NULL;
	}
	if (fstat((int)fileno(hfp), &Sb) < 0) {
	    syslog(L_ERROR, "%s cant stat %s %m", ClientHost, HISTORY);
	    return NULL;
	}
	CloseOnExec((int)fileno(hfp), TRUE);
	ino = Sb.st_ino;
    }

    /* Seek and read. */
    if (lseek(fileno(hfp), offset, SEEK_SET) == -1) {
	syslog(L_ERROR, "%s cant lseek to %ld %m", ClientHost, offset);
	return NULL;
    }
    if (innconf->nnrpdoverstats) {
	gettimeofday(&etv, NULL);
	OVERseek+=(etv.tv_sec - stv.tv_sec) * 1000;
	OVERseek+=(etv.tv_usec - stv.tv_usec) / 1000;
    }
    stv = etv;
    if (read(fileno(hfp), buff, entrysize) < 0) {
	syslog(L_ERROR, "%s cant read from %ld %m", ClientHost, offset);
	return NULL;
    }
    buff[entrysize] = '\0';
    if (strchr(buff, '\n') == NULL) {
	syslog(L_ERROR, "%s cant find end of line %ld %m", ClientHost, offset);
	return NULL;
    }
    if (innconf->nnrpdoverstats) {
	gettimeofday(&etv, NULL);
	OVERget+=(etv.tv_sec - stv.tv_sec) * 1000;
	OVERget+=(etv.tv_usec - stv.tv_usec) / 1000;
    }
    if ((p = strchr(buff, '\n')) != NULL)
	*p = '\0';

    /* Skip first two fields. */
    if ((p = strchr(buff, '\t')) == NULL) {
	syslog(L_ERROR, "%s bad_history at %ld for %s in %s", ClientHost, offset, HashToText(*key), buff);
	return NULL;
    }
    if ((p = strchr(p + 1, '\t')) == NULL)
	/* Article has expired. */
	return NULL;
    save = p + 1;

    if (IsToken(save) && ((useoffset != TRUE) || (off != NULL))) {
	strcpy(path, save);
	return path;
    }

    /* Want the full data? */
    if ((useoffset == TRUE) && (off == NULL)) {
	/* this is the case for called by CMDxpath() */
	if (innconf->storageapi)
	    return NULL;
	(void)strcpy(path, save);
	for (p = path; *p; p++) {
	    if (*p == '.')
		*p = '/';
	}
	return path;
    }

    /* Want something we can open; loop over all entries. */
    for ( ; ; save = q + 1) {
	if ((q = strchr(save, ' ')) != NULL)
	    *q = '\0';
	for (p = save; *p; p++)
	    if (*p == '.')
		*p = '/';
	(void)sprintf(path, "%s/%s", innconf->patharticles, save);
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
**
**  'size' is both the size of the input buffer and the max line
**  size as defined by the RFCs.  The max line size includes
**  the trailing CR LF (which we strip off here), but not the '\0'.
**  For NNTP commands, this limit is 512 (or 510 without the CR LF),
**  and for POST data (article) lines it's 1000 (or 998 without the CR LF).
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
    BOOL		toolong;

    toolong = FALSE;

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
	    if (count == 0) {
		*p = '\0';
		return RTeof;
	    }
	    bp = buffer;
	}

	/* Process next character. */
	count--;
	c = *bp++;
	if (c == '\n') {
	    /* If last two characters are \r\n, kill the \r as well as the \n. */
	    if (!toolong && p > start && p[-1] == '\r')
		p--;
	    break;
	}
	if (p < end)
	    *p++ = c;
	else
	    toolong = TRUE;
    }

    *p = '\0';
    return toolong ? RTlong : RTok;
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
  static char                   buff[SMBUF];
  long                          x,i;
  char *s;
  FILE *fp;
  struct stat st;

  /* Default is not to enable this code */
  BACKOFFenabled = FALSE;
  
  /* Read the runtime config file to get parameters */

  if ((innconf->backoff_db == NULL) ||
    !(innconf->backoff_k > 1L && innconf->backoff_postfast > 0L && innconf->backoff_postslow > 1L))
    return;

  /* Need this database for backing off */
  (void)strncpy(postrec_dir,innconf->backoff_db,SMBUF);
  if (stat(postrec_dir, &st) < 0) {
    syslog(L_ERROR, "%s cannot stat backoff_db '%s': %s",ClientHost,postrec_dir,strerror(errno));
    return;
  }
  BACKOFFenabled = TRUE;

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

     if (innconf->backoff_auth) {
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

    /* If lockfile is older than the value of innconf->backoff_postslow, remove it
     */
    statfailed = 0;
    time(&now);
    if (now < st.st_ctime + innconf->backoff_postslow) continue;
    syslog(L_ERROR, "%s removing stale lock file %s", ClientHost, lockname);
    unlink(lockname);
  }
}

void
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
     time_t                       lastpost;
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
     if (prevsleep > innconf->backoff_postfast)  prevsleep = innconf->backoff_postfast;
     
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
       if (n < innconf->backoff_postfast) {
         if (prevn >= innconf->backoff_trigger) {
           *sleeptime = 1 + (prevsleep * innconf->backoff_k);
         } 
       } else if (n < innconf->backoff_postslow) {
         if (prevn >= innconf->backoff_trigger) {
           *sleeptime = prevsleep;
         }
       } else {
         prevn = 0L;
       } 
       prevn++;
     }

     *sleeptime = ((*sleeptime) > innconf->backoff_postfast) ? innconf->backoff_postfast : (*sleeptime);
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



/*  $Id$
**
**  History file routines.
*/

#include "config.h"
#include "clibrary.h"
#include <netinet/in.h>

#include "dbz.h"
#include "innd.h"

typedef struct __HISCACHE {
    HASH	Hash;	/* Hash value of the message-id using Hash() */
    bool	Found;	/* Whether this entry is in the dbz file yet */
} _HIScache;

typedef enum {HIScachehit, HIScachemiss, HIScachedne} HISresult;

enum { S_HIScacheadd, S_HIScachelookup, S_HISsetup, S_HISsync,
       S_HISlogstats, S_HISclose, S_HISfilesfor, S_HIShavearticle,
       S_HISwrite, S_HISremember, S_HIS_MAX };

static struct timeval HISstat_start[S_HIS_MAX];
static struct timeval HISstat_total[S_HIS_MAX];
static unsigned long  HISstat_count[S_HIS_MAX];

static char		*HIShistpath = NULL;
static FILE		*HISwritefp;
static int		HISreadfd;
static int		HISdirty;
static _HIScache	*HIScache;
static int              HIScachesize;   /* Number of entries in HIScache */
static int              HIShitpos;      /* In cache, in history */
static int              HIShitneg;      /* In cache, not in history */
static int              HISmisses;      /* Not in cache, in history */
static int              HISdne;         /* Not in cache or history */
static time_t		HISlastlog;     /* Last time that we logged stats */   
static FILE             *HISfdlog = NULL; /* filehandle for history logging purpose */

void HISlogclose() {
   if (HISfdlog != NULL)
       Fclose(HISfdlog);
   HISfdlog = NULL;
}

void HISlogto(char *s) {
   int i;

   HISlogclose();
   if ((HISfdlog = Fopen(s, "a", INND_HISLOG)) == NULL)
       syslog(L_FATAL, "%s cant open %s %m", LogName, s);
   /* initialize our counters */
   for (i = 0; i < S_HIS_MAX; i++) {
       HISstat_start[i].tv_sec = 0;
       HISstat_start[i].tv_usec = 0;
       HISstat_total[i].tv_sec = 0;
       HISstat_total[i].tv_usec = 0;
       HISstat_count[i] = 0;
   }
}

static void HISlogger(char *s, int code) {
  struct timeval tv;
  struct tm *tm;

  if (HISfdlog == NULL) /* do nothing unless HISlogto() has been called */
      return;

  gettimeofday(&tv, NULL);
  tm = localtime(&(tv.tv_sec));
  if (HISstat_start[code].tv_sec != 0) {
      fprintf(HISfdlog, "%d/%d/%d %02d:%02d:%02d.%06d: [%d] %s (%.6f)\n",
          tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour,
          tm->tm_min, tm->tm_sec, tv.tv_usec, code, s, (float) tv.tv_sec +
          (float) tv.tv_usec / 1000000 - (float) HISstat_start[code].tv_sec -
          (float) HISstat_start[code].tv_usec / 1000000);
      if (tv.tv_usec < HISstat_start[code].tv_usec) {
          HISstat_total[code].tv_sec++;
	  HISstat_total[code].tv_usec +=
              tv.tv_usec - HISstat_start[code].tv_usec + 1000000;
      }
      else
          HISstat_total[code].tv_usec +=
              tv.tv_usec - HISstat_start[code].tv_usec;
      HISstat_total[code].tv_sec += tv.tv_sec - HISstat_start[code].tv_sec;
      HISstat_count[code]++;
      HISstat_start[code].tv_sec = 0;
      HISstat_start[code].tv_usec = 0;
   }
   else {
      fprintf(HISfdlog, "%d/%d/%d %02d:%02d:%02d.%06d: [%d] %s\n",
          tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour,
          tm->tm_min, tm->tm_sec, tv.tv_usec, code, s);
      HISstat_start[code].tv_sec = tv.tv_sec;
      HISstat_start[code].tv_usec = tv.tv_usec;
   }
}

/*
** Put an entry into the history cache 
*/
void
HIScacheadd(HASH MessageID, bool Found) {
    unsigned int  i, loc;

    HISlogger("HIScacheadd begin", S_HIScacheadd);
    if (HIScache == NULL) {
        HISlogger("HIScacheadd end", S_HIScacheadd);
	return;
    }
    memcpy(&loc, ((char *)&MessageID) + (sizeof(HASH) - sizeof(loc)), sizeof(loc));
    i = loc % HIScachesize;
    memcpy((char *)&HIScache[i].Hash, (char *)&MessageID, sizeof(HASH));
    HIScache[i].Found = Found;
    HISlogger("HIScacheadd end", S_HIScacheadd);
}

/*
** Lookup an entry in the history cache
*/
HISresult
HIScachelookup(HASH MessageID) {
    unsigned int i, loc;

    if (HIScache == NULL)
	return HIScachedne;
    HISlogger("HIScachelookup begin", S_HIScachelookup);
    memcpy(&loc, ((char *)&MessageID) + (sizeof(HASH) - sizeof(loc)), sizeof(loc));
    i = loc % HIScachesize;
    if (memcmp((char *)&HIScache[i].Hash, (char *)&MessageID, sizeof(HASH)) == 0) {
        if (HIScache[i].Found) {
            HIShitpos++;
	    HISlogger("HIScachelookup end", S_HIScachelookup);
            return HIScachehit;
        } else {
            HIShitneg++;
	    HISlogger("HIScachelookup end", S_HIScachelookup);
            return HIScachemiss;
        }
    } else {
	HISlogger("HIScachelookup end", S_HIScachelookup);
        return HIScachedne;
    }
}

/*
**  Set up the history files.
*/
void
HISsetup(void)
{
    dbzoptions opt;
    
    HISlogger("HISsetup begin", S_HISsetup);
    if (HISwritefp == NULL) {
	if (HIShistpath == NULL)
	    HIShistpath = COPY(cpcatpath(innconf->pathdb, _PATH_HISTORY));
	/* Open the history file for appending formatted I/O. */
	if ((HISwritefp = Fopen(HIShistpath, "a", INND_HISTORY)) == NULL) {
	    syslog(L_FATAL, "%s cant fopen %s %m", LogName, HIShistpath);
	    exit(1);
	}
	/* fseeko to the end of file because the result of ftello() is
           undefined for files freopen()-ed in append mode according to
           POSIX 1003.1.  ftello() is used later on to determine a new
           article's offset in the history file. Fopen() uses freopen()
           internally. */
	if (fseeko(HISwritefp, 0, SEEK_END) == -1) {
	    syslog(L_FATAL, "cant fseek to end of %s %m", HIShistpath);
	    exit(1);
	}
	close_on_exec(fileno(HISwritefp), true);

	/* Open the history file for reading. */
	if ((HISreadfd = open(HIShistpath, O_RDONLY)) < 0) {
	    syslog(L_FATAL, "%s cant open %s %m", LogName, HIShistpath);
	    exit(1);
	}
	close_on_exec(HISreadfd, true);

	/* Open the DBZ file. */
	dbzgetoptions(&opt);
	opt.writethrough = TRUE;
#ifdef	DO_TAGGED_HASH
	opt.pag_incore = INCORE_MMAP;
#else
	opt.pag_incore = INCORE_NO;
	opt.exists_incore = INND_DBZINCORE ? INCORE_MMAP : INCORE_NO;
#endif
	dbzsetoptions(opt);
	if (!dbzinit(HIShistpath)) {
	    syslog(L_FATAL, "%s cant dbzinit %s %m", LogName, HIShistpath);
	    exit(1);
	}
    }
    if (innconf->hiscachesize == 0) {
	HIScache = NULL;
	HIScachesize = 0;
    } else {
	HIScachesize = innconf->hiscachesize;
	HIScachesize *= 1024;
	if (HIScache != NULL)
	    DISPOSE(HIScache);
	HIScachesize = (HIScachesize / sizeof(_HIScache));
	HIScache = NEW(_HIScache, HIScachesize);
	memset((void *)HIScache, '\0', HIScachesize * sizeof(_HIScache));
    }
    HIShitpos = HIShitneg = HISmisses = HISdne = 0;
    HISlogger("HISsetup end", S_HISsetup);
}

/*
**  Synchronize the in-core history file (flush it).
*/
void
HISsync(void)
{
    HISlogger("HISsync begin", S_HISsync);
    TMRstart(TMR_HISSYNC);
    if (HISdirty) {
	if (!dbzsync()) {
	    syslog(L_FATAL, "%s cant dbzsync %m", LogName);
	    exit(1);
	}
	HISdirty = 0;
    }
    TMRstop(TMR_HISSYNC);
    HISlogger("HISsync end", S_HISsync);
}


static void
HISlogstats(void) {
    HISlogger("HISlogstats begin", S_HISlogstats);
    syslog(L_NOTICE, "ME HISstats %d hitpos %d hitneg %d missed %d dne",
	   HIShitpos, HIShitneg, HISmisses, HISdne);
    HIShitpos = HIShitneg = HISmisses = HISdne = 0;
    HISlogger("HISlogstats end", S_HISlogstats);
}


/*
**  Close the history files.
*/
void
HISclose(void)
{
    HISlogger("HISclose begin", S_HISclose);
    if (HISwritefp != NULL) {
	HISsync();
	if (!dbzclose())
	    syslog(L_ERROR, "%s cant dbzclose %m", LogName);
	if (Fclose(HISwritefp) == EOF)
	    syslog(L_ERROR, "%s cant fclose history %m", LogName);
	HISwritefp = NULL;
	if (close(HISreadfd) < 0)
	    syslog(L_ERROR, "%s cant close history %m", LogName);
	HISreadfd = -1;
    }
    if (HIScache) {
	HISlogstats();			/* print final HISstats */
	HISlastlog = Now.time;
	DISPOSE(HIScache);
	HIScache = NULL;
	HIScachesize = 0;
    }
    TMRstop(TMR_HISSYNC);
    HISlogger("HISclose end", S_HISclose);
}


/*
**  Get the list of files under which a Message-ID is stored.
*/
TOKEN *
HISfilesfor(const HASH MessageID)
{
    static BUFFER	Files;
    off_t		offset;
    char	        *p;
    int	                i;
    static TOKEN	token;

    HISlogger("HISfilesfor begin", S_HISfilesfor);
    TMRstart(TMR_HISGREP);
    
    if (HISwritefp == NULL) {
        TMRstop(TMR_HISGREP);
        HISlogger("HISfilesfor end", S_HISfilesfor);
    	return NULL;
    }
    
    /* Get the seek value into the history file. */
    if (!dbzfetch(MessageID, &offset)) {
	TMRstop(TMR_HISGREP);
        HISlogger("HISfilesfor end", S_HISfilesfor);
	return NULL;
    }

    /* Get space. */
    if (Files.Data == NULL) {
	Files.Size = BUFSIZ;
	Files.Data = NEW(char, Files.Size);
    }

    /* Seek to the specified location. */
    if (lseek(HISreadfd, offset, SEEK_SET) == -1) {
        TMRstop(TMR_HISGREP);
        HISlogger("HISfilesfor end", S_HISfilesfor);
	return NULL;
    }

    /* Read the text until \n or EOF. */
    for (Files.Used = 0; ; ) {
	i = read(HISreadfd,
		&Files.Data[Files.Used], Files.Size - Files.Used - 1);
	if (i <= 0)
	    return NULL;
	Files.Used += i;
	Files.Data[Files.Used] = '\0';
	if ((p = strchr(Files.Data, '\n')) != NULL) {
	    *p = '\0';
	    break;
	}

	/* If we have half our buffer left, get more space. */
	if (Files.Size - Files.Used < Files.Size / 2) {
	    Files.Size += BUFSIZ;
	    RENEW(Files.Data, char, Files.Size);
	}
    }

    /* Move past the first two fields -- Message-ID and date info. */
    if ((p = strchr(Files.Data, HIS_FIELDSEP)) == NULL) {
        TMRstop(TMR_HISGREP);
        HISlogger("HISfilesfor end", S_HISfilesfor);
	return NULL;
    }
    if ((p = strchr(p + 1, HIS_FIELDSEP)) == NULL) {
        TMRstop(TMR_HISGREP);
        HISlogger("HISfilesfor end", S_HISfilesfor);
	return NULL;
    }
    if (!IsToken(++p)) {
        TMRstop(TMR_HISGREP);
        HISlogger("HISfilesfor end", S_HISfilesfor);
	return NULL;
    }
    token = TextToToken(p);
    TMRstop(TMR_HISGREP);
    HISlogger("HISfilesfor end", S_HISfilesfor);
    return &token;
}


/*
**  Have we already seen an article?
*/
bool
HIShavearticle(const HASH MessageID)
{
    bool	   val;
    
    HISlogger("HIShavearticle begin", S_HIShavearticle);
    if ((Now.time - HISlastlog) > 120) { /* was 3600 */
	HISlogstats();
	HISlastlog = Now.time;
    }

    TMRstart(TMR_HISHAVE);
    if (HISwritefp == NULL) {
        TMRstop(TMR_HISHAVE);
        HISlogger("HIShavearticle end", S_HIShavearticle);
    	return FALSE;
    }
    switch (HIScachelookup(MessageID)) {
    case HIScachehit:
	TMRstop(TMR_HISHAVE);
        HISlogger("HIShavearticle end", S_HIShavearticle);
	return TRUE;
    case HIScachemiss:
	TMRstop(TMR_HISHAVE);
        HISlogger("HIShavearticle end", S_HIShavearticle);
	return FALSE;
    case HIScachedne:
	val = dbzexists(MessageID);
	HIScacheadd(MessageID, val);
	if (val)
	    HISmisses++;
	else
	    HISdne++;
	TMRstop(TMR_HISHAVE);
        HISlogger("HIShavearticle end", S_HIShavearticle);
	return val;
    }
    TMRstop(TMR_HISHAVE);
    HISlogger("HIShavearticle end", S_HIShavearticle);
    return FALSE;
}


/*
**  Write a history entry.
*/
bool
HISwrite(const ARTDATA *Data, const HASH hash, char *paths)
{
    static char		NOPATHS[] = "";
    off_t		offset;
    int			i;
    
    TMRstart(TMR_HISWRITE);
    HISlogger("HISwrite begin", S_HISwrite);
    if (HISwritefp == NULL) {
        TMRstop(TMR_HISWRITE);
	HISlogger("HISwrite end", S_HISwrite);
        return FALSE;
    }
    if (paths != NULL && paths[0] != '\0') {
	/* if (!innconf->storageapi) HISslashify(paths); */
    } else
	paths = NOPATHS;

    offset = ftello(HISwritefp);
    if (Data->Expires > 0)
	i = fprintf(HISwritefp, "[%s]%c%lu%c%lu%c%lu%c%s\n",
		    HashToText(hash), HIS_FIELDSEP,
		    (unsigned long)Data->Arrived, HIS_SUBFIELDSEP,
		    (unsigned long)Data->Expires, HIS_SUBFIELDSEP,
		    (unsigned long)Data->Posted, HIS_FIELDSEP, paths);
    else
	i = fprintf(HISwritefp, "[%s]%c%lu%c%s%c%lu%c%s\n",
		    HashToText(hash), HIS_FIELDSEP,
		    (unsigned long)Data->Arrived, HIS_SUBFIELDSEP,
		    HIS_NOEXP, HIS_SUBFIELDSEP,
		    (unsigned long)Data->Posted, HIS_FIELDSEP, paths);

    if (i == EOF || fflush(HISwritefp) == EOF) {
	/* The history line is now an orphan... */
	i = errno;
	syslog(L_ERROR, "%s cant write history %m", LogName);
	IOError("history", i);
	TMRstop(TMR_HISWRITE);
	HISlogger("HISwrite end", S_HISwrite);
	return FALSE;
    }

    /* Set up the database values and write them. */
    if (dbzstore(hash, offset) == DBZSTORE_ERROR) {
	i = errno;
	syslog(L_ERROR, "%s cant dbzstore [%s]@%.0f: %m", LogName,
	       HashToText(hash), (double)offset);
	IOError("history database", i);
	TMRstop(TMR_HISWRITE);
	HISlogger("HISwrite end", S_HISwrite);
	return FALSE;
    }
    HIScacheadd(hash, TRUE);
    TMRstop(TMR_HISWRITE); /* not later as HISsync() has its own timer */
    
    if (++HISdirty >= innconf->icdsynccount)
	HISsync();
    HISlogger("HISwrite end", S_HISwrite);
    return TRUE;
}

/*
**  Write a bogus history entry to keep us from seeing this article again
*/
bool
HISremember(const HASH hash)
{
    off_t		offset;
    int			i;

    HISlogger("HISremember begin", S_HISremember);
    TMRstart(TMR_HISWRITE);
    
    if (HISwritefp == NULL) {
        TMRstop(TMR_HISWRITE);
        HISlogger("HISremember end", S_HISremember);
        return FALSE;
    }
    
    offset = ftello(HISwritefp);
    /* Convert the hash to hex */
    i = fprintf(HISwritefp, "[%s]%c%lu%c%s%c%lu\n",
		HashToText(hash), HIS_FIELDSEP,
		(unsigned long)Now.time, HIS_SUBFIELDSEP,
		HIS_NOEXP, HIS_SUBFIELDSEP, (unsigned long)Now.time);

    if (i == EOF || fflush(HISwritefp) == EOF) {
	/* The history line is now an orphan... */
	i = errno;
	syslog(L_ERROR, "%s cant write history %m", LogName);
	IOError("history", i);
	TMRstop(TMR_HISWRITE);
        HISlogger("HISremember end", S_HISremember);
	return FALSE;
    } 

    /* Set up the database values and write them. */
    if (dbzstore(hash, offset) == DBZSTORE_ERROR) {
	i = errno;
	syslog(L_ERROR, "%s cant dbzstore [%s]@%.0f: %m", LogName,
	       HashToText(hash), (double)offset);
	IOError("history database", i);
	TMRstop(TMR_HISWRITE);
        HISlogger("HISremember end", S_HISremember);
	return FALSE;
    }
    HIScacheadd(hash, TRUE);
    TMRstop(TMR_HISWRITE);
    
    if (++HISdirty >= innconf->icdsynccount)
	HISsync();
    HISlogger("HISremember end", S_HISremember);
    return TRUE;
}

/*  $Revision$
**
**  History file routines.
*/
#include <stdio.h>
#include <sys/types.h>
#include "configdata.h"
#include "clibrary.h"
#include "innd.h"
#include "dbz.h"

typedef struct __HISCACHE {
    unsigned int        Hash;      /* Hash value of the message-id using dbzhash() */
    BOOL		Found;     /* Whether this entry is in the dbz file yet */
} _HIScache;

typedef enum {HIScachehit, HIScachemiss, HIScachedne} HISresult;

STATIC char		HIShistpath[] = _PATH_HISTORY;
STATIC FILE		*HISwritefp;
STATIC int		HISreadfd;
STATIC int		HISdirty;
STATIC int		HISincore = INND_DBZINCORE;
STATIC _HIScache	*HIScache;
STATIC int              HIScachesize; /* Number of entries in HIScache */
STATIC int              HIShitpos; /* The entry existed in the cache and in history */
STATIC int              HIShitneg; /* The entry existed in the cache but not in history */
STATIC int              HISmisses; /* The entry was not in the cache, but was in the history file */
STATIC int              HISdne;    /* The entry was not in cache or history */

/*
** Put an entry into the history cache 
*/
void HIScacheadd(char *MessageID, int len, BOOL Found) {
    unsigned int  i, hash, loc;
    hash_t h;
    int tocopy;

    if (HIScache == NULL)
	return;
    h = dbzhash(MessageID, len);
    tocopy = (sizeof(h) < sizeof(hash)) ? sizeof(h) : sizeof(hash);
    memcpy(&hash, &h, tocopy);
    memcpy(&loc, ((char *)&h) + (sizeof(h) - tocopy), tocopy);
    i = loc % HIScachesize;
    HIScache[i].Hash = hash;
    HIScache[i].Found = Found;
}

/*
** Lookup an entry in the history cache
*/
HISresult HIScachelookup(char *MessageID, int len) {
    unsigned int i, hash, loc;
    hash_t h;
    int tocopy;

    if (HIScache == NULL)
	return HIScachedne;
    h = dbzhash(MessageID, len);
    tocopy = (sizeof(h) < sizeof(hash)) ? sizeof(h) : sizeof(hash);
    memcpy(&hash, &h, tocopy);
    memcpy(&loc, ((char *)&h) + (sizeof(h) - tocopy), tocopy);
    i = loc % HIScachesize;
    if (HIScache[i].Hash == hash) {
        if (HIScache[i].Found) {
            HIShitpos++;
            return HIScachehit;
        } else {
            HIShitneg++;
            return HIScachemiss;
        }
    } else {
        return HIScachedne;
    }
}

/*
**  Set up the history files.
*/
void
HISsetup()
{
    char *HIScachesizestr;
    dbzoptions opt;
    
    if (HISwritefp == NULL) {
	/* Open the history file for appending formatted I/O. */
	if ((HISwritefp = fopen(HIShistpath, "a")) == NULL) {
	    syslog(L_FATAL, "%s cant fopen %s %m", LogName, HIShistpath);
	    exit(1);
	}
	CloseOnExec((int)fileno(HISwritefp), TRUE);

	/* Open the history file for reading. */
	if ((HISreadfd = open(HIShistpath, O_RDONLY)) < 0) {
	    syslog(L_FATAL, "%s cant open %s %m", LogName, HIShistpath);
	    exit(1);
	}
	CloseOnExec(HISreadfd, TRUE);

	/* Open the DBZ file. */
	dbzgetoptions(&opt);
	opt.writethrough = TRUE;
	opt.idx_incore = INCORE_NO;
	opt.exists_incore = HISincore ? INCORE_MMAP : INCORE_NO;
	dbzsetoptions(opt);
	if (!dbminit(HIShistpath)) {
	    syslog(L_FATAL, "%s cant dbminit %s %m", HIShistpath, LogName);
	    exit(1);
	}
    }
    if ((HIScachesizestr = GetConfigValue(_CONF_HISCACHESIZE)) == NULL) {
	HIScache = NULL;
	HIScachesize = 0;
    } else {
	HIScachesize = atoi(HIScachesizestr);
	if (HIScache != NULL)
	    free(HIScache);
	HIScache = NEW(_HIScache, HIScachesize);
	memset((void *)&HIScache, '\0', sizeof(HIScache));
    }
    HIShitpos = HIShitneg = HISmisses = HISdne = 0;
}


/*
**  Synchronize the in-core history file (flush it).
*/
void
HISsync()
{
    if (HISdirty) {
	if (!dbzsync()) {
	    syslog(L_FATAL, "%s cant dbzsync %m", LogName);
	    exit(1);
	}
	HISdirty = 0;
    }
}


/*
**  Close the history files.
*/
void
HISclose()
{
    if (HISwritefp != NULL) {
	HISsync();
	if (!dbmclose())
	    syslog(L_ERROR, "%s cant dbmclose %m", LogName);
	if (fclose(HISwritefp) == EOF)
	    syslog(L_ERROR, "%s cant fclose history %m", LogName);
	HISwritefp = NULL;
	if (close(HISreadfd) < 0)
	    syslog(L_ERROR, "%s cant close history %m", LogName);
	HISreadfd = -1;
    }
    if (HIScache) {
	free(HIScache);
	HIScache = NULL;
	HIScachesize = 0;
    }
}


/*
**  File in the DBZ datum for a Message-ID, making sure not to copy any
**  illegal characters.
*/
STATIC void
HISsetkey(p, keyp)
    register char	*p;
    datum		*keyp;
{
    static BUFFER	MessageID;
    register char	*dest;
    register int	i;

    /* Get space to hold the ID. */
    i = strlen(p);
    if (MessageID.Data == NULL) {
	MessageID.Data = NEW(char, i + 1);
	MessageID.Size = i;
    }
    else if (MessageID.Size < i) {
	RENEW(MessageID.Data, char, i + 1);
	MessageID.Size = i;
    }

    for (keyp->dptr = dest = MessageID.Data; *p; p++)
	if (*p == HIS_FIELDSEP || *p == '\n')
	    *dest++ = HIS_BADCHAR;
	else
	    *dest++ = *p;
    *dest = '\0';

    keyp->dsize = dest - MessageID.Data + 1;
}


/*
**  Get the list of files under which a Message-ID is stored.
*/
char *
HISfilesfor(MessageID)
    char		*MessageID;
{
    static BUFFER	Files;
    char		*dest;
    datum		key;
    datum		val;
    long		offset;
    register char	*p;
    register int	i;

    /* Get the seek value into the history file. */
    HISsetkey(MessageID, &key);
    val = dbzfetch(key);
    if (val.dptr == NULL || val.dsize != sizeof offset)
	return NULL;

    /* Get space. */
    if (Files.Data == NULL) {
	Files.Size = BUFSIZ;
	Files.Data = NEW(char, Files.Size);
    }

    /* Copy the value to an aligned spot. */
    memmove(&offset, val.dptr, sizeof(offset));
    if (lseek(HISreadfd, offset, SEEK_SET) == -1)
	return NULL;

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
    if ((p = strchr(Files.Data, HIS_FIELDSEP)) == NULL)
	return NULL;
    if ((p = strchr(p + 1, HIS_FIELDSEP)) == NULL)
	return NULL;

    /* Translate newsgroup separators to slashes, return the fieldstart. */
    for (dest = ++p; *p; p++)
	if (*p == '.')
	    *p = '/';
    return dest;
}

STATIC void HISlogstats() {
    syslog(LOG_INFO, "ME HISstats %d hitpos %d hitneg %d missed %d dne",
	   HIShitpos, HIShitneg, HISmisses, HISdne);
    HIShitpos = HIShitneg = HISmisses = HISdne = 0;
}



/*
**  Have we already seen an article?
*/
BOOL
HIShavearticle(MessageID)
    char	*MessageID;
{
    datum	   key;
    BOOL	   val;
    int            index;
    hash_t	   hash;
    STATIC time_t  lastlog;       /* Last time that we logged stats */   
    

    if ((Now.time - lastlog) > 3600) {
	HISlogstats();
	lastlog = Now.time;
    }
    switch (HIScachelookup(MessageID, strlen(MessageID))) {
    	case HIScachehit:
    	    return TRUE;
    	case HIScachemiss:
    	    return FALSE;
    }
    HISsetkey(MessageID, &key);
    val = dbzexists(key);
    HIScacheadd(MessageID, strlen(MessageID), val);
    if (val)
	HISmisses++;
    else
	HISdne++;
    return val;
}


/*
**  Turn a history filename entry from slashes to dots.  It's a pity
**  we have to do this.
*/
STATIC void
HISslashify(p)
    register char	*p;
{
    register char	*last;

    for (last = NULL; *p; p++) {
	if (*p == '/') {
	    *p = '.';
	    last = p;
	}
	else if (*p == ' ' && last != NULL)
	    *last = '/';
    }
    if (last)
	*last = '/';
}


/*
**  Write a history entry.
*/
BOOL
HISwrite(Data, paths)
    ARTDATA		*Data;
    char		*paths;
{
    static char		NOPATHS[] = "";
    long		offset;
    datum		key;
    datum		val;
    int			i;
    hash_t              hash;

    HISsetkey(Data->MessageID, &key);
    if (paths != NULL && paths[0] != '\0')
	HISslashify(paths);
    else
	paths = NOPATHS;

    offset = ftell(HISwritefp);
    if (Data->Expires > 0)
	i = fprintf(HISwritefp, "%s%c%ld%c%ld%c%ld%c%s\n",
		key.dptr, HIS_FIELDSEP,
		(long)Data->Arrived, HIS_SUBFIELDSEP, (long)Data->Expires,
		    HIS_SUBFIELDSEP, (long)Data->Posted, HIS_FIELDSEP,
		paths);
    else
	i = fprintf(HISwritefp, "%s%c%ld%c%s%c%ld%c%s\n",
		key.dptr, HIS_FIELDSEP,
		(long)Data->Arrived, HIS_SUBFIELDSEP, HIS_NOEXP,
		    HIS_SUBFIELDSEP, (long)Data->Posted, HIS_FIELDSEP,
		paths);
    if (i == EOF || fflush(HISwritefp) == EOF) {
	/* The history line is now an orphan... */
	i = errno;
	syslog(L_ERROR, "%s cant write history %m", LogName);
	IOError("history", i);
	return FALSE;
    }

    /* Set up the database values and write them. */
    val.dptr = (char *)&offset;
    val.dsize = sizeof offset;
    if (!dbzstore(key, val)) {
	i = errno;
	syslog(L_ERROR, "%s cant dbzstore %m", LogName);
	IOError("history database", i);
	return FALSE;
    }
    HIScacheadd((char *)Data->MessageID, strlen(Data->MessageID), TRUE);
    
    if (++HISdirty >= ICD_SYNC_COUNT)
	HISsync();
    return TRUE;
}

/*  $Revision$
**
**  Expire news articles.
*/
#include <stdio.h>
#include <sys/types.h>
#include "configdata.h"
#include "clibrary.h"
#include <ctype.h>
#include <sys/stat.h>
#if	defined(DO_NEED_TIME)
#include <time.h>
#endif	/* defined(DO_NEED_TIME) */
#include <sys/time.h>
#include <errno.h>
#include "paths.h"
#include "libinn.h"
#include "inndcomm.h"
#include "dbz.h"
#include "qio.h"
#include "macros.h"
#include <syslog.h>  


/*
**  Stuff that more or less duplicates stuff in innd.
*/
#define NGH_HASH(Name, p, j)    \
	for (p = Name, j = 0; *p; ) j = (j << 5) + j + *p++
#define NGH_SIZE	2048
#define NGH_BUCKET(j)	&NGHtable[j & (NGH_SIZE - 1)]

typedef struct _BUFFER {
    int		Size;
    int		Used;
    int		Left;
    char	*Data;
} BUFFER;

typedef struct _NEWSGROUP {
    char		*Name;
    char		*Rest;
    unsigned long	Last;
    unsigned long	Lastpurged;
	/* These fields are new. */
    time_t		Keep;
    time_t		Default;
    time_t		Purge;
    /* X flag => remove entire article when it expires in this group */
} NEWSGROUP;

typedef struct _EXPIRECLASS {
    time_t              Keep;
    time_t              Default;
    time_t              Purge;
    BOOL                Missing;
    BOOL                ReportedMissing;
} EXPIRECLASS;

typedef struct _NGHASH {
    int		Size;
    int		Used;
    NEWSGROUP	**Groups;
} NGHASH;

/*
**  Expire-specific stuff.
*/
#define MAGIC_TIME	49710.

typedef struct _BADGROUP {
    struct _BADGROUP	*Next;
    char		*Name;
    BOOL		HasDirectory;
} BADGROUP;

STATIC BOOL		EXPquiet;
STATIC BOOL		EXPtracing;
STATIC BOOL		EXPusepost;
STATIC BOOL		Ignoreselfexpire = FALSE;
STATIC char		*ACTIVE;
STATIC char		*SPOOL = NULL;
STATIC int		nGroups;
STATIC FILE		*EXPunlinkfile;
STATIC NEWSGROUP	*Groups;
STATIC NEWSGROUP	EXPdefault;
STATIC EXPIRECLASS      EXPclasses[NUM_STORAGE_CLASSES];
STATIC STRING		EXPreason;
STATIC time_t		EXPremember;
STATIC time_t		Now;
STATIC time_t		RealNow;

/* Statistics; for -v flag. */
STATIC char		*EXPgraph;
STATIC int		EXPverbose;
STATIC long		EXPprocessed;
STATIC long		EXPunlinked;
STATIC long		EXPhistdrop;
STATIC long		EXPhistremember;
STATIC long		EXPallgone;
STATIC long		EXPstillhere;

STATIC NORETURN CleanupAndExit(BOOL Server, BOOL Paused, int x);
#if ! defined (atof)            /* NEXT defines aotf as a macro */
extern double		atof();
#endif

STATIC int EXPsplit(char *p, char sep, char **argv, int count);

enum KR {Keep, Remove};



/*
**  Open a file or give up.
*/
STATIC FILE *EXPfopen(BOOL Remove, STRING Name, char *Mode, BOOL Needclean, BOOL Server, BOOL Paused)
{
    FILE	*F;

    if (Remove && unlink(Name) < 0 && errno != ENOENT)
	(void)fprintf(stderr, "Warning, can't remove %s, %s\n",
		Name, strerror(errno));
    if ((F = fopen(Name, Mode)) == NULL) {
	(void)fprintf(stderr, "Can't open %s in %s mode, %s\n",
		Name, Mode, strerror(errno));
	if (Needclean)
	    CleanupAndExit(Server, Paused, 1);
	else
	    exit(1);
    }
    return F;
}


/*
**  Split a line at a specified field separator into a vector and return
**  the number of fields found, or -1 on error.
*/
STATIC int EXPsplit(char *p, char sep, char **argv, int count)
{
    int	                i;

    if (!p)
      return 0;

    while (*p == sep)
      ++p;

    if (!*p)
      return 0;

    if (!p)
      return 0;

    while (*p == sep)
      ++p;

    if (!*p)
      return 0;

    for (i = 1, *argv++ = p; *p; )
	if (*p++ == sep) {
	    p[-1] = '\0';
	    for (; *p == sep; p++);
	    if (!*p)
		return i;
	    if (++i == count)
		/* Overflow. */
		return -1;
	    *argv++ = p;
	}
    return i;
}


/*
**  Parse a number field converting it into a "when did this start?".
**  This makes the "keep it" tests fast, but inverts the logic of
**  just about everything you expect.  Print a message and return FALSE
**  on error.
*/
STATIC BOOL EXPgetnum(int line, char *word, time_t *v, char *name)
{
    char	        *p;
    BOOL	        SawDot;
    double		d;

    if (caseEQ(word, "never")) {
	*v = (time_t)0;
	return TRUE;
    }

    /* Check the number.  We don't have strtod yet. */
    for (p = word; ISWHITE(*p); p++)
	continue;
    if (*p == '+' || *p == '-')
	p++;
    for (SawDot = FALSE; *p; p++)
	if (*p == '.') {
	    if (SawDot)
		break;
	    SawDot = TRUE;
	}
	else if (!CTYPE(isdigit, (int)*p))
	    break;
    if (*p) {
	(void)fprintf(stderr, "Line %d, bad `%c' character in %s field\n",
		line, *p, name);
	return FALSE;
    }
    d = atof(word);
    if (d > MAGIC_TIME)
	*v = (time_t)0;
    else
	*v = Now - (time_t)(d * 86400.);
    return TRUE;
}


/*
**  Set the expiration fields for all groups that match this pattern.
*/
STATIC void EXPmatch(char *p, NEWSGROUP *v, char mod)
{
    NEWSGROUP	        *ngp;
    int	                i;
    BOOL	        negate;

    negate = *p == '!';
    if (negate)
	p++;
    for (ngp = Groups, i = nGroups; --i >= 0; ngp++)
	if (negate ? !wildmat(ngp->Name, p) : wildmat(ngp->Name, p))
	    if (mod == 'a'
	     || (mod == 'm' && ngp->Rest[0] == NF_FLAG_MODERATED)
	     || (mod == 'u' && ngp->Rest[0] != NF_FLAG_MODERATED)) {
		ngp->Keep      = v->Keep;
		ngp->Default   = v->Default;
		ngp->Purge     = v->Purge;
		if (EXPverbose > 4) {
		    (void)printf("%s", ngp->Name);
		    (void)printf(" %13.13s", ctime(&v->Keep) + 3);
		    (void)printf(" %13.13s", ctime(&v->Default) + 3);
		    (void)printf(" %13.13s", ctime(&v->Purge) + 3);
		    (void)printf(" (%s)\n", p);
		}
	    }
}


/*
**  Parse the expiration control file.  Return TRUE if okay.
*/
STATIC BOOL EXPreadfile(FILE *F)
{
    char	        *p;
    int	                i;
    int	                j;
    int	                k;
    char	        mod;
    NEWSGROUP		v;
    BOOL		SawDefault;
    char		buff[BUFSIZ];
    char		*fields[7];
    char		**patterns;

    /* Scan all lines. */
    EXPremember = -1;
    SawDefault = FALSE;
#if 0
    /* XXX disabled until we find a way to re-enable ng-based expire. */
    patterns = NEW(char*, nGroups);
#endif
    for (i = 0; i < NUM_STORAGE_CLASSES; i++)
	EXPclasses[i].ReportedMissing = EXPclasses[i].Missing = TRUE;
    
    for (i = 1; fgets(buff, sizeof buff, F) != NULL; i++) {
	if ((p = strchr(buff, '\n')) == NULL) {
	    (void)fprintf(stderr, "Line %d too long\n", i);
	    return FALSE;
	}
	*p = '\0';
        p = strchr(buff, '#');
	if (p)
	    *p = '\0';
	else
	    p = buff + strlen(buff);
	while (--p >= buff) {
	    if (isspace((int)*p))
                *p = '\0';
            else
                break;
        }
        if (buff[0] == '\0')
	    continue;
	if ((j = EXPsplit(buff, ':', fields, SIZEOF(fields))) == -1) {
	    (void)fprintf(stderr, "Line %d too many fields\n", i);
	    return FALSE;
	}

	/* Expired-article remember line? */
	if (EQ(fields[0], "/remember/")) {
	    if (j != 2) {
		(void)fprintf(stderr, "Line %d bad format\n", i);
		return FALSE;
	    }
	    if (EXPremember != -1) {
		(void)fprintf(stderr, "Line %d duplicate /remember/\n", i);
		return FALSE;
	    }
	    if (!EXPgetnum(i, fields[1], &EXPremember, "remember"))
		return FALSE;
	    continue;
	}

	/* Storage class line? */
	if (j == 4) {
	    j = atoi(fields[0]);
	    if ((j < 0) || (j > NUM_STORAGE_CLASSES)) {
		fprintf(stderr, "Line %d bad storage class %d\n", i, j);
	    }
	
	    if (!EXPgetnum(i, fields[1], &EXPclasses[j].Keep,    "keep")
		|| !EXPgetnum(i, fields[2], &EXPclasses[j].Default, "default")
		|| !EXPgetnum(i, fields[3], &EXPclasses[j].Purge,   "purge"))
		return FALSE;
	    /* These were turned into offsets, so the test is the opposite
	     * of what you think it should be.  If Purge isn't forever,
	     * make sure it's greater then the other two fields. */
	    if (EXPclasses[j].Purge) {
		/* Some value not forever; make sure other values are in range. */
		if (EXPclasses[j].Keep && EXPclasses[j].Keep < EXPclasses[j].Purge) {
		    (void)fprintf(stderr, "Line %d keep>purge\n", i);
		    return FALSE;
		}
		if (EXPclasses[j].Default && EXPclasses[j].Default < EXPclasses[j].Purge) {
		    (void)fprintf(stderr, "Line %d default>purge\n", i);
		    return FALSE;
		}
	    }
	    EXPclasses[j].Missing = FALSE;
	    continue;
	}

	/* Regular expiration line -- right number of fields? */
#if 0
	if (j != 5) {
#endif
	    (void)fprintf(stderr, "Line %d bad format\n", i);
	    return FALSE;
#if 0
	}

	/* Parse the fields. */
	if (strchr(fields[1], 'M') != NULL)
	    mod = 'm';
	else if (strchr(fields[1], 'U') != NULL)
	    mod = 'u';
	else if (strchr(fields[1], 'A') != NULL)
	    mod = 'a';
	else {
	    (void)fprintf(stderr, "Line %d bad modflag\n", i);
	    return FALSE;
	}
	if (!EXPgetnum(i, fields[2], &v.Keep,    "keep")
	 || !EXPgetnum(i, fields[3], &v.Default, "default")
	 || !EXPgetnum(i, fields[4], &v.Purge,   "purge"))
	    return FALSE;
	/* These were turned into offsets, so the test is the opposite
	 * of what you think it should be.  If Purge isn't forever,
	 * make sure it's greater then the other two fields. */
	if (v.Purge) {
	    /* Some value not forever; make sure other values are in range. */
	    if (v.Keep && v.Keep < v.Purge) {
		(void)fprintf(stderr, "Line %d keep>purge\n", i);
		return FALSE;
	    }
	    if (v.Default && v.Default < v.Purge) {
		(void)fprintf(stderr, "Line %d default>purge\n", i);
		return FALSE;
	    }
	}

	/* Is this the default line? */
	if (fields[0][0] == '*' && fields[0][1] == '\0' && mod == 'a') {
	    if (SawDefault) {
		(void)fprintf(stderr, "Line %d duplicate default\n", i);
                return FALSE;
	    }
	    EXPdefault.Keep    = v.Keep;
	    EXPdefault.Default = v.Default;
	    EXPdefault.Purge   = v.Purge;
	    SawDefault = TRUE;
	}

	/* Assign to all groups that match the pattern and flags. */
	if ((j = EXPsplit(fields[0], ',', patterns, nGroups)) == -1) {
	    (void)fprintf(stderr, "Line %d too many patterns\n", i);
	    return FALSE;
	}
	for (k = 0; k < j; k++)
	    EXPmatch(patterns[k], &v, mod);
#endif
    }
#if 0
    DISPOSE(patterns);
#endif

    return TRUE;
}

/*
**  Should we keep the specified article?
*/
STATIC enum KR EXPkeepit(TOKEN token, time_t when, time_t Expires)
{
    EXPIRECLASS         class;

    class = EXPclasses[token.class];
    if (class.Missing) {
	if (!class.ReportedMissing) {
	    fprintf(stderr, "Class definition %d is missing from control file, assuming zero expiration\n",
		    token.class);
	} else {
	    EXPclasses[token.class].ReportedMissing = TRUE;
	}
	return Remove;
    }
    /* Bad posting date? */
    if (when > (RealNow + 86400)) {
	/* Yes -- force the article to go to right now */
	when = Expires ? class.Purge : class.Default;
    }
    if (EXPverbose > 2) {
	if (EXPverbose > 3)
	    printf("%s age = %0.2f\n", TokenToText(token), (Now - when) / 86400.);
	if (Expires == 0) {
	    if (when <= class.Default)
		(void)printf("%s too old (no exp)\n", TokenToText(token));
	} else {
	    if (when <= class.Purge)
		(void)printf("%s later than purge\n", TokenToText(token));
	    if (when >= class.Keep)
		(void)printf("%s earlier than min\n", TokenToText(token));
	    if (Now >= Expires)
		(void)printf("%s later than header\n", TokenToText(token));
	}
    }
    
    /* If no expiration, make sure it wasn't posted before the default. */
    if (Expires == 0) {
	if (when >= class.Default)
	    return Keep;
	
	/* Make sure it's not posted before the purge cut-off and
	 * that it's not due to expire. */
    } else {
	if (when >= class.Purge && (Expires >= Now || when >= class.Keep))
	    return Keep;
    }
    return Remove;

}


/*
**  An article can be removed.  Either print a note, or actually remove it.
**  Also fill in the article size.
*/
STATIC void EXPremove(TOKEN token, OFFSET_T *size, BOOL index)
{
    /* Turn into a filename and get the size if we need it. */
    if (EXPverbose > 1)
	(void)printf("\tunlink %s\n", TokenToText(token));

    if (EXPtracing) {
	EXPunlinked++;
	(void)printf("%s\n", TokenToText(token));
	return;
    }
    
    EXPunlinked++;
    if (EXPunlinkfile) {
	(void)fprintf(EXPunlinkfile, "%s\n", TokenToText(token));
	if (!ferror(EXPunlinkfile))
	    return;
	(void)fprintf(stderr, "Can't write to -z file, %s\n",
		      strerror(errno));
	(void)fprintf(stderr, "(Will ignore it for rest of run.)\n");
	(void)fclose(EXPunlinkfile);
	EXPunlinkfile = NULL;
    }
    if (!SMcancel(token) && SMerrno != SMERR_NOENT && SMerrno != SMERR_UNINIT)
	fprintf(stderr, "Can't unlink %s\n", TokenToText(token));
}

/*
**  Do the work of expiring one line.
*/
STATIC BOOL EXPdoline(FILE *out, char *line, int length)
{
    static char		IGNORING[] = "Ignoring bad line, \"%.20s...\"\n";
    static OFFSET_T	Offset;
    char	        *p;
    char	        *q;
    int	                i;
    char		*fields[4];
    time_t		Arrived;
    time_t		Expires;
    time_t		Posted;
    time_t		when;
    OFFSET_T		where;
    OFFSET_T		size;
    HASH		key;
    char		date[20];
    BOOL		HasSelfexpire = FALSE;
    BOOL		Selfexpired = FALSE;
    ARTHANDLE		*article;
    TOKEN		token;
    char		*tokentext;
    enum KR             kr;
#ifndef	DO_TAGGED_HASH
    void	*ivalue;
    idxrec	ionevalue;
#endif

    /* Split up the major fields. */
    i = EXPsplit(line, HIS_FIELDSEP, fields, SIZEOF(fields));
    if (i != 2 && i != 3) {
	(void)fprintf(stderr, IGNORING, line);
	return TRUE;
    }

    /* Check to see if this messageid has already been written to the
       text file.  Unfortunately, this is the only clean way to do this */
    if (fields[0][0] != '[') {
	fprintf(stderr, "Invalid hash %s, skipping\n", fields[0]);
	return TRUE;
    }
    if (strlen(fields[0]) != ((sizeof(HASH) * 2) + 2)) {
	fprintf(stderr, "Invalid length for hash %s, skipping\n", fields[0]);
	return TRUE;
    }
    key = TextToHash(&fields[0][1]);
    if (dbzexists(key)) {
	fprintf(stderr, "Duplicate message-id \"%s\" in history\n", fields[0]);
	return TRUE;
    }
    
    /* Split up the time field, robustly. */
    if ((p = strchr(fields[1], HIS_SUBFIELDSEP)) == NULL) {
	/* One sub-field:  when the article arrived. */
	Arrived = atol(fields[1]);
	Expires = 0;
	Posted = Arrived;
    }
    else {
	*p = '\0';
	Arrived = atol(fields[1]);
	*p++ = HIS_SUBFIELDSEP;
	if ((q = strchr(p, HIS_SUBFIELDSEP)) == NULL) {
	    /* Two sub-fields:  arrival and expiration. */
	    Expires = EQ(p, HIS_NOEXP) ? 0 : atol(p);
	    Posted = Arrived;
	}
	else {
	    /* All three sub-fields:  arrival, expiration, posted. */
	    *q = '\0';
	    Expires = EQ(p, HIS_NOEXP) ? 0 : atol(p);
	    *q++ = HIS_SUBFIELDSEP;
	    Posted = atol(q);
	}
    }

    if (i == 2) {
	/* History line for already-expired article. */
	if (Arrived < EXPremember || Arrived > Now + 86400) {
	    if (EXPverbose > 3)
		(void)printf("forget: %s\n", line);
	    EXPhistdrop++;
	    return TRUE;
	}

	/* Not time to forget about this one yet. */
	if (out) {
	    where = Offset;
	    (void)fprintf(out, "%s%c%s\n", fields[0], HIS_FIELDSEP, fields[1]);
	    Offset += strlen(fields[0]) + 1 + strlen(fields[1]) + 1;
	    if (EXPverbose > 3)
		(void)printf("remember: %s\n", line);
	    EXPhistremember++;
	}
    } else {
	/* Active article -- split up the file entries. */
	if (!IsToken(fields[2])) {
	    fprintf(stderr, "Invalid token %s, skipping\n", fields[2]);
	    return TRUE;
	}
	token = TextToToken(fields[2]);
	if (SMprobe(SELFEXPIRE, &token)) {
	    if ((article = SMretrieve(token, RETR_STAT)) == (ARTHANDLE *)NULL) {
		HasSelfexpire = TRUE;
		Selfexpired = TRUE;
	    } else {
		/* the article is still alive */
		SMfreearticle(article);
		if (!Ignoreselfexpire)
		    HasSelfexpire = TRUE;
	    }
	}
	when = EXPusepost ? Posted : Arrived;
	EXPprocessed++;
	
	if (HasSelfexpire) {
	    if (Selfexpired || token.type == TOKEN_EMPTY) {
		if (EXPremember > 0 && out != NULL) {
		    where = Offset;
		    if (Arrived > RealNow)
			Arrived = RealNow;
		    (void)sprintf(date, "%lu", (unsigned long)Arrived);
		    (void)fprintf(out, "%s%c%s%c%s\n",
				  fields[0], HIS_FIELDSEP,
				  date, HIS_SUBFIELDSEP, HIS_NOEXP);
		    Offset += strlen(fields[0]) + 1
			+ strlen(date) + 1 + STRLEN(HIS_NOEXP) + 1;
		    if (EXPverbose > 3)
			(void)printf("remember history: %s%c%s%c%s\n",
				     fields[0], HIS_FIELDSEP,
				     date, HIS_SUBFIELDSEP, HIS_NOEXP);
		    EXPallgone++;
		}
	    } else if (out != NULL) {
		tokentext = TokenToText(token);
		where = Offset;
		(void)fprintf(out, "%s%c%s%c%s\n",
			fields[0], HIS_FIELDSEP, fields[1], HIS_FIELDSEP,
			tokentext);
		Offset += strlen(fields[0]) + 1 + strlen(fields[1]) + 1
			+ strlen(tokentext) + 1;
		if (EXPverbose > 3)
		    (void)printf("remember article: %s%c%s%c%s\n",
			    fields[0], HIS_FIELDSEP, fields[1], HIS_FIELDSEP,
				 tokentext);
		EXPstillhere++;
	    }
	} else  {
	    kr = EXPkeepit(token, when, Expires);
	    if (kr == Remove) {
		EXPremove(token, &size, FALSE);
	    }
	    
	    if (out) {
		where = Offset;
		(void)fprintf(out, "%s%c%s%c%s\n",
			      fields[0], HIS_FIELDSEP, fields[1], HIS_FIELDSEP,
			      TokenToText(token));
		Offset += strlen(fields[0]) + 1 + strlen(fields[1]) + 1
		    + strlen(TokenToText(token)) + 1;
		if (EXPverbose > 3)
		    (void)printf("remember article: %s%c%s%c%s\n",
				 fields[0], HIS_FIELDSEP, fields[1], HIS_FIELDSEP,
				 TokenToText(token));
		EXPstillhere++;
	    }
	}
    }
    
    if (out == NULL)
	return TRUE;

    if (ferror(out)) {
	(void)fprintf(stderr, "Can't write new history, %s\n",
		strerror(errno));
	return FALSE;
    }

    /* Set up the DBZ data.  We don't have to sanitize the Message-ID
     * since it had to have been clean to get in there. */
    if (EXPverbose > 4)
	(void)printf("\tdbz %s@%ld\n", fields[0], where);
#ifdef	DO_TAGGED_HASH
    if (dbzstore(key, where) == DBZSTORE_ERROR) {
	fprintf(stderr, "Can't store key, \"%s\"\n", strerror(errno));
	return FALSE;
    }
#else
    ionevalue.offset = where;
    ivalue = (void *)&ionevalue;
    if (dbzstore(key, ivalue) == DBZSTORE_ERROR) {
	fprintf(stderr, "Can't store key, \"%s\"\n", strerror(errno));
	return FALSE;
    }
#endif
    return TRUE;
}



/*
**  Clean up link with the server and exit.
*/
STATIC NORETURN CleanupAndExit(BOOL Server, BOOL Paused, int x)
{
    FILE	*F;

    if (Server)
	(void)ICCreserve("");
    if (Paused && ICCgo(EXPreason) != 0) {
	(void)fprintf(stderr, "Can't unpause server, %s\n",
		strerror(errno));
	x = 1;
    }
    if (Server && ICCclose() < 0) {
	(void)fprintf(stderr, "Can't close communication link, %s\n",
		strerror(errno));
	x = 1;
    }
    if (EXPunlinkfile && fclose(EXPunlinkfile) == EOF) {
	(void)fprintf(stderr, "Can't close -z file, %s\n", strerror(errno));
	x = 1;
    }

    /* Report stats. */
    if (EXPverbose) {
	(void)printf("Article lines processed %8ld\n", EXPprocessed);
	(void)printf("Articles retained       %8ld\n", EXPstillhere);
	(void)printf("Entries expired         %8ld\n", EXPallgone);
	(void)printf("Articles dropped        %8ld\n", EXPunlinked);
	(void)printf("Old entries dropped     %8ld\n", EXPhistdrop);
	(void)printf("Old entries retained    %8ld\n", EXPhistremember);
    }

    /* Append statistics to a summary file */
    if (EXPgraph) {
	F = EXPfopen(FALSE, EXPgraph, "a", FALSE, FALSE, FALSE);
	(void)fprintf(F, "%ld %ld %ld %ld %ld %ld %ld\n",
		      (long)Now, EXPprocessed, EXPstillhere, EXPallgone,
		      EXPunlinked, EXPhistdrop, EXPhistremember);
	(void)fclose(F);
    }

    SMshutdown();

    exit(x);
}

/*
**  Print a usage message and exit.
*/
STATIC NORETURN Usage(void)
{
    (void)fprintf(stderr, "Usage: expire [flags] [expire.ctl]\n");
    exit(1);
}


int main(int ac, char *av[])
{
    static char		CANTCD[] = "Can't cd to %s, %s\n";
    int                 i;
    int	                line;
    char 	        *p;
    QIOSTATE 	        *qp;
    FILE		*F;
    char		*active;
    STRING		History;
    STRING		HistoryText;
    STRING		HistoryPath;
    STRING		HistoryDB;
    char		*Historydir;
    char		*NHistory;
    char		*NHistorydir;
#ifdef	DO_TAGGED_HASH
    char		*Historypag;
    char		*NHistorypag;
#else
    char		*Historyhash;
    char		*Historyindex;
    char		*NHistoryhash;
    char		*NHistoryindex;
#endif
    char		*EXPhistdir;
    char		buff[SMBUF];
    FILE	        *out;
    BOOL		Server;
    BOOL		Paused;
    BOOL		Bad;
    BOOL		IgnoreOld;
    BOOL		Writing;
    BOOL		UnlinkFile;
    BOOL		LowmarkFile;
    BOOL		val;
    time_t		TimeWarp;
    dbzoptions          opt;

    /* First thing, set up logging and our identity. */
    openlog("expire", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);     

    /* Set defaults. */
    Server = TRUE;
    IgnoreOld = FALSE;
    History = "history";
    HistoryPath = NULL;
    Writing = TRUE;
    TimeWarp = 0;
    UnlinkFile = FALSE;
    LowmarkFile = FALSE;

    if (ReadInnConf() < 0) exit(1);

    HistoryText = COPY(cpcatpath(innconf->pathdb, _PATH_HISTORY));
    ACTIVE = COPY(cpcatpath(innconf->pathdb, _PATH_ACTIVE));
    SPOOL = innconf->patharticles;

    (void)umask(NEWSUMASK);

    /* find the default history file directory */
    EXPhistdir = COPY(HistoryText);
    p = strrchr(EXPhistdir, '/');
    if (p != NULL) {
	*p = '\0';
    }

    /* Parse JCL. */
    while ((i = getopt(ac, av, "f:h:d:eg:iNnpqr:tv:w:xz:")) != EOF)
	switch (i) {
	default:
	    Usage();
	    /* NOTREACHED */
	case 'd':
	    HistoryPath = optarg;
	    break;
	case 'f':
	    History = optarg;
	    break;
	case 'g':
	    EXPgraph = optarg;
	    break;
	case 'h':
	    HistoryText = optarg;
	    break;
	case 'i':
	    IgnoreOld = TRUE;
	    break;
	case 'N':
	    Ignoreselfexpire = TRUE;
	    break;
	case 'n':
	    Server = FALSE;
	    break;
	case 'p':
	    EXPusepost = TRUE;
	    break;
	case 'q':
	    EXPquiet = TRUE;
	    break;
	case 'r':
	    EXPreason = optarg;
	    break;
	case 't':
	    EXPtracing = TRUE;
	    break;
	case 'v':
	    EXPverbose = atoi(optarg);
	    break;
	case 'w':
	    TimeWarp = (time_t)(atof(optarg) * 86400.);
	    break;
	case 'x':
	    Writing = FALSE;
	    break;
	case 'z':
	    EXPunlinkfile = EXPfopen(TRUE, optarg, "a", FALSE, FALSE, FALSE);
	    UnlinkFile = TRUE;
	    break;
	}
    ac -= optind;
    av += optind;
    if ((ac != 0 && ac != 1))
	Usage();

    /* Get active file, parse it. */
    if ((active = ReadInFile(ACTIVE, (struct stat *)NULL)) == NULL) {
	(void)fprintf(stderr, "Can't read %s, %s\n",
		ACTIVE, strerror(errno));
	exit(1);
    }
    (void)time(&Now);
    RealNow = Now;
    Now += TimeWarp;

    /* Parse the control file. */
    if (av[0])
	F = EQ(av[0], "-") ? stdin : EXPfopen(FALSE, av[0], "r", FALSE, FALSE, FALSE);
    else
	F = EXPfopen(FALSE, cpcatpath(innconf->pathetc, _PATH_EXPIRECTL), "r", FALSE, FALSE, FALSE);
    if (!EXPreadfile(F)) {
	(void)fclose(F);
	(void)fprintf(stderr, "Format error in expire.ctl\n");
	exit(1);
    }
    (void)fclose(F);

    /* Set up the link, reserve the lock. */
    if (EXPreason == NULL) {
	(void)sprintf(buff, "Expiring process %ld", (long)getpid());
	EXPreason = COPY(buff);
    }
    if (Server) {
	/* If we fail, leave evidence behind. */
	if (ICCopen() < 0) {
	    (void)fprintf(stderr, "Can't open channel to server, %s\n",
		    strerror(errno));
	    CleanupAndExit(FALSE, FALSE, 1);
	}
	if (ICCreserve(EXPreason) != 0) {
	    (void)fprintf(stderr, "Can't reserve server\n");
	    CleanupAndExit(FALSE, FALSE, 1);
	}
    }

    /* Make the history filenames. */
    HistoryDB = COPY(HistoryText);
    (void)sprintf(buff, "%s.dir", HistoryDB);
    Historydir = COPY(buff);
#ifdef	DO_TAGGED_HASH
    (void)sprintf(buff, "%s.pag", HistoryDB);
    Historypag = COPY(buff);
#else
    (void)sprintf(buff, "%s.index", HistoryDB);
    Historyindex = COPY(buff);
    (void)sprintf(buff, "%s.hash", HistoryDB);
    Historyhash = COPY(buff);
#endif
    if (HistoryPath)
	(void)sprintf(buff, "%s/%s.n", HistoryPath, History);
    else
	(void)sprintf(buff, "%s.n", HistoryText);
    NHistory = COPY(buff);
    (void)sprintf(buff, "%s.dir", NHistory);
    NHistorydir = COPY(buff);
#ifdef	DO_TAGGED_HASH
    (void)sprintf(buff, "%s.pag", NHistory);
    NHistorypag = COPY(buff);
#else
    (void)sprintf(buff, "%s.index", NHistory);
    NHistoryindex = COPY(buff);
    (void)sprintf(buff, "%s.hash", NHistory);
    NHistoryhash = COPY(buff);
#endif

    if (!Writing)
	out = NULL;
    else {
	/* Open new history files, relative to news lib. */
	if (chdir(EXPhistdir) < 0) {
	    (void)fprintf(stderr, CANTCD, EXPhistdir, strerror(errno));
	    CleanupAndExit(Server, FALSE, 1);
	}
	out = EXPfopen(TRUE, NHistory, "w", TRUE, Server, FALSE);
	(void)fclose(EXPfopen(TRUE, NHistorydir, "w", TRUE, Server, FALSE));
#ifdef	DO_TAGGED_HASH
	(void)fclose(EXPfopen(TRUE, NHistorypag, "w", TRUE, Server, FALSE));
#else
	(void)fclose(EXPfopen(TRUE, NHistoryindex, "w", TRUE, Server, FALSE));
	(void)fclose(EXPfopen(TRUE, NHistoryhash, "w", TRUE, Server, FALSE));
#endif
	if (EXPverbose > 3)
#ifdef	DO_TAGGED_HASH
	    (void)printf("created: %s %s %s\n",
		    NHistory, NHistorydir, NHistorypag);
#else
	    (void)printf("created: %s %s %s %s\n",
		    NHistory, NHistorydir, NHistoryindex, NHistoryhash);
#endif
	dbzgetoptions(&opt);
#ifdef	DO_TAGGED_HASH
	opt.pag_incore = INCORE_MEM;
#else
	opt.idx_incore = INCORE_MEM;
	opt.exists_incore = INCORE_MEM;
#endif
	dbzsetoptions(opt);
	if (IgnoreOld) {
	    if (!dbzfresh(NHistory, dbzsize(0L), 0)) {
		(void)fprintf(stderr, "Can't create database, %s\n",
			strerror(errno));
		CleanupAndExit(Server, FALSE, 1);
	    }
	}
	else if (!dbzagain(NHistory, HistoryDB)) {
	    (void)fprintf(stderr, "Can't dbzagain, %s\n", strerror(errno));
	    CleanupAndExit(Server, FALSE, 1);
	}
    }

    val = TRUE;
    if (!SMsetup(SM_RDWR, (void *)&val) || !SMsetup(SM_PREOPEN, (void *)&val)) {
	fprintf(stderr, "Can't setup storage manager\n");
	CleanupAndExit(Server, FALSE, 1);
    }
    if (!SMinit()) {
	fprintf(stderr, "Can't initialize storage manager: %s\n", SMerrorstr);
	CleanupAndExit(Server, FALSE, 1);
    }

    /* Main processing loop. */
    if ((qp = QIOopen(HistoryText)) == NULL) {
	(void)fprintf(stderr, "Can't open history file, %s\n",
		strerror(errno));
	CleanupAndExit(Server, FALSE, 1);
    }
    for (Bad = FALSE, line = 1, Paused = FALSE; ; line++) {
	if ((p = QIOread(qp)) != NULL) {
	    if (!EXPdoline(out, p, QIOlength(qp))) {
		Bad = TRUE;
		if (errno == ENOSPC) {
		    (void)unlink(NHistory);
		    (void)unlink(NHistorydir);
#ifdef	DO_TAGGED_HASH
		    (void)unlink(NHistorypag);
#else
		    (void)unlink(NHistoryindex);
		    (void)unlink(NHistoryhash);
#endif
		}
		break;
	    }
	    continue;
	}

	/* Read or line-format error? */
	if (QIOerror(qp)) {
	    (void)fprintf(stderr, "Can't read line %d, %s\n",
		    line, strerror(errno));
	    QIOclose(qp);
	    CleanupAndExit(Server, Paused, 1);
	}
	if (QIOtoolong(qp)) {
	    (void)fprintf(stderr, "Line %d too long\n", line);
	    QIOclose(qp);
	    CleanupAndExit(Server, Paused, 1);
	}

	/* We hit EOF. */
	if (Paused || !Server)
	    /* Already paused or we don't want to pause -- we're done. */
	    break;
	if (ICCpause(EXPreason) != 0) {
	    (void)fprintf(stderr, "Can't pause server, %s\n",
		    strerror(errno));
	    QIOclose(qp);
	    CleanupAndExit(Server, Paused, 1);
	}
	Paused = TRUE;
    }
    QIOclose(qp);

    if (Writing) {
	/* Close the output files. */
	if (ferror(out) || fflush(out) == EOF || fclose(out) == EOF) {
	    (void)fprintf(stderr, "Can't close %s, %s\n",
		NHistory, strerror(errno));
	    Bad = TRUE;
	}
	if (!dbzclose()) {
	    (void)fprintf(stderr, "Can't close history, %s\n",
		    strerror(errno));
	    Bad = TRUE;
	}

	if (UnlinkFile && EXPunlinkfile == NULL)
	    /* Got -z but file was closed; oops. */
	    Bad = TRUE;

	/* If we're done okay, and we're not tracing, slip in the new files. */
	if (EXPverbose) {
	    if (Bad)
		(void)printf("Expire errors: history files not updated.\n");
	    if (EXPtracing)
		(void)printf("Expire tracing: history files not updated.\n");
	}
	if (!Bad && !EXPtracing) {
	    if (chdir(EXPhistdir) < 0) {
		(void)fprintf(stderr, CANTCD, EXPhistdir, strerror(errno));
		CleanupAndExit(Server, Paused, 1);
	    }
	    /* If user used the -d flag, mark we're done and exit. */
	    if (HistoryPath != NULL) {
		(void)sprintf(buff, "%s.done", NHistory);
		(void)fclose(EXPfopen(FALSE, buff, "w", TRUE, Server, FALSE));
		CleanupAndExit(Server, FALSE, 0);
	    }

	    if (rename(NHistory, HistoryText) < 0
	     || rename(NHistorydir, Historydir) < 0
#ifdef	DO_TAGGED_HASH
	     || rename(NHistorypag, Historypag) < 0) {
#else
	     || rename(NHistoryindex, Historyindex) < 0
	     || rename(NHistoryhash, Historyhash) < 0) {
#endif
		(void)fprintf(stderr, "Can't replace history files, %s\n",
			strerror(errno));
		/* Yes -- leave the server paused. */
		CleanupAndExit(Server, FALSE, 1);
	    }
	}
    }

    CleanupAndExit(Server, Paused, Bad ? 1 : 0);
    /* NOTREACHED */
    abort();
}

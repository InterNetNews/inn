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
    BOOL                Poison;
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

#define MAXOVERLINE	4096
/*
**  Expire-specific stuff.
*/
#define MAGIC_TIME	49710.

typedef struct _BADGROUP {
    struct _BADGROUP	*Next;
    char		*Name;
    BOOL		HasDirectory;
} BADGROUP;

STATIC BADGROUP		*EXPbadgroups;
STATIC BOOL		EXPlinks;
STATIC BOOL		EXPquiet;
STATIC BOOL		EXPsizing;
STATIC BOOL		EXPtracing;
STATIC BOOL		EXPusepost;
STATIC BOOL		EXPkeep;
STATIC BOOL		EXPearliest;
STATIC BOOL		ClassicExpire = FALSE;
STATIC BOOL		Ignoreselfexpire = FALSE;
STATIC BOOL		StorageAPI;
STATIC char		*ACTIVE;
STATIC char		*SPOOL = NULL;
STATIC int		nGroups;
STATIC FILE		*EXPunlinkfile;
STATIC FILE		*EXPunlinkindex;
STATIC FILE		*EXPlowmarkfile;
STATIC long		EXPsaved;
STATIC NEWSGROUP	*Groups;
STATIC NEWSGROUP	EXPdefault;
STATIC EXPIRECLASS      EXPclasses[NUM_STORAGE_CLASSES];
STATIC NGHASH		NGHtable[NGH_SIZE];
STATIC STRING		EXPreason;
STATIC time_t		EXPremember;
STATIC time_t		Now;
STATIC time_t		RealNow;

/* Statistics; for -v flag. */
STATIC char		*EXPgraph;
STATIC int		EXPverbose;
STATIC long		EXPprocessed;
STATIC long		EXPunlinked;
STATIC long		EXPoverindexdrop;
STATIC long		EXPhistdrop;
STATIC long		EXPhistremember;
STATIC long		EXPallgone;
STATIC long		EXPstillhere;

STATIC BOOL		OVERmmap;

#if ! defined (atof)            /* NEXT defines aotf as a macro */
extern double		atof();
#endif

STATIC int EXPsplit(char *p, char sep, char **argv, int count);

enum KRP {Keep, Remove, Poison};



/*
**  Hash a newsgroup and see if we get it.
*/
STATIC NEWSGROUP *NGfind(char *Name)
{
    char	        *p;
    int	                i;
    unsigned int	j;
    NEWSGROUP	        **ngp;
    char		c;
    NGHASH		*htp;

    /* SUPPRESS 6 *//* Over/underflow from plus expression */
    NGH_HASH(Name, p, j);
    htp = NGH_BUCKET(j);
    for (c = *Name, ngp = htp->Groups, i = htp->Used; --i >= 0; ngp++)
	if (c == ngp[0]->Name[0] && EQ(Name, ngp[0]->Name))
	    return ngp[0];
    return NULL;
}


/*
**  Sorting predicate to put newsgroups in rough order of their activity.
*/
STATIC int NGcompare(CPOINTER p1, CPOINTER p2)
{
    NEWSGROUP	**ng1;
    NEWSGROUP	**ng2;

    ng1 = CAST(NEWSGROUP**, p1);
    ng2 = CAST(NEWSGROUP**, p2);
    return ng1[0]->Last - ng2[0]->Last;
}


/*
**  Build the newsgroup structures from the active file.
*/
STATIC void BuildGroups(char *active)
{
    NGHASH	        *htp;
    NEWSGROUP	        *ngp;
    char	        *p;
    char	        *q;
    int	                i;
    unsigned	        j;
    int	                lines;
    int			NGHbuckets;
    char		*fields[5];

    /* Count the number of groups. */
    for (p = active, i = 0; (p = strchr(p, '\n')) != NULL; p++, i++)
	continue;
    nGroups = i;
    Groups = NEW(NEWSGROUP, i);

    /* Set up the default hash buckets. */
    NGHbuckets = i / NGH_SIZE;
    if (NGHbuckets == 0)
	NGHbuckets = 1;
    for (i = NGH_SIZE, htp = NGHtable; --i >= 0; htp++) {
	htp->Size = NGHbuckets;
	htp->Groups = NEW(NEWSGROUP*, htp->Size);
	htp->Used = 0;
    }

    /* Fill in the array. */
    lines = 0;
    for (p = active, ngp = Groups, i = nGroups; --i >= 0; ngp++, p = q + 1) {
	lines++;
	if ((q = strchr(p, '\n')) == NULL) {
	    (void)fprintf(stderr, "%s: line %d missing newline\n",
		    ACTIVE, lines);
	    exit(1);
	}
	*q = '\0';
	if (EXPsplit(p, ' ', fields, SIZEOF(fields)) != 4) {
	    (void)fprintf(stderr, "%s: line %d wrong number of fields\n",
		    ACTIVE, lines);
	    exit(1);
	}
	ngp->Name = fields[0];
	ngp->Last = atol(fields[1]);
	ngp->Rest = fields[3];

	/* Find the right bucket for the group, make sure there is room. */
	/* SUPPRESS 6 *//* Over/underflow from plus expression */
	NGH_HASH(ngp->Name, p, j);
	htp = NGH_BUCKET(j);
	if (htp->Used >= htp->Size) {
	    htp->Size += NGHbuckets;
	    RENEW(htp->Groups, NEWSGROUP*, htp->Size);
	}
	htp->Groups[htp->Used++] = ngp;
    }

    /* Sort each hash bucket. */
    for (i = NGH_SIZE, htp = NGHtable; --i >= 0; htp++)
    if (htp->Used > 1)
	qsort((POINTER)htp->Groups, (SIZE_T)htp->Used, sizeof htp->Groups[0],
		NGcompare);

    if (EXPverbose > 3)
	printf("Setting lowmark Last field to maxint.\n");
    /* Ok, now change our use of the Last field.  Set them all to maxint. */
    for (i = NGH_SIZE, htp = NGHtable; --i >= 0; htp++) {
	NEWSGROUP	**ngpa;
	int		k;

	for (ngpa = htp->Groups, k = htp->Used; --k >= 0; ngpa++) {
	    ngpa[0]->Last = ~(unsigned long) 0;
	    ngpa[0]->Lastpurged = 0;
	}
    }
    if (EXPverbose > 3)
	printf("Done setting lowmark Last field to maxint.\n");
}



/*
**  Open a file or give up.
*/
STATIC FILE *EXPfopen(BOOL Remove, STRING Name, char *Mode)
{
    FILE	*F;

    if (Remove && unlink(Name) < 0 && errno != ENOENT)
	(void)fprintf(stderr, "Warning, can't remove %s, %s\n",
		Name, strerror(errno));
    if ((F = fopen(Name, Mode)) == NULL) {
	(void)fprintf(stderr, "Can't open %s in %s mode, %s\n",
		Name, Mode, strerror(errno));
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
	    if (++i == count)
		/* Overflow. */
		return -1;
	    p[-1] = '\0';
	    for (*argv++ = p; *p == sep; p++)
		continue;
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
	else if (!CTYPE(isdigit, *p))
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
		ngp->Poison    = v->Poison;
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
    patterns = NEW(char*, nGroups);
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
	    if (isspace(*p))
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
	if (j != 5) {
	    (void)fprintf(stderr, "Line %d bad format\n", i);
	    return FALSE;
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
	v.Poison = (strchr(fields[1], 'X') != NULL);
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
	    EXPdefault.Poison  = v.Poison;
	    SawDefault = TRUE;
	}

	/* Assign to all groups that match the pattern and flags. */
	if ((j = EXPsplit(fields[0], ',', patterns, nGroups)) == -1) {
	    (void)fprintf(stderr, "Line %d too many patterns\n", i);
	    return FALSE;
	}
	for (k = 0; k < j; k++)
	    EXPmatch(patterns[k], &v, mod);
    }
    DISPOSE(patterns);

    return TRUE;
}


/*
**  Handle a newsgroup that isn't in the active file.
*/
STATIC NEWSGROUP *EXPnotfound(char *Entry)
{
    static NEWSGROUP	Removeit;
    BADGROUP	        *bg;
    char	        *p;
    struct stat		Sb;
    char		buff[SPOOLNAMEBUFF];

    /* See if we already know about this group. */
    for (bg = EXPbadgroups; bg; bg = bg->Next)
	if (EQ(Entry, bg->Name))
	    break;
    if (bg == NULL) {
	bg = NEW(BADGROUP, 1);
	bg->Name = COPY(Entry);
	(void)strcpy(buff, bg->Name);
	for (p = buff; *p; p++)
	    if (*p == '.')
		*p = '/';
	bg->HasDirectory = stat(buff, &Sb) >= 0 && S_ISDIR(Sb.st_mode);
	bg->Next = EXPbadgroups;
	EXPbadgroups = bg;
	if (!EXPquiet) {
	    (void)fflush(stdout);
	    (void)fprintf(stderr, "Group not matched (removed?) %s -- %s\n",
		Entry,
		bg->HasDirectory ? "Using default expiration"
				 : "Purging all articles");
	}
    }

    /* Directory still there; use default expiration. */
    if (bg->HasDirectory)
	return &EXPdefault;

    /* No directory -- remove it all now. */
    if (Removeit.Keep == 0) {
	Removeit.Keep = Now;
	Removeit.Default = Now;
	Removeit.Purge = Now;
    }
    return &Removeit;
}


/*
**  Should we keep the specified article?
*/
STATIC enum KRP EXPkeepit(char *Entry, time_t when, time_t Expires)
{
    char	        *p;
    char	        save;
    NEWSGROUP	        *ngp;
    EXPIRECLASS         class;
    TOKEN               token;
    unsigned long	ArtNum;
    enum KRP		retval = Remove;

    if (IsToken(Entry)) {
	token = TextToToken(Entry);
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
		printf("%s age = %0.2f\n", Entry, (Now - when) / 86400.);
	    if (Expires == 0) {
		if (when <= class.Default)
		    (void)printf("%s too old (no exp)\n", Entry);
	    } else {
		if (when <= class.Purge)
		    (void)printf("%s later than purge\n", Entry);
		if (when >= class.Keep)
		    (void)printf("%s earlier than min\n", Entry);
		if (Now >= Expires)
		    (void)printf("%s later than header\n", Entry);
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

    if ((p = strpbrk(Entry, "/:")) == NULL) {
	(void)fflush(stdout);
	(void)fprintf(stderr, "Bad entry, \"%s\"\n", Entry);
	return Remove;
    }
    save = *p;
    *p = '\0';
    if ((ngp = NGfind(Entry)) == NULL)
	ngp = EXPnotfound(Entry);
    *p = save;

    /* Bad posting date? */
    if (when > RealNow + 86400) {
	/* Yes -- force the article to go right now. */
	when = Expires ? ngp->Purge : ngp->Default;
    }

    if (EXPverbose > 2) {
	if (EXPverbose > 3)
	    (void)printf("%s age = %0.2f\n", Entry, (Now - when) / 86400.);
	if (Expires == 0) {
	    if (when <= ngp->Default)
		(void)printf("%s too old (no exp)\n", Entry);
	}
	else {
	    if (when <= ngp->Purge)
		(void)printf("%s later than purge\n", Entry);
	    if (when >= ngp->Keep)
		(void)printf("%s earlier than min\n", Entry);
	    if (Now >= Expires)
		(void)printf("%s later than header\n", Entry);
	}

    }

    /* If no expiration, make sure it wasn't posted before the default. */
    if (Expires == 0) {
	if (when >= ngp->Default)
	    retval = Keep;

    /* Make sure it's not posted before the purge cut-off and
     * that it's not due to expire. */
    } else {
        if (when >= ngp->Purge && (Expires >= Now || when >= ngp->Keep))
	    retval = Keep;
    }
    ArtNum = atol(p + 1);
    if (retval == Keep) {
	if (EXPverbose > 3)
	    printf("Lowmark checking for article number %u (Last = %u)\n",
		ArtNum, ngp->Last);
	if (ngp->Last > ArtNum) {
	    ngp->Last = ArtNum;
	    if (EXPverbose > 3)
		printf("New lowmark %u\n", ArtNum);
	}
	return Keep;
    } else {
	if (EXPverbose > 3)
	    printf("Lowmark checking for article number %u (Last = %u)\n",
		ArtNum, ngp->Lastpurged);
	if (ngp->Lastpurged < ArtNum) {
	    ngp->Lastpurged = ArtNum;
	    if (EXPverbose > 3)
		printf("New lowmark %u\n", ArtNum);
	}
	return ngp->Poison ? Poison : Remove;
    }
}


/*
**  An article can be removed.  Either print a note, or actually remove it.
**  Also fill in the article size.
*/
STATIC void EXPremove(char *p, long *size, BOOL index)
{
    char	        *q;
    struct stat		Sb;

    /* Turn into a filename and get the size if we need it. */
    if (!IsToken(p)) {
	for (q = p; *q; q++) {
	    if (*q == '.')
		*q = '/';
	    if (*q == '\t' || *q == ' ') {
		*q = '\0';
		break;
	    }
	}
	if (EXPsizing && *size < 0 && stat(p, &Sb) >= 0)
	    *size = (int)(((long)Sb.st_size >> 10) + (((long)Sb.st_size >> 9) & 1));
    }
    if (EXPverbose > 1)
	(void)printf("\tunlink %s\n", p);

    if (EXPtracing) {
	EXPunlinked++;
	(void)printf("%s\n", p);
	return;
    }
    if (index) {
	if (EXPunlinkindex) {
	    (void)fprintf(EXPunlinkindex, "%s\n", p);
	}
	EXPoverindexdrop++;
	return;
    }
    EXPunlinked++;
    if (EXPunlinkfile) {
	(void)fprintf(EXPunlinkfile, "%s\n", p);
	if (!ferror(EXPunlinkfile))
	    return;
	(void)fprintf(stderr, "Can't write to -z file, %s\n",
	    strerror(errno));
	(void)fprintf(stderr, "(Will ignore it for rest of run.)\n");
	(void)fclose(EXPunlinkfile);
	EXPunlinkfile = NULL;
    }
    if (IsToken(p)) {
	if (!SMcancel(TextToToken(p)) && SMerrno != SMERR_NOENT && SMerrno != SMERR_UNINIT)
	    fprintf(stderr, "Can't unlink %s\n", p);
    } else {
	if (unlink(p) < 0 && errno != ENOENT)
	    (void)fprintf(stderr, "Can't unlink %s, %s\n", p, strerror(errno));
    }
}


/*
**  Do the work of expiring one line.
*/
STATIC BOOL EXPdoline(FILE *out, char *line, int length, char **arts, enum KRP *krps)
{
    static char		IGNORING[] = "Ignoring bad line, \"%.20s...\"\n";
    static long		Offset;
    static BUFFER	New;
    char	        *p;
    char	        *q;
    char	        *first;
    int	                i;
    int			count;
    char		*fields[4];
    time_t		Arrived;
    time_t		Expires;
    time_t		Posted;
    time_t		when;
    long		where;
    long		size;
    BOOL                poisoned;
    BOOL		keeper;
    BOOL		remove;
    HASH		key;
    char		date[20];
    BOOL		Hastoken;
    BOOL		Hasover;
    BOOL		HasSelfexpire = FALSE;
    BOOL		Selfexpired = FALSE;
    ARTHANDLE		*article;
    TOKEN		token;
    int			linelen;
    static char		*OVERline = NULL;
    static char		*Xrefbuf = NULL;
    char		*Xref;
    char		*tokentext;

    /* Split up the major fields. */
    i = EXPsplit(line, HIS_FIELDSEP, fields, SIZEOF(fields));
    if (i != 2 && i != 3) {
	(void)fprintf(stderr, IGNORING, line);
	return TRUE;
    }

    /* Check to see if this messageid has already been written to the
       text file.  Unfortunately, this is the only clean way to do this */
    switch (fields[0][0]) {
    case '[':
	if (strlen(fields[0]) != ((sizeof(HASH) * 2) + 2)) {
	    fprintf(stderr, "Invalid length for hash %s, skipping\n", fields[0]);
	    return TRUE;
	}
	key = TextToHash(&fields[0][1]);
	Hastoken = TRUE;
	break;
    case '<':
	key = HashMessageID(fields[0]);
	Hastoken = FALSE;
	break;
    default:
	fprintf(stderr, "Invalid message-id \"%s\" in history text\n", fields[0]);
	Hastoken = FALSE;
	return TRUE;
    }
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
    }
    else {
	/* Active article -- split up the file entries. */
	if (Hastoken) {
	    if (!IsToken(fields[2])) {
		fprintf(stderr, "Invalid token %s, skipping\n", fields[2]);
		return TRUE;
	    }
	    token = TextToToken(fields[2]);
	    if (!Ignoreselfexpire && SMprobe(SELFEXPIRE, &token)) {
		HasSelfexpire = TRUE;
		if ((article = SMretrieve(token, RETR_STAT)) == (ARTHANDLE *)NULL)
		    /* the article is cancelled or has been expired by the
		       method's functionality */
		    Selfexpired = TRUE;
		else
		    /* the article is still alive */
		    SMfreearticle(article);
	    }
	    if ((!HasSelfexpire || (HasSelfexpire && !Selfexpired)) && token.index < OVER_NONE) {
		Hasover = TRUE;
		if ((p = OVERretrieve(&token, &linelen)) == (char *)NULL)
		    return TRUE;
		if (OVERmmap) {
		    if (!OVERline)
			OVERline = NEW(char, MAXOVERLINE);
		    if (linelen > MAXOVERLINE - 1)
			linelen = MAXOVERLINE - 1;
		    memcpy(OVERline, p, linelen);
		    OVERline[linelen] = '\0';
		} else {
		    OVERline = p;
		}
		if ((Xref = strstr(OVERline, "\tXref:")) == NULL) {
		    return TRUE;
		}
		if ((Xref = strchr(Xref, ' ')) == NULL)
		    return TRUE;
		for (Xref++; *Xref == ' '; Xref++);
		if ((Xref = strchr(Xref, ' ')) == NULL)
		    return TRUE;
		for (Xref++; *Xref == ' '; Xref++);
		if (!Xrefbuf)
		    Xrefbuf = NEW(char, MAXOVERLINE);
		memcpy(Xrefbuf, Xref, linelen - (OVERline - Xref));
		Xrefbuf[linelen - (OVERline - Xref)] = '\0';
		count = EXPsplit(Xrefbuf, ' ', arts, nGroups);
	    } else {
		Hasover = FALSE;
	    }
	} else {
	    Hasover = FALSE;
	}
	when = EXPusepost ? Posted : Arrived;
	if (!HasSelfexpire) {
	    if (!ClassicExpire || !Hastoken || !Hasover) {
		count = EXPsplit(fields[2], ' ', arts, nGroups);
	    }
	    if (count == -1) {
		(void)fprintf(stderr, IGNORING, line);
		return TRUE;
	    }
	    /* First check all postings */
	    poisoned = FALSE;
	    keeper = FALSE;
	    remove = FALSE;
	    for (i = 0; i < count; ++i) {
	      if ((krps[i] = EXPkeepit(arts[i], when, Expires)) == Poison)
		  poisoned = TRUE;
	      if (EXPkeep && (krps[i] == Keep))
		  keeper = TRUE;
	      if ((krps[i] == Remove))
		  remove = TRUE;
	    }
	}
	EXPprocessed++;

	if (HasSelfexpire) {
	    if (Selfexpired || token.type == TOKEN_EMPTY || token.cancelled) {
		if (Hasover) {
		    for (i = 0; i < count; i++) {
			p = arts[i];
			if (*p == '\0')
			    /* Shouldn't happen. */
			    continue;
			EXPremove(p, &size, TRUE);
		    }
		}
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
		if (Hasover && !OVERstore(&token, OVERline, linelen))
		    return TRUE;
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
	} else if (ClassicExpire && Hastoken && Hasover) {
	    if (EXPearliest) {
		if (remove || poisoned ||
		    token.type == TOKEN_EMPTY || token.cancelled) {
		    for (i = 0; i < count; i++) {
			p = arts[i];
			if (*p == '\0')
			    /* Shouldn't happen. */
			    continue;
			EXPremove(p, &size, TRUE);
		    }
		    EXPremove(fields[2], &size, FALSE);
		    if (EXPsizing && size > 0)
			EXPsaved += size;
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
		    if (!OVERstore(&token, OVERline, linelen))
			return TRUE;
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
	    } else { /* not earliest mode */
		if (!keeper || token.type == TOKEN_EMPTY || token.cancelled) {
		    for (i = 0; i < count; i++) {
			p = arts[i];
			if (*p == '\0')
			    /* Shouldn't happen. */
			    continue;
			EXPremove(p, &size, TRUE);
		    }
		    EXPremove(fields[2], &size, FALSE);
		    if (EXPsizing && size > 0)
			EXPsaved += size;
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
		    if (!OVERstore(&token, OVERline, linelen))
			return TRUE;
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
	    }
	} else {
	    /* Get space to hold the remaining file name entries. */
	    if (New.Data == NULL) {
		New.Size = length;
		New.Data = NEW(char, New.Size);
	    }
	    else if (New.Size < length) {
		New.Size = length;
		RENEW(New.Data, char, New.Size);
	    }

	    /* The "first" variable tells us if we haven't saved the first
	     * article yet.  This only matters if we're doing link-saving. */
	    first = EXPlinks && count > 1 ? arts[0] : (char *)NULL;

	    if (EXPearliest) {
		for (size = -1, q = New.Data, i = 0; i < count; i++) {
		    p = arts[i];
		    if (*p == '\0')
			/* Shouldn't happen. */
			continue;
		    if (krps[i] == Keep) {
			if (EXPverbose > 1)
			    (void)printf("keep %s\n", p);
			if (q > New.Data)
			    *q++ = ' ';
			q += strlen(strcpy(q, p));
			continue;
		    }
		    break; /* expired, stop looking */
		}

		if(i < count) { /* clobber them all */
		    q = New.Data; /* no files for history line */
		    for (i = 0; i < count; i++) {
			p = arts[i];
			if (*p == '\0')
			    /* Shouldn't happen. */
			    continue;
			EXPremove(p, &size, FALSE);
		    }
		}        
	    } else { /* not earliest mode */
		/* Loop over all file entries, see if we should keep each one. */
		for (size = -1, q = New.Data, i = 0; i < count; i++) {
		    p = arts[i];
		    if (*p == '\0')
			/* Shouldn't happen. */
			continue;
		    if (!poisoned && ((krps[i] == Keep) || keeper)) {
			if (EXPverbose > 1)
			    (void)printf("keep %s\n", p);
			if (first != NULL) {
			    /* Keeping one and haven't kept the first; so save it. */
			    if (i > 0)
				q += strlen(strcpy(q, first));
			    first = NULL;
			}
			if (q > New.Data)
			    *q++ = ' ';
			q += strlen(strcpy(q, p));
			continue;
		    }

		    /* Don't delete the file if preserving symbolic links to it. */
		    if (EXPlinks && i == 0 && count > 1)
			continue;
		    EXPremove(arts[i], &size, FALSE);
		}

		/* If saving links and didn't have to save the leader, delete it. */
		if (EXPlinks && first != NULL)
		    EXPremove(first, &size, FALSE);
	    } /* not earliest mode */

	    if (q == New.Data) {
		if (EXPsizing && size > 0)
		    EXPsaved += size;
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
	    }
	    else if (out) {
		if (Hastoken && Hasover) {
		    if (token.type == TOKEN_EMPTY || token.cancelled) {
			count = EXPsplit(Xrefbuf, ' ', arts, nGroups);
			for (i = 0; i < count; i++) {
			    p = arts[i];
			    if (*p == '\0')
				/* Shouldn't happen. */
				continue;
			    EXPremove(p, &size, TRUE);
			}
			EXPremove(fields[2], &size, FALSE);
			if (EXPsizing && size > 0)
			    EXPsaved += size;
			where = Offset;
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
		    } else {
			if (!OVERstore(&token, OVERline, linelen))
			    return TRUE;
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
		    }
		} else {
		    where = Offset;
		    (void)fprintf(out, "%s%c%s%c%s\n",
			    fields[0], HIS_FIELDSEP, fields[1], HIS_FIELDSEP,
			    New.Data);
		    Offset += strlen(fields[0]) + 1 + strlen(fields[1]) + 1
			    + strlen(New.Data) + 1;
		    if (EXPverbose > 3)
			(void)printf("remember article: %s%c%s%c%s\n",
				fields[0], HIS_FIELDSEP, fields[1], HIS_FIELDSEP,
				New.Data);
		}
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
    if (dbzstore(key, where) == DBZSTORE_ERROR) {
	fprintf(stderr, "Can't store key, \"%s\"\n", strerror(errno));
	return FALSE;
    }
    return TRUE;
}



/*
**  Clean up link with the server and exit.
*/
STATIC NORETURN CleanupAndExit(BOOL Server,BOOL Paused, int x)
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
    if (EXPunlinkindex && fclose(EXPunlinkindex) == EOF) {
	(void)fprintf(stderr, "Can't close -u file, %s\n", strerror(errno));
	x = 1;
    }
    if (EXPunlinkfile && fclose(EXPunlinkfile) == EOF) {
	(void)fprintf(stderr, "Can't close -z file, %s\n", strerror(errno));
	x = 1;
    }

    if (EXPlowmarkfile) {
	NGHASH		*htp;
	NEWSGROUP	**ngpa;
	int		i;
	int		k;

	/* Dump this list of lowmarks. */
	for (i = NGH_SIZE, htp = NGHtable; --i >= 0; htp++) {
	    for (ngpa = htp->Groups, k = htp->Used; --k >= 0; ngpa++) {
		if (ngpa[0]->Last < ~(unsigned long) 0) {
		    (void)fprintf(EXPlowmarkfile, "%s %u\n",
			ngpa[0]->Name, ngpa[0]->Last);
		} else if (ngpa[0]->Lastpurged > 0) {
		    (void)fprintf(EXPlowmarkfile, "%s %u\n",
			ngpa[0]->Name, ngpa[0]->Lastpurged + 1);
		}
	    }
	}

	if (fclose(EXPlowmarkfile) == EOF) {
	   (void)fprintf(stderr, "Can't close -Z file, %s\n", strerror(errno));
	   x = 1;
	}
    }

    /* Report stats. */
    if (EXPsizing)
	(void)printf("%s approximately %ldk\n",
	    EXPtracing ? "Would remove" : "Removed", EXPsaved);
    if (EXPverbose) {
	(void)printf("Article lines processed %8ld\n", EXPprocessed);
	(void)printf("Articles retained       %8ld\n", EXPstillhere);
	(void)printf("Entries expired         %8ld\n", EXPallgone);
	if (StorageAPI) {
	    (void)printf("Articles dropped        %8ld\n", EXPunlinked);
	    (void)printf("Overview index dropped  %8ld\n", EXPoverindexdrop);
	} else {
	    (void)printf("Files unlinked          %8ld\n", EXPunlinked);
	}
	(void)printf("Old entries dropped     %8ld\n", EXPhistdrop);
	(void)printf("Old entries retained    %8ld\n", EXPhistremember);
    }

    /* Append statistics to a summary file */
    if (EXPgraph) {
	F = EXPfopen(FALSE, EXPgraph, "a");
	if (StorageAPI) {
	    (void)fprintf(F, "%ld %ld %ld %ld %ld %ld %ld %ld\n",
		(long)Now, EXPprocessed, EXPstillhere, EXPallgone,
		EXPunlinked, EXPoverindexdrop, EXPhistdrop, EXPhistremember);
	} else {
	    (void)fprintf(F, "%ld %ld %ld %ld %ld %ld %ld\n",
		(long)Now, EXPprocessed, EXPstillhere, EXPallgone,
		EXPunlinked, EXPhistdrop, EXPhistremember);
	}
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
    char		**arts;
    enum KRP            *krps;
    STRING		History;
    STRING		HistoryText;
    STRING		HistoryPath;
    STRING		HistoryDB;
    STRING		OverPath;
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

    /* Set defaults. */
    Server = TRUE;
    IgnoreOld = FALSE;
    History = "history";
    HistoryPath = NULL;
    OverPath = NULL;
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
    while ((i = getopt(ac, av, "cf:h:D:d:eg:iklNnpqr:stu:v:w:xz:Z:")) != EOF)
	switch (i) {
	default:
	    Usage();
	    /* NOTREACHED */
	case 'c':
	    ClassicExpire = TRUE;
	    break;
	case 'D':
	    OverPath = optarg;
	    break;
	case 'd':
	    HistoryPath = optarg;
	    break;
        case 'e':
	    EXPearliest = TRUE;
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
	case 'k':
	    EXPkeep = TRUE;
	    break;
	case 'l':
	    EXPlinks = TRUE;
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
	case 's':
	    EXPsizing = TRUE;
	    break;
	case 't':
	    EXPtracing = TRUE;
	    break;
	case 'u':
	    EXPunlinkindex = EXPfopen(TRUE, optarg, "a");
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
	    EXPunlinkfile = EXPfopen(TRUE, optarg, "a");
	    UnlinkFile = TRUE;
	    break;
	case 'Z':
	    if (EXPverbose > 3)
		printf("Opening lowmark file %s\n", optarg);
	    EXPlowmarkfile = EXPfopen(TRUE, optarg, "a");
	    LowmarkFile = TRUE;
	    break;
	}
    ac -= optind;
    av += optind;
    if (ac != 0 && ac != 1)
	Usage();

    /* Get active file, parse it. */
    if ((active = ReadInFile(ACTIVE, (struct stat *)NULL)) == NULL) {
	(void)fprintf(stderr, "Can't read %s, %s\n",
		ACTIVE, strerror(errno));
	exit(1);
    }
    BuildGroups(active);
    (void)time(&Now);
    RealNow = Now;
    Now += TimeWarp;

    /* Parse the control file. */
    if (av[0])
	F = EQ(av[0], "-") ? stdin : EXPfopen(FALSE, av[0], "r");
    else
	F = EXPfopen(FALSE, cpcatpath(innconf->pathetc, _PATH_EXPIRECTL), "r");
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
	    exit(1);
	}
	out = EXPfopen(TRUE, NHistory, "w");
	(void)fclose(EXPfopen(TRUE, NHistorydir, "w"));
#ifdef	DO_TAGGED_HASH
	(void)fclose(EXPfopen(TRUE, NHistorypag, "w"));
#else
	(void)fclose(EXPfopen(TRUE, NHistoryindex, "w"));
	(void)fclose(EXPfopen(TRUE, NHistoryhash, "w"));
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
		exit(1);
	    }
	}
	else if (!dbzagain(NHistory, HistoryDB)) {
	    (void)fprintf(stderr, "Can't dbzagain, %s\n", strerror(errno));
	    exit(1);
	}
    }

    if (chdir(SPOOL) < 0) {
	(void)fprintf(stderr, CANTCD, SPOOL, strerror(errno));
	exit(1);
    }

    if (!SMinit()) {
	fprintf(stderr, "Can't initialize storage manager: %s\n", SMerrorstr);
	exit(1);
    }

    /* Main processing loop. */
    StorageAPI = innconf->storageapi;
    OVERmmap = innconf->overviewmmap;
    if (OVERmmap)
	val = TRUE;
    else
	val = FALSE;
    if (!OVERsetup(OVER_MMAP, (void *)&val)) {
	fprintf(stderr, "Can't setup unified overview mmap: %s\n", strerror(errno));
	exit(1);
    }
    if (!OVERsetup(OVER_MODE, "r")) {
	fprintf(stderr, "Can't setup unified overview mode: %s\n", strerror(errno));
	exit(1);
    }
    if (OverPath)
	if (!OVERsetup(OVER_NEWDIR, (void *)OverPath)) {
	    fprintf(stderr, "Can't setup unified overview path: %s\n", strerror(errno));
	    exit(1);
	}
    if (StorageAPI) {
	if (!OVERinit()) {
	    fprintf(stderr, "Can't initialize unified overview: %s\n", strerror(errno));
	    exit(1);
	}
	if (Writing && !OVERnewinit()) {
	    fprintf(stderr, "Can't initialize new unified overview: %s\n", strerror(errno));
	    exit(1);
	}
    }
    arts = NEW(char*, nGroups);
    krps = NEW(enum KRP, nGroups);
    if ((qp = QIOopen(HistoryText)) == NULL) {
	(void)fprintf(stderr, "Can't open history file, %s\n",
		strerror(errno));
	CleanupAndExit(Server, FALSE, 1);
    }
    for (Bad = FALSE, line = 1, Paused = FALSE; ; line++) {
	if ((p = QIOread(qp)) != NULL) {
	    if (!EXPdoline(out, p, QIOlength(qp), arts, krps)) {
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
        if (StorageAPI && !OVERreinit())
	    CleanupAndExit(Server, Paused, 1);
    }
    QIOclose(qp);
    DISPOSE(krps);
    DISPOSE(arts);

    if (Writing) {
	/* Close the output files. */
	if (ferror(out) || fflush(out) == EOF || fclose(out) == EOF) {
	    (void)fprintf(stderr, "Can't close %s, %s\n",
		NHistory, strerror(errno));
	    Bad = TRUE;
	}
	if (!dbmclose()) {
	    (void)fprintf(stderr, "Can't close history, %s\n",
		    strerror(errno));
	    Bad = TRUE;
	}

	if (UnlinkFile && EXPunlinkfile == NULL)
	    /* Got -z but file was closed; oops. */
	    Bad = TRUE;

	if (LowmarkFile && EXPlowmarkfile == NULL)
            /* Got -Z but file was closed; oops. */
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
		(void)fclose(EXPfopen(FALSE, buff, "w"));
		if (StorageAPI && Writing) {
		    if (OverPath) {
			(void)sprintf(buff, "%s/overview.done", OverPath);
			(void)fclose(EXPfopen(FALSE, buff, "w"));
		    } else {
			if (!OVERreplace()) {
			    (void)fprintf(stderr, "Can't replace overview data, %s\n",
				strerror(errno));
			    CleanupAndExit(Server, FALSE, 1);
			}
		    }
		}
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
	    if (StorageAPI && Writing) {
		if (OverPath) {
		    (void)sprintf(buff, "%s/overview.done", OverPath);
		    (void)fclose(EXPfopen(FALSE, buff, "w"));
		} else {
		    if (!OVERreplace()) {
			(void)fprintf(stderr, "Can't replace overview data, %s\n",
			    strerror(errno));
			CleanupAndExit(Server, FALSE, 1);
		    }
		}
	    }
	}
    }

    CleanupAndExit(Server, Paused, Bad ? 1 : 0);
    /* NOTREACHED */
    abort();
}

#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <sys/mman.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include "dbz.h"
#include "configdata.h"
#include "macros.h"
#include "clibrary.h"
#include "libinn.h"
#include "paths.h"
#include "storage.h"
#include "qio.h"
#include "ov.h"
#include "ovmethods.h"
#include "ovinterface.h"

STATIC int	OVnumpatterns;
STATIC char	**OVpatterns;
time_t		OVrealnow;
BOOL		OVstatall;

STATIC BOOL	OVdelayrm;
STATIC BOOL	OVusepost;
STATIC BOOL	OVquiet;
STATIC BOOL	OVkeep;
STATIC BOOL	OVearliest;
STATIC BOOL	OVignoreselfexpire;
STATIC time_t	OVnow;
STATIC OV_METHOD	ov;
STATIC FILE		*EXPunlinkfile;
STATIC char		*ACTIVE;
STATIC ARTOVERFIELD	*ARTfields;
STATIC int		ARTfieldsize;
STATIC BOOL		ReadOverviewfmt = FALSE;
STATIC char		**arts;
STATIC enum KRP		*krps;

/* Statistics */
STATIC long		EXPprocessed;
STATIC long		EXPunlinked;
STATIC long		EXPoverindexdrop;

/*
**  Stuff that is used for group based expire
*/
#define NGH_HASH(Name, p, j)    \
	for (p = Name, j = 0; *p; ) j = (j << 5) + j + *p++
#define NGH_SIZE	2048
#define NGH_BUCKET(j)	&NGHtable[j & (NGH_SIZE - 1)]

#define OVFMT_UNINIT	-2
#define OVFMT_NODATE	-1
#define OVFMT_NOXREF	-1

STATIC int		Dateindex = OVFMT_UNINIT; 
STATIC int		Xrefindex = OVFMT_UNINIT;
STATIC int		Messageidindex = OVFMT_UNINIT;

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
} BADGROUP;

STATIC BADGROUP		*EXPbadgroups;
STATIC int		nGroups;
STATIC NEWSGROUP	*Groups;
STATIC NEWSGROUP	EXPdefault;
STATIC NGHASH		NGHtable[NGH_SIZE];

#if ! defined (atof)            /* NEXT defines aotf as a macro */
extern double		atof();
#endif

enum KRP {Keep, Remove, Poison};

BOOL OVopen(int mode) {
    int	i;
    BOOL val;
    char *p;

    if (ov.open)
	/* already opened */
	return TRUE;

    /* if innconf isn't already read in, do so. */
    if (innconf == NULL) {
	if (ReadInnConf() < 0) {
	    return FALSE;
	}
    }
    if (!innconf->enableoverview) {
	syslog(L_FATAL, "enableoverview is not true");
	(void)fprintf(stderr, "enableoverview is not true\n");
	return FALSE;
    }
    if (innconf->ovmethod == NULL) {
	syslog(L_FATAL, "ovmethod is not defined");
	(void)fprintf(stderr, "ovmethod is not defined\n");
	return FALSE;
    }
    for (i=0;i<NUM_OV_METHODS;i++) {
	if (!strcmp(innconf->ovmethod, ov_methods[i].name))
	    break;
    }
    if (i == NUM_OV_METHODS) {
	syslog(L_FATAL, "%s is not found for ovmethod", innconf->ovmethod);
	(void)fprintf(stderr, "%s is not found for ovmethod\n", innconf->ovmethod);
	return FALSE;
    }
    ov = ov_methods[i];
    val = (*ov.open)(mode);
    if (atexit(OVclose) < 0) {
	OVclose();
	return FALSE;
    }
    if (innconf->ovgrouppat != NULL) {
	for (i = 1, p = innconf->ovgrouppat; *p && (p = strchr(p+1, ',')); i++);
	OVnumpatterns = i;
	OVpatterns = NEW(char *, OVnumpatterns);
	for (i = 0, p = strtok(innconf->ovgrouppat, ","); p != NULL && i <= OVnumpatterns ; i++, p = strtok(NULL, ","))
	    OVpatterns[i] = COPY(p);
	if (i != OVnumpatterns) {
	    syslog(L_FATAL, "extra ',' in pattern");
	    (void)fprintf(stderr, "extra ',' in pattern");
	    return FALSE;
	}
    }
    return val;
}

BOOL OVgroupstats(char *group, int *lo, int *hi, int *count, int *flag) {
    if (!ov.open) {
	/* must be opened */
	syslog(L_ERROR, "ovopen must be called first");
	(void)fprintf(stderr, "ovopen must be called first");
	return FALSE;
    }
    return ((*ov.groupstats)(group, lo, hi, count, flag));
}

BOOL OVgroupadd(char *group, ARTNUM lo, ARTNUM hi, char *flag) {
    /* lomark should never be changed in each ovmethod if lo is 0 */
    if (!ov.open) {
	/* must be opened */
	syslog(L_ERROR, "ovopen must be called first");
	(void)fprintf(stderr, "ovopen must be called first");
	return FALSE;
    }
    return ((*ov.groupadd)(group, lo, hi, flag));
}

BOOL OVgroupdel(char *group) {
    if (!ov.open) {
	/* must be opened */
	syslog(L_ERROR, "ovopen must be called first");
	(void)fprintf(stderr, "ovopen must be called first");
	return FALSE;
    }
    return ((*ov.groupdel)(group));
}

BOOL OVadd(TOKEN token, char *data, int len, time_t arrived, time_t expires) {
    char		*next, *nextcheck;
    static char		*xrefdata, *patcheck, *overdata;
    char		*xrefstart, *xrefend;
    static int		xrefdatalen = 0, overdatalen = 0;
    BOOL		found = FALSE;
    int			xreflen;
    int			i;
    char		*group;
    ARTNUM		artnum;

    if (!ov.open) {
	/* must be opened */
	syslog(L_ERROR, "ovopen must be called first");
	(void)fprintf(stderr, "ovopen must be called first");
	return FALSE;
    }

    /*
     * find last Xref: in the overview line.  Note we need to find the *last*
     * Xref:, since there have been corrupted articles on Usenet with Xref:
     * fragments stuck in other header lines.  The last Xref: is guaranteed
     * to be from our server.
     */

    for (next = data; ((len - (next - data)) > 6 ) && ((next = memchr(next, 'X', len - (next - data))) != NULL); ) {
        if (memcmp(next, "Xref: ", 6) == 0) {
            found =  TRUE;
            xrefstart = next;
        }
        next++;
    }

    if (!found)
        return FALSE;

    next = xrefstart;
    for (i = 0; (i < 2) && (next < (data + len)); i++) {
        if ((next = memchr(next, ' ', len - (next - data))) == NULL)
            return FALSE;
        next++;
    }
    xreflen = len - (next - data);

    /*
     * If there are other fields beyond Xref in overview, then
     * we must find Xref's end, or data following is misinterpreted.
     */
    if (xrefend = memchr(next, '\t', xreflen))
	xreflen = xrefend - next;

    if (xrefdatalen == 0) {
        xrefdatalen = BIG_BUFFER;
        xrefdata = NEW(char, xrefdatalen);
        if (innconf->ovgrouppat != NULL)
            patcheck = NEW(char, xrefdatalen);
    }
    if (xreflen > xrefdatalen) {
        xrefdatalen = xreflen;
        RENEW(xrefdata, char, xrefdatalen + 1);
        if (innconf->ovgrouppat != NULL)
            RENEW(patcheck, char, xrefdatalen + 1);
    }
    if (overdatalen == 0) {
	overdatalen = BIG_BUFFER;
	overdata = NEW(char, overdatalen);
    }
    if (len + 16 > overdatalen) {
	overdatalen = len + 16;
	RENEW(overdata, char, overdatalen);
    }

    if (innconf->ovgrouppat != NULL) {
        memcpy(patcheck, next, xreflen);
        patcheck[xreflen] = '\0';
        for (group = patcheck; group && *group; group = memchr(nextcheck, ' ', xreflen - (nextcheck - patcheck))) {
            while (isspace((int)*group))
                group++;
            if ((nextcheck = memchr(group, ':', xreflen - (patcheck - group))) == NULL)
                return FALSE;
            *nextcheck++ = '\0';
            if (!OVgroupmatch(group)) {
                if (!SMprobe(SELFEXPIRE, &token, NULL) && innconf->groupbaseexpiry)
                    /* this article will never be expired, since it does not
                       have self expiry function in stored method and
                       groupbaseexpiry is true */
                    return FALSE;
                return TRUE;
            }
        }
    }
    memcpy(xrefdata, next, xreflen);
    xrefdata[xreflen] = '\0';
    for (group = xrefdata; group && *group; group = memchr(next, ' ', xreflen - (next - xrefdata))) {
        /* Parse the xref part into group name and article number */
        while (isspace((int)*group))
            group++;
        if ((next = memchr(group, ':', xreflen - (group - xrefdata))) == NULL)
            return FALSE;
        *next++ = '\0';
        artnum = atoi(next);
        if (artnum <= 0)
            continue;

        sprintf(overdata, "%d\t", artnum);
        i = strlen(overdata);
        memcpy(overdata + i, data, len);
        i += len;
        memcpy(overdata + i, "\r\n", 2);
        i += 2;

	if(! (*ov.add)(group, artnum, token, overdata, i, arrived, expires))
	    return FALSE;
    }

    return TRUE;
}

BOOL OVcancel(TOKEN token) {
    if (!ov.open) {
	/* must be opened */
	syslog(L_ERROR, "ovopen must be called first");
	(void)fprintf(stderr, "ovopen must be called first");
	return FALSE;
    }
    return ((*ov.cancel)(token));
}

void *OVopensearch(char *group, int low, int high) {
    if (!ov.open) {
	/* must be opened */
	syslog(L_ERROR, "ovopen must be called first");
	(void)fprintf(stderr, "ovopen must be called first");
	return FALSE;
    }
    return ((*ov.opensearch)(group, low, high));
}

BOOL OVsearch(void *handle, ARTNUM *artnum, char **data, int *len, TOKEN *token, time_t *arrived) {
    if (!ov.open) {
	/* must be opened */
	syslog(L_ERROR, "ovopen must be called first");
	(void)fprintf(stderr, "ovopen must be called first");
	return FALSE;
    }
    return ((*ov.search)(handle, artnum, data, len, token, arrived));
}

void OVclosesearch(void *handle) {
    if (!ov.open) {
	/* must be opened */
	syslog(L_ERROR, "ovopen must be called first");
	(void)fprintf(stderr, "ovopen must be called first");
	return;
    }
    (*ov.closesearch)(handle);
    return;
}

BOOL OVgetartinfo(char *group, ARTNUM artnum, char **data, int *len, TOKEN *token) {
    if (!ov.open) {
	/* must be opened */
	syslog(L_ERROR, "ovopen must be called first");
	(void)fprintf(stderr, "ovopen must be called first");
	return FALSE;
    }
    return ((*ov.getartinfo)(group, artnum, data, len, token));
}

BOOL OVexpiregroup(char *group, int *lo) {
    if (!ov.open) {
	/* must be opened */
	syslog(L_ERROR, "ovopen must be called first");
	(void)fprintf(stderr, "ovopen must be called first");
	return FALSE;
    }
    return ((*ov.expiregroup)(group, lo));
}

BOOL OVctl(OVCTLTYPE type, void *val) {
    if (!ov.open) {
	/* must be opened */
	syslog(L_ERROR, "ovopen must be called first");
	(void)fprintf(stderr, "ovopen must be called first");
	return FALSE;
    }
    switch (type) {
    case OVGROUPBASEDEXPIRE:
	if (!innconf->groupbaseexpiry) {
	    syslog(L_ERROR, "OVGROUPBASEDEXPIRE is not allowed if groupbaseexpiry if false");
	    (void)fprintf(stderr, "OVGROUPBASEDEXPIRE is not allowed if groupbaseexpiry if false");
	    return FALSE;
	}
	if (((OVGE *)val)->delayrm) {
	    if ((((OVGE *)val)->filename == NULL) || (strlen(((OVGE *)val)->filename) == 0)) {
		syslog(L_ERROR, "file name must be specified");
		(void)fprintf(stderr, "file name must be specified");
	  	return FALSE;
	    }
	    if ((EXPunlinkfile = fopen(((OVGE *)val)->filename, "w")) == NULL) {
		syslog(L_ERROR, "fopen: %s failed: %m", ((OVGE *)val)->filename);
		(void)fprintf(stderr, "fopen: %s failed: %s", ((OVGE *)val)->filename, 
			      strerror(errno));
		return FALSE;
	    }
	}
	OVdelayrm = ((OVGE *)val)->delayrm;
	OVusepost = ((OVGE *)val)->usepost;
	OVrealnow = ((OVGE *)val)->now;
	OVnow = ((OVGE *)val)->now + (time_t)((OVGE *)val)->timewarp;
	OVquiet = ((OVGE *)val)->quiet;
	OVkeep = ((OVGE *)val)->keep;
	OVearliest = ((OVGE *)val)->earliest;
	OVignoreselfexpire = ((OVGE *)val)->ignoreselfexpire;
	return TRUE;
    case OVSTATALL:
	OVstatall = *(BOOL *)val;
	return TRUE;
    default:
	return ((*ov.ctl)(type, val));
    }
}

void OVclose(void) {
    int			i;
    BADGROUP	        *bg, *bgnext;
    ARTOVERFIELD	*fp;
    NGHASH		*htp;

    if (!ov.open)
	return;
    (*ov.close)();
    memset(&ov, '\0', sizeof(ov));
    if (EXPprocessed != 0) {
	if (!OVquiet) {
	    (void)printf("    Article lines processed %8ld\n", EXPprocessed);
	    (void)printf("    Articles dropped        %8ld\n", EXPunlinked);
	    (void)printf("    Overview index dropped  %8ld\n", EXPoverindexdrop);
	}
	EXPprocessed = EXPunlinked = EXPoverindexdrop = 0;
    }
    if (innconf->ovgrouppat != NULL) {
	for (i = 0 ; i < OVnumpatterns ; i++)
	    DISPOSE(OVpatterns[i]);
	DISPOSE(OVpatterns);
    }
    for (bg = EXPbadgroups; bg; bg = bgnext) {
	bgnext = bg->Next;
	DISPOSE(bg->Name);
	DISPOSE(bg);
    }
    for (fp = ARTfields, i = 0; i < ARTfieldsize ; i++, fp++) {
	DISPOSE(fp->Header);
    }
    DISPOSE(ARTfields);
    if (ACTIVE != NULL) {
	DISPOSE(ACTIVE);
	ACTIVE = NULL;
    }
    if (Groups != NULL) {
	DISPOSE(Groups);
	Groups = NULL;
    }
    for (i = 0, htp = NGHtable ; i < NGH_SIZE ; i++, htp++) {
	if (htp->Groups != NULL) {
	    DISPOSE(htp->Groups);
	    htp->Groups = NULL;
	}
    }
    return;
}

/*
**  Hash a newsgroup and see if we get it.
*/
STATIC NEWSGROUP *NGfind(char *Name)
{
    char		*p;
    int			i;
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

    ng1 = (NEWSGROUP **)p1;
    ng2 = (NEWSGROUP **)p2;
    return ng1[0]->Last - ng2[0]->Last;
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
	    (void)fprintf(stderr, "%s: line %d missing newline\n", ACTIVE, lines);
	    exit(1);
	}
	*q = '\0';
	if (EXPsplit(p, ' ', fields, SIZEOF(fields)) != 4) {
	    (void)fprintf(stderr, "%s: line %d wrong number of fields\n", ACTIVE, lines);
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

    /* Ok, now change our use of the Last field.  Set them all to maxint. */
    for (i = NGH_SIZE, htp = NGHtable; --i >= 0; htp++) {
	NEWSGROUP	**ngpa;
	int		k;

	for (ngpa = htp->Groups, k = htp->Used; --k >= 0; ngpa++) {
	    ngpa[0]->Last = ~(unsigned long) 0;
	    ngpa[0]->Lastpurged = 0;
	}
    }
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
	*v = OVnow - (time_t)(d * 86400.);
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
    SawDefault = FALSE;
    patterns = NEW(char*, nGroups);
    
    for (i = 1; fgets(buff, sizeof buff, F) != NULL; i++) {
	if ((p = strchr(buff, '\n')) == NULL) {
	    (void)fprintf(stderr, "Line %d too long\n", i);
	    DISPOSE(patterns);
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
	    DISPOSE(patterns);
	    return FALSE;
	}

	/* Expired-article remember line? */
	if (EQ(fields[0], "/remember/")) {
	    continue;
	}

	/* Regular expiration line -- right number of fields? */
	if (j != 5) {
	    (void)fprintf(stderr, "Line %d bad format\n", i);
	    DISPOSE(patterns);
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
	    DISPOSE(patterns);
	    return FALSE;
	}
	v.Poison = (strchr(fields[1], 'X') != NULL);
	if (!EXPgetnum(i, fields[2], &v.Keep,    "keep")
	 || !EXPgetnum(i, fields[3], &v.Default, "default")
	 || !EXPgetnum(i, fields[4], &v.Purge,   "purge")) {
	    DISPOSE(patterns);
	    return FALSE;
	}
	/* These were turned into offsets, so the test is the opposite
	 * of what you think it should be.  If Purge isn't forever,
	 * make sure it's greater then the other two fields. */
	if (v.Purge) {
	    /* Some value not forever; make sure other values are in range. */
	    if (v.Keep && v.Keep < v.Purge) {
		(void)fprintf(stderr, "Line %d keep>purge\n", i);
		DISPOSE(patterns);
		return FALSE;
	    }
	    if (v.Default && v.Default < v.Purge) {
		(void)fprintf(stderr, "Line %d default>purge\n", i);
		DISPOSE(patterns);
		return FALSE;
	    }
	}

	/* Is this the default line? */
	if (fields[0][0] == '*' && fields[0][1] == '\0' && mod == 'a') {
	    if (SawDefault) {
		(void)fprintf(stderr, "Line %d duplicate default\n", i);
		DISPOSE(patterns);
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
	    DISPOSE(patterns);
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
    char		buff[SPOOLNAMEBUFF];

    /* See if we already know about this group. */
    for (bg = EXPbadgroups; bg; bg = bg->Next)
	if (EQ(Entry, bg->Name))
	    break;
    if (bg == NULL) {
	bg = NEW(BADGROUP, 1);
	bg->Name = COPY(Entry);
	bg->Next = EXPbadgroups;
	EXPbadgroups = bg;
    }
    /* remove it all now. */
    if (Removeit.Keep == 0) {
	Removeit.Keep = OVnow;
	Removeit.Default = OVnow;
	Removeit.Purge = OVnow;
    }
    return &Removeit;
}

/*
**  Should we keep the specified article?
*/
STATIC enum KRP EXPkeepit(char *Entry, time_t when, time_t expires)
{
    char	        *p;
    NEWSGROUP	        *ngp;
    enum KRP		retval = Remove;

    if ((ngp = NGfind(Entry)) == NULL)
	ngp = EXPnotfound(Entry);

    /* Bad posting date? */
    if (when > OVrealnow + 86400) {
	/* Yes -- force the article to go right now. */
	when = expires ? ngp->Purge : ngp->Default;
    }

    /* If no expiration, make sure it wasn't posted before the default. */
    if (expires == 0) {
	if (when >= ngp->Default)
	    retval = Keep;

    /* Make sure it's not posted before the purge cut-off and
     * that it's not due to expire. */
    } else {
        if (when >= ngp->Purge && (expires >= OVnow || when >= ngp->Keep))
	    retval = Keep;
    }
    if (retval == Keep) {
	return Keep;
    } else {
	return ngp->Poison ? Poison : Remove;
    }
}

/*
**  An article can be removed.  Either print a note, or actually remove it.
**  Takes in the Xref information so that it can pass this to the storage
**  API callback used to generate the list of files to remove.
*/
void OVEXPremove(TOKEN token, BOOL deletedgroups, char **xref, int ngroups)
{
    EXPunlinked++;
    if (deletedgroups) {
	EXPprocessed++;
	EXPoverindexdrop++;
    }
    if (EXPunlinkfile && xref != NULL) {
        SMprintfiles(EXPunlinkfile, token, xref, ngroups);
	if (!ferror(EXPunlinkfile))
	    return;
	fprintf(stderr, "Can't write to -z file, %s\n", strerror(errno));
	fprintf(stderr, "(Will ignore it for rest of run.)\n");
	fclose(EXPunlinkfile);
	EXPunlinkfile = NULL;
    }
    if (!SMcancel(token) && SMerrno != SMERR_NOENT && SMerrno != SMERR_UNINIT)
	fprintf(stderr, "Can't unlink %s\n", TokenToText(token));
}

/*
**  Read the overview schema.
*/
STATIC void ARTreadschema(void)
{
    FILE			*F;
    char			*p;
    ARTOVERFIELD		*fp;
    int				i;
    char			buff[SMBUF];
    BOOL			foundxref = FALSE;
    BOOL			foundxreffull = FALSE;

    /* Open file, count lines. */
    if ((F = fopen(cpcatpath(innconf->pathetc, _PATH_SCHEMA), "r")) == NULL)
	return;
    for (i = 0; fgets(buff, sizeof buff, F) != NULL; i++)
	continue;
    (void)fseek(F, (OFFSET_T)0, SEEK_SET);
    ARTfields = NEW(ARTOVERFIELD, i + 1);

    /* Parse each field. */
    for (fp = ARTfields; fgets(buff, sizeof buff, F) != NULL; ) {
	/* Ignore blank and comment lines. */
	if ((p = strchr(buff, '\n')) != NULL)
	    *p = '\0';
	if ((p = strchr(buff, COMMENT_CHAR)) != NULL)
	    *p = '\0';
	if (buff[0] == '\0')
	    continue;
	if ((p = strchr(buff, ':')) != NULL) {
	    *p++ = '\0';
	    fp->NeedsHeader = EQ(p, "full");
	}
	else
	    fp->NeedsHeader = FALSE;
	fp->HasHeader = FALSE;
	fp->Header = COPY(buff);
	fp->Length = strlen(buff);
	if (caseEQ(buff, "Xref")) {
	    foundxref = TRUE;
	    foundxreffull = fp->NeedsHeader;
	}
	fp++;
    }
    ARTfieldsize = fp - ARTfields;
    (void)fclose(F);
    if (!foundxref || !foundxreffull) {
	(void)fprintf(stderr, "'Xref:full' must be included in %s", cpcatpath(innconf->pathetc, _PATH_SCHEMA));
	exit(1);
    }
}

/*
**  Return a field from the overview line or NULL on error.  Return a copy
**  since we might be re-using the line later.
*/
STATIC char *OVERGetHeader(char *p, int field)
{
    static char		*buff;
    static int		buffsize;
    int	                i;
    ARTOVERFIELD	*fp;
    char		*next, *q;

    fp = &ARTfields[field];

    /* Skip leading headers. */
    for (; field-- >= 0 && *p; p++)
	if ((p = strchr(p, '\t')) == NULL)
	    return NULL;
    if (*p == '\0')
	return NULL;

    if (fp->HasHeader)
        p += fp->Length + 2;

    if (fp->NeedsHeader) {		/* find an exact match */
	 while (strncmp(fp->Header, p, fp->Length) != 0) {
	      if ((p = strchr(p, '\t')) == NULL) 
		return NULL;
	      p++;
	 }
	 p += fp->Length + 2;
    }

    /* Figure out length; get space. */
    if ((next = strpbrk(p, "\n\r\t")) != NULL) {
	i = next - p;
    } else {
	i = strlen(p);
    }
    if (buffsize == 0) {
	buffsize = i;
	buff = NEW(char, buffsize + 1);
    }
    else if (buffsize < i) {
	buffsize = i;
	RENEW(buff, char, buffsize + 1);
    }

    (void)strncpy(buff, p, i);
    buff[i] = '\0';
    return buff;
}

/*
**  Read overview.fmt and find index for headers
*/
STATIC OVfindheaderindex() {
    FILE	*F;
    char	*active;
    int		i;

    if (ReadOverviewfmt)
	return;
    if (innconf->groupbaseexpiry) {
	ACTIVE = COPY(cpcatpath(innconf->pathdb, _PATH_ACTIVE));
	if ((active = ReadInFile(ACTIVE, (struct stat *)NULL)) == NULL) {
	    (void)fprintf(stderr, "Can't read %s, %s\n",
	    ACTIVE, strerror(errno));
	    exit(1);
	}
	BuildGroups(active);
	arts = NEW(char *, nGroups);
	krps = NEW(enum KRP, nGroups);
	F = fopen(cpcatpath(innconf->pathetc, _PATH_EXPIRECTL), "r");
	if (!EXPreadfile(F)) {
	    (void)fclose(F);
	    (void)fprintf(stderr, "Format error in expire.ctl\n");
	    exit(1);
	}
	(void)fclose(F);
    }
    ARTreadschema();
    if (Dateindex == OVFMT_UNINIT) {
	for (Dateindex = OVFMT_NODATE, i = 0; i < ARTfieldsize; i++) {
	    if (caseEQ(ARTfields[i].Header, "Date")) {
		Dateindex = i;
	    } else if (caseEQ(ARTfields[i].Header, "Xref")) {
		Xrefindex = i;
	    } else if (caseEQ(ARTfields[i].Header, "Message-ID")) {
		Messageidindex = i;
	    }
	}
    }
    ReadOverviewfmt = TRUE;
    return;
}

/*
**  Do the work of expiring one line.  Assumes article still exists in the
**  spool.  Returns TRUE if article should be purged, or return FALSE.
*/
BOOL OVgroupbasedexpire(TOKEN token, char *group, char *data, int len, time_t arrived, time_t expires)
{
    static char		*Group = NULL;
    char	        *p;
    int	                i;
    int			count;
    time_t		posted;
    time_t		when;
    BOOL                poisoned;
    BOOL		keeper;
    BOOL		remove;
    BOOL                purge;
    ARTHANDLE		*article;
    char		*Xref;

    if (SMprobe(SELFEXPIRE, &token, NULL)) {
	if (!OVignoreselfexpire)
	    /* this article should be kept */
	    return FALSE;
    }
    if (!ReadOverviewfmt) {
	OVfindheaderindex();
    }

    if (OVusepost) {
	if ((p = OVERGetHeader(data, Dateindex)) == NULL) {
	    EXPoverindexdrop++;
	    return TRUE;
	}
	if ((when = parsedate(p, NULL)) == -1) {
	    EXPoverindexdrop++;
	    return TRUE;
	}
    } else {
	when = arrived;
    }
    if ((Xref = OVERGetHeader(data, Xrefindex)) == NULL) {
	if (Group != NULL) {
	    DISPOSE(Group);
	}
	Group = NEW(char, strlen(group) + 2);
	strcpy(Group, group);
	strcat(Group, ":");
	Xref = Group;
    } else {
	if ((Xref = strchr(Xref, ' ')) == NULL) {
	    EXPoverindexdrop++;
	    return TRUE;
	}
	for (Xref++; *Xref == ' '; Xref++);
    }
    if ((count = EXPsplit(Xref, ' ', arts, nGroups)) == -1) {
	EXPoverindexdrop++;
	return TRUE;
    }

    /* arts is now an array of strings, each of which is a group name, a
       colon, and an article number.  EXPkeepit wants just pure group names,
       so replace the colons with nuls (deleting the overview entry if it
       isn't in the expected form). */
    for (i = 0; i < count; i++) {
        p = strchr(arts[i], ':');
        if (p == NULL) {
            fflush(stdout);
            fprintf(stderr, "Bad entry, \"%s\"\n", arts[i]);
            EXPoverindexdrop++;
            return TRUE;
        }
        *p = '\0';
    }

    /* First check all postings */
    poisoned = FALSE;
    keeper = FALSE;
    remove = FALSE;
    purge = TRUE;
    for (i = 0; i < count; ++i) {
	if ((krps[i] = EXPkeepit(arts[i], when, expires)) == Poison)
	    poisoned = TRUE;
	if (OVkeep && (krps[i] == Keep))
	    keeper = TRUE;
	if ((krps[i] == Remove) && EQ(group, arts[i]))
	    remove = TRUE;
        if ((krps[i] == Keep))
            purge = FALSE;
    }
    EXPprocessed++;

    if (OVearliest) {
	if (remove || poisoned || token.type == TOKEN_EMPTY) {
	    /* delete article if this is first entry */
	    if (EQ(group, arts[0])) {
		for (i = 0; i < count; i++)
		    arts[i][strlen(arts[i])] = ':';
		OVEXPremove(token, FALSE, arts, count);
	    }
	    EXPoverindexdrop++;
	    return TRUE;
	}
    } else { /* not earliest mode */
	if ((!keeper && remove) || token.type == TOKEN_EMPTY) {
	    /* delete article if purge is set, indicating that it has
	       expired out of every group to which it was posted */
	    if (purge) {
		for (i = 0; i < count; i++)
		    arts[i][strlen(arts[i])] = ':';
		OVEXPremove(token, FALSE, arts, count);
	    }
	    EXPoverindexdrop++;
	    return TRUE;
	}
    }

    /* this article should be kept */
    return FALSE;
}

BOOL OVhisthasmsgid(char *data) {
    char		*p;
    static STRING	History;
    HASH		key;
    OFFSET_T		offset;
    static FILE		*F;
    int			i, c;
    char		buff[(sizeof(TOKEN) * 2) + 3];

    if (!ReadOverviewfmt) {
	OVfindheaderindex();
    }
    if (History == NULL) {
	History = COPY(cpcatpath(innconf->pathdb, _PATH_HISTORY));
	if (!dbzinit(History)) {
	    syslog(L_ERROR, "OVhisthasmsgid: dbzinit failed '%s'", History);
	    return FALSE;
	}
	if ((F = fopen(History, "r")) == NULL) {
	    syslog(L_ERROR, "OVhisthasmsgid: fopen failed '%s', %m", History);
	    return FALSE;
	}
    }
    if ((p = OVERGetHeader(data, Messageidindex)) == NULL)
	return FALSE;
    key = HashMessageID(p);
    if (!dbzfetch(key, &offset))
	return FALSE;
    if (fseek(F, offset, SEEK_SET) == -1) {
	syslog(L_ERROR, "OVhisthasmsgid: fseek failed to %ld '%s', %m", offset, History);
	return FALSE;
    }
    for (i = 2; (c = getc(F)) != EOF && c != '\n'; )
	if (c == HIS_FIELDSEP && --i == 0)
	    break;
    if (c != HIS_FIELDSEP)
	return FALSE;
    i = 0;
    while ((c = getc(F)) != EOF && c != ' ' && c != '\n' && i < (sizeof(TOKEN) * 2) + 2) {
	buff[i++] = (char)c;
    }
    buff[i] = '\0';
    if (IsToken(buff))
	return TRUE;
    return FALSE;
}

BOOL OVgroupmatch(char *group) {
    int		i;
    BOOL	wanted = FALSE;

    if (OVnumpatterns == 0 || group == NULL)
	return TRUE;
    for (i = 0; i < OVnumpatterns; i++) {
	switch (OVpatterns[i][0]) {
	case '!':
	    if (!wanted && wildmat(group, &OVpatterns[i][1]))
		break;
	case '@':
	    if (wildmat(group, &OVpatterns[i][1])) {
		return FALSE;
	    }
	    break;
	default:
	    if (wildmat(group, OVpatterns[i]))
		wanted = TRUE;
	}
    }
    return wanted;
}

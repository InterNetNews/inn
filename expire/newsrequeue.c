/*  $Id$
**
**  Requeue outgoing news based on news logs.
*/

#include "config.h"
#include "clibrary.h"
#include <ctype.h>
#include <errno.h>
#include <syslog.h>  
#include <sys/stat.h>

#include "dbz.h"
#include "inn/qio.h"
#include "libinn.h"
#include "macros.h"
#include "paths.h"


/*
**  Hashing functions.  See innd for comments.
*/
#define NGH_HASH(Name, p, j)    \
	for (p = Name, j = 0; *p; ) j = (j << 5) + j + *p++
#define NGH_SIZE	128
#define NGH_BUCKET(j)	&NGHtable[j & (NGH_SIZE - 1)]

typedef struct _NGHASH {
    int			Size;
    int			Used;
    struct _NEWSGROUP	**Groups;
} NGHASH;


/*
**  A site has a filename, and flag saying if we already sent it here.
*/
typedef struct _SITE {
    char		*Name;
    char		**Exclusions;
    char		*Patterns;
    char		**Distributions;
    char		*Flags;
    bool		Sent;
} SITE;

/*
**  A newsgroup has a name, and a set of sites that get the group.
*/
typedef struct _NEWSGROUP {
    char		*Name;
    char		Flag;
    int			nSites;
    SITE		**Sites;
} NEWSGROUP;


/*
**  Bit array, indexed by character (8bit chars only).  If ARTpathbits['x']
**  is non-zero, then 'x' is a valid character for a host name.
*/
static char		ARTpathbits[256];
#define ARThostchar(c)	(ARTpathbits[(int) c] != '\0')


/*
**  Global variables.
*/
static SITE		*Sites;
static int		nSites;
static NEWSGROUP	*Groups;
static int		nGroups;
static NGHASH		NGHtable[NGH_SIZE];



/*
**  Read the active file and fill in the Groups array.  Note that
**  NEWSGROUP.Sites is filled in later.
*/
static void
ParseActive(const char *name)
{
    char	*active;
    char	*p;
    char	*q;
    int         i;
    unsigned	j;
    NGHASH	*htp;
    NEWSGROUP	*ngp;
    int		NGHbuckets;

    /* Read the file, count the number of groups. */
    if ((active = ReadInFile(name, (struct stat *)NULL)) == NULL) {
	(void)fprintf(stderr, "Can't read \"%s\", %s\n",
		name, strerror(errno));
	exit(1);
    }
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

    /* Fill in the newsgroups array. */
    for (p = active, ngp = Groups, i = nGroups; --i >= 0; ngp++, p = q + 1) {
	if ((q = strchr(p, '\n')) == NULL) {
	    (void)fprintf(stderr, "Missing newline near \"%.10s...\"\n", p);
	    exit(1);
	}
	*q = '\0';
	ngp->Name = p;
	ngp->nSites = 0;

	/* Get the first character after the third space. */
	for (j = 0; *p; p++)
	    if (*p == ' ' && ++j == 3)
		break;
	if (*p == '\0') {
	    (void)fprintf(stderr, "Bad format near \"%.10s...\"\n", ngp->Name);
	    exit(1);
	}
	ngp->Flag = p[1];

	/* Find the right bucket for the group, make sure there is room. */
	/* SUPPRESS 6 *//* Over/underflow from plus expression */
	p = strchr(ngp->Name, ' ');
	*p = '\0';
	NGH_HASH(ngp->Name, p, j);
	htp = NGH_BUCKET(j);
	if (htp->Used >= htp->Size) {
	    htp->Size += NGHbuckets;
	    RENEW(htp->Groups, NEWSGROUP*, htp->Size);
	}
	htp->Groups[htp->Used++] = ngp;
    }

    /* Note that we don't bother to sort the buckets. */
}


/*
**  Split text into comma-separated fields.
*/
static char **
CommaSplit(text)
    char		*text;
{
    register int	i;
    register char	*p;
    register char	**argv;
    char		**save;

    /* How much space do we need? */
    for (i = 2, p = text; *p; p++)
	if (*p == ',')
	    i++;

    for (argv = save = NEW(char*, i), *argv++ = p = text; *p; )
	if (*p == ',') {
	    *p++ = '\0';
	    *argv++ = p;
	}
	else
	    p++;
    *argv = NULL;
    return save;
}


/*
**  Read the newsfeeds file and fill in the Sites array.  Finish off the
**  Groups array.
*/
static void
ParseNewsfeeds(const char *name)
{
    char                *p;
    char                *to;
    int                 i;
    NEWSGROUP           *ngp;
    SITE                *sp;
    char		**strings;
    char		*save;
    char		*newsfeeds;

    /* Read in the file, get a gross count of the number of sites. */
    if ((newsfeeds = ReadInFile(name, (struct stat *)NULL)) == NULL) {
	(void)fprintf(stderr, "Can't read \"%s\", %s\n",
		name, strerror(errno));
	exit(1);
    }
    for (p = newsfeeds, i = 0; (p = strchr(p, '\n')) != NULL; p++, i++)
	continue;

    /* Scan the file, parse all multi-line entries. */
    for (strings = NEW(char*, i + 1), i = 0, to = p = newsfeeds; *p; ) {
	for (save = to; *p; ) {
	    if (*p == '\n') {
		p++;
		*to = '\0';
		break;
	    }
	    if (*p == '\\' && p[1] == '\n')
		while (*++p && CTYPE(isspace, (int)*p))
		    continue;
	    else
		*to++ = *p++;
	}
	*to++ = '\0';
	if (*save == COMMENT_CHAR || *save == '\0')
	    continue;
	strings[i++] = COPY(save);
    }
    DISPOSE(newsfeeds);
    if (i == 0) {
	(void)fprintf(stderr, "No sites.\n");
	exit(1);
    }

    /* Get space for the sites. */
    nSites = i;
    Sites = NEW(SITE, nSites);
    for (i = nGroups, ngp = Groups; --i >= 0; ngp++)
	ngp->Sites = NEW(SITE*, nSites);

    /* Do initial processing of the site entries. */
    for (i = 0, sp = Sites; i < nSites; i++, sp++) {
	/* Nip off the first and second fields. */
	sp->Name = strings[i];
	if ((p = strchr(sp->Name, NF_FIELD_SEP)) == NULL) {
	    (void)fprintf(stderr, "No separator for site \"%.10s...\"\n",
		    sp->Name);
	    exit(1);
	}
	*p++ = '\0';
	sp->Patterns = p;

	/* Nip off the third field. */
	if ((p = strchr(sp->Patterns, NF_FIELD_SEP)) == NULL) {
	    (void)fprintf(stderr, "No flags for site \"%s\"\n", sp->Name);
	    exit(1);
	}
	*p++ = '\0';
	sp->Flags = p;

	/* Nip off the last field, build the filename. */
	if ((p = strchr(sp->Flags, NF_FIELD_SEP)) == NULL) {
	    (void)fprintf(stderr, "No last field for site \"%s\"\n", sp->Name);
	    exit(1);
	}
	*p++ = '\0';

	/* Handle the subfields. */
	if ((p = strchr(sp->Name, NF_SUBFIELD_SEP)) != NULL) {
	    *p++ = '\0';
	    sp->Exclusions = CommaSplit(p);
	}
	else
	    sp->Exclusions = NULL;
	if ((p = strchr(sp->Patterns, NF_SUBFIELD_SEP)) != NULL) {
	    *p++ = '\0';
	    sp->Distributions = CommaSplit(p);
	}
	else
	    sp->Distributions = NULL;
    }
}


/*
**  Build the subscription list for a site.
*/
static void
BuildSubList(sp, subbed)
    register SITE	*sp;
    char		*subbed;
{
    static char		SEPS[] = ",";
    register char	subvalue;
    register char	*pat;
    register char	*p;
    register NEWSGROUP	*ngp;
    register int	i;
    bool		JustModerated;
    bool		JustUnmoderated;

    if (EQ(sp->Name, "ME"))
	return;

    /* Fill in the subbed array with the mask of groups. */
    memset(subbed, SUB_DEFAULT, nGroups);
    if ((pat = strtok(sp->Patterns, SEPS)) != NULL)
	do {
	    subvalue = *pat != SUB_NEGATE;
	    if (!subvalue)
		pat++;
	    for (p = subbed, ngp = Groups, i = nGroups; --i >= 0; ngp++, p++)
		if (wildmat(ngp->Name, pat))
		    *p = subvalue;
	} while ((pat = strtok((char *)NULL, SEPS)) != NULL);

    /* Parse the flags.. */
    JustModerated = FALSE;
    JustUnmoderated = FALSE;
    if ((p = strtok(sp->Flags, SEPS)) != NULL)
	do {
	    switch (*p) {
	    case 'W':
		if (EQ(p, "Wnm"))
		    break;
		/* FALLTHROUGH */
	    default:
		(void)fprintf(stderr, "Ignoring \"%s\" flag for \"%s\"\n",
			p, sp->Name);
		break;
	    case 'N':
		while (*++p)
		    switch (*p) {
		    default:
			(void)fprintf(stderr, "Unknown N%c flag for \"%s\"\n",
				*p, sp->Name);
			break;
		    case 'm': JustModerated = TRUE;	break;
		    case 'u': JustUnmoderated = TRUE;	break;
		    }
		break;
	    case 'T':
		break;
	    }
	} while ((p = strtok((char *)NULL, SEPS)) != NULL);

    /* Modify the subscription list based on the flags. */
    if (JustModerated)
	for (p = subbed, ngp = Groups, i = nGroups; --i >= 0; ngp++, p++)
	    if (ngp->Flag != NF_FLAG_MODERATED)
		*p = FALSE;
    if (JustUnmoderated)
	for (p = subbed, ngp = Groups, i = nGroups; --i >= 0; ngp++, p++)
	    if (ngp->Flag == NF_FLAG_MODERATED)
		*p = FALSE;

    /* Tell the groups that this site gets that they should feed this site. */
    for (p = subbed, ngp = Groups, i = nGroups; --i >= 0; ngp++)
	if (*p++)
	    ngp->Sites[ngp->nSites++] = sp;
}



/*
**  Print a usage message and exit.
*/
static void
Usage(void)
{
    fprintf(stderr, "Usage error.\n");
    exit(1);
}


int
main(ac, av)
    int			ac;
    char		*av[];
{
    int	i;
    QIOSTATE		*qp;
    char		*p;
    char		*q;
    char		*r;
    char		*s;
    char		*line;
    FILE		*F;
    const char		*Active;
    const char		*History;
    const char		*Newsfeeds;
    char		save;
    int			nntplinklog;

    /* First thing, set up logging and our identity. */
    openlog("newsrequeue", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);

    /* Set defaults. */
    if (ReadInnConf() < 0) exit(1);

    Active = concatpath(innconf->pathdb, _PATH_ACTIVE);
    History = concatpath(innconf->pathdb, _PATH_HISTORY);
    Newsfeeds = concatpath(innconf->pathetc, _PATH_NEWSFEEDS);
    nntplinklog = innconf->nntplinklog;

    /* Parse JCL. */
    while ((i = getopt(ac, av, "a:h:n:")) != EOF)
	switch (i) {
	default:
	    Usage();
	    /* NOTREACHED */
	case 'a':
	    Active = optarg;
	    break;
	case 'h':
	    History = optarg;
	    break;
	case 'n':
	    Newsfeeds = optarg;
	    break;
	}
    ac -= optind;
    av += optind;

    /* Parse positional parameters; at most one, the input file. */
    switch (ac) {
    default:
	Usage();
	/* NOTREACHED */
    case 0:
	break;
    case 1:
	if (freopen(av[0], "r", stdin) == NULL) {
	    (void)fprintf(stderr, "Can't open \"%s\" for input, %s\n",
		    av[0], strerror(errno));
	    exit(1);
	}
	break;
    }
    qp = QIOfdopen((int)fileno(stdin));

    /* Open the history file. */
    if (!dbzinit(History)) {
	(void)fprintf(stderr, "Can't set up \"%s\" database, %s\n",
		History, strerror(errno));
	exit(1);
    }
    if ((F = fopen(History, "r")) == NULL) {
	(void)fprintf(stderr, "Can't open \"%s\" for reading, %s\n",
		History, strerror(errno));
	exit(1);
    }
    /* Now we're ready to start reading input. */
    for (i = 1; ; i++) {
	if ((line = QIOread(qp)) == NULL) {
	    /* Read or line-format error? */
	    if (QIOerror(qp)) {
		(void)fprintf(stderr, "Can't read line %d, %s\n",
			i, strerror(errno));
		exit(1);
	    }
	    if (QIOtoolong(qp)) {
		(void)fprintf(stderr, "Line %d too long\n", i);
		exit(1);
	    }

	    /* We hit EOF. */
	    break;
	}

	/* Check the log character (correct for zero-origin subscripts. */
	switch (line[STRLEN("Jan 23 12:52:12.631 +") - 1]) {
	default:
	    (void)fprintf(stderr, "Ignoring \"%s\"\n", line);
	    continue;
	case ART_CANC:
	case ART_REJECT:
	    continue;
	case ART_ACCEPT:
	case ART_JUNK:
	    break;
	}

	/* Snip off the Message-ID. */
	if ((p = strchr(line, '<')) == NULL
	 || (q = strchr(p, '>')) == NULL) {
	    (void)fprintf(stderr, "No Message-ID in \"%s\"\n", line);
	    continue;
	}
	save = *++q;
	*q = '\0';

	/* Skip the (filename) if it's there. */
	if (save != '\0' && ((r = strchr(q + 1, '(')) != NULL) &&
	    ((s = strchr(r + 1, ')')) != NULL)) {
	    *s = '\0';
	    if (innconf->logartsize) {
		if ((s = strchr(s + 1, ' ')) != NULL)
		    (void)printf("%s %s %s\n", r + 1, p, s + 1);
		else
		    continue;
	    } else
		(void)printf("%s %s %s\n", r + 1, p, s + 1);
	} else {
	    continue;
	}
    }
    /* That's all she wrote. */
    QIOclose(qp);
    (void)fclose(F);
    (void)dbzclose();
    exit(0);
    /* NOTREACHED */
}

/*  $Id$
**
**  Buffered file exploder for innd.
*/

#include "config.h"
#include "clibrary.h"
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>

#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/qio.h"
#include "libinn.h"
#include "paths.h"
#include "map.h"

/*
**  Hash functions for hashing sitenames.
*/
#define SITE_HASH(Name, p, j)    \
	for (p = Name, j = 0; *p; ) j = (j << 5) + j + *p++
#define SITE_SIZE	128
#define SITE_BUCKET(j)	&SITEtable[j & (SITE_SIZE - 1)]


/*
**  Entry for a single active site.
*/
typedef struct _SITE {
    bool	Dropped;
    const char	*Name;
    int		CloseLines;
    int		FlushLines;
    time_t	LastFlushed;
    time_t	LastClosed;
    int 	CloseSeconds;
    int		FlushSeconds;
    FILE	*F;
    const char	*Filename;
    char	*Buffer;
} SITE;


/*
**  Site hashtable bucket.
*/
typedef struct _SITEHASH {
    int		Size;
    int		Used;
    SITE	*Sites;
} SITEHASH;


/* Global variables. */
static char	*Format;
static const char *Map;
static int	BufferMode;
static int	CloseEvery;
static int	FlushEvery;
static int	CloseSeconds;
static int	FlushSeconds;
static sig_atomic_t	GotInterrupt;
static SITEHASH	SITEtable[SITE_SIZE];
static TIMEINFO	Now;


/*
**  Set up the site information.  Basically creating empty buckets.
*/
static void
SITEsetup(void)
{
    SITEHASH	*shp;

    for (shp = SITEtable; shp < ARRAY_END(SITEtable); shp++) {
	shp->Size = 3;
	shp->Sites = xmalloc(shp->Size * sizeof(SITE));
	shp->Used = 0;
    }
}


/*
**  Close a site
*/
static void
SITEclose(SITE *sp)
{
    FILE	*F;

    if ((F = sp->F) != NULL) {
	if (fflush(F) == EOF || ferror(F)
	 || fchmod((int)fileno(F), 0664) < 0
	 || fclose(F) == EOF)
            syswarn("%s cannot close %s", sp->Name, sp->Filename);
	sp->F = NULL;
    }
}

/*
**  Close all open sites.
*/
static void
SITEcloseall(void)
{
    SITEHASH	*shp;
    SITE	*sp;
    int	i;

    for (shp = SITEtable; shp < ARRAY_END(SITEtable); shp++)
	for (sp = shp->Sites, i = shp->Used; --i >= 0; sp++)
	    SITEclose(sp);
}


/*
**  Open the file for a site.
*/
static void SITEopen(SITE *sp)
{
    int			e;

    if ((sp->F = xfopena(sp->Filename)) == NULL
     && ((e = errno) != EACCES || chmod(sp->Filename, 0644) < 0
      || (sp->F = xfopena(sp->Filename)) == NULL)) {
        syswarn("%s cannot fopen %s", sp->Name, sp->Filename);
	if ((sp->F = fopen("/dev/null", "w")) == NULL)
	    /* This really should not happen. */
            sysdie("%s cannot fopen /dev/null", sp->Name);
    }
    else if (fchmod((int)fileno(sp->F), 0444) < 0)
        syswarn("%s cannot fchmod %s", sp->Name, sp->Filename);
	
    if (BufferMode != '\0')
	setbuf(sp->F, sp->Buffer);

    /* Reset all counters. */
    sp->FlushLines = 0;
    sp->CloseLines = 0;
    sp->LastFlushed = Now.time;
    sp->LastClosed = Now.time;
    sp->Dropped = false;
}


/*
**  Find a site, possibly create if not found.
*/
static SITE *
SITEfind(char *Name, bool CanCreate)
{
    char	*p;
    int	i;
    unsigned int	j;
    SITE	*sp;
    SITEHASH		*shp;
    char		c;
    char		buff[BUFSIZ];

    /* Look for site in the hash table. */
    SITE_HASH(Name, p, j);
    shp = SITE_BUCKET(j);
    for (c = *Name, sp = shp->Sites, i = shp->Used; --i >= 0; sp++)
	if (c == sp->Name[0] && strcasecmp(Name, sp->Name) == 0)
	    return sp;
    if (!CanCreate)
	return NULL;

    /* Adding a new site -- grow hash bucket if we need to. */
    if (shp->Used == shp->Size - 1) {
	shp->Size *= 2;
        shp->Sites = xrealloc(shp->Sites, shp->Size * sizeof(SITE));
    }
    sp = &shp->Sites[shp->Used++];

    /* Fill in the structure for the new site. */
    sp->Name = xstrdup(Name);
    snprintf(buff, sizeof(buff), Format, Map ? MAPname(Name) : sp->Name);
    sp->Filename = xstrdup(buff);
    if (BufferMode == 'u')
	sp->Buffer = NULL;
    else if (BufferMode == 'b')
	sp->Buffer = xmalloc(BUFSIZ);
    SITEopen(sp);

    return sp;
}


/*
**  Flush a site -- close and re-open the file.
*/
static void
SITEflush(SITE *sp)
{
    FILE	*F;

    if ((F = sp->F) != NULL) {
	if (fflush(F) == EOF || ferror(F)
	 || fchmod((int)fileno(F), 0664) < 0
	 || fclose(F) == EOF)
            syswarn("%s cannot close %s", sp->Name, sp->Filename);
	sp->F = NULL;
    }
    if (!sp->Dropped)
	SITEopen(sp);
}


/*
**  Flush all open sites.
*/
static void
SITEflushall(void)
{
    SITEHASH	*shp;
    SITE	*sp;
    int	i;

    for (shp = SITEtable; shp < ARRAY_END(SITEtable); shp++)
	for (sp = shp->Sites, i = shp->Used; --i >= 0; sp++)
	    SITEflush(sp);
}


/*
**  Write data to a site.
*/
static void
SITEwrite(char *name, char *text, size_t len)
{
    SITE	*sp;

    sp = SITEfind(name, true);
    if (sp->F == NULL)
	SITEopen(sp);

    if (fwrite(text, 1, len, sp->F) != len)
        syswarn("%s cannot write", sp->Name);

    /* Bump line count; see if time to close or flush. */
    if (CloseEvery && ++(sp->CloseLines) >= CloseEvery) {
	SITEflush(sp);
	return;
    }
    if (CloseSeconds && sp->LastClosed + CloseSeconds < Now.time) {
	SITEflush(sp);
	return;
    }
    if (FlushEvery && ++(sp->FlushLines) >= FlushEvery) {
	if (fflush(sp->F) == EOF || ferror(sp->F))
            syswarn("%s cannot flush %s", sp->Name, sp->Filename);
	sp->LastFlushed = Now.time;
	sp->FlushLines = 0;
    }
    else if (FlushSeconds && sp->LastFlushed + FlushSeconds < Now.time) {
	if (fflush(sp->F) == EOF || ferror(sp->F))
            syswarn("%s cannot flush %s", sp->Name, sp->Filename);
	sp->LastFlushed = Now.time;
	sp->FlushLines = 0;
    }
}


/*
**  Handle a command message.
*/
static void
Process(char *p)
{
    SITE	*sp;

    if (*p == 'b' && strncmp(p, "begin", 5) == 0)
	/* No-op. */
	return;

    if (*p == 'f' && strncmp(p, "flush", 5) == 0) {
	for (p += 5; ISWHITE(*p); p++)
	    continue;
	if (*p == '\0')
	    SITEflushall();
	else if ((sp = SITEfind(p, false)) != NULL)
	    SITEflush(sp);
	/*else
	    fprintf(stderr, "buffchan flush %s unknown site\n", p);*/
	return;
    }

    if (*p == 'd' && strncmp(p, "drop", 4) == 0) {
	for (p += 4; ISWHITE(*p); p++)
	    continue;
	if (*p == '\0')
	    SITEcloseall();
	else if ((sp = SITEfind(p, false)) == NULL)
            warn("drop %s unknown site", p);
	else {
	    SITEclose(sp);
	    sp->Dropped = true;
	}
	return;
    }

    if (*p == 'r' && strncmp(p, "readmap", 7) == 0) {
	MAPread(Map);
	return;
    }

    /* Other command messages -- ignored. */
    warn("unknown message %s", p);
}


/*
**  Mark that we got a signal; let two signals kill us.
*/
static RETSIGTYPE
CATCHinterrupt(int s)
{
    GotInterrupt = true;
    xsignal(s, SIG_DFL);
}


int
main(int ac, char *av[])
{
    QIOSTATE	*qp;
    int	i;
    int	Fields;
    char	*p;
    char	*next;
    char	*line;
    char		*Directory;
    bool		Redirect;
    FILE		*F;
    char		*ERRLOG;

    /* First thing, set up our identity. */
    message_program_name = "buffchan";

    /* Set defaults. */
    if (!innconf_read(NULL))
        exit(1);
    ERRLOG = concatpath(innconf->pathlog, _PATH_ERRLOG);
    Directory = NULL;
    Fields = 1;
    Format = NULL;
    Redirect = true;
    GotInterrupt = false;
    umask(NEWSUMASK);

    xsignal(SIGHUP, CATCHinterrupt);
    xsignal(SIGINT, CATCHinterrupt);
    xsignal(SIGQUIT, CATCHinterrupt);
    xsignal(SIGPIPE, CATCHinterrupt);
    xsignal(SIGTERM, CATCHinterrupt);
    xsignal(SIGALRM, CATCHinterrupt);

    /* Parse JCL. */
    while ((i = getopt(ac, av, "bc:C:d:f:l:L:m:p:rs:u")) != EOF)
	switch (i) {
	default:
            die("usage error");
            break;
	case 'b':
	case 'u':
	    BufferMode = i;
	    break;
	case 'c':
	    CloseEvery = atoi(optarg);
	    break;
	case 'C':
	    CloseSeconds = atoi(optarg);
	    break;
	case 'd':
	    Directory = optarg;
	    if (Format == NULL)
		Format =xstrdup("%s");
	    break;
	case 'f':
	    Fields = atoi(optarg);
	    break;
	case 'l':
	    FlushEvery = atoi(optarg);
	    break;
	case 'L':
	    FlushSeconds = atoi(optarg);
	    break;
	case 'm':
	    Map = optarg;
	    MAPread(Map);
	    break;
	case 'p':
	    if ((F = fopen(optarg, "w")) == NULL)
                sysdie("cannot fopen %s", optarg);
	    fprintf(F, "%ld\n", (long)getpid());
	    if (ferror(F) || fclose(F) == EOF)
                sysdie("cannot fclose %s", optarg);
	    break;
	case 'r':
	    Redirect = false;
	    break;
	case 's':
	    Format = optarg;
	    break;
	}
    ac -= optind;
    av += optind;
    if (ac)
	die("usage error");

    /* Do some basic set-ups. */
    if (Redirect)
	freopen(ERRLOG, "a", stderr);
    if (Format == NULL) {
        Format = concatpath(innconf->pathoutgoing, "%s");
    }
    if (Directory && chdir(Directory) < 0)
        sysdie("cannot chdir to %s", Directory);
    SITEsetup();

    /* Read input. */
    for (qp = QIOfdopen((int)fileno(stdin)); !GotInterrupt ; ) {
	if ((line = QIOread(qp)) == NULL) {
	    if (QIOerror(qp)) {
                syswarn("cannot read");
		break;
	    }
	    if (QIOtoolong(qp)) {
                warn("long line");
		continue;
	    }

	    /* Normal EOF. */
	    break;
	}

	/* Command? */
	if (*line == EXP_CONTROL && *++line != EXP_CONTROL) {
	    Process(line);
	    continue;
	}

	/* Skip the right number of leading fields. */
	for (i = Fields, p = line; *p; p++)
	    if (*p == ' ' && --i <= 0)
		break;
	if (*p == '\0')
	    /* Nothing to write.  Probably shouldn't happen. */
	    continue;

	/* Add a newline, get the length of all leading fields. */
	*p++ = '\n';
	i = p - line;

	if (GetTimeInfo(&Now) < 0) {
            syswarn("cannot get time");
	    break;
	}

	/* Rest of the line is space-separated list of filenames. */
	for (; *p; p = next) {
	    /* Skip whitespace, get next word. */
	    while (*p == ' ')
		p++;
	    for (next = p; *next && *next != ' '; next++)
		continue;
	    if (*next)
		*next++ = '\0';

	    SITEwrite(p, line, i);
	}

    }

    SITEcloseall();
    exit(0);
    /* NOTREACHED */
}

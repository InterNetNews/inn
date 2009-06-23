/*  $Id$
**
**  Rebuild history/overview databases.
*/

#include "config.h"
#include "clibrary.h"
#include "portable/wait.h"
#include <assert.h>
#include <errno.h>
#include <syslog.h>
#include <time.h>

#include "inn/buffer.h"
#include "inn/history.h"
#include "inn/innconf.h"
#include "inn/libinn.h"
#include "inn/messages.h"
#include "inn/newsuser.h"
#include "inn/ov.h"
#include "inn/paths.h"
#include "inn/qio.h"
#include "inn/storage.h"
#include "inn/vector.h"
#include "inn/wire.h"

/*
**  If we have getloadavg, include the appropriate header file.  Otherwise,
**  just assume that we always have a load of 0.
*/
#if HAVE_GETLOADAVG
# if HAVE_SYS_LOADAVG_H
#  include <sys/loadavg.h>
# endif
#else
static int
getloadavg(double loadavg[], int nelem)
{
    int i;

    for (i = 0; i < nelem && i < 3; i++)
        loadavg[i] = 0;
    return i;
}
#endif

static const char usage[] = "\
Usage: makehistory [-abFIOSx] [-f file] [-l count] [-L load] [-s size] [-T tmpdir]\n\
\n\
    -a          open output history file in append mode\n\
    -b          delete bad articles from spool\n\
    -F          fork when writing overview\n\
    -f file     write history entries to file (default $pathdb/history)\n\
    -I          do not create overview for articles numbered below lowmark\n\
    -l count    size of overview updates (default 10000)\n\
    -L load     pause when load average exceeds threshold\n\
    -O          create overview entries for articles\n\
    -S          write overview data to standard output\n\
    -s size     size new history database for approximately size entries\n\
    -T tmpdir   use directory tmpdir for temporary files\n\
    -x          don't write history entries\n";


/*
**  Information about the schema of the news overview files.
*/
typedef struct _ARTOVERFIELD {
    char	*Headername;
    int		HeadernameLength;
    bool	NeedHeadername;
    const char	*Header;
    int		HeaderLength;
    bool	HasHeader;
} ARTOVERFIELD;

#define DEFAULT_SEGSIZE	10000;

bool NukeBadArts;
char *ActivePath = NULL;
char *HistoryPath = NULL;
struct history *History;
FILE *Overchan;
bool DoOverview;
bool Fork;
bool Cutofflow = false;
char *TmpDir;
int OverTmpSegSize, OverTmpSegCount;
FILE *OverTmpFile;
char *OverTmpPath = NULL;
bool NoHistory;
OVSORTTYPE sorttype;
bool WriteStdout = false;

/* Misc variables needed for the overview creation code. */
static char             BYTES[] = "Bytes";
static char             DATE[] = "Date";
static char             EXPIRES[] = "Expires";
static char             LINES[] = "Lines";
static char		MESSAGEID[] = "Message-ID";
static char		XREF[] = "Xref";
static ARTOVERFIELD	*ARTfields; /* Overview fields for which overview data
                                     * is generated. */
static size_t		ARTfieldsize;
static ARTOVERFIELD     *Bytesp = (ARTOVERFIELD *)NULL;
static ARTOVERFIELD     *Datep = (ARTOVERFIELD *)NULL;
static ARTOVERFIELD     *Expp = (ARTOVERFIELD *)NULL;
static ARTOVERFIELD     *Linesp = (ARTOVERFIELD *)NULL;
static ARTOVERFIELD     *Msgidp = (ARTOVERFIELD *)NULL;
static ARTOVERFIELD     *Xrefp = (ARTOVERFIELD *)NULL;
static ARTOVERFIELD	*Missfields; /* Header fields not used in overview
                                      * but that we need (e.g. Expires:). */
static size_t		Missfieldsize = 0;

static void OverAddAllNewsgroups(void);

/*
**  Check and parse a Message-ID header line.  Return private space.
*/
static const char *
GetMessageID(char *p)
{
    static struct buffer buffer = { 0, 0, 0, NULL };

    while (ISWHITE(*p))
	p++;
    if (p[0] != '<' || p[strlen(p) - 1] != '>')
	return "";

    /* Copy into re-used memory space, including NUL. */
    buffer_set(&buffer, p, strlen(p)+1);
    return buffer.data;
}

/*
 * The overview temp file is used to accumulate overview lines as articles are
 * scanned.  The format is
 * (1st) newsgroup name\tToken\toverview data.
 * When about 10000 lines of this overview data are accumulated, the data
 * file is sorted and then read back in and the data added to overview.
 * The sorting/batching helps improve efficiency.
 */

/*
 * Flush the unwritten OverTempFile data to disk, sort the file, read it 
 * back in, and add it to overview. 
 */

static void
FlushOverTmpFile(void)
{
    char temp[SMBUF];
    char *SortedTmpPath;
    int i, pid, fd;
    TOKEN token;
    QIOSTATE *qp;
    int count;
    char *line, *p;
    char *q = NULL;
    char *r = NULL;
    time_t arrived, expires;
    static int first = 1;

    if (OverTmpFile == NULL)
	return;
    if (fflush(OverTmpFile) == EOF || ferror(OverTmpFile) || fclose(OverTmpFile) == EOF)
        sysdie("cannot close temporary overview file");
    if(Fork) {
        if(!first) { /* if previous one is running, wait for it */
	    int status;
	    wait(&status);
	    if((WIFEXITED(status) && WEXITSTATUS(status) != 0)
		    || WIFSIGNALED(status))
		exit(1);
	}

	pid = fork();
	if(pid == -1)
            sysdie("cannot fork");
	if(pid > 0) {
	    /* parent */
	    first = 0;
	    free(OverTmpPath);
	    OverTmpPath = NULL;
	    return;
	}

	/* child */
	/* init the overview setup. */
	if (!OVopen(OV_WRITE)) {
            warn("cannot open overview");
	    _exit(1);
	}
	if (!OVctl(OVSORT, (void *)&sorttype)) {
            warn("cannot obtain overview sorting information");
	    OVclose();
	    _exit(1);
	}
	if (!OVctl(OVCUTOFFLOW, (void *)&Cutofflow)) {
            warn("cannot obtain overview cutoff information");
	    OVclose();
	    _exit(1);
	}
    }

    /* This is a bit odd, but as long as other user's files can't be deleted
       out of the temporary directory, it should work.  We're using mkstemp to
       create a file and then passing its name to sort, which will then open
       it again and overwrite it. */
    SortedTmpPath = concatpath(TmpDir, "hisTXXXXXX");
    fd = mkstemp(SortedTmpPath);
    if (fd < 0) {
        syswarn("cannot create temporary file");
        OVclose();
        Fork ? _exit(1) : exit(1);
    }
    close(fd);
    snprintf(temp, sizeof(temp), "exec %s -T %s -t'%c' -o %s %s",
             INN_PATH_SORT, TmpDir, '\t', SortedTmpPath, OverTmpPath);
    
    i = system(temp) >> 8;
    if (i != 0) {
        syswarn("cannot sort temporary overview file (%s exited %d)",
                INN_PATH_SORT, i);
	OVclose();
	Fork ? _exit(1) : exit(1);
    }

    /* don't need old path anymore. */
    unlink(OverTmpPath);
    free(OverTmpPath);
    OverTmpPath = NULL;

    /* read sorted lines. */
    if ((qp = QIOopen(SortedTmpPath)) == NULL) {
        syswarn("cannot open sorted overview file %s", SortedTmpPath);
	OVclose();
	Fork ? _exit(1) : exit(1);
    }

    for (count = 1; ; ++count) {
	line = QIOread(qp);
	if (line == NULL) {
	    if (QIOtoolong(qp)) {
                warn("overview line %d is too long", count);
		continue;
	    } else
		break;
	}
	if ((p = strchr(line, '\t')) == NULL 
	    || (q = strchr(p+1, '\t')) == NULL
	    || (r = strchr(q+1, '\t')) == NULL) {
            warn("sorted overview file %s has a bad line at %d",
                 SortedTmpPath, count);
	    continue;
	}
	/* p+1 now points to start of token, q+1 points to start of overline. */
	if (sorttype == OVNEWSGROUP) {
	    *p++ = '\0';
	    *q++ = '\0';
	    *r++ = '\0';
	    arrived = (time_t)atol(p);
	    expires = (time_t)atol(q);
	    q = r;
	    if ((r = strchr(r, '\t')) == NULL) {
                warn("sorted overview file %s has a bad line at %d",
                     SortedTmpPath, count);
		continue;
	    }
	    *r++ = '\0';
	} else {
	    *p++ = '\0';
	    *q++ = '\0';
	    *r++ = '\0';
	    arrived = (time_t)atol(line);
	    expires = (time_t)atol(p);
	}
	token = TextToToken(q);
	if (OVadd(token, r, strlen(r), arrived, expires) == OVADDFAILED) {
	    if (OVctl(OVSPACE, (void *)&i) && i == OV_NOSPACE) {
                warn("no space left for overview");
		OVclose();
		Fork ? _exit(1) : exit(1);
	    }
            warn("cannot write overview data \"%.40s\"", q);
	}
    }
    /* Check for errors and close. */
    if (QIOerror(qp)) {
        syswarn("cannot read sorted overview file %s", SortedTmpPath);
	OVclose();
	Fork ? _exit(1) : exit(1);
    }
    QIOclose(qp);
    /* unlink sorted tmp file */
    unlink(SortedTmpPath);
    free(SortedTmpPath);
    if(Fork) {
	OVclose();
	_exit(0);
    }
}
	    

/*
**  Write a line to the overview temp file, standard output, our child process
**  that's doing the writing, or directly to overview, whichever is
**  appropriate.
*/
static void
WriteOverLine(TOKEN *token, const char *xrefs, int xrefslen, 
	      char *overdata, int overlen, time_t arrived, time_t expires)
{
    char temp[SMBUF];
    const char *p, *q, *r;
    int i, fd;

    /* If WriteStdout is set, just print the overview information to standard
       output and return. */
    if (WriteStdout) {
        printf("%s %ld %ld ", TokenToText(*token), (long) arrived,
               (long) expires);
        fwrite(overdata, 1, overlen, stdout);
        fputc('\n', stdout);
        return;
    }

    /* If the overview method doesn't care about sorted data, don't bother
       with the temporary file and just write the data out to our child
       process or to the overview database directly. */
    if (sorttype == OVNOSORT) {
	if (Fork) {
	    fprintf(Overchan, "%s %ld %ld ", TokenToText(*token), (long)arrived, (long)expires);
	    if (fwrite(overdata, 1, overlen, Overchan) != (size_t) overlen)
                sysdie("writing overview failed");
	    fputc('\n', Overchan);
	} else if (OVadd(*token, overdata, overlen, arrived, expires) == OVADDFAILED) {
	    if (OVctl(OVSPACE, (void *)&i) && i == OV_NOSPACE) {
                warn("no space left for overview");
		OVclose();
		exit(1);
	    }
            warn("cannot write overview data for article %s",
                 TokenToText(*token));
	}
	return;
    }

    /* Otherwise, we need the temporary file.  Create it if it doesn't already
       exist. */
    if (OverTmpPath == NULL) {
	/* need new temp file, so create it. */
        OverTmpPath = concatpath(TmpDir, "histXXXXXX");
        fd = mkstemp(OverTmpPath);
        if (fd < 0)
            sysdie("cannot create temporary file");
        OverTmpFile = fdopen(fd, "w");
	if (OverTmpFile == NULL)
            sysdie("cannot open %s", OverTmpPath);
	OverTmpSegCount = 0;
    }

    /* Print out the data to the teporary file with the appropriate keys for
       sorting. */
    if (sorttype == OVNEWSGROUP) {
	/* find first ng name in xref. */
	for (p = xrefs, q=NULL ; p < xrefs+xrefslen ; ++p) {
	    if (*p == ' ') {
		q = p+1; /* found space */
		break;
	    }
	}
	if (!q) {
            warn("bogus Xref data for %s", TokenToText(*token));
	    /* XXX do nuke here? */
	    return;
	}

	for (p = q, r=NULL ; p < xrefs+xrefslen ; ++p) {
	    if (*p == ':') {
		r=p;
		break;
	    }
	}
	if (!r) {
            warn("bogus Xref data for %s", TokenToText(*token));
	    /* XXX do nuke here? */
	    return;
	}
	/* q points to start of ng name, r points to its end. */
        assert((ptrdiff_t) sizeof(temp) > r - q + 1);
	memcpy(temp, q, r - q + 1);
        temp[r - q + 1] = '\0';
	fprintf(OverTmpFile, "%s\t%10lu\t%lu\t%s\t", temp,
                (unsigned long) arrived, (unsigned long) expires,
                TokenToText(*token));
    } else
	fprintf(OverTmpFile, "%10lu\t%lu\t%s\t", (unsigned long) arrived,
                (unsigned long) expires,
                TokenToText(*token));

    fwrite(overdata, overlen, 1, OverTmpFile);
    fprintf(OverTmpFile, "\n");
    OverTmpSegCount++;

    if (OverTmpSegSize != 0 && OverTmpSegCount >= OverTmpSegSize) {
	FlushOverTmpFile();
    }
}


/*
**  Read the overview schema.
*/
static void
ARTreadschema(bool Overview)
{
    const struct cvector        *standardoverview;
    const struct vector         *extraoverview;
    ARTOVERFIELD                *fp;
    unsigned int                i;

    if (Overview) {
	/* Count the number of overview fields and allocate ARTfields. */
        standardoverview = overview_fields();
        extraoverview = overview_extra_fields(true);
	ARTfields = xmalloc((standardoverview->count + extraoverview->count + 1)
                            * sizeof(ARTOVERFIELD));

	/* Parse each field. */
        for (i = 0, fp = ARTfields; i < standardoverview->count; i++) {
            fp->NeedHeadername = false;
	    fp->Headername = xstrdup(standardoverview->strings[i]);
	    fp->HeadernameLength = strlen(standardoverview->strings[i]);
	    fp->Header = (char *)NULL;
	    fp->HasHeader = false;
	    fp->HeaderLength = 0;
            /* We need to keep the value of fp for :bytes and :lines
             * because we will need them afterwards; we will then not
             * have to compare fp->Headername to both "Bytes" and "Lines"
             * for each fp. */
            if (strncasecmp(standardoverview->strings[i],
                            BYTES, strlen(BYTES)) == 0)
                Bytesp = fp;
            if (strncasecmp(standardoverview->strings[i],
                            DATE, strlen(DATE)) == 0)
                Datep = fp;
            if (strncasecmp(standardoverview->strings[i],
                            LINES, strlen(LINES)) == 0)
                Linesp = fp;
            if (strncasecmp(standardoverview->strings[i],
                            MESSAGEID, strlen(MESSAGEID)) == 0)
                Msgidp = fp;
	    fp++;
	}
        for (i = 0; i < extraoverview->count; i++) {
            fp->NeedHeadername = true;
            fp->Headername = xstrdup(extraoverview->strings[i]);
            fp->HeadernameLength = strlen(extraoverview->strings[i]);
            fp->Header = (char *)NULL;
            fp->HasHeader = false;
            fp->HeaderLength = 0;
            if (strncasecmp(extraoverview->strings[i],
                            XREF, strlen(XREF)) == 0)
                Xrefp = fp;
            if (strncasecmp(extraoverview->strings[i],
                            EXPIRES, strlen(EXPIRES)) == 0)
                Expp = fp;
            fp++;
        }

	ARTfieldsize = fp - ARTfields;
    }
    if (Bytesp == (ARTOVERFIELD *)NULL)
        Missfieldsize++;
    if (Datep == (ARTOVERFIELD *)NULL)
        Missfieldsize++;
    if (Expp == (ARTOVERFIELD *)NULL)
        Missfieldsize++;
    if (Linesp == (ARTOVERFIELD *)NULL)
        Missfieldsize++;
    if (Msgidp == (ARTOVERFIELD *)NULL)
	Missfieldsize++;
    if (Missfieldsize > 0) {
	Missfields = xmalloc(Missfieldsize * sizeof(ARTOVERFIELD));
        fp = Missfields;
        if (Bytesp == (ARTOVERFIELD *)NULL) {
            fp->NeedHeadername = false;
            fp->Headername = xstrdup(BYTES);
            fp->HeadernameLength = strlen(BYTES);
            fp->Header = (char *)NULL;
            fp->HasHeader = false;
            fp->HeaderLength = 0;
            Bytesp = fp++;
        }
        if (Datep == (ARTOVERFIELD *)NULL) {
            fp->NeedHeadername = false;
            fp->Headername = xstrdup(DATE);
            fp->HeadernameLength = strlen(DATE);
            fp->Header = (char *)NULL;
            fp->HasHeader = false;
            fp->HeaderLength = 0;
            Datep = fp++;
        }
        if (Expp == (ARTOVERFIELD *)NULL) {
            fp->NeedHeadername = false;
            fp->Headername = xstrdup(EXPIRES);
            fp->HeadernameLength = strlen(EXPIRES);
            fp->Header = (char *)NULL;
            fp->HasHeader = false;
            fp->HeaderLength = 0;
            Expp = fp++;
        }
        if (Linesp == (ARTOVERFIELD *)NULL) {
            fp->NeedHeadername = false;
            fp->Headername = xstrdup(LINES);
            fp->HeadernameLength = strlen(LINES);
            fp->Header = (char *)NULL;
            fp->HasHeader = false;
            fp->HeaderLength = 0;
            Linesp = fp++;
        }
        if (Msgidp == (ARTOVERFIELD *)NULL) {
            fp->NeedHeadername = false;
            fp->Headername = xstrdup(MESSAGEID);
            fp->HeadernameLength = strlen(MESSAGEID);
            fp->Header = (char *)NULL;
            fp->HasHeader = false;
            fp->HeaderLength = 0;
            Msgidp = fp++;
        }
        if (Overview && Xrefp == (ARTOVERFIELD *)NULL) {
	    fp->NeedHeadername = false;
	    fp->Headername = xstrdup(XREF);
	    fp->HeadernameLength = strlen(XREF);
	    fp->Header = (char *)NULL;
	    fp->HasHeader = false;
	    fp->HeaderLength = 0;
	    Xrefp = fp++;
	}
    }
}

/*
 * Handle a single article.  This routine's fairly complicated. 
 */
static void
DoArt(ARTHANDLE *art)
{
    ARTOVERFIELD		*fp;
    const char                  *p, *end;
    char                        *q;
    static struct buffer        buffer = { 0, 0, 0, NULL };
    static char			SEP[] = "\t";
    static char			NUL[] = "\0";
    static char			COLONSPACE[] = ": ";
    size_t			i, j, len;
    const char			*MessageID;
    time_t			Arrived;
    time_t			Expires;
    time_t			Posted;
    char			overdata[BIG_BUFFER];
    char                        Bytes[BIG_BUFFER];
    char                        Lines[BIG_BUFFER];
    struct artngnum		ann;
    int                         DotStuffedLines = 0;
    int                         lines = 0;
    bool                        hasCounts = false;

    /* Set up place to store headers. */
    for (fp = ARTfields, i = 0; i < ARTfieldsize; i++, fp++) {
	if (fp->HeaderLength) {
	    fp->Header = 0;
	}
	fp->HeaderLength = 0;
	fp->HasHeader = false;
    }
    if (Missfieldsize > 0) {
	for (fp = Missfields, i = 0; i < Missfieldsize; i++, fp++) {
	    if (fp->HeaderLength) {
		fp->Header = 0;
	    }
	    fp->HeaderLength = 0;
	    fp->HasHeader = false;
	}
    }
    for (fp = ARTfields, i = 0; i < ARTfieldsize; i++, fp++) {
        fp->Header = wire_findheader(art->data, art->len, fp->Headername);

        /* Someone managed to break their server so that they were appending
           multiple Xref headers, and INN had a bug where it wouldn't notice
           this and reject the article.  Just in case, see if there are
           multiple Xref headers and use the last one. */
        if (fp == Xrefp) {
            const char *next = fp->Header;
            size_t left;

            while (next != NULL) {
                next = wire_endheader(fp->Header, art->data + art->len - 1);
                if (next == NULL)
                    break;
                next++;
                left = art->len - (next - art->data);
                next = wire_findheader(next, left, fp->Headername);
                if (next != NULL)
                    fp->Header = next;
            }
        }

        /* Work out the real values for :bytes and :lines. */
        if (fp == Bytesp || fp == Linesp) {
            /* Parse the article only once. */
            if (!hasCounts && (p = wire_findbody(art->data, art->len)) != NULL) {
                end = art->data + art->len;
                /* p is at the beginning of the body, if any. */
                for (; *p != '\0';) {
                    /* p is at the beginning of a new line, if any. */
                    lines++;
                    if (*p == '.')
                        DotStuffedLines++;
                    if ((p = wire_nextline(p, end)) == NULL) {
                        /* p is at the final ".\r\n". */
                        lines--;
                        DotStuffedLines--;
                        break;
                    }
                }
            }

            if (fp == Bytesp) {
                /* :bytes does not count the final ".\r\n", so remove 3. */
                snprintf(Bytes, sizeof(Bytes), "%lu",
                         (unsigned long) art->len - DotStuffedLines - 3);
                fp->Header = Bytes;
                fp->HeaderLength = strlen(Bytes);
            } else {
                snprintf(Lines, sizeof(Lines), "%lu", (unsigned long) lines);
                fp->Header = Lines;
                fp->HeaderLength = strlen(Lines);
            }
            fp->HasHeader = true;
            hasCounts = true;
            continue;
        }

        /* Now, if we have a header, find and record its length. */
        if (fp->Header != NULL) {
	    fp->HasHeader = true;
            p = wire_endheader(fp->Header, art->data + art->len - 1);
            if (p == NULL)
		continue;

            /* The true length of the header is p - fp->Header + 1, but p
               points to the \n at the end of the header, so subtract 2 to
               peel off the \r\n (we're guaranteed we're dealing with
               wire-format articles. */
            fp->HeaderLength = p - fp->Header - 1;
	}

    }
    if (Missfieldsize > 0) {
	for (fp = Missfields, i = 0; i < Missfieldsize; i++, fp++) {
            fp->Header = wire_findheader(art->data, art->len, fp->Headername);
            if (fp->Header != NULL) {
		fp->HasHeader = true;
                p = wire_endheader(fp->Header, art->data + art->len - 1);
                if (p == NULL)
                    continue;
		fp->HeaderLength = p - fp->Header - 1;
	    }
	}
    }
    if (DoOverview && Xrefp->HeaderLength == 0) {
	if (!SMprobe(SMARTNGNUM, art->token, (void *)&ann)) {
	    Xrefp->Header = NULL;
	    Xrefp->HeaderLength = 0;
	} else {
            if (ann.artnum == 0 || ann.groupname == NULL)
                return;
            len = strlen(innconf->pathhost) + 1 + strlen(ann.groupname) + 1
                + 16 + 1;
            if (len > BIG_BUFFER) {
                Xrefp->Header = NULL;
                Xrefp->HeaderLength = 0;
            } else {
                snprintf(overdata, sizeof(overdata), "%s %s:%lu",
                         innconf->pathhost, ann.groupname, ann.artnum);
                Xrefp->Header = overdata;
                Xrefp->HeaderLength = strlen(overdata);
            }
            if (ann.groupname != NULL)
                free(ann.groupname);
        }
    }

    MessageID = (char *)NULL;
    Arrived = art->arrived;
    Expires = 0;
    Posted = 0;

    if (!Msgidp->HasHeader) {
        warn("no Message-ID header in %s", TokenToText(*art->token));
	if (NukeBadArts)
	    SMcancel(*art->token);
	return;
    }

    buffer_set(&buffer, Msgidp->Header, Msgidp->HeaderLength);
    buffer_append(&buffer, NUL, 1);
    for (i = 0, q = buffer.data; i < buffer.left; q++, i++)
	if (*q == '\t' || *q == '\n' || *q == '\r')
	    *q = ' ';
    MessageID = GetMessageID(buffer.data);
    if (*MessageID == '\0') {
        warn("no Message-ID header in %s", TokenToText(*art->token));
	if (NukeBadArts)
	    SMcancel(*art->token);
	return;
    }

    /*
     * check if msgid is in history if in update mode, or if article is 
     * newer than start time of makehistory. 
     */

    if (!Datep->HasHeader) {
	Posted = Arrived;
    } else {
        buffer_set(&buffer, Datep->Header, Datep->HeaderLength);
        buffer_append(&buffer, NUL, 1);
        Posted = parsedate_rfc2822_lax(buffer.data);
        if (Posted == (time_t) -1)
	    Posted = Arrived;
    }

    if (Expp->HasHeader) {
        buffer_set(&buffer, Expp->Header, Expp->HeaderLength);
        buffer_append(&buffer, NUL, 1);
        Expires = parsedate_rfc2822_lax(buffer.data);
        if (Expires == (time_t) -1)
            Expires = 0;
    }

    if (DoOverview && Xrefp->HeaderLength > 0) {
	for (fp = ARTfields, j = 0; j < ARTfieldsize; j++, fp++) {
	    if (fp == ARTfields)
                buffer_set(&buffer, "", 0);
	    else
                buffer_append(&buffer, SEP, strlen(SEP));
            if (fp->HeaderLength == 0)
                continue;
	    if (fp->NeedHeadername) {
                buffer_append(&buffer, fp->Headername, fp->HeadernameLength);
                buffer_append(&buffer, COLONSPACE, strlen(COLONSPACE));
	    }
	    i = buffer.left;
            buffer_resize(&buffer, buffer.left + fp->HeaderLength);
            end = fp->Header + fp->HeaderLength - 1;
            for (p = fp->Header, q = &buffer.data[i]; p <= end; p++) {
                if (*p == '\r' && p < end && p[1] == '\n') {
                    p++;
                    continue;
                }
                if (*p == '\0' || *p == '\t' || *p == '\n' || *p == '\r')
                    *q++ = ' ';
                else
                    *q++ = *p;
                buffer.left++;
            }
	}
	WriteOverLine(art->token, Xrefp->Header, Xrefp->HeaderLength,
		      buffer.data, buffer.left, Arrived, Expires);
    }

    if (!NoHistory) {
	bool r;

	r = HISwrite(History, MessageID,
		     Arrived, Posted, Expires, art->token);
	if (r == false)
            sysdie("cannot write history line");
    }
}


/*
** Add all groups to overview group.index. --rmt
*/
static void
OverAddAllNewsgroups(void)
{
    QIOSTATE *qp;
    int count;
    char *q,*p;
    char *line;
    ARTNUM hi, lo;

    if ((qp = QIOopen(ActivePath)) == NULL)
        sysdie("cannot open %s", ActivePath);
    for (count = 1; (line = QIOread(qp)) != NULL; count++) {
	if ((p = strchr(line, ' ')) == NULL) {
            warn("bad active line %d: %.40s", count, line);
	    continue;
	}
	*p++ = '\0';
	hi = (ARTNUM)atol(p);
	if ((p = strchr(p, ' ')) == NULL) {
            warn("bad active line %d: %.40s", count, line);
	    continue;
	}
	*p++ = '\0';
	lo = (ARTNUM)atol(p);
	if ((q = strrchr(p, ' ')) == NULL) {
            warn("bad active line %d: %.40s", count, line);
	    continue;
	}
	/* q+1 points to NG flag */
	if (!OVgroupadd(line, lo, hi, q+1))
            die("cannot add %s to overview group index", line);
    }
    /* Test error conditions; QIOtoolong shouldn't happen. */
    if (QIOtoolong(qp))
        die("active file line %d is too long", count);
    if (QIOerror(qp))
        sysdie("cannot read %s around line %d", ActivePath, count);
    QIOclose(qp);
}


int
main(int argc, char **argv)
{
    ARTHANDLE *art = NULL;
    bool AppendMode;
    int LoadAverage;
    double load[1];
    int i;
    bool val;
    char *HistoryDir;
    char *RebuiltflagPath;
    char *p;
    char *buff;
    size_t npairs = 0;
    FILE *F;

    /* First thing, set up logging and our identity. */
    openlog("makehistory", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);
    message_program_name = "makehistory";
	
    /* Set defaults. */
    if (!innconf_read(NULL))
        exit(1);
    HistoryPath = concatpath(innconf->pathdb, INN_PATH_HISTORY);
    ActivePath = concatpath(innconf->pathdb, INN_PATH_ACTIVE);
    TmpDir = innconf->pathtmp;
    RebuiltflagPath = concatpath(innconf->pathrun, INN_PATH_REBUILDOVERVIEW);

    OverTmpSegSize = DEFAULT_SEGSIZE;
    OverTmpSegCount = 0;
    NukeBadArts = false;
    DoOverview = false;
    Fork = false;
    AppendMode = false;
    LoadAverage = 0;
    NoHistory = false;

    while ((i = getopt(argc, argv, "abFf:Il:L:OSs:T:x")) != EOF) {
	switch(i) {
	case 'a':
	    AppendMode = true;
	    break;
	case 'b':
	    NukeBadArts = true;
	    break;
	case 'F':
	    Fork = true;
	    break;
	case 'f':
	    HistoryPath = optarg;
	    break;
	case 'I':
	    Cutofflow = true;
	    break;
	case 'l':
	    OverTmpSegSize = atoi(optarg);
	    break;
        case 'L':
            LoadAverage = atoi(optarg);
            break;
	case 'O':
	    DoOverview = true;
	    break;
        case 'S':
            WriteStdout = true;
            OverTmpSegSize = 0;
            break;
	case 's':
	    npairs = atoi(optarg);
	    break;
	case 'T':
	    TmpDir = optarg;
	    break;
	case 'x':
	    NoHistory = true;
	    break;
	    
	default:
	    fprintf(stderr, "%s", usage);
            exit(1);
	    break;
	}
    }
    argc -= optind;
    argv += optind;
    if (argc) {
        fprintf(stderr, "%s", usage);
        exit(1);
    }

    if (!NoHistory) {
        if ((p = strrchr(HistoryPath, '/')) == NULL) {
            /* find the default history file directory */
            HistoryDir = innconf->pathdb;
        } else {
            *p = '\0';
            HistoryDir = xstrdup(HistoryPath);
            *p = '/';
        }
        if (chdir(HistoryDir) < 0)
            sysdie("cannot chdir to %s", HistoryDir);
    }

    /* Change to the runasuser user and runasgroup group if necessary. */
    if (!NoHistory || !WriteStdout) {
        if (getenv("INN_TESTSUITE") == NULL)
            ensure_news_user_grp(true, true);
    }

    /* Read in the overview schema. */
    ARTreadschema(DoOverview);
    
    if (DoOverview && !WriteStdout) {
	/* init the overview setup. */
	if (!OVopen(OV_WRITE))
            sysdie("cannot open overview");
	if (!OVctl(OVSORT, (void *)&sorttype))
            die("cannot obtain overview sort information");
	if (!Fork) {
	    if (!OVctl(OVCUTOFFLOW, (void *)&Cutofflow))
                die("cannot obtain overview cutoff information");
	    OverAddAllNewsgroups();
	} else {
	    OverAddAllNewsgroups();
	    if (sorttype == OVNOSORT) {
		buff = concat(innconf->pathbin, "/", "overchan", NULL);
		if ((Overchan = popen(buff, "w")) == NULL)
                    sysdie("cannot fork overchan process");
		free(buff);
	    }
	    OVclose();
	}
    }

    /* Init the Storage Manager */
    val = true;
    if (!SMsetup(SM_RDWR, (void *)&val) || !SMsetup(SM_PREOPEN, (void *)&val))
        sysdie("cannot set up storage manager");
    if (!SMinit())
        sysdie("cannot initialize storage manager: %s", SMerrorstr);

    /* Initialise the history manager */
    if (!NoHistory) {
	int flags = HIS_RDWR | HIS_INCORE;

	if (!AppendMode)
	    flags |= HIS_CREAT;
	History = HISopen(NULL, innconf->hismethod, flags);
	if (History == NULL)
            sysdie("cannot create history handle");
	HISctl(History, HISCTLS_NPAIRS, &npairs);
	if (!HISctl(History, HISCTLS_PATH, HistoryPath))
            sysdie("cannot open %s", HistoryPath);
    }

    /*
     * Scan the entire spool, nuke any bad arts if needed, and process each
     * article.  We take a break when the load is too high.
     */
	
    while ((art = SMnext(art, RETR_ALL)) != NULL) {
	if (art->len == 0) {
	    if (NukeBadArts && art->data == NULL && art->token != NULL)
		SMcancel(*art->token);
	    continue;
	}

	DoArt(art);

        if (LoadAverage > 0) {
            while (getloadavg(load, 1) > 0 &&
                   (int) (load[0]) >= LoadAverage) {
                sleep(1);
            }
        }
    }

    if (!NoHistory) {
	/* close history file. */
	if (!HISclose(History))
            sysdie("cannot close history file");
    }

    if (DoOverview) {
	if (sorttype == OVNOSORT && Fork)
	    if (fflush(Overchan) == EOF || ferror(Overchan) || pclose(Overchan) == EOF)
                sysdie("cannot flush overview data");
	if (sorttype != OVNOSORT && !WriteStdout) {
	    int status;
	    FlushOverTmpFile();
	    if(Fork)
		wait(&status);
	}
        if (!WriteStdout) {
            if ((F = fopen(RebuiltflagPath, "w")) == NULL)
                sysdie("cannot open %s", RebuiltflagPath);
            if (fprintf(F, "%ld\n", (long) time (NULL)) == EOF || ferror(F))
                sysdie("cannot write rebuilt flag file");
            fclose(F);
        }
    }
    if (!Fork && !WriteStdout)
	OVclose();
    exit(0);
}

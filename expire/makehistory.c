/*  $Id$
**
**  Rebuild history/overview databases.
*/
#include "config.h"
#include "clibrary.h"
#include <errno.h>
#include <syslog.h>  

#include "dbz.h"
#include "libinn.h"
#include "macros.h"
#include "ov.h"
#include "paths.h"
#include "qio.h"
#include "storage.h"

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifndef WEXITSTATUS
# define WEXITSTATUS(status)    ((((unsigned)(status)) >> 8) & 0xFF)
#endif
#ifndef WIFEXITED
# define WIFEXITED(status)      ((((unsigned)(status)) & 0xFF) == 0)
#endif
#ifndef WIFSIGNALED
# define WIFSIGNALED(status)    ((((unsigned)(status)) & 0xFF) > 0 \
                                 && (((unsigned)(status)) & 0xFF00) == 0)
#endif


/*
**  Information about the schema of the news overview files.
*/
typedef struct _ARTOVERFIELD {
    char	*Headername;
    int		HeadernameLength;
    BOOL	NeedHeadername;
    char	*Header;
    int		HeaderLength;
    BOOL	HasHeader;
} ARTOVERFIELD;

#define DEFAULT_SEGSIZE	10000;

BOOL NukeBadArts;
char *SchemaPath = NULL;
char *ActivePath = NULL;
char *HistoryPath = NULL;
FILE *HistFile;
FILE *Overchan;
BOOL DoOverview;
BOOL Fork;
BOOL Cutofflow = FALSE;
char *TmpDir;
int OverTmpSegSize, OverTmpSegCount;
FILE *OverTmpFile;
char *OverTmpPath = NULL;
BOOL NoHistory;
OVSORTTYPE sorttype;
int RetrMode;

TIMEINFO Now;

/* Misc variables needed for the overview creation code. */
STATIC char		MESSAGEID[] = "Message-ID:";
STATIC char		EXPIRES[] = "Expires:";
STATIC char		DATE[] = "Date:";
STATIC char		XREF[] = "Xref:";
STATIC ARTOVERFIELD	*ARTfields; /* overview fields listed in overview.fmt */
STATIC int		ARTfieldsize;
STATIC ARTOVERFIELD	*Datep = (ARTOVERFIELD *)NULL;
STATIC ARTOVERFIELD	*Msgidp = (ARTOVERFIELD *)NULL;
STATIC ARTOVERFIELD	*Expp = (ARTOVERFIELD *)NULL;
STATIC ARTOVERFIELD	*Xrefp = (ARTOVERFIELD *)NULL;
STATIC ARTOVERFIELD	*Missfields; /* header fields not listed in 
					overview.fmt, but ones that we need
					(e.g. message-id */
STATIC int		Missfieldsize = 0;

typedef struct _BUFFER {
    long	Size;
    long	Used;
    long	Left;
    char	*Data;
} BUFFER;

void OverAddAllNewsgroups(void);

/*
 * Misc routines needed by DoArt...
 */

void BUFFset(BUFFER *bp, const char *p, const int length)
{
    if ((bp->Left = length) != 0) {
	/* Need more space? */
	if (bp->Size < length) {
	    bp->Size = length;
	    RENEW(bp->Data, char, bp->Size);
	}

	/* Try to test for non-overlapping copies. */
	memmove((POINTER)bp->Data, (POINTER)p, (SIZE_T)length);
    }
    bp->Used = 0;
}

void BUFFappend(BUFFER *bp, const char *p, const int len) {
    int i;

    if (len == 0)
	return;
    /* Note end of buffer, grow it if we need more room */
    i = bp->Used + bp->Left;
    if (i + len > bp->Size) {
	/* Round size up to next 1K */
	bp-> Size += (len + 0x3FF) & ~0x3FF;
	RENEW(bp->Data, char, bp->Size);
    }
    bp->Left += len;
    memcpy((POINTER)&bp->Data[i], (POINTER)p, len);
}

/*
**  Check and parse an date header line.  Return the new value or
**  zero on error.
*/
static long
GetaDate(char *p)
{
    time_t		t;

    while (ISWHITE(*p))
	p++;
    if ((t = parsedate(p, &Now)) == -1)
	return 0L;
    return (long)t;
}

/*
**  Check and parse a Message-ID header line.  Return private space.
*/
static char *
GetMessageID(char *p)
{
    static BUFFER	B;
    int			length;
    static char		NIL[] = "";

    while (ISWHITE(*p))
	p++;
    if (p[0] != '<' || p[strlen(p) - 1] != '>')
	return NIL;

    /* Copy into re-used memory space. */
    length = strlen(p);
    if (B.Size == 0) {
	B.Size = length;
	B.Data = NEW(char, B.Size + 1);
    }
    else if (B.Size < length) {
	B.Size = length;
	RENEW(B.Data, char, B.Size + 1);
    }
    (void)memcpy((POINTER)B.Data, (POINTER)p, (SIZE_T)length + 1);

    for (p = B.Data; *p; p++)
	if (*p == HIS_FIELDSEP)
	    *p = HIS_BADCHAR;
    return B.Data;
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

void
FlushOverTmpFile(void)
{
    char temp[SMBUF];
    char *SortedTmpPath;
    int i, pid;
    TOKEN token;
    QIOSTATE *qp;
    int count;
    char *line, *p, *q, *r;
    time_t arrived, expires;
    static int first = 1;

    if (OverTmpFile == NULL)
	return;
    if (fflush(OverTmpFile) == EOF || ferror(OverTmpFile) || fclose(OverTmpFile) == EOF) {
	(void)fprintf(stderr, "Can't close OverTmp file, %s\n",	strerror(errno));
	exit(1);
    }
    if(Fork) {
        if(!first) { /* if previous one is running, wait for it */
	    int status;
	    wait(&status);
	    if(WIFEXITED(status) && WEXITSTATUS(status) != 0
		    || WIFSIGNALED(status))
		exit(1);
	}

	pid = fork();
	if(pid == -1) {
	    fprintf(stderr, "makehistory: fork(): %s\n", strerror(errno));
	    exit(1);
	}
	if(pid > 0) {
	    /* parent */
	    first = 0;
	    DISPOSE(OverTmpPath);
	    OverTmpPath = NULL;
	    return;
	}

	/* child */
	/* init the overview setup. */
	if (!OVopen(OV_WRITE)) {
	    fprintf(stderr, "makehistory: OVopen failed\n");
	    _exit(1);
	}
	if (!OVctl(OVSORT, (void *)&sorttype)) {
	    fprintf(stderr, "makehistory: OVctl(OVSORT) failed\n");
	    OVclose();
	    _exit(1);
	}
	if (!OVctl(OVCUTOFFLOW, (void *)&Cutofflow)) {
	    fprintf(stderr, "makehistory: OVctl(OVCUTOFFLOW) failed\n");
	    OVclose();
	    _exit(1);
	}
    }

    sprintf(temp, "%s/hisTXXXXXX", TmpDir);
    mktemp(temp);
    SortedTmpPath = COPY(temp);

    sprintf(temp, "exec %s -T %s -t'%c' -o %s %s", _PATH_SORT,
	    TmpDir, '\t', SortedTmpPath, OverTmpPath);
    
    i = system(temp) >> 8;
    if (i != 0) {
	fprintf(stderr,
                "makehistory: Can't sort OverTmp file (%s exit %d), %s\n",
		_PATH_SORT, i, strerror(errno));
	OVclose();
	Fork ? _exit(1) : exit(1);
    }

    /* don't need old path anymore. */
    unlink(OverTmpPath);
    DISPOSE(OverTmpPath);
    OverTmpPath = NULL;

    /* read sorted lines. */
    if ((qp = QIOopen(SortedTmpPath)) == NULL) {
	fprintf(stderr, "makehistory: Can't open sorted over file %s, %s\n",
		SortedTmpPath, strerror(errno));
	OVclose();
	Fork ? _exit(1) : exit(1);
    }

    for (count = 1; (line = QIOread(qp)) != NULL ; ++count) {
	if ((p = strchr(line, '\t')) == NULL 
	    || (q = strchr(p+1, '\t')) == NULL
	    || (r = strchr(q+1, '\t')) == NULL) {
	    fprintf(stderr, "makehistory: sorted over %s has bad line %d\n",
		    SortedTmpPath, count);
	    OVclose();
	    Fork ? _exit(1) : exit(1);
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
	        fprintf(stderr, "makehistory: sorted over %s has bad line %d\n",
		    SortedTmpPath, count);
		OVclose();
		Fork ? _exit(1) : exit(1);
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
	if (!OVadd(token, r, strlen(r), arrived, expires)) {
	    if (OVctl(OVSPACE, (void *)&i) && i == OV_NOSPACE) {
		fprintf(stderr, "makehistory: no space left for overview\n");
		OVclose();
		Fork ? _exit(1) : exit(1);
	    }
	    fprintf(stderr, "makehistory: Can't write overview data \"%s\"\n", q);
	}
    }
    /* Check for errors and close. */
    if (QIOtoolong(qp)) {
	fprintf(stderr, "makehistory: Line %d is too long\n", count);
	OVclose();
	Fork ? _exit(1) : exit(1);
    }
    if (QIOerror(qp)) {
	(void)fprintf(stderr, "makehistory: Can't read sorted tmp file %s, %s\n", 
		      SortedTmpPath, strerror(errno));
	OVclose();
	Fork ? _exit(1) : exit(1);
    }
    QIOclose(qp);
    /* unlink sorted tmp file */
    unlink(SortedTmpPath);
    DISPOSE(SortedTmpPath);
    if(Fork) {
	OVclose();
	_exit(0);
    }
}
	    

/*
 * Write a line to the overview temp file. 
 */
void
WriteOverLine(TOKEN *token, char *xrefs, int xrefslen, 
	      char *overdata, int overlen, time_t arrived, time_t expires)
{
    char temp[SMBUF];
    char *p, *q, *r;
    int i;

    if (sorttype == OVNOSORT) {
	if (Fork) {
	    (void)fprintf(Overchan, "%s %ld %ld ", TokenToText(*token), arrived, expires);
	    if (fwrite((POINTER)overdata, (SIZE_T)1, (SIZE_T)overlen, Overchan) != overlen) {
		fprintf(stderr, "makehistory: writing overview failed\n");
		exit(1);
	    }
	    fputc('\n', Overchan);
	} else if (!OVadd(*token, overdata, overlen, arrived, expires)) {
	    if (OVctl(OVSPACE, (void *)&i) && i == OV_NOSPACE) {
		fprintf(stderr, "makehistory: no space left for overview\n");
		OVclose();
		exit(1);
	    }
	    fprintf(stderr, "makehistory: Can't write overview data for article \"%s\"\n", TokenToText(*token));
	}
	return;
    }
    if (OverTmpPath == NULL) {
	/* need new temp file, so create it. */
	(void)sprintf(temp, "%s/histXXXXXX", TmpDir);
	(void)mktemp(temp);
	OverTmpPath = COPY(temp);
	if ((OverTmpFile = fopen(OverTmpPath, "w")) == NULL) {
	    fprintf(stderr, "makehistory: can't open %s:%s\n", OverTmpPath,
			  strerror(errno));
	    exit(1);
	}
	OverTmpSegCount = 0;
    }
    if (sorttype == OVNEWSGROUP) {
	/* find first ng name in xref. */
	for (p = xrefs, q=NULL ; p < xrefs+xrefslen ; ++p) {
	    if (*p == ' ') {
		q = p+1; /* found space */
		break;
	    }
	}
	if (!q) {
	    fprintf(stderr, "makehistory: bogus xref data for %s\n",TokenToText(*token));
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
	    fprintf(stderr, "makehistory: bogus xref data for %s\n",TokenToText(*token));
	    /* XXX do nuke here? */
	    return;
	}
	/* q points to start of ng name, r points to its end. */
	strncpy(temp, q, r-q);
	temp[r-q] = '\0';
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
static void ARTreadschema(BOOL Overview)
{
    FILE                        *F;
    char                        *p;
    ARTOVERFIELD                *fp;
    int                         i;
    char                        buff[SMBUF];
    BOOL                        foundxreffull = FALSE;

    if (Overview) {
	/* Open file, count lines. */
	if ((F = fopen(SchemaPath, "r")) == NULL) {
	    (void)fprintf(stderr, "Can't open %s, %s\n", SchemaPath, strerror(errno));
	    exit(1);
	}
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
		fp->NeedHeadername = EQ(p, "full");
	    }
	    else
		fp->NeedHeadername = FALSE;
	    fp->Headername = COPY(buff);
	    fp->HeadernameLength = strlen(buff);
	    fp->Header = (char *)NULL;
	    fp->HasHeader = FALSE;
	    fp->HeaderLength = 0;
	    if (caseEQn(buff, DATE, STRLEN(DATE)-1))
		Datep = fp;
	    if (caseEQn(buff, MESSAGEID, STRLEN(MESSAGEID)-1))
		Msgidp = fp;
	    if (caseEQn(buff, EXPIRES, STRLEN(EXPIRES)-1))
		Expp = fp;
	    if (caseEQn(buff, XREF, STRLEN(XREF)-1)) {
		Xrefp = fp;
		foundxreffull = fp->NeedHeadername;
            }
	    fp++;
	}
	ARTfieldsize = fp - ARTfields;
	(void)fclose(F);
    }
    if (Msgidp == (ARTOVERFIELD *)NULL)
	Missfieldsize++;
    if (Datep == (ARTOVERFIELD *)NULL)
	Missfieldsize++;
    if (Expp == (ARTOVERFIELD *)NULL)
	Missfieldsize++;
    if (Overview && (Xrefp == (ARTOVERFIELD *)NULL || !foundxreffull)) {
	(void)fprintf(stderr, "'Xref:full' must be included in %s\n", SchemaPath);
	exit(1);
    }
    if (Missfieldsize > 0) {
	Missfields = NEW(ARTOVERFIELD, Missfieldsize);
        fp = Missfields;
	if (Msgidp == (ARTOVERFIELD *)NULL) {
	    fp->NeedHeadername = FALSE;
	    fp->Headername = COPY(MESSAGEID);
	    fp->HeadernameLength = strlen(MESSAGEID)-1;
	    fp->Header = (char *)NULL;
	    fp->HasHeader = FALSE;
	    fp->HeaderLength = 0;
	    Msgidp = fp++;
	}
	if (Datep == (ARTOVERFIELD *)NULL) {
	    fp->NeedHeadername = FALSE;
	    fp->Headername = COPY(DATE);
	    fp->HeadernameLength = strlen(DATE)-1;
	    fp->Header = (char *)NULL;
	    fp->HasHeader = FALSE;
	    fp->HeaderLength = 0;
	    Datep = fp++;
	}
	if (Expp == (ARTOVERFIELD *)NULL) {
	    fp->NeedHeadername = FALSE;
	    fp->Headername = COPY(EXPIRES);
	    fp->HeadernameLength = strlen(EXPIRES)-1;
	    fp->Header = (char *)NULL;
	    fp->HasHeader = FALSE;
	    fp->HeaderLength = 0;
	    Expp = fp++;
	}
        if (Overview && Xrefp == (ARTOVERFIELD *)NULL) {
	    fp->NeedHeadername = FALSE;
	    fp->Headername = COPY(XREF);
	    fp->HeadernameLength = strlen(XREF)-1;
	    fp->Header = (char *)NULL;
	    fp->HasHeader = FALSE;
	    fp->HeaderLength = 0;
	    Xrefp = fp++;
	}
    }
}

/*
 * Handle a single article.  This routine's fairly complicated. 
 */
void
DoArt(ARTHANDLE *art)
{
    ARTOVERFIELD		*fp;
    char			*p, *p1, *p2;
    static BUFFER 		Buff;
    static char			SEP[] = "\t";
    static char			NUL[] = "\0";
    static char			COLONSPACE[] = ": ";
    int				i, j, len;
    char			*MessageID;
    time_t			Arrived;
    time_t			Expires;
    time_t			Posted;
    char			*hash;
    HASH			key;
    char			overdata[BIG_BUFFER];
    char			bytes[BIG_BUFFER];
    struct artngnum		ann;

    /* Set up place to store headers. */
    for (fp = ARTfields, i = ARTfieldsize; --i >= 0; fp++) {
	if (fp->HeaderLength) {
	    fp->Header = 0;
	}
	fp->HeaderLength = 0;
	fp->HasHeader = FALSE;
    }
    if (Missfieldsize > 0) {
	for (fp = Missfields, i = Missfieldsize; --i >= 0; fp++) {
	    if (fp->HeaderLength) {
		fp->Header = 0;
	    }
	    fp->HeaderLength = 0;
	    fp->HasHeader = FALSE;
	}
    }
    for (i = ARTfieldsize, fp = ARTfields; --i >= 0;fp++) {
	if ((fp->Header = (char *)HeaderFindMem(art->data, art->len, fp->Headername, fp->HeadernameLength)) != (char *)NULL) {
	    fp->HasHeader = TRUE;
	    for (p = fp->Header, p1 = p2 = (char *)NULL; p < art->data + art->len; p++) {
		if (p2 != (char *)NULL && *p2 == '\r' &&
		    p1 != (char *)NULL && *p1 == '\n' &&
		    !ISWHITE(*p))
		    break;
		p2 = p1;
		p1 = p;
	    }
	    if (p >= art->data + art->len) {
		/* not found for this header */
		continue;
	    }
            fp->HeaderLength = p2 - fp->Header;
	} else if (RetrMode == RETR_ALL && strcmp(fp->Headername, "Bytes") == 0)
	{
		sprintf(bytes, "%d", art->len);
		fp->HasHeader = TRUE;
		fp->Header = bytes;
		fp->HeaderLength = strlen(bytes);
	}
    }
    if (Missfieldsize > 0) {
	for (i = Missfieldsize, fp = Missfields; --i >= 0;fp++) {
	    if ((fp->Header = (char *)HeaderFindMem(art->data, art->len, fp->Headername, fp->HeadernameLength)) != (char *)NULL) {
		fp->HasHeader = TRUE;
		for (p = fp->Header, p1 = p2 = (char *)NULL; p < art->data + art->len; p++) {
		    if (p2 != (char *)NULL && *p2 == '\r' &&
			p1 != (char *)NULL && *p1 == '\n' &&
			!ISWHITE(*p))
		        break;
		    p2 = p1;
		    p1 = p;
		}
		if (p >= art->data + art->len) {
		    /* not found for this header */
		  continue;
		}
		fp->HeaderLength = p2 - fp->Header;
	    }
	}
    }
    if (DoOverview && Xrefp->HeaderLength == 0) {
	if (!SMprobe(SMARTNGNUM, art->token, (void *)&ann)) {
	    Xrefp->Header = NULL;
	    Xrefp->HeaderLength = 0;
	} else
	    return;
	if (ann.artnum == 0)
	    return;
	len = strlen(XREF) + 1 + strlen(innconf->pathhost) + 1 + strlen(ann.groupname) + 1 + 16 + 1;
	if (len > BIG_BUFFER) {
	    Xrefp->Header = NULL;
	    Xrefp->HeaderLength = 0;
	} else {
	    sprintf(overdata, "%s %s %s:%lu", XREF, innconf->pathhost,
                    ann.groupname, ann.artnum);
	    Xrefp->Header = overdata;
	    Xrefp->HeaderLength = strlen(overdata);
	}
    }

    MessageID = (char *)NULL;
    Arrived = art->arrived;
    Expires = 0;
    Posted = 0;

    if (!Msgidp->HasHeader) {
	(void)fprintf(stderr, "No %s in %s\n", MESSAGEID, TokenToText(*art->token));
	if (NukeBadArts)
	    (void)SMcancel(*art->token);
	return;
    }

    if (Buff.Data == NULL)
	Buff.Data = NEW(char, 1);
    BUFFset(&Buff, Msgidp->Header, Msgidp->HeaderLength);
    BUFFappend(&Buff, NUL, STRLEN(NUL));
    for (i = 0, p = Buff.Data; i < Buff.Left; p++, i++)
	if (*p == '\t' || *p == '\n' || *p == '\r')
	    *p = ' ';
    MessageID = GetMessageID(Buff.Data);
    if (*MessageID == '\0') {
	(void)fprintf(stderr, "No %s in %s\n", MESSAGEID, TokenToText(*art->token));
	if (NukeBadArts)
	    (void)SMcancel(*art->token);
	return;
    }

    /*
     * check if msgid is in history if in update mode, or if article is 
     * newer than start time of makehistory. 
     */
    key = HashMessageID(MessageID);
    hash = HashToText(key);

    if (!Datep->HasHeader) {
	Posted = Arrived;
    } else {
	BUFFset(&Buff, Datep->Header, Datep->HeaderLength);
	BUFFappend(&Buff, NUL, STRLEN(NUL));
	for (i = 0, p = Buff.Data; i < Buff.Left; p++, i++)
	    if (*p == '\t' || *p == '\n' || *p == '\r')
		*p = ' ';
	if ((Posted = GetaDate(Buff.Data)) == 0)
	    Posted = Arrived;
    }

    if (Expp->HasHeader) {
	BUFFset(&Buff, Expp->Header, Expp->HeaderLength);
	BUFFappend(&Buff, NUL, STRLEN(NUL));
	for (i = 0, p = Buff.Data; i < Buff.Left; p++, i++)
	    if (*p == '\t' || *p == '\n' || *p == '\r')
		*p = ' ';
	Expires = GetaDate(Buff.Data);
    }

    if (DoOverview && Xrefp->HeaderLength > 0) {
	for (j = ARTfieldsize, fp = ARTfields; --j >= 0;fp++) {
	    if (fp == ARTfields)
		BUFFset(&Buff, "", 0);
	    else
		BUFFappend(&Buff, SEP, STRLEN(SEP));
	    if (fp->NeedHeadername) {
		BUFFappend(&Buff, fp->Headername, fp->HeadernameLength);
		BUFFappend(&Buff, COLONSPACE, STRLEN(COLONSPACE));
	    }
	    i = Buff.Left;
	    BUFFappend(&Buff, fp->Header, fp->HeaderLength);
	    for (p = &Buff.Data[i]; i < Buff.Left; p++, i++)
		if (*p == '\t' || *p == '\n' || *p == '\r')
		    *p = ' ';
	}
	WriteOverLine(art->token, Xrefp->Header, Xrefp->HeaderLength,
		      Buff.Data, Buff.Left, Arrived, Expires);
    }

    if (!NoHistory) {
	if (Expires > 0)
	    i = fprintf(HistFile, "[%s]%c%lu%c%lu%c%lu%c%s\n",
			hash, HIS_FIELDSEP,
			(unsigned long)Arrived, HIS_SUBFIELDSEP,
			(unsigned long)Expires,
			HIS_SUBFIELDSEP, (unsigned long)Posted, HIS_FIELDSEP,
			TokenToText(*art->token));
	else
	    i = fprintf(HistFile, "[%s]%c%lu%c%s%c%lu%c%s\n",
			hash, HIS_FIELDSEP,
			(unsigned long)Arrived, HIS_SUBFIELDSEP, HIS_NOEXP,
			HIS_SUBFIELDSEP, (unsigned long)Posted, HIS_FIELDSEP,
			TokenToText(*art->token));
	if (i == EOF || ferror(HistFile)) {
	    (void)fprintf(stderr, "makehistory: Can't write history line, %s\n", strerror(errno));
	    exit(1);
	}
    }
}


void
Usage(void)
{
    fprintf(stderr, "Usage: makehistory [-b] [-f file] [-O] [-I] [-l overtmpsegsize [-a] [-x] [-T tmpdir]\n");
    fprintf(stderr, "\t-b -- delete bad articles from spool\n");
    fprintf(stderr, "\t-e -- read entire articles to compute proper Bytes headers\n");
    fprintf(stderr, "\t-f -- write history entries to file (default $pathdb/history)\n");
    fprintf(stderr, "\t-a -- open output history file in append mode\n");
    fprintf(stderr, "\t-O -- create overview entries for articles\n");
    fprintf(stderr, "\t-I -- do not create overview entries for articles below lowmark in active\n");
    fprintf(stderr, "\t-l nnn -- set size of batches too do overview updates in (default 10000)\n");
    fprintf(stderr,"\t\tentries not already in main history file.\n");
    fprintf(stderr, "\t-x -- don't bother writing any history entries at all\n");
    fprintf(stderr, "\t-T tmpdir -- use directory tmpdir for temp files. \n");
    fprintf(stderr, "\t-F -- fork when writing overview\n");
    exit(1);
}

/*
** Add all groups to overview group.index. --rmt
*/
void
OverAddAllNewsgroups(void)
{
    QIOSTATE *qp;
    int count;
    char *q,*p;
    char *line;
    ARTNUM hi, lo;

    if ((qp = QIOopen(ActivePath)) == NULL) {
	fprintf(stderr, "makehistory: Can't open %s, %s\n", ActivePath, strerror(errno));
	exit(1);
    }
    for (count = 1; (line = QIOread(qp)) != NULL; count++) {
	if ((p = strchr(line, ' ')) == NULL) {
	    fprintf(stderr, "makehistory: Bad line %d, \"%s\"\n", count, line);
	    continue;
	}
	*p++ = '\0';
	hi = (ARTNUM)atol(p);
	if ((p = strchr(p, ' ')) == NULL) {
	    fprintf(stderr, "makehistory: Bad line %d, \"%s\"\n", count, line);
	    continue;
	}
	*p++ = '\0';
	lo = (ARTNUM)atol(p);
	if ((q = strrchr(p, ' ')) == NULL) {
	    fprintf(stderr, "makehistory: Bad line %d, \"%s\"\n", count, line);
	    continue;
	}
	/* q+1 points to NG flag */
	if (!OVgroupadd(line, lo, hi, q+1)) {
	    fprintf(stderr, "makehistory: Can't add %s to overview group.index\n", line);
	    exit(1);
	}
    }
    /* Test error conditions; QIOtoolong shouldn't happen. */
    if (QIOtoolong(qp)) {
	fprintf(stderr, "makehistory: Line %d is too long\n", count);
	exit(1);
    }
    if (QIOerror(qp)) {
	fprintf(stderr, "makehistory: Can't read %s around line %d, %s\n",
		      ActivePath, count, strerror(errno));
	exit(1);
    }
    QIOclose(qp);
}

int
main(int argc, char **argv)
{
    ARTHANDLE *art = NULL;
    BOOL AppendMode;
    int i, val;
    char *HistoryDir;
    char *p;
    char *OldHistoryPath;
    dbzoptions opt;
    char *buff;
    
    /* First thing, set up logging and our identity. */
    openlog("makehistory", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);     
	
    /* Set defaults. */
    if (ReadInnConf() < 0) exit(1);
    HistoryPath = COPY(cpcatpath(innconf->pathdb, _PATH_HISTORY));
    OldHistoryPath = COPY(cpcatpath(innconf->pathdb, _PATH_HISTORY));
    ActivePath = COPY(cpcatpath(innconf->pathdb, _PATH_ACTIVE));
    TmpDir = innconf->pathtmp;
    SchemaPath = COPY(cpcatpath(innconf->pathetc, _PATH_SCHEMA));

    OverTmpSegSize = DEFAULT_SEGSIZE;
    OverTmpSegCount = 0;
    NukeBadArts = FALSE;
    DoOverview = FALSE;
    Fork = FALSE;
    AppendMode = FALSE;
    NoHistory = FALSE;
    RetrMode = RETR_HEAD;

    while ((i = getopt(argc, argv, "aebf:Il:OT:xF")) != EOF) {
	switch(i) {
	case 'T':
	    TmpDir = optarg;
	    break;
	case 'x':
	    NoHistory = TRUE;
	    break;
	case 'a':
	    AppendMode = TRUE;
	    break;
	case 'b':
	    NukeBadArts = TRUE;
	    break;
	case 'f':
	    HistoryPath = optarg;
	    break;
	case 'I':
	    Cutofflow = TRUE;
	    break;
	case 'l':
	    OverTmpSegSize = atoi(optarg);
	    break;
	case 'O':
	    DoOverview = TRUE;
	    break;
	case 'F':
	    Fork = TRUE;
	    break;
	case 'e':
	    RetrMode = RETR_ALL;
	    break;
	    
	default:
	    Usage();
	    break;
	}
    }
    argc -= optind;
    argv += optind;
    if (argc) {
	Usage();
    }

    if ((p = strrchr(HistoryPath, '/')) == NULL) {
	/* find the default history file directory */
	HistoryDir = innconf->pathdb;
    } else {
	*p = '\0';
	HistoryDir = COPY(HistoryPath);
	*p = '/';
    }

    if (chdir(HistoryDir) < 0) {
	fprintf(stderr, "makehistory: can't cd to %s\n", HistoryDir);
	exit(1);
    }

    /* Read in the overview schema */
    ARTreadschema(DoOverview);
    
    if (DoOverview) {
	/* init the overview setup. */
	if (!OVopen(OV_WRITE)) {
	    fprintf(stderr, "makehistory: OVopen failed\n");
	    exit(1);
	}
	if (!OVctl(OVSORT, (void *)&sorttype)) {
	    fprintf(stderr, "makehistory: OVctl(OVSORT) failed\n");
	    exit(1);
	}
	if (!Fork) {
	    if (!OVctl(OVCUTOFFLOW, (void *)&Cutofflow)) {
	        fprintf(stderr, "makehistory: OVctl(OVCUTOFFLOW) failed\n");
	        exit(1);
	    }
	    OverAddAllNewsgroups();
	} else {
	    OverAddAllNewsgroups();
	    if (sorttype == OVNOSORT) {
		buff = concat(innconf->pathbin, "/", "overchan", NULL);
		if ((Overchan = popen(buff, "w")) == NULL) {
		    fprintf(stderr, "makehistory: forking overchan failed\n");
		    exit(1);
		}
		DISPOSE(buff);
	    }
	    OVclose();
	}
    }

    /* Init the Storage Manager */
    val = TRUE;
    if (!SMsetup(SM_RDWR, (void *)&val) || !SMsetup(SM_PREOPEN, (void *)&val)) {
	fprintf(stderr, "Can't setup storage manager\n");
	exit(1);
    }
    if (!SMinit()) {
	fprintf(stderr, "makehistory: Can't initialize storage manager: %s\n", SMerrorstr);
	exit(1);
    }

    if (!NoHistory) {
	/* Open the history file */
	HistFile = fopen(HistoryPath, AppendMode ? "a" : "w");
	if (HistFile == NULL) {
	    fprintf(stderr, "makehistory: can't open %s: %s\n", HistoryPath, strerror(errno));
	    exit(1);
	}
    }

    /* Get the time.  Only get it once, which is good enough. */
    if (GetTimeInfo(&Now) < 0) {
	(void)fprintf(stderr, "Can't get the time, %s\n", strerror(errno));
	exit(1);
    }

    /*
     * Scan the entire spool, nuke any bad arts if needed, and process each
     * article.
     */
	
    while ((art = SMnext(art, RetrMode)) != NULL) {
	if (art->len == 0) {
	    if (NukeBadArts && art->data == NULL && art->token != NULL)
		(void)SMcancel(*art->token);
	    continue;
	}
	DoArt(art);
    }

    if (!NoHistory) {
	/* close history file. */
	if (fflush(HistFile) == EOF || ferror(HistFile) || fclose(HistFile) == EOF) {
	    (void)fprintf(stderr, "Can't close history file, %s\n",	strerror(errno));
	    exit(1);
	}
    }

    if (DoOverview) {
	if (sorttype == OVNOSORT && Fork) {
	    if (fflush(Overchan) == EOF || ferror(Overchan) || pclose(Overchan) == EOF) {
		(void)fprintf(stderr, "Can't flush overview data , %s\n", strerror(errno));
		exit(1);
	    }
	}
	if (sorttype != OVNOSORT) {
	    int status;
	    FlushOverTmpFile();
	    if(Fork)
		wait(&status);
	}
    }
    if(!Fork)
	OVclose();
    exit(0);
}


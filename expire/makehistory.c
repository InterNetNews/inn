/*  $Revision$
**
**  Rebuild the history database.
**
**  1997-04-06: Fix off-by-one error when expanding buffer to fit history
**  lines. <nick@zeta.org.au>
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
#include "storage.h"
#include "qio.h"
#include "macros.h"
#include "mydir.h"
#include <syslog.h>  


typedef struct _BUFFER {
    long	Size;
    long	Used;
    long	Left;
    char	*Data;
} BUFFER;

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

typedef enum {NO_TRANS, FROM_HIST, FROM_SPOOL} TRANS;

STATIC char		*ACTIVE = NULL;
STATIC char		*HISTORYDIR;
STATIC char		*HISTORY = NULL;
STATIC char		MESSAGEID[] = "Message-ID:";
STATIC char		EXPIRES[] = "Expires:";
STATIC char		DATE[] = "Date:";
STATIC char		XREF[] = "Xref:";
STATIC BOOL		INNDrunning;
STATIC char		*TextFile;
STATIC char		Reason[] = "makehistory is running";
STATIC TIMEINFO		Now;
STATIC char		*SCHEMA = NULL;
STATIC ARTOVERFIELD	*ARTfields;
STATIC int		ARTfieldsize;
STATIC ARTOVERFIELD	*Datep = (ARTOVERFIELD *)NULL;
STATIC ARTOVERFIELD	*Msgidp = (ARTOVERFIELD *)NULL;
STATIC ARTOVERFIELD	*Expp = (ARTOVERFIELD *)NULL;
STATIC ARTOVERFIELD	*Xrefp = (ARTOVERFIELD *)NULL;
STATIC ARTOVERFIELD	*Missfields;
STATIC int		Missfieldsize = 0;
STATIC char		*IndexFile;
STATIC BOOL		OVERmmap;
STATIC char		*CRLF = "\r\n";
#define CRLFSIZE	2

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
    int				missing = 0;

    if (Overview) {
	/* Open file, count lines. */
	if ((F = fopen(SCHEMA, "r")) == NULL) {
	    (void)fprintf(stderr, "Can't open %s, %s\n", SCHEMA, strerror(errno));
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
	    if (caseEQn(buff, EXPIRES, STRLEN(EXPIRES))-1)
		Expp = fp;
	    if (caseEQn(buff, XREF, STRLEN(XREF))-1)
		Xrefp = fp;
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
    if (Xrefp == (ARTOVERFIELD *)NULL)
	Missfieldsize++;
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
	if (Xrefp == (ARTOVERFIELD *)NULL) {
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
**  Change to a directory or exit out.
*/
STATIC void
xchdir(char *where)
{
    if (chdir(where) < 0) {
	(void)fprintf(stderr, "Can't change to \"%s\", %s\n",
		where, strerror(errno));
	exit(1);
    }
}


/*
**  Remove the DBZ files for the specified base text file.
*/
STATIC void
RemoveDBZFiles(char *p)
{
    static char	NOCANDO[] = "Can't remove \"%s\", %s\n";
    char	buff[SMBUF];

    (void)sprintf(buff, "%s.dir", p);
    if (unlink(buff) && errno != ENOENT)
	(void)fprintf(stderr, NOCANDO, buff, strerror(errno));
#ifdef	DO_TAGGED_HASH
    (void)sprintf(buff, "%s.pag", p);
    if (unlink(buff) && errno != ENOENT)
	(void)fprintf(stderr, NOCANDO, buff, strerror(errno));
#else
    (void)sprintf(buff, "%s.index", p);
    if (unlink(buff) && errno != ENOENT)
	(void)fprintf(stderr, NOCANDO, buff, strerror(errno));
    (void)sprintf(buff, "%s.hash", p);
    if (unlink(buff) && errno != ENOENT)
	(void)fprintf(stderr, NOCANDO, buff, strerror(errno));
#endif
}

/*
**  Rebuild the DBZ file from the text file.
*/
STATIC void Rebuild(long size, BOOL IgnoreOld, BOOL Overwrite)
{
    QIOSTATE	        *qp;
    char	        *p, *q;
    char	        *save;
    long	        count;
    long		where;
    HASH		key;
    char		temp[SMBUF];
    dbzoptions          opt;
#ifndef	DO_TAGGED_HASH
    TOKEN	token;
    void        *ivalue;
    idxrec      ionevalue;  
    idxrecext   iextvalue; 
#endif

    xchdir(HISTORYDIR);

    /* Open the text file. */
    qp = QIOopen(TextFile);
    if (qp == NULL) {
	(void)fprintf(stderr, "Can't open \"%s\", %s\n",
		TextFile, strerror(errno));
	exit(1);
    }

    /* If using the standard history file, force DBZ to use history.n. */
    if (EQ(TextFile, HISTORY) && !Overwrite) {
	(void)sprintf(temp, "%s.n", HISTORY);
	if (link(HISTORY, temp) < 0) {
	    (void)fprintf(stderr, "Can't make temporary link to \"%s\", %s\n",
		    temp, strerror(errno));
	    exit(1);
	}
	RemoveDBZFiles(temp);
	p = temp;
    }
    else {
	temp[0] = '\0';
	RemoveDBZFiles(TextFile);
	p = TextFile;
    }

    /* Open the new database, using the old file if desired and possible. */
    dbzgetoptions(&opt);
#ifdef	DO_TAGGED_HASH
    opt.pag_incore = INCORE_MEM;
#else
    opt.idx_incore = INCORE_MEM;
    opt.exists_incore = INCORE_MEM;
#endif
    dbzsetoptions(opt);
    if (IgnoreOld) {
	if (!dbzfresh(p, dbzsize(size), 0)) {
	    (void)fprintf(stderr, "Can't do dbzfresh, %s\n",
		    strerror(errno));
	    if (temp[0])
		(void)unlink(temp);
	    exit(1);
	}
    }
    else {
	if (!dbzagain(p, HISTORY)) {
	    (void)fprintf(stderr, "Can't do dbzagain, %s\n", strerror(errno));
	    if (temp[0])
		(void)unlink(temp);
	    exit(1);
	}
    }

    /* Loop through all lines in the text file. */
    count = 0;
    for (where = QIOtell(qp); (p = QIOread(qp)) != NULL; where = QIOtell(qp)) {
	count++;
	if ((save = strchr(p, HIS_FIELDSEP)) == NULL) {
	    (void)fprintf(stderr, "Bad line #%ld \"%.30s...\"\n", count, p);
	    if (temp[0])
		(void)unlink(temp);
	    exit(1);
	}
	*save = '\0';
	switch (*p) {
	case '[':
	    if (strlen(p) != ((sizeof(HASH) * 2) + 2)) {
		fprintf(stderr, "Invalid length for hash %s, skipping\n", p);
		continue;
	    }
	    key = TextToHash(p+1);
#ifndef	DO_TAGGED_HASH
	    if (((save = strchr(save + 1, '@')) != NULL) &&
		((q = strchr(save + 1, '@')) != NULL)) {
		*(++q) = '\0';
		if (!IsToken(save)) {
		    fprintf(stderr, "Invalid token %s for hash %s, skipping\n", q, p);
		    continue;
		}
		if (innconf->extendeddbz) {
		    iextvalue.offset[HISTOFFSET] = where;
		    token = TextToToken(save);
		    OVERsetoffset(&token, &iextvalue.offset[OVEROFFSET], &iextvalue.overindex, &iextvalue.overlen); 
		    ivalue = (void *)&iextvalue;
		} else {
		    ionevalue.offset = where;
		    ivalue = (void *)&ionevalue;
		}
	    } else {
		if (innconf->extendeddbz) {
		    iextvalue.offset[HISTOFFSET] = where;
		    iextvalue.offset[OVEROFFSET] = 0;
		    iextvalue.overindex = OVER_NONE;
		    iextvalue.overlen = 0;
		    ivalue = (void *)&iextvalue;
		} else {
		    ionevalue.offset = where;
		    ivalue = (void *)&ionevalue;
		}
	    }
#endif
	    break;
	case '<':
	    key = HashMessageID(p);
#ifndef	DO_TAGGED_HASH
	    ionevalue.offset = where;
	    ivalue = (void *)&ionevalue;
#endif
	    break;
	default:
	    fprintf(stderr, "Invalid message-id \"%s\" in history text\n", p);
	    continue;
	}
#ifdef	DO_TAGGED_HASH
	switch (dbzstore(key, (OFFSET_T)where)) {
#else
	switch (dbzstore(key, ivalue)) {
#endif
	case DBZSTORE_EXISTS:
            fprintf(stderr, "Duplicate message-id \"%s\" in history text\n", p);
	    break;
	case DBZSTORE_ERROR:
	    fprintf(stderr, "Can't store \"%s\", %s\n",
		    p, strerror(errno));
	    if (temp[0])
		(void)unlink(temp);
	    exit(1);
	default:
	    break;
	}
    }
    if (QIOerror(qp)) {
	(void)fprintf(stderr, "Can't read \"%s\" near line %ld, %s\n",
		TextFile, count, strerror(errno));
	if (temp[0])
	    (void)unlink(temp);
	exit(1);
    }
    if (QIOtoolong(qp)) {
	(void)fprintf(stderr, "Line %ld is too long\n", count);
	if (temp[0])
	    (void)unlink(temp);
	exit(1);
    }

    /* Close files. */
    QIOclose(qp);
    if (!dbzclose()) {
	(void)fprintf(stderr, "Can't close history, %s\n", strerror(errno));
	if (temp[0])
	    (void)unlink(temp);
	exit(1);
    }

    if (temp[0])
	(void)unlink(temp);
}

STATIC int
split(char *p, char sep, char **argv, int count)
{
    int                 i;

    if (!p || !*p)
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
** read article into memory as wireformatted article
** add Xref header if it does not exist
*/
STATIC BOOL
ReadInMem(char *art, ARTHANDLE *arth, char *Tradspooldir)
{
    char		buff[SPOOLNAMEBUFF];
    static char		*path = (char *)NULL;
    QIOSTATE		*qp;
    static BUFFER	artbuff;
    char		*p;
    BOOL		FoundXref = FALSE;
    static char		*pathhost = (char *)NULL;
    static int		pathhostlen;
    int			len;
    struct stat		sb;

    if (pathhost == (char *)NULL) {
	if ((pathhost = innconf->pathhost) == (char *)NULL) {
	    (void)fprintf(stderr, "Can't get pathhost\n");
	    return FALSE;
	}
	pathhostlen = (int)strlen(pathhost);
    }
    if (path == (char *)NULL) {
	path = NEW(char, STRLEN(XREF) + 1 + pathhostlen + 1 + SPOOLNAMEBUFF + CRLFSIZE * 2 + 1);
    }
    strcpy(path, art);
    for (p = path; *p; p++)
	if (*p == '.')
	    *p = '/';
    sprintf(buff, "%s/%s", Tradspooldir, path);
    if ((qp = QIOopen(buff)) == NULL) {
	return FALSE;
    }
    if (artbuff.Left == 0) {
	artbuff.Data = NEW(char, BIG_BUFFER);
	artbuff.Left = BIG_BUFFER;
    }
    artbuff.Used = 0;
    while ((p = QIOread(qp)) != NULL && *p != '\0') {
	if (caseEQn(p, XREF, STRLEN(XREF)))
	    FoundXref = TRUE;
	if (artbuff.Left - artbuff.Used < QIOlength(qp) + CRLFSIZE * 2 + 1) {
	    artbuff.Data = RENEW(artbuff.Data, char, artbuff.Left * 2 + QIOlength(qp) - artbuff.Left + artbuff.Used + CRLFSIZE * 2 + 1);
	    artbuff.Left = artbuff.Left * 2 + QIOlength(qp) - artbuff.Left + artbuff.Used + CRLFSIZE * 2 + 1;
	}
	if (*p == '.') {
	    artbuff.Data[artbuff.Used++] = '.';
	}
	memcpy(artbuff.Data + artbuff.Used, p, QIOlength(qp));
	artbuff.Used += QIOlength(qp);
	memcpy(artbuff.Data + artbuff.Used, CRLF, CRLFSIZE);
	artbuff.Used += CRLFSIZE;
    }
    if (!FoundXref) {
	/* assumes not crossposted */
	sprintf(path, "%s %s %s\r\n\r\n", XREF, pathhost, art);
	if ((p = strrchr(path, '/')) != (char *)NULL)
	    *p = ':';
	else {
	    QIOclose(qp);
	    return FALSE;
	}
	len = strlen(path);
	if (artbuff.Left - artbuff.Used < len) {
	    artbuff.Data = RENEW(artbuff.Data, char, artbuff.Left * 2 + len - artbuff.Left + artbuff.Used + 1);
	    artbuff.Left = artbuff.Left * 2 + len - artbuff.Left + artbuff.Used + 1;
	}
	memcpy(artbuff.Data + artbuff.Used, path, len);
	artbuff.Used += len;
    } else {
	memcpy(artbuff.Data + artbuff.Used, CRLF, CRLFSIZE);
	artbuff.Used += CRLFSIZE;
    }
    while ((p = QIOread(qp)) != NULL) {
	if (artbuff.Left - artbuff.Used < QIOlength(qp) + CRLFSIZE * 2 + 1) {
	    artbuff.Data = RENEW(artbuff.Data, char, artbuff.Left * 2 + QIOlength(qp) - artbuff.Left + artbuff.Used + CRLFSIZE * 2 + 1);
	    artbuff.Left = artbuff.Left * 2 + QIOlength(qp) - artbuff.Left + artbuff.Used + CRLFSIZE * 2 + 1;
	}
	if (*p == '.') {
	    artbuff.Data[artbuff.Used++] = '.';
	}
	memcpy(artbuff.Data + artbuff.Used, p, QIOlength(qp));
	artbuff.Used += QIOlength(qp);
	memcpy(artbuff.Data + artbuff.Used, CRLF, CRLFSIZE);
	artbuff.Used += CRLFSIZE;
    }
    artbuff.Data[artbuff.Used++] = '.';
    memcpy(artbuff.Data + artbuff.Used, CRLF, CRLFSIZE);
    artbuff.Used += CRLFSIZE;
    if (arth->arrived == 0) {
	if (fstat(QIOfileno(qp), &sb) < 0) {
	    QIOclose(qp);
	    return FALSE;
	}
	arth->arrived = sb.st_mtime;
    }
    QIOclose(qp);
    arth->data = artbuff.Data;
    arth->len = artbuff.Used;
    return TRUE;
}

/*
**  Remove a bad article.
*/
STATIC void
Removeit(char *name)
{
    char	*p;

    /* Already in the spool directory, so skip right past the name;
     * the strchr can't return NULL. */
    p = strchr(name, '/') + 1;
    if (unlink(p) < 0 && errno != ENOENT)
	(void)fprintf(stderr, "Can't unlink %s, %s\n",
		name, strerror(errno));
    else
	(void)fprintf(stderr, "Removing %s\n", name);
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
**  Process a single article.
*/
STATIC void
DoMemArt(ARTHANDLE *art, BOOL Overview, BOOL Update, FILE *out, FILE *index, BOOL RemoveBad)
{
    ARTOVERFIELD		*fp;
    char			*p, *p1, *p2, *q;
    static BUFFER 		Buff;
    static char			SEP[] = "\t";
    static char			NUL[] = "\0";
    static char			COLONSPACE[] = ": ";
    int				i, j;
    char			*MessageID;
    time_t			Arrived;
    time_t			Expires;
    time_t			Posted;
    char			*hash;

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

    MessageID = (char *)NULL;
    Arrived = art->arrived;
    Expires = 0;
    Posted = 0;

    if (!Msgidp->HasHeader) {
	(void)fprintf(stderr, "No %s in %s\n", MESSAGEID, TokenToText(*art->token));
	if (RemoveBad)
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
	if (RemoveBad)
	    (void)SMcancel(*art->token);
	return;
    }
    if (Update) {
        /* Server already know about this one? */
	if (dbzexists(HashMessageID(MessageID)))
	    return;
    }
    hash = HashToText(HashMessageID(MessageID));

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

    if (Overview) {
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
	if (!OVERstore(art->token, Buff.Data, Buff.Left)) {
	    (void)fprintf(stderr, "Can't write overview data, %s\n", strerror(errno));
	    exit(1);
	}
    } else {
	art->token->index = OVER_NONE;
    }
    if (index != (FILE *)NULL && Xrefp->HasHeader && (!Overview || (Overview && (art->token->index < OVER_NONE)))) {
	BUFFset(&Buff, Xrefp->Header, Xrefp->HeaderLength);
	BUFFappend(&Buff, NUL, STRLEN(NUL));
	for (i = 0, p = Buff.Data; i < Buff.Left; p++, i++)
	    if (*p == '\t' || *p == '\n' || *p == '\r')
		*p = ' ';
	if ((p = strchr(Buff.Data, ' ')) == NULL)
	    (void)fprintf(stderr, "Can't find Xref content, %s\n", Buff.Data);
	else {
	    for (p++; *p == ' '; p++);
	    q = p;
	    while ((p = strchr(p, ' ')) != NULL) {
	        *p = '\0';
	        i = fprintf(index, "[%s] %s\n", hash, q);
	        if (i == EOF || ferror(index)) {
		    (void)fprintf(stderr, "Can't write index line, %s\n", strerror(errno));
		    exit(1);
	        }
		for (p++; *p == ' '; p++);
	        q = p;
	    }
	    i = fprintf(index, "[%s] %s\n", hash, q);
	    if (i == EOF || ferror(index)) {
		(void)fprintf(stderr, "Can't write index line, %s\n", strerror(errno));
		exit(1);
	    }
	}
    }
    if (out != NULL) {
	if (Expires > 0)
	    i = fprintf(out, "[%s]%c%lu%c%lu%c%lu%c%s\n",
		hash, HIS_FIELDSEP,
		(unsigned long)Arrived, HIS_SUBFIELDSEP,
		(unsigned long)Expires,
		HIS_SUBFIELDSEP, (unsigned long)Posted, HIS_FIELDSEP,
		TokenToText(*art->token));
	else
	    i = fprintf(out, "[%s]%c%lu%c%s%c%lu%c%s\n",
			hash, HIS_FIELDSEP,
			(unsigned long)Arrived, HIS_SUBFIELDSEP, HIS_NOEXP,
			HIS_SUBFIELDSEP, (unsigned long)Posted, HIS_FIELDSEP,
			TokenToText(*art->token));
	if (i == EOF || ferror(out)) {
	    (void)fprintf(stderr, "Can't write history line, %s\n", strerror(errno));
	    exit(1);
	}
    }
}

/*
** read articles from history and store them thru storage api
*/
STATIC BOOL
TranslateFromHistory(FILE *out, char *OldHistory, char *Tradspooldir, BOOL UnlinkCrosspost, BOOL RemoveOld, BOOL Overview, FILE *index)
{
    static char		IGNORING[] = "Ignoring bad line, \"%.20s...\"\n";
    QIOSTATE		*qp;
    int			line;
    int			linelen;
    char		*p, *q;
    char		*fields[4];
    int			i;
    TOKEN		token;
    static BUFFER	artbuff;
    ARTHANDLE		arth, *art;
    static char		*OVERline = NULL;
    static char		*Xrefbuf = NULL;
    char		*Xref;
    char		**arts;
    int			count;
    int			crossnum;
    time_t		Arrived;
    char		buff[SPOOLNAMEBUFF];

    if ((qp = QIOopen(ACTIVE)) == NULL) {
	(void)fprintf(stderr, "Can't open %s, %s\n", ACTIVE, strerror(errno));
	return FALSE;
    }
    for (count = 1; (p = QIOread(qp)) != NULL; count++) {
	if ((p = strchr(p, ' ')) == NULL) {
	    (void)fprintf(stderr, "Bad line %ld, \"%s\"\n", count, p);
	    continue;
	}
    }
    /* Test error conditions; QIOtoolong shouldn't happen. */
    if (QIOtoolong(qp)) {
	(void)fprintf(stderr, "Line %ld is too long\n", count);
	QIOclose(qp);
	return FALSE;
    }
    if (QIOerror(qp)) {
	(void)fprintf(stderr, "Can't read %s around line %ld, %s\n",
		ACTIVE, count, strerror(errno));
	QIOclose(qp);
	return FALSE;
    }
    QIOclose(qp);
    arts = NEW(char*, count);

    if ((qp = QIOopen(OldHistory)) == NULL) {
	(void)fprintf(stderr, "Can't open old history file, %s\n",
	    strerror(errno));
	return FALSE;
    }
    for (line = 1; ; line++) {
	if ((p = QIOread(qp)) != NULL) {
	    i = split(p, HIS_FIELDSEP, fields, SIZEOF(fields));
	    if (i != 2 && i != 3) {
		(void)fprintf(stderr, IGNORING, line);
		continue;
	    }
	    switch (fields[0][0]) {
	    case '[':
		if (strlen(fields[0]) != ((sizeof(HASH) * 2) + 2)) {
		    fprintf(stderr, "Invalid length for hash %s, skipping\n", fields[0]);   
		    break;
		}
		if (i == 2) {
		    if (out != NULL) {
			i = fprintf(out, "%s%c%s\n", fields[0], HIS_FIELDSEP, fields[1]);
			if (i == EOF || ferror(out)) {
			    (void)fprintf(stderr, "Can't write history line, %s\n", strerror(errno));     
			    exit(1);
			}
		    }
		    break;
		} else if (!RemoveOld) {
		    /* just make index */
		    if (!IsToken(fields[2])) {
			fprintf(stderr, "Invalid token %s, skipping\n", fields[2]);   
			break;
		    }
		    if (out != NULL) {
			i = fprintf(out, "%s%c%s%c%s\n", fields[0], HIS_FIELDSEP, fields[1], HIS_FIELDSEP, fields[2]);
			if (i == EOF || ferror(out)) {
			    (void)fprintf(stderr, "Can't write history line, %s\n", strerror(errno));     
			    exit(1);
			}
		    }
		    if (Overview) {
			token = TextToToken(fields[2]);
			if (token.index < OVER_NONE) {
			    if ((p = OVERretrieve(&token, &linelen)) == (char *)NULL)
				break;
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
			    if (index != (FILE *)NULL) {
			        if ((Xref = strstr(OVERline, "\tXref:")) == NULL) {
				    break;
			        }
			        if ((Xref = strchr(Xref, ' ')) == NULL)
				    break;
			        for (Xref++; *Xref == ' '; Xref++);
			        if ((Xref = strchr(Xref, ' ')) == NULL)
				    break;
			        for (Xref++; *Xref == ' '; Xref++);
			        if (!Xrefbuf)
				    Xrefbuf = NEW(char, MAXOVERLINE);
			        memcpy(Xrefbuf, Xref, linelen - (Xref - OVERline));
			        Xrefbuf[linelen - (Xref - OVERline)] = '\0';
			        if ((p = strchr(Xrefbuf, '\t')) != NULL)
				    *p = '\0';
			        p = q = Xrefbuf;
			        while ((p = strchr(p, ' ')) != NULL) {
				    *p = '\0';
				    i = fprintf(index, "%s %s\n", fields[0], q);
				    if (i == EOF || ferror(index)) {
				        (void)fprintf(stderr, "Can't write index line, %s\n", strerror(errno));
				        exit(1);
				    }
				    for (p++; *p == ' '; p++);
				    q = p;
			        }
			        i = fprintf(index, "%s %s\n", fields[0], q);
			        if (i == EOF || ferror(index)) {
				    (void)fprintf(stderr, "Can't write index line, %s\n", strerror(errno));
				    exit(1);
			        }
			    }
			}
		    }
		    break;
		}
		if (!IsToken(fields[2])) {
		    fprintf(stderr, "Invalid token %s, skipping\n", fields[2]);   
		    break;
		}
		token = TextToToken(fields[2]);
		if ((art = SMretrieve(token, RETR_ALL)) == (ARTHANDLE *)NULL) {
		    /* fprintf(stderr, "Cannot retrieve %s, skipping\n", fields[2]); */
		    break;
		}
		if (artbuff.Left == 0) {
		    artbuff.Data = NEW(char, art->len);
		    artbuff.Left = art->len;
		} else if (art->len > artbuff.Left) {
		    artbuff.Data = RENEW(artbuff.Data, char, art->len);
		    artbuff.Left = art->len;
		}
		arth.data = artbuff.Data;
		arth.len = art->len;
		arth.arrived = art->arrived;
		arth.token = &token;
		(void)memcpy((POINTER)artbuff.Data, (POINTER)art->data, (SIZE_T)art->len);
		artbuff.Used = art->len;
		SMfreearticle(art);
		(void)SMcancel(token);
		token = SMstore(arth);
		if (token.type == TOKEN_EMPTY) {
		    fprintf(stderr, "Cannot store %s, skipping\n", fields[0]);
		    break;
		}
		arth.token = &token;
		DoMemArt(&arth, Overview, FALSE, out, index, TRUE);
		break;
	    case '<':
		if (i == 2) {
		    if (out != NULL) {
			i = fprintf(out, "[%s]%c%s\n", HashToText(HashMessageID(fields[0])), HIS_FIELDSEP, fields[1]);
			if (i == EOF || ferror(out)) {
			    (void)fprintf(stderr, "Can't write history line, %s\n", strerror(errno));     
			    exit(1);
			}
		    }
		    break;
		}
		if ((p = strchr(fields[1], HIS_SUBFIELDSEP)) == (char *)NULL)
		    Arrived = atol(fields[1]);
		else {
		    *p = '\0';
		    Arrived = atol(fields[1]);
		    *p = HIS_SUBFIELDSEP;
		}
		arth.arrived = Arrived;
		arth.token = (TOKEN *)NULL;
		crossnum = split(fields[2], ' ', arts, count);
		if (!ReadInMem(arts[0], &arth, Tradspooldir)) {
		    /* maybe article is cancelled, just recored the hash */
		    if (out != NULL) {
			i = fprintf(out, "[%s]%c%s\n", HashToText(HashMessageID(fields[0])), HIS_FIELDSEP, fields[1]);
			if (i == EOF || ferror(out)) {
			    (void)fprintf(stderr, "Can't write history line, %s\n", strerror(errno));     
			    exit(1);
			}
		    }
		    break;
		}
		if (RemoveOld)
		    i = 0;
		else
		    i = 1;
		if (!UnlinkCrosspost)
		    crossnum = 1;
		for (; i < crossnum; i++) {
		    for (p = arts[i]; *p; p++)
			if (*p == '.')
			    *p = '/';
		    sprintf(buff, "%s/%s", Tradspooldir, arts[i]);
		    (void)unlink(buff);
		}
		token = SMstore(arth);
		if (token.type == TOKEN_EMPTY) {
		    fprintf(stderr, "Cannot store %s, skipping\n", fields[0]);
		    break;
		}
		arth.token = &token;
		DoMemArt(&arth, Overview, FALSE, out, index, TRUE);
		break;
	    default:
		fprintf(stderr, "Invalid message-id \"%s\" in history text\n", fields[0]);
		break;
	    }
	} else
	    break;
    }
    return TRUE;
}

/*
**  Process a single article.
*/
STATIC void
DoArticle(QIOSTATE *qp, struct stat *Sbp, char *name, FILE *out, BOOL RemoveBad, BOOL Update)
{
    static char		IGNORE[] = "Ignoring duplicate %s header in %s\n";
    static char		BADHDR[] = "Bad %s header in %s\n";
    char		*p;
    char		*MessageID;
    time_t		Arrived;
    time_t		Expires;
    time_t		Posted;
    int			i;

    /* Read the file for Message-ID and Expires header. */
    Arrived = Sbp->st_mtime;
    Expires = 0;
    MessageID = NULL;
    Posted = 0;
    while ((p = QIOread(qp)) != NULL && *p != '\0')
	switch (*p) {
	default:
	    break;
	case 'M': case 'm':
	    if (caseEQn(p, MESSAGEID, STRLEN(MESSAGEID))) {
		if (MessageID)
		    (void)fprintf(stderr, IGNORE, MESSAGEID, name);
		else {
		    MessageID = GetMessageID(&p[STRLEN(MESSAGEID)]);
		    if (*MessageID == '\0')
			(void)fprintf(stderr, BADHDR, MESSAGEID, name);
		}
	    }
	    break;
	case 'E': case 'e':
	    if (caseEQn(p, EXPIRES, STRLEN(EXPIRES))) {
		if (Expires > 0)
		    (void)fprintf(stderr, IGNORE, EXPIRES, name);
		else {
		    Expires = GetaDate(&p[STRLEN(EXPIRES)]);
		    if (Expires == 0)
			(void)fprintf(stderr, BADHDR, EXPIRES, name);
		}
	    }
	    break;
	case 'D': case 'd':
	    if (caseEQn(p, DATE, STRLEN(DATE))) {
		if (Posted > 0)
		    (void)fprintf(stderr, IGNORE, DATE, name);
		else {
		    Posted = GetaDate(&p[STRLEN(DATE)]);
		    if (Posted == 0)
			(void)fprintf(stderr, BADHDR, DATE, name);
		}
	    }
	    break;
	}

    /* Check for errors, close the input. */
    if (p == NULL) {
	if (QIOerror(qp)) {
	    (void)fprintf(stderr, "Can't read %s, %s\n",
		    name, strerror(errno));
	    return;
	}
	if (QIOtoolong(qp)) {
	    (void)fprintf(stderr, "Line too long in %s\n", name);
	    return;
	}
    }

    /* Make sure we have everything we need. */
    if (MessageID == NULL || *MessageID == '\0') {
	if (MessageID == NULL)
	    (void)fprintf(stderr, "No %s in %s\n", MESSAGEID, name);
	if (RemoveBad)
	    Removeit(name);
	return;
    }

    if (Update) {
	/* Server already know about this one? */
	if (dbzexists(HashMessageID(MessageID)))
	    return;
    }

    /* Output the line. */
    if (Posted == 0)
	Posted = Arrived;
    if (Expires > 0)
	i = fprintf(out, "%s%c%lu%c%lu%c%lu%c%s\n",
                    MessageID, HIS_FIELDSEP,
                    (unsigned long)Arrived, HIS_SUBFIELDSEP,
                    (unsigned long)Expires,
		    HIS_SUBFIELDSEP, (unsigned long)Posted, HIS_FIELDSEP,
                    name);
    else
	i = fprintf(out, "%s%c%lu%c%s%c%lu%c%s\n",
                    MessageID, HIS_FIELDSEP,
                    (unsigned long)Arrived, HIS_SUBFIELDSEP, HIS_NOEXP,
		    HIS_SUBFIELDSEP, (unsigned long)Posted, HIS_FIELDSEP,
                    name);
    if (i == EOF || ferror(out)) {
	(void)fprintf(stderr, "Can't write history line, %s\n",
		strerror(errno));
	exit(1);
    }
}


/*
**  Process one newsgroup directory.
*/
STATIC void
DoNewsgroup(char *group, FILE *out, BOOL RemoveBad, BOOL Update, TRANS Translate, char *Tradspooldir, BOOL UnlinkCrosspost, BOOL RemoveOld, BOOL Overview, FILE *index)
{
    DIR			*dp;
    DIRENTRY		*ep;
    QIOSTATE		*qp;
    char		*p;
    char		*q;
    struct stat		Sb;
    char		buff[SPOOLNAMEBUFF];
    char		artbuff[SPOOLNAMEBUFF];
#if	defined(HAVE_SYMLINK)
    char		linkbuff[SPOOLNAMEBUFF];
    int			oerrno;
#endif	/* defined(HAVE_SYMLINK) */
    ARTHANDLE		arth;
    char		*groupname;
    TOKEN		token;

    (void)strcpy(buff, group);
    groupname = COPY(group);
    for (p = group; *p; p++)
	if (*p == '.')
	    *p = '/';
    if (Translate == FROM_SPOOL)
	xchdir(Tradspooldir);
    else
	xchdir(innconf->patharticles);
    if (chdir(group) < 0)
	return;

    if ((dp = opendir(".")) == NULL) {
	(void)fprintf(stderr, "Can't opendir %s, %s\n", group, strerror(errno));
	return;
    }

    q = &buff[strlen(buff)];
    *q++ = '/';

    /* Read all entries in the directory. */
    while ((ep = readdir(dp)) != NULL) {
	p = ep->d_name;
	if (!CTYPE(isdigit, *p) || strspn(p, "0123456789") != strlen(p))
	    continue;

	strcpy(q, p);

	/* Is this a regular file? */
	if (stat(p, &Sb) < 0) {
	    (void)fprintf(stderr, "Can't stat %s, %s\n",
		    buff, strerror(errno));
#if	defined(HAVE_SYMLINK)
	    /* Symlink to nowhere? */
	    oerrno = errno;
	    (void)memset((POINTER)linkbuff, '\0', sizeof linkbuff);
	    if (lstat(p, &Sb) >= 0
	     && readlink(p, linkbuff, sizeof linkbuff - 1) >= 0) {
		linkbuff[sizeof linkbuff - 1] = '\0';
		(void)fprintf(stderr, "Bad symlink %s -> %s, %s\n",
			buff, linkbuff, strerror(oerrno));
		if (RemoveBad)
		    Removeit(buff);
	    }
#endif	/* defined(HAVE_SYMLINK) */
	    if (Translate == FROM_SPOOL && UnlinkCrosspost)
		(void)unlink(p);
	    continue;
	}
	if (!S_ISREG(Sb.st_mode)) {
	    (void)fprintf(stderr, "%s is not a file\n", buff);
	    continue;
	}
	/* ignore articles added after we started running because:
	** - innd should be catching these already if it is running.
	** - expire may have run and changed to a new history file.
	*/
	if (Sb.st_mtime > Now.time) {
	    continue;
	}
	if (Translate == FROM_SPOOL && UnlinkCrosspost)
	    if (Sb.st_nlink > 1) {
		/* assumes crossposted, if st_nlink > 1 */
		(void)unlink(p);
		continue;
	    }

	if (Translate == FROM_SPOOL) {
	    arth.arrived = Sb.st_mtime;
	    arth.token = (TOKEN *)NULL;
	    sprintf(artbuff, "%s/%s", group, p);
	    if (!ReadInMem(artbuff, &arth, Tradspooldir)) {
		break;
	    }
	    if (RemoveOld)
		(void)unlink(p);
	    token = SMstore(arth);
	    if (token.type == TOKEN_EMPTY) {
		fprintf(stderr, "Cannot store %s, skipping\n", artbuff);
		break;
	    }
	    arth.token = &token;
	    DoMemArt(&arth, Overview, FALSE, out, index, TRUE);
	} else {
	    /* Open the article. */
	    if ((qp = QIOopen(p)) == NULL) {
		(void)fprintf(stderr, "Can't open %s, %s\n",
			buff, strerror(errno));
		continue;
	    }
	    DoArticle(qp, &Sb, buff, out, RemoveBad, Update);
	    QIOclose(qp);
	}
    }

    (void)closedir(dp);
}


/*
**  Tell innd to add a history line.
*/
STATIC BOOL
AddThis(char *line, BOOL Verbose)
{
    int			i;
    char		*arrive;
    char		*exp;
    char		*posted;
    char		*paths;
    char		*av[6];

    if ((arrive = strchr(line, HIS_FIELDSEP)) == NULL
     || (exp = strchr(arrive + 1, HIS_SUBFIELDSEP)) == NULL
     || (posted = strchr(exp + 1, HIS_SUBFIELDSEP)) == NULL
     || (paths = strchr(exp + 1, HIS_FIELDSEP)) == NULL) {
	(void)fprintf(stderr, "Got bad history line \"%s\"\n", line);
	return FALSE;
    }
    av[0] = line;
    *arrive = '\0';
    av[1] = arrive + 1;
    *exp = '\0';
    av[2] = exp + 1;
    *posted = '\0';
    av[3] = posted + 1;
    *paths = '\0';
    av[4] = paths + 1;
    av[5] = NULL;
    if (EQ(av[2], HIS_NOEXP))
        av[2] = "0";

    i = ICCcommand(SC_ADDHIST, av, (char **)NULL);
    *arrive = HIS_FIELDSEP;
    *exp = HIS_SUBFIELDSEP;
    *posted = HIS_SUBFIELDSEP;
    *paths = HIS_FIELDSEP;
    if (i < 0) {
        (void)fprintf(stderr, "Can't add history line \"%s\", %s\n",
               line, strerror(errno));
        return FALSE;
    }
    if (Verbose)
	(void)fprintf(stderr, "Added %s\n", line);
    return TRUE;
}



/*
**  Close the server link, and exit.
*/
STATIC NORETURN
ErrorExit(BOOL Updating, BOOL Stopped)
{
    if (Updating) {
	if (!INNDrunning && Stopped && ICCgo(Reason) < 0)
	    (void)fprintf(stderr, "Can't restart server, %s\n",
		    strerror(errno));
	if (ICCclose() < 0)
	    (void)fprintf(stderr, "Can't close link, %s\n",
		    strerror(errno));
    }
    exit(1);
}


/*
**  Print a usage message and exit.
*/
STATIC NORETURN
Usage(void)
{
    (void)fprintf(stderr,
	    "Usage: makehistory [ -A file ][ -a file ][ -b ][ -D traditional_spooldir ][ -d overdir ][ -f file ][ -h oldhistory ][ -I indexfile ][ -i ][ -n ][ -O ][ -o ][ -T dir ][ -t {H|h|S|s} ][ -v ][ -f file ][ -R ][ -r ][ -S ][ -s size ][ -U ][ -u ]\n");
    exit(1);
    /* NOTREACHED */
}


int
main(int ac, char *av[])
{
    QIOSTATE		*qp;
    FILE		*out;
    char		*line;
    char		*p;
    char		*q;
    long		count;
    BUFFER		B;
    long		size;
    int			i;
    BOOL		JustRebuild;
    BOOL		DoRebuild;
    BOOL		IgnoreOld;
    BOOL		Overwrite;
    BOOL		Update;
    BOOL		RemoveBad;
    BOOL		Verbose;
    BOOL		Overview;
    BOOL		Notraditional;
    BOOL		RemoveOld;
    BOOL		UnlinkCrosspost;
    BOOL		Nowrite;
    BOOL		val;
    TRANS		Translate;
    char		temp[SMBUF];
    char		*TempTextFile;
    char		*Tradspooldir;
    char		*OldHistory;
    char		*tv[2];
    STRING		tmpdir;
    STRING		OverPath;
    char		*Tflag;
    char		*mode;
    char		*oldtemp;
    ARTHANDLE		*art = (ARTHANDLE *)NULL;
    FILE		*index = (FILE *)NULL;

    /* First thing, set up logging and our identity. */
    openlog("makehistory", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);     
	
    /* Set defaults. */
    if (ReadInnConf() < 0) exit(1);
    HISTORY = COPY(cpcatpath(innconf->pathdb, _PATH_HISTORY));
    ACTIVE = COPY(cpcatpath(innconf->pathdb, _PATH_ACTIVE));
    SCHEMA = COPY(cpcatpath(innconf->pathetc, _PATH_SCHEMA));
    TextFile = HISTORY;
    DoRebuild = TRUE;
    JustRebuild = FALSE;
    IgnoreOld = FALSE;
    Update = FALSE;
    RemoveBad = FALSE;
    Overwrite = FALSE;
    Verbose = FALSE;
    IndexFile = NULL;
    Overview = FALSE;
    Notraditional = FALSE;
    RemoveOld = FALSE;
    UnlinkCrosspost = FALSE;
    Nowrite = FALSE;
    Translate = NO_TRANS;
    Tradspooldir = innconf->patharticles;
    OldHistory = HISTORY;
    Tflag = "";
    mode = "w";
    size = 0;
    oldtemp = NULL;
    OverPath = NULL;
    tmpdir = innconf->pathtmp;
    (void)umask(NEWSUMASK);

    /* Parse JCL. */
    while ((i = getopt(ac, av, "A:a:bD:d:f:h:I:inOoRrs:ST:t:uUvx")) != EOF)
	switch (i) {
	default:
	    Usage();
	    /* NOTREACHED */
	case 'A':
	    mode = "a";
	    oldtemp = optarg;
	    break;
	case 'a':
	    ACTIVE = optarg;
	    break;
	case 'b':
	    RemoveBad = TRUE;
	    break;
	case 'D':
	    Tradspooldir = optarg;
	    break;
	case 'd':
	    OverPath = optarg;
	    break;
	case 'f':
	    TextFile = optarg;
	    break;
	case 'h':
	    OldHistory = optarg;
	    break;
	case 's':
	    size = atol(optarg);
	    /* FALLTHROUGH */
	case 'i':
	    IgnoreOld = TRUE;
	    break;
	case 'I':
	    IndexFile = optarg;
	    break;
	case 'n':
	    DoRebuild = FALSE;
	    break;
	case 'o':
	    Overwrite = TRUE;
	    IgnoreOld = TRUE;
	    break;
	case 'O':
	    Overview = TRUE;
	    break;
	case 'R':
	    RemoveOld = TRUE;
	    break;
	case 'r':
	    JustRebuild = TRUE;
	    break;
	case 'S':
	    Notraditional = TRUE;
	    break;
	case 'T':
	    tmpdir = optarg;
	    Tflag = NEW(char, 3 + strlen(optarg) + 1);
	    (void)sprintf(Tflag, "-T %s", optarg);
	    break;
	case 't':
	    switch(*optarg) {
	    case 'h':
	    case 'H':
		if (Translate != NO_TRANS)
		    Usage();
		Translate = FROM_HIST;
		break;
	    case 's':
	    case 'S':
		if (Translate != NO_TRANS)
		    Usage();
		Translate = FROM_SPOOL;
		break;
	    default:
		Usage();
	    }
	    break;
	case 'U':
	    UnlinkCrosspost = TRUE;
	    break;
	case 'u':
	    Update = TRUE;
	    break;
	case 'v':
	    Verbose = TRUE;
	    break;
	case 'x':
	    Nowrite = TRUE;
	    out = NULL;
	    break;
	}
    ac -= optind;
    av += optind;
    if (ac || (Overwrite && Update) || (Verbose && !Update) ||
	(Update && Translate != NO_TRANS) ||
	(Nowrite && !(Translate == FROM_HIST && Overview && !RemoveOld)) ||
	(!Nowrite && Translate == FROM_HIST && !OverPath))
	Usage();
    if ((p = strrchr(TextFile, '/')) == NULL) {
	/* find the default history file directory */
	HISTORYDIR = COPY(HISTORY);
	p = strrchr(HISTORYDIR, '/');
	if (p != NULL) {
	    *p = '\0';
	}
    } else {
	*p = '\0';
	HISTORYDIR = COPY(TextFile);
	*p = '/';
    }

    /* If we're not gonna scan the database, get out. */
    if (JustRebuild) {
	Rebuild(size, IgnoreOld, Overwrite);
	exit(0);
    }

    /* Get the time.  Only get it once, which is good enough. */
    if (GetTimeInfo(&Now) < 0) {
	(void)fprintf(stderr, "Can't get the time, %s\n", strerror(errno));
	exit(1);
    }

    /* Open history file. */
    xchdir(HISTORYDIR);

    if (Update || !Overwrite) {
	(void)sprintf(temp, "%s/histXXXXXX", tmpdir);
	(void)mktemp(temp);
	TempTextFile = oldtemp ? COPY(oldtemp) : COPY(temp);
    }
    else
	TempTextFile = NULL;
    if (Update) {
	if (ICCopen() < 0) {
	    (void)fprintf(stderr, "Can't talk to server, %s\n",
		    strerror(errno));
	    exit(1);
	}
	tv[0] = Reason;
	tv[1] = NULL;
	if (DoRebuild && ICCcommand(SC_THROTTLE, tv, (char **)NULL) < 0) {
	    (void)fprintf(stderr, "Can't throttle innd, %s\n",
		    strerror(errno));
	    exit(1);
	}
	if (!dbzinit(TextFile)) {
	    (void)fprintf(stderr, "Can't open dbz file, %s\n",
		    strerror(errno));
	    ErrorExit(TRUE, DoRebuild);
	}
    }

    if (!Nowrite && (out = fopen(TempTextFile ? TempTextFile : TextFile, mode)) == NULL) {
	(void)fprintf(stderr, "Can't write to history file, %s\n",
		strerror(errno));
	exit(1);
    }
    ARTreadschema(Overview);
    if (Overview) {
	OVERmmap = innconf->overviewmmap;
	if (Translate == FROM_HIST && OVERmmap)
	    val = TRUE;
	else
	    val = FALSE;
	if (!OVERsetup(OVER_MMAP, (void *)&val)) {
	    (void)fprintf(stderr, "Can't setup unified overview mmap\n");
	}
	val = TRUE;
	if (!OVERsetup(OVER_BUFFERED, (void *)&val)) {
	    fprintf(stderr, "Can't setup unified overview buffered\n");
	    exit(1);
	}
	if (!OVERsetup(OVER_PREOPEN, (void *)&val)) {
	    fprintf(stderr, "Can't setup unified overview preopen\n");
	    exit(1);
	}
	if (Nowrite)
	    mode = "r";
	if (!OVERsetup(OVER_MODE, (void *)mode)) {
	    fprintf(stderr, "Can't setup unified overview mode\n");
	    exit(1);
	}
	if (OverPath)
	    if (!OVERsetup(OVER_DIR, (void *)OverPath)) {
		fprintf(stderr, "Can't setup unified overview path\n");
		exit(1);
	    }
	if (!OVERinit()) {
	    (void)fprintf(stderr, "Can't initialize unified overview\n");
	}
	if (IndexFile && (index = fopen(IndexFile, "w")) == (FILE *)NULL) {
	    (void)fprintf(stderr, "Can't open index file, %s\n",
		strerror(errno));
	    exit(1);
	}
    }
    val = TRUE;
    if (!SMsetup(SM_RDWR, (void *)&val) || !SMsetup(SM_PREOPEN, (void *)&val)) {
	fprintf(stderr, "Can't setup storage manager\n");
	exit(1);
    }
    if (innconf->storageapi && !SMinit()) {
	fprintf(stderr, "Can't initialize storage manager: %s\n", SMerrorstr);
	exit(1);
    }

    if (Translate == NO_TRANS || Translate == FROM_SPOOL) {
	if (!Notraditional) {
	    /* Start scanning the directories. */
	    if ((qp = QIOopen(ACTIVE)) == NULL) {
		(void)fprintf(stderr, "Can't open %s, %s\n", ACTIVE, strerror(errno));
		exit(1);
	    }
	    for (count = 1; (line = QIOread(qp)) != NULL; count++) {
		if ((p = strchr(line, ' ')) == NULL) {
		    (void)fprintf(stderr, "Bad line %ld, \"%s\"\n", count, line);
		    continue;
		}
		*p = '\0';
		DoNewsgroup(line, out, RemoveBad, Update, Translate, Tradspooldir, UnlinkCrosspost, RemoveOld, Overview, index);
	    }
	    /* Test error conditions; QIOtoolong shouldn't happen. */
	    if (QIOtoolong(qp)) {
		(void)fprintf(stderr, "Line %ld is too long\n", count);
		ErrorExit(Update, DoRebuild);
	    }
	    if (QIOerror(qp)) {
		(void)fprintf(stderr, "Can't read %s around line %ld, %s\n",
			ACTIVE, count, strerror(errno));
		ErrorExit(Update, DoRebuild);
	    }
	    QIOclose(qp);
	}
	/* we'll not tranlate from spool thru storage api */
	if (Translate == NO_TRANS)
	    /* Start scanning articles stored by storage api */
	    while ((art = SMnext(art, RETR_HEAD)) != NULL) {
		if (art->len == 0) {
		    if (RemoveBad)
			(void)SMcancel(*art->token);
		    continue;
		}
		DoMemArt(art, Overview, Update, out, index, RemoveBad);
	    }
    } else if (Translate == FROM_HIST) {
	if (!TranslateFromHistory(out, OldHistory, Tradspooldir, UnlinkCrosspost, RemoveOld, Overview, index))
	    ErrorExit(Update, DoRebuild);
	if (Nowrite)
	    exit(0);
    }
    if (fflush(out) == EOF || ferror(out) || fclose(out) == EOF) {
	(void)fprintf(stderr, "Can't close history file, %s\n",
	strerror(errno));
	ErrorExit(Update, DoRebuild);
    }
    if (index != (FILE *)NULL &&
	(fflush(index) == EOF || ferror(index) || fclose(index) == EOF)) {
	(void)fprintf(stderr, "Can't close index file, %s\n",
	strerror(errno));
	ErrorExit(Update, DoRebuild);
    }

    /* Move. */
    xchdir(HISTORYDIR);

    if (Update) {
	INNDrunning = TRUE;
	if (!dbzclose()) {
	    (void)fprintf(stderr, "Can't close DBZ file, %s\n",
		    strerror(errno));
	    ErrorExit(Update, DoRebuild);
	}
	if (DoRebuild && ICCgo(Reason) < 0) {
	    (void)fprintf(stderr, "Can't restart innd, %s\n", strerror(errno));
	    ErrorExit(Update, DoRebuild);
	}
    }

    /* Make a temporary file, sort the text file into it. */
    (void)sprintf(temp, "%s/histXXXXXX", tmpdir);
    (void)mktemp(temp);
    i = 50 + strlen(TempTextFile ? TempTextFile : TextFile) + strlen(temp);
    p = NEW(char, i);
    (void)sprintf(p, "exec sort %s -t'%c' +1n -o %s %s",
	    Tflag, HIS_FIELDSEP, temp, TempTextFile ? TempTextFile : TextFile);

    i = system(p) >> 8;
    if (i != 0) {
	(void)fprintf(stderr, "Can't sort history file (exit %d), %s\n",
		i, strerror(errno));
	ErrorExit(Update, DoRebuild);
    }
    DISPOSE(p);

    if (TempTextFile) {
        if (unlink(TempTextFile) && errno != ENOENT)
	    (void)fprintf(stderr, "Can't remove \"%s\", %s\n",
		    TempTextFile, strerror(errno));
	DISPOSE(TempTextFile);
	TempTextFile = NULL;
    }

    /* Open the sorted file, get ready to write the final text file. */
    if ((qp = QIOopen(temp)) == NULL) {
	(void)fprintf(stderr, "Can't open work file \"%s\", %s\n",
		temp, strerror(errno));
	ErrorExit(Update, DoRebuild);
    }
    if (!Update && (out = fopen(TextFile, "w")) == NULL) {
	(void)fprintf(stderr, "Can't start writing %s, %s\n",
		TextFile, strerror(errno));
	(void)fprintf(stderr, "Work file %s untouched.\n", temp);
	ErrorExit(Update, DoRebuild);
    }

    /* Get space to keep the joined history lines. */
    B.Size = 100;
    B.Used = 0;
    B.Data = NEW(char, B.Size);
    p = COPY("");

    /* Read the sorted work file. */
    for (count = 0; (line = QIOread(qp)) != NULL; ) {
	count++;
	if ((q = strchr(line, HIS_FIELDSEP)) == NULL) {
	    (void)fprintf(stderr, "Work file line %ld had bad format\n",
		    count);
	    ErrorExit(Update, DoRebuild);
	}
	*q = '\0';
	if (EQ(p, line)) {
	    if (*p != '[') {
		/* Same Message-ID as last time -- get filename */
		if ((q = strchr(q + 1, HIS_FIELDSEP)) == NULL) {
		    (void)fprintf(stderr, "Work file line %ld missing filename\n",
			    count);
		    ErrorExit(Update, DoRebuild);
		}
		i = strlen(q);
		if (B.Size < B.Used + i + 3) {
		    B.Size = B.Used + i + 3;
		    RENEW(B.Data, char, B.Size);
		}
		*q = ' ';
		(void)strcpy(&B.Data[B.Used], q);
		B.Used += i;
	    }
	}
	else {
	    /* Different Message-ID; end old line, start new one. */
	    if (*p) {
		if (!Update)
		    (void)fprintf(out, "%s\n", B.Data);
		else if (!AddThis(B.Data, Verbose))
		    ErrorExit(Update, DoRebuild);
	    }
	    DISPOSE(p);
	    p = COPY(line);

	    *q = HIS_FIELDSEP;
	    i = strlen(line);
	    if (B.Size <= i) {
		B.Size = i + 2;
		RENEW(B.Data, char, B.Size);
	    }
	    (void)strcpy(B.Data, line);
	    B.Used = i;
	}
	if (!Update && ferror(out)) {
	    (void)fprintf(stderr, "Can't write output from line %ld, %s\n",
		    count, strerror(errno));
	    ErrorExit(Update, DoRebuild);
	}
    }

    /* Check for errors and close. */
    if (QIOtoolong(qp)) {
	(void)fprintf(stderr, "Line %ld is too long\n", count);
	ErrorExit(Update, DoRebuild);
    }
    if (QIOerror(qp)) {
	(void)fprintf(stderr, "Can't read work file, %s\n", strerror(errno));
	ErrorExit(Update, DoRebuild);
    }
    QIOclose(qp);
    if (unlink(temp) && errno != ENOENT)
	(void)fprintf(stderr, "Can't remove \"%s\", %s\n",
		temp, strerror(errno));

    if (*p) {
	/* Add tail end of last line. */
	if (!Update)
	    (void)fprintf(out, "%s\n", B.Data);
	else if (!AddThis(B.Data, Verbose))
	    ErrorExit(Update, DoRebuild);
    }

    /* Close the output file. */
    if (!Update) {
        if (fflush(out) == EOF || fclose(out) == EOF) {
	    (void)fprintf(stderr, "Can't close history file, %s\n",
		    strerror(errno));
	    ErrorExit(Update, DoRebuild);
        }
	if (DoRebuild)
	    Rebuild(size ? size : count, TRUE, Overwrite);
    }
    else if (ICCclose() < 0)
	(void)fprintf(stderr, "Can't close link, %s\n", strerror(errno));

    exit(0);
    /* NOTREACHED */
}

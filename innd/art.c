/*  $Id$
**
**  Article-processing.
*/
#include "config.h"
#include "clibrary.h"
#include <netinet/in.h>
#include <sys/uio.h>

#include "art.h"
#include "dbz.h"
#include "innd.h"
#include "ov.h"
#include "storage.h"

typedef struct iovec	IOVEC;

extern BOOL DoLinks;
extern BOOL DoCancels;

#if	defined(S_IXUSR)
#define EXECUTE_BITS	(S_IXUSR | S_IXGRP | S_IXOTH)
#else
#define EXECUTE_BITS	0111
#endif	/* defined(S_IXUSR) */

#if	!defined(S_ISDIR)
#define S_ISDIR(mode)	(((mode) & 0xF000) == 0x4000)
#endif


/*
**  For speed we build a binary tree of the headers, sorted by their
**  name.  We also store the header's Name fields in the tree to avoid
**  doing an extra indirection.
*/
typedef struct _TREE {
    STRING		Name;
    ARTHEADER		*Header;
    struct _TREE	*Before;
    struct _TREE	*After;
} TREE;

STATIC TREE		*ARTheadertree;

/*
**  For doing the overview database, we keep a list of the headers and
**  a flag saying if they're written in brief or full format.
*/
typedef struct _ARTOVERFIELD {
    ARTHEADER		*Header;
    BOOL		NeedHeader;
} ARTOVERFIELD;

STATIC ARTOVERFIELD		*ARTfields;


/*
**  General newsgroup we care about, and what we put in the Path line.
*/
STATIC char		ARTctl[] = "control";
STATIC char		ARTjnk[] = "junk";
STATIC char		*ARTpathme;

/*
**  Different types of rejected articles.
*/
typedef enum {REJECT_DUPLICATE, REJECT_SITE, REJECT_FILTER, REJECT_DISTRIB,
	      REJECT_GROUP, REJECT_UNAPP, REJECT_OTHER} Reject_type;

/*
**  Flag array, indexed by character.  Character classes for Message-ID's.
*/
STATIC char		ARTcclass[256];
#define CC_MSGID_ATOM	01
#define CC_MSGID_NORM	02
#define CC_HOSTNAME	04
#define ARTnormchar(c)	((ARTcclass[(unsigned char)(c)] & CC_MSGID_NORM) != 0)
#define ARTatomchar(c)	((ARTcclass[(unsigned char)(c)] & CC_MSGID_ATOM) != 0)
#define ARThostchar(c)	((ARTcclass[(unsigned char)(c)] & CC_HOSTNAME) != 0)

STATIC int	CRwithoutLF;
STATIC int	LFwithoutCR;

/*
**  The header table.  Not necessarily sorted, but the first character
**  must be uppercase.
*/
ARTHEADER	ARTheaders[] = {
    /*	Name			Type	... */
    {	"Approved",		HTstd },
#define _approved		 0
    {	"Control",		HTstd },
#define _control		 1
    {	"Date",			HTreq },
#define _date			 2
    {	"Distribution",		HTstd },
#define _distribution		 3
    {	"Expires",		HTstd },
#define _expires		 4
    {	"From",			HTreq },
#define _from			 5
    {	"Lines",		HTstd },
#define _lines			 6
    {	"Message-ID",		HTreq },
#define _message_id		 7
    {	"Newsgroups",		HTreq },
#define _newsgroups		 8
    {	"Path",			HTreq },
#define _path			 9
    {	"Reply-To",		HTstd },
#define _reply_to		10
    {	"Sender",		HTstd },
#define _sender			11
    {	"Subject",		HTreq },
#define _subject		12
    {	"Supersedes",		HTstd },
#define _supersedes		13
    {	"Bytes",		HTstd },
#define _bytes			14
    {	"Also-Control",		HTstd },
    {	"References",		HTstd },
#define _references		16
    {	"Xref",			HTsav },
#define _xref			17
    {	"Keywords",		HTstd },
#define _keywords		18
    {   "X-Trace",		HTstd },
#define _xtrace			19
    {	"Date-Received",	HTobs },
    {	"Posted",		HTobs },
    {	"Posting-Version",	HTobs },
    {	"Received",		HTobs },
    {	"Relay-Version",	HTobs },
    {	"NNTP-Posting-Host",	HTstd },
    {	"Followup-To",		HTstd },
#define _followup_to		26
    {	"Organization",		HTstd },
    {	"Content-Type",		HTstd },
    {	"Content-Base",		HTstd },
    {	"Content-Disposition",	HTstd },
    {	"X-Newsreader",		HTstd },
    {	"X-Mailer",		HTstd },
    {	"X-Newsposter",		HTstd },
    {	"X-Cancelled-By",	HTstd },
    {	"X-Canceled-By",	HTstd },
    {	"Cancel-Key",		HTstd },
};

ARTHEADER	*ARTheadersENDOF = ENDOF(ARTheaders);

#if defined(DO_PERL) || defined(DO_PYTHON)
const char	*filterPath;
#endif /* DO_PERL || DO_PYTHON */



/*
**  Mark that the site gets this article.
*/
void SITEmark(SITE *sp, NEWSGROUP *ngp) {
    SITE	*funnel; 

    sp->Sendit = TRUE; 
    if (sp->ng == NULL) 
	sp->ng = ngp; 
    if (sp->Funnel != NOSITE) { 
	funnel = &Sites[sp->Funnel]; 
	if (funnel->ng == NULL) 
	    funnel->ng = ngp; 
    } 
}

/*
**
*/
BOOL ARTreadschema(void)
{
    static char			*SCHEMA = NULL;
    FILE		        *F;
    int		                i;
    char	 	        *p;
    ARTOVERFIELD	        *fp;
    ARTHEADER		        *hp;
    BOOL			ok;
    char			buff[SMBUF];
    BOOL			foundxref = FALSE;
    BOOL			foundxreffull = FALSE;

    if (ARTfields != NULL) {
	DISPOSE(ARTfields);
	ARTfields = NULL;
    }

    /* Open file, count lines. */
    if (SCHEMA == NULL)
	SCHEMA = COPY(cpcatpath(innconf->pathetc, _PATH_SCHEMA));
    if ((F = Fopen(SCHEMA, "r", TEMPORARYOPEN)) == NULL)
	return FALSE;
    for (i = 0; fgets(buff, sizeof buff, F) != NULL; i++)
	continue;
    (void)fseek(F, (OFFSET_T)0, SEEK_SET);
    ARTfields = NEW(ARTOVERFIELD, i + 1);

    /* Parse each field. */
    for (ok = TRUE, fp = ARTfields; fgets(buff, sizeof buff, F) != NULL; ) {
	/* Ignore blank and comment lines. */
	if ((p = strchr(buff, '\n')) != NULL)
	    *p = '\0';
	if ((p = strchr(buff, COMMENT_CHAR)) != NULL)
	    *p = '\0';
	if (buff[0] == '\0')
	    continue;
	if ((p = strchr(buff, ':')) != NULL) {
	    *p++ = '\0';
	    fp->NeedHeader = EQ(p, "full");
	}
	else
	    fp->NeedHeader = FALSE;
	if (caseEQ(buff, "Xref")) {
	    foundxref = TRUE;
	    foundxreffull = fp->NeedHeader;
	}
	for (hp = ARTheaders; hp < ENDOF(ARTheaders); hp++)
	    if (caseEQ(buff, hp->Name)) {
		fp->Header = hp;
		break;
	    }
	if (hp == ENDOF(ARTheaders)) {
	    syslog(L_ERROR, "%s bad_schema unknown header \"%s\"",
		LogName, buff);
	    ok = FALSE;
	    continue;
	}
	fp++;
    }
    fp->Header = NULL;

    (void)Fclose(F);
    if (!foundxref || !foundxreffull) {
	syslog(L_FATAL, "%s 'Xref:full' must be included in %s", LogName, SCHEMA);
	exit(1); 
    }
    return ok;
}


/*
**  Build a balanced tree for the headers in subscript range [lo..hi).
**  This only gets called once, and the tree only has about 20 entries,
**  so we don't bother to unroll the recursion.
*/
static TREE *
ARTbuildtree(Table, lo, hi)
    ARTHEADER	**Table;
    int		lo;
    int		hi;
{
    int		mid;
    TREE	*tp;

    mid = lo + (hi - lo) / 2;
    tp = NEW(TREE, 1);
    tp->Header = Table[mid];
    tp->Name = tp->Header->Name;
    if (mid == lo)
	tp->Before = NULL;
    else
	tp->Before = ARTbuildtree(Table, lo, mid);
    if (mid == hi - 1)
	tp->After = NULL;
    else
	tp->After = ARTbuildtree(Table, mid + 1, hi);
    return tp;
}


/*
**  Sorting predicate for qsort call in ARTsetup.
*/
STATIC int
ARTcompare(p1, p2)
    CPOINTER p1;
    CPOINTER p2;
{
    ARTHEADER	**h1;
    ARTHEADER	**h2;

    h1 = (ARTHEADER **) p1;
    h2 = (ARTHEADER **) p2;
    return strcasecmp(h1[0]->Name, h2[0]->Name);
}


/*
**  Setup the article processing.
*/
void ARTsetup(void)
{
    STRING	        p;
    ARTHEADER	        *hp;
    ARTHEADER		**table;
    int	                i;

    /* Set up the character class tables.  These are written a
     * little strangely to work around a GCC2.0 bug. */
    (void)memset((POINTER)ARTcclass, 0, sizeof ARTcclass);
    p = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    while ((i = *p++) != 0) {
        ARTcclass[i] = CC_HOSTNAME | CC_MSGID_ATOM | CC_MSGID_NORM;
    }
    p = "!#$%&'*+-/=?^_`{|}~";
    while ((i = *p++) != 0) {
	ARTcclass[i] = CC_MSGID_ATOM | CC_MSGID_NORM;
    }
    p = "\"(),.:;<@[\\]";
    while ((i = *p++) != 0) {
	ARTcclass[i] = CC_MSGID_NORM;
    }

    /* The RFC's don't require it, but we add underscore to the list of valid
     * hostname characters. */
    ARTcclass['.'] |= CC_HOSTNAME;
    ARTcclass['-'] |= CC_HOSTNAME;
    ARTcclass['_'] |= CC_HOSTNAME;

    /* Allocate space in the header table. */
    for (hp = ARTheaders; hp < ENDOF(ARTheaders); hp++) {
	hp->Size = strlen(hp->Name);
	hp->Allocated = hp->Value == NULL && hp->Type != HTobs
			&& hp != &ARTheaders[_bytes];
	if (hp->Allocated)
	    hp->Value = NEW(char, MAXHEADERSIZE*2);
    }

    /* Build the header tree. */
    table = NEW(ARTHEADER*, SIZEOF(ARTheaders));
    for (i = 0; i < SIZEOF(ARTheaders); i++)
	table[i] = &ARTheaders[i];
    qsort((POINTER)table, SIZEOF(ARTheaders), sizeof *table, ARTcompare);
    ARTheadertree = ARTbuildtree(table, 0, SIZEOF(ARTheaders));
    DISPOSE(table);

    /* Get our Path name, kill trailing !. */
    ARTpathme = COPY(Path.Data);
    ARTpathme[Path.Used - 1] = '\0';

    /* Set up database; ignore errors. */
    (void)ARTreadschema();
}


STATIC void
ARTfreetree(tp)
    TREE	*tp;
{
    TREE	*next;

    for ( ; tp != NULL; tp = next) {
	if (tp->Before)
	    ARTfreetree(tp->Before);
	next = tp->After;
	DISPOSE(tp);
    }
}


void ARTclose(void)
{
    ARTHEADER	*hp;

    /* Free space in the header table. */
    for (hp = ARTheaders; hp < ENDOF(ARTheaders); hp++)
	if (hp->Allocated)
	    DISPOSE(hp->Value);

    if (ARTfields != NULL) {
	DISPOSE(ARTfields);
	ARTfields = NULL;
    }
    ARTfreetree(ARTheadertree);
}

/*
**  Parse a Path line, splitting it up into NULL-terminated array of strings.
**  The argument is modified!
*/
STATIC char **ARTparsepath(char *p, int *countp)
{
    STATIC char		*NULLPATH[1] = { NULL };
    STATIC int		oldlength;
    STATIC char		**hosts;
    int	                i;
    char	        **hp;

    /* We can be called with a non-existant or empty path. */
    if (p == NULL || *p == '\0') {
	*countp = 0;
	return NULLPATH;
    }

    /* Get an array of character pointers. */
    i = strlen(p);
    if (hosts == NULL) {
	oldlength = i;
	hosts = NEW(char*, oldlength + 1);
    }
    else if (oldlength <= i) {
	oldlength = i;
	RENEW(hosts, char*, oldlength + 1);
    }

    /* Loop over text. */
    for (hp = hosts; *p; *p++ = '\0') {
	/* Skip leading separators. */
	for (; *p && !ARThostchar(*p); p++)
	    continue;
	if (*p == '\0')
	    break;

	/* Mark the start of the host, move to the end of it. */
	for (*hp++ = p; *p && ARThostchar(*p); p++)
	    continue;
	if (*p == '\0')
	    break;
    }
    *hp = NULL;
    *countp = hp - hosts;
    return hosts;
}

/* Write an article using the storage api.  Put it together in memory and
   call out to the api. */
STATIC TOKEN ARTstore(BUFFER *Article, ARTDATA *Data) {
    char                *path;
    char                *p;
    char                *end;
    unsigned long       size;
    char                *artbuff;
    ARTHANDLE           arth;
    int                 i;
    TOKEN               result;
    char		bytesbuff[SMBUF];
    static BUFFER	Headers;

    result.type = TOKEN_EMPTY;

    if (((path = (char *)HeaderFindMem(Article->Data, Article->Used, "Path", 4)) == NULL)
	|| (path == Article->Data)) {
	/* This should not happen */
	syslog(L_ERROR, "%s internal %s no Path header",
	       Data->MessageID, LogName);
	return result;
    }

    size = Article->Used + 6 + ARTheaders[_xref].Length + 4 + 3 + Path.Used + Pathalias.Used + 64 + 1;
    p = artbuff = NEW(char, size);
    if ((Path.Used >= Article->Used - (int)(path - Article->Data)) || strncmp(Path.Data, path, Path.Used) != 0) {
	Hassamepath = FALSE;
	memcpy(p, Article->Data, path - Article->Data);
	p += path - Article->Data;
	memcpy(p, Path.Data, Path.Used);
	p += Path.Used;
	if (AddAlias) {
	    memcpy(p, Pathalias.Data, Pathalias.Used);
	    p += Pathalias.Used;
	}
	memcpy(p, path, Data->Body - path - 1);
	p += Data->Body - path - 1;
    } else {
	Hassamepath = TRUE;
	if (AddAlias) {
	    memcpy(p, Article->Data, path - Article->Data);
	    p += path - Article->Data;
	    memcpy(p, Path.Data, Path.Used);
	    p += Path.Used;
	    memcpy(p, Pathalias.Data, Pathalias.Used);
	    p += Pathalias.Used;
	    memcpy(p, path + Path.Used, Data->Body - path - Path.Used - 1);
	    p += Data->Body - path - Path.Used - 1;
	} else {
	    memcpy(p, Article->Data, Data->Body - Article->Data - 1);
	    p += Data->Body - Article->Data - 1;
	}
    }

    if (NeedPath) {
	Data->Path = path;
	for (i = Data->Body - path; --i >= 0; path++)
	    if (*path == '\r' || *path == '\n')
		break;
	Data->PathLength = path - Data->Path;
    }

    if (ARTheaders[_lines].Found == 0) {
	sprintf(Data->Lines, "Lines: %d\r\n", Data->LinesValue);
	i = strlen(Data->Lines);
	memcpy(p, Data->Lines, i);
	p += i;
	/* Install in header table; STRLEN("Lines: ") == 7. */
	strcpy(ARTheaders[_lines].Value, Data->Lines + 7);
	ARTheaders[_lines].Length = i - 9;
	ARTheaders[_lines].Found = 1;
    }
    
    memcpy(p, "Xref: ", 6);
    p += 6;
    memcpy(p, HDR(_xref), ARTheaders[_xref].Length);
    end = p += ARTheaders[_xref].Length;
    end += 2; /* include trailing "\r\n" for Headers generation */
    memcpy(p, "\r\n\r\n", 4);
    p += 4;
    ARTheaders[_xref].Found = 1;
    memcpy(p, Data->Body, &Article->Data[Article->Used] - Data->Body);
    p += &Article->Data[Article->Used] - Data->Body;
    memcpy(p, ".\r\n", 3);
    p += 3;

    Data->SizeValue = p - artbuff;
    sprintf(Data->Size, "%ld", Data->SizeValue);
    Data->SizeLength = strlen(Data->Size);
    HDR(_bytes) = Data->Size;
    ARTheaders[_bytes].Length = Data->SizeLength;
    ARTheaders[_bytes].Found = 1;

    arth.data = artbuff;
    arth.len = Data->SizeValue;
    arth.arrived = (time_t)0;
    arth.token = (TOKEN *)NULL;

    SMerrno = SMERR_NOERROR;
    result = SMstore(arth);
    if (result.type == TOKEN_EMPTY) {
	if (SMerrno == SMERR_NOMATCH)
	    ThrottleNoMatchError();
	else if (SMerrno != SMERR_NOERROR)
	    IOError("SMstore", SMerrno);
	DISPOSE(artbuff);
	return result;
    }

    if (!NeedHeaders) {
	DISPOSE(artbuff);
	return result;
    }

        /* Figure out how much space we'll need and get it. */
    (void)sprintf(bytesbuff, "Bytes: %ld\r\n", size);

    if (Headers.Data == NULL) {
	Headers.Size = end - artbuff;
	Headers.Data = NEW(char, Headers.Size + 1);
    }
    else if (Headers.Size <= (end - artbuff)) {
	Headers.Size = end - artbuff;
	RENEW(Headers.Data, char, Headers.Size + 1);
    }

    /* Add the data. */
    BUFFset(&Headers, bytesbuff, strlen(bytesbuff));
    BUFFappend(&Headers, artbuff, end - artbuff);
    BUFFtrimcr(&Headers);
    Data->Headers = &Headers;

    DISPOSE(artbuff);
    return result;
}

/*
**  Parse a header that starts at in, copying it to out.  Return pointer to
**  the start of the next header and fill in *deltap with what should
**  get added to the output pointer.  (This nicely lets us clobber obsolete
**  headers by setting it to zero.)
*/
STATIC char *ARTparseheader(char *in, char *out, int *deltap, STRING *errorp)
{
    static char		buff[SMBUF];
    static char		COLONSPACE[] = "No colon-space in \"%s\" header";
    char	        *start;
    TREE	        *tp;
    ARTHEADER	        *hp;
    char	        c;
    char	        *p;
    int	                i;
    char	        *colon;

    /* Find a non-continuation line. */
    for (colon = NULL, start = out; ; ) {
	switch (*in) {
	case '\0':
	    *errorp = "EOF in headers";
	    return NULL;
	case ':':
	    if (colon == NULL) {
		colon = out;
		if (start == colon) {
		    *errorp = "Field without name in header";
		    return NULL;
		}
	    }
	    break;
	}
	if ((*out++ = *in++) == '\n' && !ISWHITE(*in))
	    break;
    }
    *deltap = out - start;
    if (colon == NULL || !ISWHITE(colon[1])) {
	if ((p = strchr(start, '\n')) != NULL)
	    *p = '\0';
	(void)sprintf(buff, COLONSPACE, MaxLength(start, start));
	*errorp = buff;
	return NULL;
    }

    /* See if this is a system header.  A fairly tightly-coded
     * binary search. */
    c = CTYPE(islower, *start) ? toupper(*start) : *start;
    for (*colon = '\0', tp = ARTheadertree; tp; ) {
	if ((i = c - tp->Name[0]) == 0
	 && (i = strcasecmp(start, tp->Name)) == 0)
	    break;
	if (i < 0)
	    tp = tp->Before;
	else
	    tp = tp->After;
    }
    *colon = ':';

    if (tp == NULL) {
	/* Not a system header, make sure we have <word><colon><space>. */
	for (p = colon; --p > start; )
	    if (ISWHITE(*p)) {
		(void)sprintf(buff, "Space before colon in \"%s\" header",
			MaxLength(start, start));
		*errorp = buff;
		return NULL;
	    }
	if (p < start)
	    return NULL;
	return in;
    }

    /* Found a known header; is it obsolete? */
    hp = tp->Header;
    if (hp->Type == HTobs) {
	*deltap = 0;
	return in;
    }

    /* Skip the Bytes header */
    if (hp == &ARTheaders[_bytes])
	return in;

    if (hp->Type == HTsav) {
	*deltap = 0;
    }

    /* If body of header is all blanks, drop the header. */
    for (p = colon + 1; ISWHITE(*p); p++)
	continue;
    if (*p == '\0' || *p == '\n' || (p[0] == '\r' && p[1] == '\n')) {
	*deltap = 0;
	return in;
    }

    hp->Found++;

    /* Zap in the canonical form of the header, undoing the \0 that
     * strcpy put out (strncpy() spec isn't trustable, unfortunately). */
    (void)strcpy(start, hp->Name);
    start[hp->Size] = ':';

    /* Copy the header if not too big. */
    i = (out - 1 - 1) - p;
    if (i >= MAXHEADERSIZE) {
	(void)sprintf(buff, "\"%s\" header too long", hp->Name);
	*errorp = buff;
	return NULL;
    }
    hp->Length = i;
    (void)memcpy((POINTER)hp->Value, (POINTER)p, (SIZE_T)i);
    hp->Value[i] = '\0';

    return in;
}


/*
**  Check Message-ID format based on RFC 822 grammar, except that (as per
**  RFC 1036) whitespace, non-printing, and '>' characters are excluded.
**  Based on code by Paul Eggert posted to news.software.b on 22-Nov-90
**  in <#*tyo2'~n@twinsun.com>, with additional email discussion.
**  Thanks, Paul.
*/
BOOL ARTidok(const char *MessageID)
{
    int	                c;
    const char	        *p;

    /* Check the length of the message ID. */
    if (MessageID == NULL || strlen(MessageID) > NNTP_MSGID_MAXLEN)
        return FALSE;

    /* Scan local-part:  "< atom|quoted [ . atom|quoted]" */
    p = MessageID;
    if (*p++ != '<')
	return FALSE;
    for (; ; p++) {
	if (ARTatomchar(*p))
	    while (ARTatomchar(*++p))
		continue;
	else {
	    if (*p++ != '"')
		return FALSE;
	    for ( ; ; ) {
		switch (c = *p++) {
		case '\\':
		    c = *p++;
		    /* FALLTHROUGH */
		default:
		    if (ARTnormchar(c))
			continue;
		    return FALSE;
		case '"':
		    break;
		}
		break;
	    }
	}
	if (*p != '.')
	    break;
    }

    /* Scan domain part:  "@ atom|domain [ . atom|domain] > \0" */
    if (*p++ != '@')
	return FALSE;
    for ( ; ; p++) {
	if (ARTatomchar(*p))
	    while (ARTatomchar(*++p))
		continue;
	else {
	    if (*p++ != '[')
		return FALSE;
	    for ( ; ; ) {
		switch (c = *p++) {
		case '\\':
		    c = *p++;
		    /* FALLTHROUGH */
		default:
		    if (ARTnormchar(c))
			continue;
		    /* FALLTHROUGH */
		case '[':
		    return FALSE;
		case ']':
		    break;
		}
		break;
	    }
	}
	if (*p != '.')
	    break;
    }

    return *p == '>' && *++p == '\0';
}


/*
**  Clean up an article.  This is mainly copying in-place, stripping bad
**  headers.  Also fill in the article data block with what we can find.
**  Return NULL if the article is okay, or a string describing the error.
*/
STATIC STRING ARTclean(BUFFER *Article, ARTDATA *Data)
{
    static char		buff[SMBUF];
    ARTHEADER		*hp;
    char	        *in;
    char	        *out;
    int	                i;
    char	        *p;
    STRING		error;
    int			delta;

    /* Read through the headers one at a time. */
    Data->Feedsite = "?";
    Data->Size[0] = '0';
    Data->Size[1] = '\0';
    for (hp = ARTheaders; hp < ENDOF(ARTheaders); hp++) {
	if (hp->Value && hp->Type != HTobs)
	    *hp->Value = '\0';
	hp->Found = 0;
    }
    CRwithoutLF = LFwithoutCR = 0;
    for (error = NULL, in = out = Article->Data; ; out += delta, in = p) {
	if (*in == '\0') {
	    error = "No body";
	    break;
	}
	if (in[0] == '\r' && in[1] != '\n')
	    CRwithoutLF++;
	if (in[0] == '\n' && in > Article->Data && in[-1] != '\r')
	    LFwithoutCR++;
	if (((*in == '\n' || (in[0] == '\r' && in[1] == '\n'))
             && out > Article->Data && out[-1] == '\n'))
	    /* Found the header separator; break out. */
	    break;

	/* Check the validity of this header. */
	if ((p = ARTparseheader(in, out, &delta, &error)) == NULL)
	    break;
    }
    Data->Body = out + 1;
    in++;

    /* Try to set this now, so we can report it in errors. */
    Data->MessageID = NULL;
    p = HDR(_message_id);
    if (*p && ARTidok(p))
	Data->MessageID = p;
    Data->MessageIDLength = Data->MessageID ? strlen(Data->MessageID) : 0;
    if (error == NULL && Data->MessageID == NULL)
	error = "Bad \"Message-ID\" header";

    if (error)
	return error;

    /* Make sure all the headers we need are there, and no duplicates. */
    for (hp = ARTheaders; hp < ENDOF(ARTheaders); hp++)
	if (hp->Type == HTreq) {
	    if (*hp->Value == '\0') {
		(void)sprintf(buff, "Missing \"%s\" header", hp->Name);
		return buff;
	    }
	    if (hp->Found > 1) {
		(void)sprintf(buff, "Duplicate \"%s\" header", hp->Name);
		return buff;
	    }
	}

    /* Scan the body, counting lines. */
    for (i = 0; *in; ) {
	if (in[0] == '\r' && in[1] != '\n')
	    CRwithoutLF++;
	if (in[0] == '\n' && in[-1] != '\r')
	    LFwithoutCR++;
	if (*in == '\n')
	    i++;
	*out++ = *in++;
    }
    *out = '\0';
    if (Article->Data + Article->Used != in + 1) {
	i++;
	(void)sprintf(buff, "Line %d includes null character", i);
	return buff;
    }
    Article->Used = out - Article->Data;
    Data->LinesValue = (i - 1 < 0) ? 0 : (i - 1);
    
    if (innconf->linecountfuzz) {
	p = HDR(_lines);
	if (*p && (delta = i - atoi(p)) != 0 && abs(delta) >
						innconf->linecountfuzz) {
	    if ((in = strchr(p, '\n')) != NULL)
		*in = '\0';
	    (void)sprintf(buff, "Linecount %s != %d +- %d",
		MaxLength(p, p), i, innconf->linecountfuzz);
	    return buff;
	}
    }

    /* Is article too old? */
    p = HDR(_date);
    if ((Data->Posted = parsedate(p, &Now)) == -1) {
	(void)sprintf(buff, "Bad \"Date\" header -- \"%s\"", MaxLength(p, p));
	return buff;
    }
    if (innconf->artcutoff && Data->Posted < Now.time - innconf->artcutoff) {
	(void)sprintf(buff, "Too old -- \"%s\"", MaxLength(p, p));
	return buff;
    }
    if (Data->Posted > Now.time + DATE_FUZZ) {
	(void)sprintf(buff, "Article posted in the future -- \"%s\"",
		MaxLength(p, p));
	return buff;
    }
    Data->Arrived = Now.time;
    p = HDR(_expires);
    Data->Expires = 0;
    if (*p != '\0' && (Data->Expires = parsedate(p, &Now)) == -1) {
#if	0
	(void)sprintf(buff, "Bad \"Expires\" header -- \"%s\"",
		MaxLength(p, p));
	return buff;
#endif
    }

    /* Colon or whitespace in the Newsgroups header? */
    if (strchr(HDR(_newsgroups), ':') != NULL)
	return "Colon in \"Newsgroups\" header";
    for (p = HDR(_newsgroups); *p; p++)
	if (ISWHITE(*p)) {
	    (void)sprintf(buff,
		    "Whitespace in \"Newsgroups\" header -- \"%s\"",
		    MaxLength(HDR(_newsgroups), p));
	    return buff;
	}

    return NULL;
}


/*
**  Start a log message about an article.
*/
STATIC void
ARTlog(Data, code, text)
    ARTDATA		*Data;
    char		code;
    char		*text;
{
    int			i;
    BOOL		Done;

    /* We could be a bit faster by not dividing Now.usec by 1000,
     * but who really wants to log at the Microsec level? */
    Done = code == ART_ACCEPT || code == ART_JUNK;
    if (text)
	i = fprintf(Log, "%.15s.%03d %c %s %s %s%s",
		ctime(&Now.time) + 4, (int)(Now.usec / 1000),
		code, Data->Feedsite,
		Data->MessageID == NULL ? "(null)" : Data->MessageID,
		text, Done ? "" : "\n");
    else
	i = fprintf(Log, "%.15s.%03d %c %s %s%s",
		ctime(&Now.time) + 4, (int)(Now.usec / 1000),
		code, Data->Feedsite,
		Data->MessageID == NULL ? "(null)" : Data->MessageID,
		Done ? "" : "\n");
    if (i == EOF || (Done && !BufferedLogs && fflush(Log)) || ferror(Log)) {
	i = errno;
	syslog(L_ERROR, "%s cant write log_start %m", LogName);
	IOError("logging article", i);
	clearerr(Log);
    }
}


/*
**  We are going to reject an article, record the reason and
**  and the article.
*/
STATIC void
ARTreject(code, cp, buff, article)
    Reject_type code;
    CHANNEL     *cp;
    char	*buff;
    BUFFER	*article;
{
  /* Remember why the article was rejected (for the status file) */

  switch (code) {
    case REJECT_DUPLICATE:
      cp->Duplicate++;
      cp->DuplicateSize += article->Used;
      break;
    case REJECT_SITE:
      cp->Unwanted_s++;
      break;
    case REJECT_FILTER:
      cp->Unwanted_f++;
      break;
    case REJECT_DISTRIB:
      cp->Unwanted_d++;
      break;
    case REJECT_GROUP:
      cp->Unwanted_g++;
      break;
    case REJECT_UNAPP:
      cp->Unwanted_u++;
      break;
    case REJECT_OTHER:
      cp->Unwanted_o++;
      break;
    default:
      /* should never be here */
      syslog(L_NOTICE, "%s unknown reject type received by ARTreject()",
	     LogName);
      break;
  }
      /* error */
}


/*
**  Verify if a cancel message is valid.  If the user posting the cancel
**  matches the user who posted the article, return the list of filenames
**  otherwise return NULL.
*/
STATIC TOKEN *ARTcancelverify(const ARTDATA *Data, const char *MessageID, const HASH hash)
{
    char	        *p, *q;
    char	        *local;
    char		buff[SMBUF];
    ARTHANDLE		*art;
    TOKEN		*token;

    if ((token = HISfilesfor(hash)) == NULL)
	return NULL;
    if ((art = SMretrieve(*token, RETR_HEAD)) == NULL)
	return NULL;
    if ((local = (char *)HeaderFindMem(art->data, art->len, "Sender", 6)) == NULL
     && (local = (char *)HeaderFindMem(art->data, art->len, "From", 4)) == NULL) {
	SMfreearticle(art);
	return NULL;
    }
    for (p = local; p < art->data + art->len; p++) {
	if (*p == '\r' || *p == '\n')
	    break;
    }
    if (p == art->data + art->len) {
	SMfreearticle(art);
	return NULL;
    }
    q = NEW(char, p - local + 1);
    memcpy(q, local, p - local);
    SMfreearticle(art);
    q[p - local] = '\0';
    HeaderCleanFrom(q);

    /* Compare canonical forms. */
    p = COPY(Data->Poster);
    HeaderCleanFrom(p);
    if (!EQ(q, p)) {
	token = NULL;
	(void)sprintf(buff, "\"%.50s\" wants to cancel %s by \"%.50s\"",
		      p, MaxLength(MessageID, MessageID), q);
	ARTlog(Data, ART_REJECT, buff);
    }
    DISPOSE(p);
    DISPOSE(q);
    return token;
}


/*
**  Process a cancel message.
*/
void ARTcancel(const ARTDATA *Data, const char *MessageID, const BOOL Trusted)
{
    char		buff[SMBUF+16];
    HASH                hash;
    TOKEN		*token;

    TMRstart(TMR_ARTCNCL);
    if (!DoCancels && !Trusted) {
	TMRstop(TMR_ARTCNCL);
	return;
    }

    if (!ARTidok(MessageID)) {
	syslog(L_NOTICE, "%s bad cancel Message-ID %s", Data->Feedsite,
	       MaxLength(MessageID, MessageID));
	TMRstop(TMR_ARTCNCL);
        return;
    }

    hash = HashMessageID(MessageID);
    
    if (!HIShavearticle(hash)) {
	/* Article hasn't arrived here, so write a fake entry using
	 * most of the information from the cancel message. */
	if (innconf->verifycancels && !Trusted) {
	    TMRstop(TMR_ARTCNCL);
	    return;
	}
	HISremember(hash);
	(void)sprintf(buff, "Cancelling %s", MaxLength(MessageID, MessageID));
	ARTlog(Data, ART_CANC, buff);
	TMRstop(TMR_ARTCNCL);
	return;
    }
    if (innconf->verifycancels) {
	token = Trusted ? HISfilesfor(hash)
	    : ARTcancelverify(Data, MessageID, hash);
    } else {
	token = HISfilesfor(hash);
    }
    if (token == NULL) {
	TMRstop(TMR_ARTCNCL);
	return;
    }
    
    /* Get stored message and zap them. */
    if (!SMcancel(*token) && SMerrno != SMERR_NOENT && SMerrno != SMERR_UNINIT)
	syslog(L_ERROR, "%s cant cancel %s", LogName, TokenToText(*token));
    if (innconf->immediatecancel && !SMflushcacheddata(SM_CANCELEDART))
	syslog(L_ERROR, "%s cant cancel cached %s", LogName, TokenToText(*token));
    (void)sprintf(buff, "Cancelling %s", MaxLength(MessageID, MessageID));
    ARTlog(Data, ART_CANC, buff);
    TMRstop(TMR_ARTCNCL);
}



/*
**  Process a control message.  Cancels are handled here, but any others
**  are passed out to an external program in a specific directory that
**  has the same name as the first word of the control message.
*/
STATIC void ARTcontrol(ARTDATA *Data, HASH hash, char *Control, CHANNEL *cp)
{
    char	        *p;
    char		buff[SMBUF];
    char		*av[6];
    struct stat		Sb;
    char	        c;
    char		**hops;
    int			hopcount;

    /* See if it's a cancel message. */
    c = *Control;
    if (c == 'c' && EQn(Control, "cancel", 6)) {
	for (p = &Control[6]; ISWHITE(*p); p++)
	    continue;
	if (*p && ARTidok(p))
	    ARTcancel(Data, p, FALSE);
	return;
    }

    /* Nip off the first word into lowercase. */
    for (p = Control; *p && !ISWHITE(*p); p++)
	if (CTYPE(isupper, *p))
	    *p = tolower(*p);
    if (*p)
	*p++ = '\0';

    /* Treat the control message as a place to send the article, if
     * the name is "safe" -- no slashes in the pathname. */
    if (p - Control + STRLEN( _PATH_BADCONTROLPROG) >= SMBUF-4
     || strchr(Control, '/') != NULL)
	FileGlue(buff, innconf->pathcontrol, '/', _PATH_BADCONTROLPROG);
    else {
	FileGlue(buff, innconf->pathcontrol, '/', Control);
	if (stat(buff, &Sb) < 0 || (Sb.st_mode & EXECUTE_BITS) == 0
         || S_ISDIR(Sb.st_mode))
	    FileGlue(buff, innconf->pathcontrol, '/', _PATH_BADCONTROLPROG);
    }

    /* If it's an ihave or sendme, check the site named in the message. */
    if ((c == 'i' && EQ(Control, "ihave"))
     || (c == 's' && EQ(Control, "sendme"))) {
	while (ISWHITE(*p))
	    p++;
	if (*p == '\0') {
	    syslog(L_NOTICE, "%s malformed %s no site %s",
		    LogName, Control, Data->Name);
	    return;
	}
	if (EQ(p, ARTpathme)) {
	    /* Do nothing -- must have come from a replicant. */
	    syslog(L_NOTICE, "%s %s_from_me %s",
		Data->Feedsite, Control, Data->Name);
	    return;
	}
	if (!SITEfind(p)) {
	    if (c == 'i')
		syslog(L_ERROR, "%s bad_ihave in %s",
		    Data->Feedsite, Data->Newsgroups);
	    else
		syslog(L_ERROR, "%s bad_sendme dont feed %s",
		    Data->Feedsite, Data->Name);
	    return;
	}
    }

    if (!(innconf->usecontrolchan)) {
	/* Build the command vector and execute it. */
	av[0] = buff;
	av[1] = COPY(Data->Poster);
	av[2] = COPY(Data->Replyto);
	av[3] = Data->Name;
	if (innconf->logipaddr) {
	    hops = ARTparsepath(HDR(_path), &hopcount);
	    av[4] = hops && hops[0] ? hops[0] : CHANname(cp);
	} else {
	    av[4] = (char *)Data->Feedsite;
	}
	av[5] = NULL;
	HeaderCleanFrom(av[1]);
	HeaderCleanFrom(av[2]);
	if (Spawn(innconf->nicekids, STDIN, (int)fileno(Errlog),
					(int)fileno(Errlog), av) < 0)
	    /* We know the strrchr below can't fail. */
	    syslog(L_ERROR, "%s cant spawn %s for %s %m",
		LogName, MaxLength(av[0], strrchr(av[0], '/')), Data->Name);
	DISPOSE(av[1]);
	DISPOSE(av[2]);
    }
}


/*
**  Split a Distribution header, making a copy and skipping leading and
**  trailing whitespace (which the RFC allows).
*/
STATIC void DISTparse(char **list, ARTDATA *Data)
{
    static BUFFER	Dist;
    char	        *p;
    char	        *q;
    int	                i;
    int	                j;

    /* Get space to store the copy. */
    for (i = 0, j = 0; (p = list[i]) != NULL; i++)
	j += 1 + strlen(p);
    if (Dist.Data == NULL) {
	Dist.Size = j;
	Dist.Data = NEW(char, Dist.Size + 1);
    }
    else if (Dist.Size <= j) {
	Dist.Size = j + 16;
	RENEW(Dist.Data, char, Dist.Size + 1);
    }

    /* Loop over each element, skip and trim whitespace. */
    for (q = Dist.Data, i = 0, j = 0; (p = list[i]) != NULL; i++) {
	while (ISWHITE(*p))
	    p++;
	if (*p) {
	    if (j)
		*q++ = ',';
	    for (list[j++] = p; *p && !ISWHITE(*p); )
		*q++ = *p++;
	    *p = '\0';
	}
    }
    list[j] = NULL;

    *q = '\0';
    Data->Distribution = Dist.Data;
    Data->DistributionLength = q - Dist.Data;
}


/*
**  A somewhat similar routine, except that this handles negated entries
**  in the list and is used to check the distribution sub-field.
*/
STATIC BOOL DISTwanted(char **list, char *p)
{
    char	        *q;
    char	        c;
    BOOL	        sawbang;

    for (sawbang = FALSE, c = *p; (q = *list) != NULL; list++)
	if (*q == '!') {
	    sawbang = TRUE;
	    if (c == *++q && EQ(p, q))
		return FALSE;
	}
	else if (c == *q && EQ(p, q))
	    return TRUE;

    /* If we saw any !foo's and didn't match, then assume they are all
     * negated distributions and return TRUE, else return false. */
    return sawbang;
}


/*
**  See if any of the distributions in the article are wanted by the site.
*/
STATIC BOOL DISTwantany(char **site, char **article)
{
    for ( ; *article; article++)
	if (DISTwanted(site, *article))
	    return TRUE;
    return FALSE;
}


/*
**  Send the current article to all sites that would get it if the
**  group were created.
*/
STATIC void ARTsendthegroup(char *name)
{
    SITE	        *sp;
    int	                i;
    NEWSGROUP		*ngp;

    for (ngp = NGfind(ARTctl), sp = Sites, i = nSites; --i >= 0; sp++)
	if (sp->Name != NULL && SITEwantsgroup(sp, name)) {
	    SITEmark(sp, ngp);
	}
}


/*
**  Check if site doesn't want this group even if it's crossposted
**  to a wanted group.
*/
STATIC void ARTpoisongroup(char *name)
{
    SITE	        *sp;
    int	                i;

    for (sp = Sites, i = nSites; --i >= 0; sp++)
	if (sp->Name != NULL && (sp->PoisonEntry || ME.PoisonEntry) &&
            SITEpoisongroup(sp, name))
	    sp->Poison = TRUE;
}


/*
** Assign article numbers to the article and create the Xref line.
** If we end up not being able to write the article, we'll get "holes"
** in the directory and active file.
*/
STATIC void ARTassignnumbers(void)
{
    char	        *p;
    int	                i;
    NEWSGROUP	        *ngp;

    p = HDR(_xref);
    strncpy(p, Path.Data, Path.Used - 1);
    p += Path.Used - 1;
    for (i = 0; (ngp = GroupPointers[i]) != NULL; i++) {
	/* If already went to this group (i.e., multiple groups are aliased
	 * into it), then skip it. */
	if (ngp->PostCount > 0)
	    continue;

	/* Bump the number. */
	ngp->PostCount++;
	ngp->Last++;
	if (!FormatLong(ngp->LastString, (long)ngp->Last, ngp->Lastwidth)) {
	    syslog(L_ERROR, "%s cant update_active %s", LogName, ngp->Name);
	    continue;
	}
	ngp->Filenum = ngp->Last;
	(void)sprintf(p, " %s:%lu", ngp->Name, ngp->Filenum);
	p += strlen(p);
    }
    ARTheaders[_xref].Length=strlen(HDR(_xref));
}


/*
**  Parse the data from the xref header and assign the numbers.
**  This involves replacing the GroupPointers entries.
*/
STATIC BOOL
ARTxrefslave()
{
    char	*p;
    char	*q;
    char	*name;
    char	*next;
    NEWSGROUP	*ngp;
    int	        i, len;
    char        xrefbuf[MAXHEADERSIZE*2];
    BOOL	nogroup = TRUE;

    if (!ARTheaders[_xref].Found)
    	return FALSE;
    if ((q = name = strchr(HDR(_xref), ' ')) == NULL)
    	return FALSE;
    while ( *++name == ' ' );
    if ( *name == '\0' )
    	return FALSE;

    p = xrefbuf;
    strncpy(p, HDR(_xref), q - HDR(_xref));
    p += q - HDR(_xref);
    
    for (i = 0; *name; name = next) {
	/* Mark end of this entry and where next one starts. */
	if ((next = strchr(name, ' ')) != NULL) {
	    len = strlen(name);
	    for (next++; name + len > next && *next == ' '; next++)
		*next = '\0';
	    if (name + len == next)
		next = "";
	} else
	    next = "";

	/* Split into news.group/# */
	if ((q = strchr(name, ':')) == NULL) {
	    syslog(L_ERROR, "%s bad_format %s", LogName, name);
	    continue;
	}
	*q = '\0';
	if ((ngp = NGfind(name)) == NULL) {
	    syslog(L_ERROR, "%s bad_newsgroup %s", LogName, name);
	    continue;
	}
	ngp->Filenum = atol(q + 1);

	/* Update active file if we got a new high-water mark. */
	if (ngp->Last < ngp->Filenum) {
	    ngp->Last = ngp->Filenum;
	    if (!FormatLong(ngp->LastString, (long)ngp->Last,
		    ngp->Lastwidth)) {
		syslog(L_ERROR, "%s cant update_active %s",
		    LogName, ngp->Name);
		continue;
	    }
	}

	/* Mark that this group gets the article. */
	ngp->PostCount++;
	GroupPointers[i++] = ngp;

	/* Turn news.group/# into news.group:#, append to Xref. */
	sprintf(p, " %s:%ld", name, ngp->Filenum);
	len = strlen(p);
	p += len;
	nogroup = FALSE;
    }
    if (nogroup)
	return FALSE;
    ARTheaders[_xref].Length = strlen(xrefbuf);
    strcpy(HDR(_xref), xrefbuf);
    return TRUE;
}

/*
**  Return TRUE if a list of strings has a specific one.  This is a
**  generic routine, but is used for seeing if a host is in the Path line.
*/
STATIC BOOL ListHas(char **list, char *p)
{
    char	        *q;
    char	        c;

    for (c = *p; (q = *list) != NULL; list++)
	if (caseEQ(p, q))
	    return TRUE;
    return FALSE;
}


/*
**  Propagate an article to the sites have "expressed an interest."
*/
STATIC void ARTpropagate(ARTDATA *Data, char **hops, int hopcount, char **list, BOOL ControlStore, BOOL OverviewCreated)
{
    SITE	        *sp;
    int	                i;
    int	                j;
    int 	        Groupcount;
    int			Followcount;
    int			Crosscount;
    char	        *p, *q;
    SITE	        *funnel;
    BUFFER	        *bp;
    BOOL		sendit;

    /* Work out which sites should really get it. */
    Groupcount = Data->Groupcount;
    Followcount = Data->Followcount;
    Crosscount = Groupcount + Followcount * Followcount;
    for (sp = Sites, i = nSites; --i >= 0; sp++) {
	if ((sp->IgnoreControl && ControlStore) ||
	    (sp->NeedOverviewCreation && !OverviewCreated))
	    sp->Sendit = FALSE;
	if (sp->Seenit || !sp->Sendit)
	    continue;
	sp->Sendit = FALSE;
	
	if (sp->Originator) {
	    if (!HDR(_xtrace)[0]) {
		if (!sp->FeedwithoutOriginator)
		    continue;
	    } else {
		if ((p = strchr(HDR(_xtrace), ' ')) != NULL) {
		    *p = '\0';
		    for (j = 0, sendit = FALSE; (q = sp->Originator[j]) != NULL; j++) {
		        if (*q == '@') {
			    if (wildmat(HDR(_xtrace), &q[1])) {
			        *p = ' ';
			        sendit = FALSE;
				break;
			    }
		        } else {
			    if (wildmat(HDR(_xtrace), q))
			        sendit = TRUE;
		        }
		    }
		    *p = ' ';
		    if (!sendit)
			continue;
		} else
		    continue;
	    }
	}

	if (sp->Master != NOSITE && Sites[sp->Master].Seenit)
	    continue;

	if (sp->MaxSize && Data->SizeValue > sp->MaxSize)
	    /* Too big for the site. */
	    continue;

	if (sp->MinSize && Data->SizeValue < sp->MinSize)
	    /* Too small for the site. */
	    continue;

	if ((sp->Hops && hopcount > sp->Hops)
	 || (!sp->IgnorePath && ListHas(hops, sp->Name))
	 || (sp->Groupcount && Groupcount > sp->Groupcount)
	 || (sp->Followcount && Followcount > sp->Followcount)
	 || (sp->Crosscount && Crosscount > sp->Crosscount))
	    /* Site already saw the article; path too long; or too much
	     * cross-posting. */
	    continue;

	if (list
	 && sp->Distributions
	 && !DISTwantany(sp->Distributions, list))
	    /* Not in the site's desired list of distributions. */
	    continue;
	if (sp->DistRequired && list == NULL)
	    /* Site requires Distribution header and there isn't one. */
	    continue;

	if (sp->Exclusions) {
	    for (j = 0; (p = sp->Exclusions[j]) != NULL; j++)
		if (ListHas(hops, p))
		    break;
	    if (p != NULL)
		/* A host in the site's exclusion list was in the Path. */
		continue;
	}

	/* Write that the site is getting it, and flag to send it. */
	if (innconf->logsitename && fprintf(Log, " %s", sp->Name) == EOF || ferror(Log)) {
	    j = errno;
	    syslog(L_ERROR, "%s cant write log_site %m", LogName);
	    IOError("logging site", j);
	    clearerr(Log);
	}
	sp->Sendit = TRUE;
	sp->Seenit = TRUE;
	if (sp->Master != NOSITE)
	    Sites[sp->Master].Seenit = TRUE;
    }
    if (putc('\n', Log) == EOF
     || (!BufferedLogs && fflush(Log))
     || ferror(Log)) {
	syslog(L_ERROR, "%s cant write log_end %m", LogName);
	clearerr(Log);
    }

    /* Handle funnel sites. */
    for (sp = Sites, i = nSites; --i >= 0; sp++)
	if (sp->Sendit && sp->Funnel != NOSITE) {
	    sp->Sendit = FALSE;
	    funnel = &Sites[sp->Funnel];
	    funnel->Sendit = TRUE;
	    if (funnel->FNLwantsnames) {
		bp = &funnel->FNLnames;
		p = &bp->Data[bp->Used];
		if (bp->Used) {
		    *p++ = ' ';
		    bp->Used++;
		}
		bp->Used += strlen(strcpy(p, sp->Name));
	    }
	}
}






#if	defined(DO_KEYWORDS)
/*
** Additional code for sake of manufacturing Keywords: headers out of air
** in order to provide better (scorable) XOVER data, containing bits of
** article body content which have a reasonable expectation of utility.
**
** Basic idea: Simple word-counting.  We find words in the article body,
** separated by whitespace.  Remove punctuation.  Sort words, count
** unique words, sort those counts.  Write the resulting Keywords: header
** containing the poster's original Keywords: (if any) followed by a magic
** cookie separator and then the sorted list of words.
*/

#include <regex.h>	/* for regular expression-based word elimination. */
			/* see code between lines of "-----------------". */

#define	MIN_WORD_LENGTH	3	/* 1- and 2-char words don't count. */
#define	MAX_WORD_LENGTH	28	/* fits "antidisestablishmentarianism". */

/*
** A trivial structure for keeping track of words via both
** index to the overall word list and their counts.
*/
struct word_entry {
    int	index;
    int	length;
    int	count;
};

/*
** Wrapper for qsort(3) comparison of word_entry (frequency).
*/

int
wvec_freq_cmp(p1, p2)
    CPOINTER p1;
    CPOINTER p2;
{
    struct word_entry *w1, *w2;

    w1 = (struct word_entry *) p1;
    w2 = (struct word_entry *) p2;
    return w2->count - w1->count;	/* decreasing sort */
}

/*
** Wrapper for qsort(3) comparison of word_entry (word length).
*/

int
wvec_length_cmp(p1, p2)
    CPOINTER p1;
    CPOINTER p2;
{
    struct word_entry *w1, *w2;

    w1 = (struct word_entry *) p1;
    w2 = (struct word_entry *) p2;
    return w2->length - w1->length;	/* decreasing sort */
}

/*
** Wrapper for qsort(3), for pointer-to-pointer strings.
*/

int
ptr_strcmp(p1, p2)
    CPOINTER p1;
    CPOINTER p2;
{
    int cdiff;
    char **s1, **s2;

    s1 = (char **) p1;
    s2 = (char **) p2;
    if (cdiff = (**s1)-(**s2))
	return cdiff;
    return strcmp((*s1)+1, (*s2)+1);
}

/*
**  Build new Keywords.
*/

STATIC void
ARTmakekeys(hp, body, v, l)
    register ARTHEADER	*hp;		/* header data */
    register char	*body, *v;	/* article body, old kw value */
    register int	l;		/* old kw length */
{

    int		word_count, word_length, bodylen, word_index, distinct_words;
    int		last;
    char	*text, *orig_text, *text_end, *this_word, *chase, *punc;
    static struct word_entry	*word_vec;
    static char	**word;
    static char	*whitespace  = " \t\r\n";

    /* ---------------------------------------------------------------- */
    /* Prototype setup: Regex match preparation. */
    static	int	regex_lib_init = 0;
    static	regex_t	preg;
    static	char	*elim_regexp = "^\\([-+/0-9][-+/0-9]*\\|.*1st\\|.*2nd\\|.*3rd\\|.*[04-9]th\\|about\\|after\\|ago\\|all\\|already\\|also\\|among\\|and\\|any\\|anybody\\|anyhow\\|anyone\\|anywhere\\|are\\|bad\\|because\\|been\\|before\\|being\\|between\\|but\\|can\\|could\\|did\\|does\\|doing\\|done\\|dont\\|during\\|eight\\|eighth\\|eleven\\|else\\|elsewhere\\|every\\|everywhere\\|few\\|five\\|fifth\\|first\\|for\\|four\\|fourth\\|from\\|get\\|going\\|gone\\|good\\|got\\|had\\|has\\|have\\|having\\|he\\|her\\|here\\|hers\\|herself\\|him\\|himself\\|his\\|how\\|ill\\|into\\|its\\|ive\\|just\\|kn[eo]w\\|least\\|less\\|let\\|like\\|look\\|many\\|may\\|more\\|m[ou]st\\|myself\\|next\\|nine\\|ninth\\|not\\|now\\|off\\|one\\|only\\|onto\\|our\\|out\\|over\\|really\\|said\\|saw\\|says\\|second\\|see\\|set\\|seven\\|seventh\\|several\\|shall\\|she\\|should\\|since\\|six\\|sixth\\|some\\|somehow\\|someone\\|something\\|somewhere\\|such\\|take\\|ten\\|tenth\\|than\\|that\\|the\\|their\\!|them\\|then\\|there\\|therell\\|theres\\|these\\|they\\|thing\\|things\\|third\\|this\\|those\\|three\\|thus\\|together\\|told\\|too\\|twelve\\|two\\|under\\|upon\\|very\\|via\\|want\\|wants\\|was\\|wasnt\\|way\\|were\\|weve\\|what\\|whatever\\|when\\|where\\|wherell\\|wheres\\|whether\\|which\\|while\\|who\\|why\\|will\\|will\\|with\\|would\\|write\\|writes\\|wrote\\|yes\\|yet\\|you\\|your\\|youre\\|yourself\\)$";

    if (word_vec == 0) {
	word_vec = NEW(struct word_entry, innconf->keymaxwords);
	if (word_vec == 0)
	    return;
	word = NEW(char *,innconf->keymaxwords);
	if (word == NULL) {
	    DISPOSE(word_vec);
	    return;
	}
    }
    
    if (regex_lib_init == 0) {
	regex_lib_init++;

	if (regcomp(&preg, elim_regexp, REG_ICASE|REG_NOSUB) != 0) {
	    syslog(L_FATAL, "%s regcomp failure", LogName);
	    abort();
	}
    }
    /* ---------------------------------------------------------------- */

    /* first re-init kw from original value. */
    if (l > innconf->keylimit - (MAX_WORD_LENGTH+5))	/* mostly arbitrary cutoff: */
        l = innconf->keylimit - (MAX_WORD_LENGTH+5);	/* room for minimal word vec */
    hp->Value = malloc(innconf->keylimit+1);
    if ((v != NULL) && (*v != '\0')) {
        strncpy(hp->Value, v, l);
        hp->Value[l] = '\0';
    } else
        *hp->Value = '\0';
    l = hp->Length = strlen(hp->Value);

    /*
     * now figure acceptable extents, and copy body to working string.
     * (Memory-intensive for hefty articles: limit to non-ABSURD articles.)
     */
    bodylen = strlen(body);
    if ((bodylen < 100) || (bodylen > innconf->keyartlimit)) /* too small/big to bother */
	return;
    
    orig_text = text = strdup(body);	/* orig_text is for free() later on */
    if (text == (char *) NULL)  /* malloc failure? */
	return;

    
    text_end = text + bodylen;

    /* abusive punctuation stripping: turn it all into SPCs. */
    for (punc = text; *punc; punc++)
	if (!CTYPE(isalpha, *punc))
	    *punc = ' ';

    /* move to first word. */
    text += strspn(text, whitespace);
    word_count = 0;

    /* hunt down words */
    while ((text < text_end) &&		/* while there might be words... */
	   (*text != '\0') &&
	   (word_count < innconf->keymaxwords)) {

	/* find a word. */
	word_length = strcspn(text, whitespace);
	if (word_length == 0)
	    break;			/* no words left */

	/* bookkeep to save word location, then move through text. */
	word[word_count++] = this_word = text;
	text += word_length;
	*(text++) = '\0';
	text += strspn(text, whitespace);	/* move to next word. */

	/* 1- and 2-char words don't count, nor do excessively long ones. */
	if ((word_length < MIN_WORD_LENGTH) ||
	    (word_length > MAX_WORD_LENGTH)) {
	    word_count--;
	    continue;
	}

	/* squash to lowercase. */
	for (chase = this_word; *chase; chase++)
	    if (CTYPE(isupper, *chase))
		*chase = tolower(*chase);
    }

    /* If there were no words, we're done. */
    if (word_count < 1)
	goto out;

    /* Sort the words. */
    qsort(word, word_count, sizeof(word[0]), ptr_strcmp);

    /* Count unique words. */
    distinct_words = 0;			/* the 1st word is "pre-figured". */
    word_vec[0].index = 0;
    word_vec[0].length = strlen(word[0]);
    word_vec[0].count = 1;

    for (word_index = 1;		/* we compare (N-1)th and Nth words. */
	 word_index < word_count;
	 word_index++) {
	if (strcmp(word[word_index-1], word[word_index]) == 0)
	    word_vec[distinct_words].count++;
	else {
	    distinct_words++;
	    word_vec[distinct_words].index = word_index;
	    word_vec[distinct_words].length = strlen(word[word_index]);
	    word_vec[distinct_words].count = 1;
	}
    }

    /* Sort the counts. */
    distinct_words++;			/* we were off-by-1 until this. */
    qsort(word_vec, distinct_words, sizeof(struct word_entry), wvec_freq_cmp);

    /* Sub-sort same-frequency words on word length. */
    for (last = 0, word_index = 1;	/* again, (N-1)th and Nth entries. */
	 word_index < distinct_words;
	 word_index++) {
	if (word_vec[last].count != word_vec[word_index].count) {
	    if ((word_index - last) != 1)	/* 2+ entries to sub-sort. */
		qsort(&word_vec[last], word_index - last,
		      sizeof(struct word_entry), wvec_length_cmp);
	    last = word_index;
	}
    }
    /* do it one last time for the only-one-appearance words. */
    if ((word_index - last) != 1)
	qsort(&word_vec[last], word_index - last,
	      sizeof(struct word_entry), wvec_length_cmp);

    /* Scribble onto end of Keywords:. */
    strcpy(hp->Value + l, ",\377");		/* magic separator, 'ÿ' */
    for (chase = hp->Value + l + 2, word_index = 0;
	 word_index < distinct_words;
	 word_index++) {
	/* ---------------------------------------------------------------- */
	/* "noise" words don't count */
	if (regexec(&preg, word[word_vec[word_index].index], 0, NULL, 0) == 0)
	    continue;
	/* ---------------------------------------------------------------- */

	/* add to list. */
	*chase++ = ',';
	strcpy(chase, word[word_vec[word_index].index]);
	chase += word_vec[word_index].length;

	if (chase - hp->Value > (innconf->keylimit - (MAX_WORD_LENGTH + 4)))
	    break;
    }
    /* note #words we didn't get to add. */
    /* This code can potentially lead to a buffer overflow if the number of
       ignored words is greater than 100, under some circumstances.  It's
       temporarily disabled until fixed. */
#if 0
    if (word_index < distinct_words - 1)
	sprintf(chase, ",%d", (distinct_words - word_index) - 1);
#endif
    hp->Length = strlen(hp->Value);

out:
    /* We must dispose of the original strdup'd text area. */
    free(orig_text);
}
#endif	/* defined(DO_KEYWORDS) */



/*
**  Build up the overview data.
*/
STATIC void ARTmakeoverview(ARTDATA *Data, BOOL Filename)
{
    static char			SEP[] = "\t";
    static char			COLONSPACE[] = ": ";
    static BUFFER		Overview;
    ARTOVERFIELD	        *fp;
    ARTHEADER		        *hp;
    char		        *p;
    int		                i;
#if	defined(DO_KEYWORDS)
    char			*key_old_value = NULL;
    int				key_old_length = 0;
#endif	/* defined(DO_KEYWORDS) */


    if (ARTfields == NULL) {
	/* User error. */
	return;
    }
    
    /* Setup. */
    if (Overview.Data == NULL)
	Overview.Data = NEW(char, 1);
    Data->Overview = &Overview;

    BUFFset(&Overview, "", 0);

    /* Write the data, a field at a time. */
    for (fp = ARTfields; fp->Header; fp++) {
	if (fp != ARTfields)
	    BUFFappend(&Overview, SEP, STRLEN(SEP));
	hp = fp->Header;

#if	defined(DO_KEYWORDS)
	if (innconf->keywords) {
	    /* Ensure that there are Keywords: to shovel. */
	    if (hp == &ARTheaders[_keywords]) {
		key_old_value  = hp->Value;
		key_old_length = hp->Length;
		ARTmakekeys(hp, Data->Body, key_old_value, key_old_length);
		hp->Found++;	/* now faked, whether present before or not. */
	    }
	}
#endif	/* defined(DO_KEYWORDS) */

	if (!hp->Found)
	    continue;
	if (fp->NeedHeader) {
	    BUFFappend(&Overview, hp->Name, hp->Size);
	    BUFFappend(&Overview, COLONSPACE, STRLEN(COLONSPACE));
	}
	i = Overview.Left;
	if (caseEQ(hp->Name, "Newsgroups")) {
	    /* HDR(_newsgroups) is separated by '\0', so use Data->Newsgroups
	       instead */
	    BUFFappend(&Overview, Data->Newsgroups, Data->NewsgroupsLength);
	} else {
	    BUFFappend(&Overview, hp->Value, hp->Length);
	}
	for (p = &Overview.Data[i]; i < Overview.Left; p++, i++)
	    if (*p == '\t' || *p == '\n' || *p == '\r')
		*p = ' ';

#if	defined(DO_KEYWORDS)
	if (innconf->keywords) {
	    if (key_old_value) {
		if (hp->Value)
		    free(hp->Value);		/* malloc'd within */
		hp->Value  = key_old_value;
		hp->Length = key_old_length;
		hp->Found--;
		key_old_value = NULL;
	    }
	}
#endif	/* defined(DO_KEYWORDS) */
    }
}


/*
**  This routine is the heart of it all.  Take a full article, parse it,
**  file or reject it, feed it to the other sites.  Return the NNTP
**  message to send back.
*/
STRING ARTpost(CHANNEL *cp)
{
    static BUFFER	Files;
    static BUFFER	Header;
    static char		buff[SPOOLNAMEBUFF];
    char	        *p;
    int	                i;
    int	                j;
    NEWSGROUP	        *ngp;
    NEWSGROUP	        **ngptr;
    int	                *isp;
    SITE	        *sp;
    ARTDATA		Data;
    BOOL		Approved;
    BOOL		Accepted;
    BOOL		LikeNewgroup;
    BOOL		ToGroup;
    BOOL		GroupMissing;
    BOOL		MadeOverview = FALSE;
    BOOL		ControlStore = FALSE;
    BOOL		NonExist = FALSE;
    BOOL		OverviewCreated = FALSE;
    BOOL                IsControl = FALSE;
    BUFFER		*article;
    HASH                hash;
    char		**groups;
    char		**hops;
    int			hopcount;
    char		**distributions;
    STRING		error;
    char		ControlWord[SMBUF];
    int			oerrno;
    TOKEN               token;
    int			canpost;
    char		*groupbuff[2];
#if defined(DO_PERL) || defined(DO_PYTHON)
    char		*filterrc;
#endif /* defined(DO_PERL) || defined(DO_PYTHON) */
    BOOL		NoHistoryUpdate;

    /* Preliminary clean-ups. */
    article = &cp->In;
    error = ARTclean(article, &Data);
    
    /* Fill in other Data fields. */
    Data.Poster = HDR(_sender);
    if (*Data.Poster == '\0')
	Data.Poster = HDR(_from);
    Data.Replyto = HDR(_reply_to);
    if (*Data.Replyto == '\0')
	Data.Replyto = HDR(_from);
    hops = ARTparsepath(HDR(_path), &hopcount);
    if (error != NULL &&
	(Data.MessageID == NULL || hops == 0 || hops[0]=='\0')) {
	sprintf(buff, "%d %s", NNTP_REJECTIT_VAL, error);
	return buff;
    }
    AddAlias = FALSE;
    if (Pathalias.Data != NULL && !ListHas(hops, innconf->pathalias))
	AddAlias = TRUE;
    if (innconf->logipaddr) {
	Data.Feedsite = RChostname(cp);
	if (Data.Feedsite == NULL)
	    Data.Feedsite = CHANname(cp);
	if (strcmp("0.0.0.0", Data.Feedsite) == 0)
	    Data.Feedsite = hops && hops[0] ? hops[0] : CHANname(cp);
    } else {
	Data.Feedsite = hops && hops[0] ? hops[0] : CHANname(cp);
    }
    Data.FeedsiteLength = strlen(Data.Feedsite);
    (void)sprintf(Data.TimeReceived, "%lu", Now.time);
    Data.TimeReceivedLength = strlen(Data.TimeReceived);

    hash = HashMessageID(Data.MessageID);
    Data.Hash = &hash;
    if (HIShavearticle(hash)) {
	sprintf(buff, "%d Duplicate", NNTP_REJECTIT_VAL);
	ARTlog(&Data, ART_REJECT, buff);
	ARTreject(REJECT_DUPLICATE, cp, buff, article);
	return buff;
    }

    if (error != NULL) {
	sprintf(buff, "%d %s", NNTP_REJECTIT_VAL, error);
	ARTlog(&Data, ART_REJECT, buff);
	if (innconf->remembertrash && (Mode == OMrunning) && !HISremember(hash))
	    syslog(L_ERROR, "%s cant write history %s %m",
		       LogName, Data.MessageID);
	ARTreject(REJECT_OTHER, cp, buff, article);
	return buff;
    }

    /* And now check the path for unwanted sites -- Andy */
    for( j = 0 ; ME.Exclusions && ME.Exclusions[j] ; j++ ) {
        if( ListHas(hops, ME.Exclusions[j]) ) {
	    (void)sprintf(buff, "%d Unwanted site %s in path",
			NNTP_REJECTIT_VAL, ME.Exclusions[j]);
	    ARTlog(&Data, ART_REJECT, buff);
	    if (innconf->remembertrash && (Mode == OMrunning) &&
			!HISremember(hash))
		syslog(L_ERROR, "%s cant write history %s %m",
		       LogName, Data.MessageID);
	    ARTreject(REJECT_SITE, cp, buff, article);
	    return buff;
        }
    }

#if defined(DO_PERL) || defined(DO_PYTHON)
    filterPath = HeaderFindMem(article->Data, article->Used, "Path", 4) ;
#endif /* DO_PERL || DO_PYHTON */

#if defined(DO_PYTHON)
    TMRstart(TMR_PYTHON);
    filterrc = PYartfilter(Data.Body,
			   &article->Data[article->Used] - Data.Body,
			   Data.LinesValue);
    TMRstop(TMR_PYTHON);
    if (filterrc != NULL) {
        (void)sprintf(buff, "%d %.200s", NNTP_REJECTIT_VAL, filterrc);
        syslog(L_NOTICE, "rejecting[python] %s %s", Data.MessageID, buff);
        ARTlog(&Data, ART_REJECT, buff);
        if (innconf->remembertrash && (Mode == OMrunning) &&
			!HISremember(hash))
            syslog(L_ERROR, "%s cant write history %s %m",
                   LogName, Data.MessageID);
        ARTreject(REJECT_FILTER, cp, buff, article);
        return buff;
    }
#endif /* DO_PYTHON */

    /* I suppose some masochist will run with Python and Perl in together */

#if defined(DO_PERL)
    TMRstart(TMR_PERL);
    filterrc = PLartfilter(Data.Body, Data.LinesValue);
    TMRstop(TMR_PERL);
    if (filterrc) {
        sprintf(buff, "%d %.200s", NNTP_REJECTIT_VAL, filterrc);
        syslog(L_NOTICE, "rejecting[perl] %s %s", Data.MessageID, buff);
        ARTlog(&Data, ART_REJECT, buff);
        if (innconf->remembertrash && (Mode == OMrunning) &&
			!HISremember(hash))
            syslog(L_ERROR, "%s cant write history %s %m",
                   LogName, Data.MessageID);
        ARTreject(REJECT_FILTER, cp, buff, article);
        return buff;
    }
#endif /* DO_PERL */

    /* I suppose some masochist will run with both TCL and Perl in together */

#if defined(DO_TCL)
    if (TCLFilterActive) {
	int code;
	ARTHEADER *hp;

	/* make info available to Tcl */

	TCLCurrArticle = article;
	TCLCurrData = &Data;
        (void)Tcl_UnsetVar(TCLInterpreter, "Body", TCL_GLOBAL_ONLY);
        (void)Tcl_UnsetVar(TCLInterpreter, "Headers", TCL_GLOBAL_ONLY);
	for (hp = ARTheaders; hp < ENDOF(ARTheaders); hp++) {
	    if (hp->Found) {
		Tcl_SetVar2(TCLInterpreter, "Headers", (char *) hp->Name,
			    hp->Value, TCL_GLOBAL_ONLY);
	    }
	}
#if 1
        Tcl_SetVar(TCLInterpreter, "Body", Data.Body, TCL_GLOBAL_ONLY);
#endif
	/* call filter */

        code = Tcl_Eval(TCLInterpreter, "filter_news");
        (void)Tcl_UnsetVar(TCLInterpreter, "Body", TCL_GLOBAL_ONLY);
        (void)Tcl_UnsetVar(TCLInterpreter, "Headers", TCL_GLOBAL_ONLY);
        if (code == TCL_OK) {
	    if (strcmp(TCLInterpreter->result, "accept") != 0) {
	        (void)sprintf(buff, "%d %.200s", NNTP_REJECTIT_VAL, 
			      TCLInterpreter->result);
		syslog(L_NOTICE, "rejecting[tcl] %s %s", Data.MessageID, buff);
		ARTlog(&Data, ART_REJECT, buff);
                if (innconf->remembertrash && (Mode == OMrunning) &&
				!HISremember(hash))
                    syslog(L_ERROR, "%s cant write history %s %m",
                           LogName, Data.MessageID);
		ARTreject(REJECT_FILTER, cp, buff, article);
		return buff;
	    }
	} else {
	    /* the filter failed: complain and then turn off filtering */
	    syslog(L_ERROR, "TCL proc filter_news failed: %s",
		   TCLInterpreter->result);
	    TCLfilter(FALSE);
	}
    }
#endif /* defined(DO_TCL) */

    /* Stash a copy of the Newsgroups header. */
    p = HDR(_newsgroups);
    i = strlen(p);
    if (Header.Data == NULL) {
	Header.Size = i;
	Header.Data = NEW(char, Header.Size + 1);
    }
    else if (Header.Size <= i) {
	Header.Size = i + 16;
	RENEW(Header.Data, char, Header.Size + 1);
    }
    (void)strcpy(Header.Data, p);
    Data.Newsgroups = Header.Data;
    Data.NewsgroupsLength = i;

    /* If we limit what distributions we get, see if we want this one. */
    p = HDR(_distribution);
    distributions = *p ? CommaSplit(p) : NULL;
    if (distributions) {
      if (*distributions[0] == '\0') {
	(void)sprintf(buff, "%d bogus distribution \"%s\"",
		NNTP_REJECTIT_VAL,
		MaxLength(p, p));
	ARTlog(&Data, ART_REJECT, buff);
        if (innconf->remembertrash && Mode == OMrunning && !HISremember(hash))
            syslog(L_ERROR, "%s cant write history %s %m",
                   LogName, Data.MessageID);
	DISPOSE(distributions);
	ARTreject(REJECT_DISTRIB, cp, buff, article);
	return buff;
      } else {
	DISTparse(distributions, &Data);
	if (ME.Distributions
	 && !DISTwantany(ME.Distributions, distributions)) {
	    (void)sprintf(buff, "%d Unwanted distribution \"%s\"",
		    NNTP_REJECTIT_VAL,
		    MaxLength(distributions[0], distributions[0]));
	    ARTlog(&Data, ART_REJECT, buff);
            if (innconf->remembertrash && (Mode == OMrunning) &&
				!HISremember(hash))
                syslog(L_ERROR, "%s cant write history %s %m",
                       LogName, Data.MessageID);
	    DISPOSE(distributions);
	    ARTreject(REJECT_DISTRIB, cp, buff, article);
	    return buff;
	}
      }
    }
    else {
	Data.Distribution = "?";
	Data.DistributionLength = 1;
    }

    for (i = nSites, sp = Sites; --i >= 0; sp++) {
	sp->Poison = FALSE;
	sp->Sendit = FALSE;
	sp->Seenit = FALSE;
	sp->FNLnames.Used = 0;
	sp->ng = NULL;
    }

    groups = NGsplit(HDR(_followup_to));
    for (i = 0; groups[i] != NULL; i++)
	continue;
    Data.Followcount = i;
    groups = NGsplit(HDR(_newsgroups));
    for (i = 0; groups[i] != NULL; i++)
	continue;
    Data.Groupcount = i;
    if (Data.Followcount == 0)
	Data.Followcount = Data.Groupcount;

    /* Parse the Control header. */
    LikeNewgroup = FALSE;
    if (HDR(_control)[0] != '\0') {
        IsControl = TRUE;

	/* Nip off the first word into lowercase. */
	strncpy(ControlWord, HDR(_control), sizeof ControlWord);
	ControlWord[sizeof ControlWord - 1] = '\0';
	for (p = ControlWord; *p && !ISWHITE(*p); p++)
	    if (CTYPE(isupper, *p))
		*p = tolower(*p);
	*p = '\0';
	LikeNewgroup = EQ(ControlWord, "newgroup")
		    || EQ(ControlWord, "rmgroup");

	if (innconf->ignorenewsgroups && LikeNewgroup) {
	    for (p++; *p && ISWHITE(*p); p++);
	    groupbuff[0] = p;
	    for (p++; *p; p++) {
		if (NG_ISSEP(*p)) {
		    *p = '\0';
		    break;
		}
	    }
	    p = groupbuff[0];
	    for (p++; *p; p++) {
		if (ISWHITE(*p)) {
		    *p = '\0';
		    break;
		}
	    }
	    groupbuff[1] = NULL;
	    groups = groupbuff;
	    Data.Groupcount = 2;
	    if (Data.Followcount == 0)
		Data.Followcount = Data.Groupcount;
	}
	/* Control messages to "foo.ctl" are treated as if they were
	 * posted to "foo".  I should probably apologize for all the
	 * side-effects in the if. */
	for (i = 0; (p = groups[i++]) != NULL; )
	    if ((j = strlen(p) - 4) > 0
	     && *(p += j) == '.'
	     && p[1] == 'c' && p[2] == 't' && p[3] == 'l')
		*p = '\0';
    }

    /* Loop over the newsgroups, see which ones we want, and get the
     * total space needed for the Xref line.  At the end of this section
     * of code, j will have the needed length, the appropriate site
     * entries will have their Sendit and ng fields set, and GroupPointers
     * will have pointers to the relevant newsgroups. */
    ToGroup = NoHistoryUpdate = FALSE;
    p = HDR(_approved);
    Approved = *p != '\0';
    ngptr = GroupPointers;
    j = 0;
    for (GroupMissing = Accepted = FALSE; (p = *groups) != NULL; groups++) {
	if ((ngp = NGfind(p)) == NULL) {
	    GroupMissing = TRUE;
	    if (LikeNewgroup && Approved) {
		/* Newgroup/rmgroup being sent to a group that doesn't
		 * exist.  Assume it is being sent to the group being
		 * created or removed, nd send the group to all sites that
		 * would or would have had the group if it were created. */
		ARTsendthegroup(*groups);
		Accepted = TRUE;
	    } else
		NonExist = TRUE;
	    ARTpoisongroup(*groups);

	    if (innconf->mergetogroups) {
		/* Try to collapse all "to" newsgroups. */
		if (*p != 't' || *++p != 'o' || *++p != '.' || *++p == '\0')
		    continue;
		ngp = NGfind("to");
		ToGroup = TRUE;
		if ((sp = SITEfind(p)) != NULL) {
		    SITEmark(sp, ngp);
		}
	    } else {
		continue;
	    }
	}
	
	ngp->PostCount = 0;
	/* Ignore this group? */
	if (ngp->Rest[0] == NF_FLAG_IGNORE) {
	    /* See if any of this group's sites considers this group poison. */
	    for (isp = ngp->Poison, i = ngp->nPoison; --i >= 0; isp++)
		if (*isp >= 0)
		    Sites[*isp].Poison = TRUE;
	    continue;
	}

	/* Basic validity check. */
	if (ngp->Rest[0] == NF_FLAG_MODERATED && !Approved) {
	    (void)sprintf(buff, "%d Unapproved for \"%s\"",
		    NNTP_REJECTIT_VAL, ngp->Name);
	    ARTlog(&Data, ART_REJECT, buff);
            if (innconf->remembertrash && (Mode == OMrunning) &&
				!HISremember(hash))
                syslog(L_ERROR, "%s cant write history %s %m",
                       LogName, Data.MessageID);
	    if (distributions)
		DISPOSE(distributions);
	    ARTreject(REJECT_UNAPP, cp, buff, article);
	    return buff;
	}

	/* See if any of this group's sites considers this group poison. */
	for (isp = ngp->Poison, i = ngp->nPoison; --i >= 0; isp++)
	    if (*isp >= 0)
		Sites[*isp].Poison = TRUE;

	/* Check if we accept articles in this group from this peer, after
	   poisoning.  This means that articles that we accept from them will
	   be handled correctly if they're crossposted. */
	canpost = RCcanpost(cp, p);
	if (!canpost) {  /* At least one group cannot be fed by this peer.
	 		    If we later reject the post as unwanted group,
			    don't remember it.  If we accept, do remember */
	    NoHistoryUpdate = TRUE;
	    continue;
	}
	else if (canpost < 0) {
	    (void)sprintf(buff, "%d Won't accept posts in \"%s\"",
		NNTP_REJECTIT_VAL, MaxLength(p, p));
	    ARTlog(&Data, ART_REJECT, buff);
	    if (distributions)
		DISPOSE(distributions);
	    ARTreject(REJECT_GROUP, cp, buff, article);
	    return buff;
	}

	/* Valid group, feed it to that group's sites. */
	Accepted = TRUE;
	for (isp = ngp->Sites, i = ngp->nSites; --i >= 0; isp++)
	    if (*isp >= 0) {
		sp = &Sites[*isp];
		if (!sp->Poison)
		    SITEmark(sp, ngp);
	    }

	/* If it's excluded, don't file it. */
	if (ngp->Rest[0] == NF_FLAG_EXCLUDED)
	    continue;

	/* Expand aliases, mark the article as getting filed in the group. */
	if (ngp->Alias != NULL)
	    ngp = ngp->Alias;
	*ngptr++ = ngp;
	ngp->PostCount = 0;
	j += ngp->NameLength + 1 + MAXARTFNAME + 1;
    }

    /* Loop over sites to find Poisons/ControlOnly and undo Sendit flags. */
    for (i = nSites, sp = Sites; --i >= 0; sp++)
	if (sp->Poison
            || (sp->ControlOnly && !IsControl)
            || (sp->DontWantNonExist && NonExist))
	    sp->Sendit = FALSE;		

    /* Control messages not filed in "to" get filed only in control.name
     * or control. */
    if (IsControl && Accepted && !ToGroup) {
	ControlStore = TRUE;
	FileGlue(buff, "control", '.', ControlWord);
	if ((ngp = NGfind(buff)) == NULL)
	    ngp = NGfind(ARTctl);
	ngp->PostCount = 0;
	ngptr = GroupPointers;
	*ngptr++ = ngp;
	j = ngp->NameLength + 1 + MAXARTFNAME;
	for (isp = ngp->Sites, i = ngp->nSites; --i >= 0; isp++)
	    if (*isp >= 0) {
		sp = &Sites[*isp];
		SITEmark(sp, ngp);
	    }
    }

    /* If !Accepted, then none of the article's newgroups exist in our
     * active file.  Proper action is to drop the article on the floor.
     * If ngp == GroupPointers, then all the new articles newsgroups are
     * "j" entries in the active file.  In that case, we have to file it
     * under junk so that downstream feeds can get it. */
    if (!Accepted || ngptr == GroupPointers) {
	if (!Accepted) {
	    if (NoHistoryUpdate) {
		(void)sprintf(buff, "%d Can't post to \"%s\"",
		    NNTP_REJECTIT_VAL,
		    MaxLength(Data.Newsgroups, Data.Newsgroups));
	    } else {
	    (void)sprintf(buff, "%d Unwanted newsgroup \"%s\"",
		NNTP_REJECTIT_VAL,
		MaxLength(Data.Newsgroups, Data.Newsgroups));
	    }
	    ARTlog(&Data, ART_REJECT, buff);
	    if (!innconf->wanttrash) {
		if (innconf->remembertrash && (Mode == OMrunning) &&
			!NoHistoryUpdate && !HISremember(hash))
		    syslog(L_ERROR, "%s cant write history %s %m",
                       LogName, Data.MessageID);
		if (distributions)
		    DISPOSE(distributions);
		ARTreject(REJECT_GROUP, cp, buff, article);
		return buff;
	    } else {
	    /* if !GroupMissing, then all the groups the article was posted
	     * to have a flag of "x" in our active file, and therefore
	     * we should throw the article away:  if you have set
	     * innconf->remembertrash set, then you want all trash except that
	     * which you explicitly excluded in your active file. */
		if (!GroupMissing) {
                    if (innconf->remembertrash && (Mode == OMrunning) &&
				!NoHistoryUpdate && !HISremember(hash))
			syslog(L_ERROR, "%s cant write history %s %m",
					LogName, Data.MessageID);
		    if (distributions)
				DISPOSE(distributions);
		    ARTreject(REJECT_GROUP, cp, buff, article);
		    return buff;
		}
	    }
	}
	ngp = NGfind(ARTjnk);
	*ngptr++ = ngp;
	ngp->PostCount = 0;
	j = STRLEN(ARTjnk) + 1 + MAXARTFNAME;

	/* Junk can be fed to other sites. */
	for (isp = ngp->Sites, i = ngp->nSites; --i >= 0; isp++)
	    if (*isp >= 0) {
		sp = &Sites[*isp];
		if (!sp->Poison && !(sp->ControlOnly && !IsControl))
		    SITEmark(sp, ngp);
	    }
    }
    *ngptr = NULL;
    j++;

    /* Allocate exactly enough space for the textual representation */
    j = (sizeof(TOKEN) * 2) + 3;

    /* Make sure the filename buffer has room. */
    if (Files.Data == NULL) {
	Files.Size = j;
	Files.Data = NEW(char, Files.Size + 1);
    }
    else if (Files.Size <= j) {
	Files.Size = j;
	RENEW(Files.Data, char, Files.Size + 1);
    }

    if (innconf->xrefslave) {
    	if (ARTxrefslave() == FALSE) {
    	    if (HDR(_xref)) {
                (void)sprintf(buff,
                    "%d Xref header \"%s\" invalid in xrefslave mode",
		    NNTP_REJECTIT_VAL,
		    MaxLength(HDR(_xref), HDR(_xref)));
	    } else {
                (void)sprintf(buff,
                    "%d Xref header required in xrefslave mode",
		    NNTP_REJECTIT_VAL);
	    }
            ARTlog(&Data, ART_REJECT, buff);
	    if (distributions)
	        DISPOSE(distributions);
	    ARTreject(REJECT_OTHER, cp, buff, article);
	    return buff;
    	}
    } else {
            ARTassignnumbers();
    }

    /* Now we can file it. */
    if (++ICDactivedirty >= innconf->icdsynccount) {
	ICDwriteactive();
	ICDactivedirty = 0;
    }
    TMRstart(TMR_ARTWRITE);
    for (i = 0; (ngp = GroupPointers[i]) != NULL; i++)
	ngp->PostCount = 0;
    
    token = ARTstore(article, &Data);
    if (token.type == TOKEN_EMPTY) {
	syslog(L_ERROR, "%s cant store article: %s", LogName, SMerrorstr);
	sprintf(buff, "%d cant store article", NNTP_RESENDIT_VAL);
	ARTlog(&Data, ART_REJECT, buff);
	if ((Mode == OMrunning) && !HISremember(hash))
	    syslog(L_ERROR, "%s cant write history %s %m",
		   LogName, Data.MessageID);
	if (distributions)
	    DISPOSE(distributions);
	ARTreject(REJECT_OTHER, cp, buff, article);
	TMRstop(TMR_ARTWRITE);
	return buff;
    }
    TMRstop(TMR_ARTWRITE);
    ARTmakeoverview(&Data, FALSE);
    MadeOverview = TRUE;
    if (innconf->enableoverview && !innconf->useoverchan) {
	TMRstart(TMR_OVERV);
	if (!OVadd(token, Data.Overview->Data, Data.Overview->Left, Data.Arrived, Data.Expires)) {
	    if (OVctl(OVSPACE, (void *)&i) && i == OV_NOSPACE)
		IOError("creating overview", ENOSPC);
	    else
		IOError("creating overview", 0);
	    syslog(L_ERROR, "%s cant store overview for %s", LogName, TokenToText(token));
	    OverviewCreated = FALSE;
	} else {
	    OverviewCreated = TRUE;
	}
	TMRstop(TMR_OVERV);
    }
    strcpy(Files.Data, TokenToText(token));
    strcpy(Data.Name, Files.Data);
    Data.NameLength = strlen(Data.Name);

    /* Update history if we didn't get too many I/O errors above. */
    if ((Mode != OMrunning) || !HISwrite(&Data, hash, Files.Data, &token)) {
	i = errno;
	syslog(L_ERROR, "%s cant write history %s %m", LogName, Data.MessageID);
	(void)sprintf(buff, "%d cant write history, %s",
		NNTP_RESENDIT_VAL, strerror(errno));
	ARTlog(&Data, ART_REJECT, buff);
	if (distributions)
	    DISPOSE(distributions);
	ARTreject(REJECT_OTHER, cp, buff, article);
	return buff;
    }

    /* We wrote the history, so modify it and save it for output. */
    if (innconf->xrefslave) {
	if ((p = memchr(HDR(_xref), ' ', ARTheaders[_xref].Length)) == NULL) {
	    Data.Replic = HDR(_xref);
	    Data.ReplicLength = 0;
	} else {
	    Data.Replic = p + 1;
	    Data.ReplicLength = ARTheaders[_xref].Length - (p + 1 - HDR(_xref));
	}
    } else {
	Data.Replic = HDR(_xref) + Path.Used;
	Data.ReplicLength = ARTheaders[_xref].Length - Path.Used;
    }
    Data.StoredGroup = Data.Replic;
    if ((p = memchr(Data.Replic, ':', Data.ReplicLength)) == NULL) {
	Data.StoredGroupLength = 0;
    } else {
	Data.StoredGroupLength = p - Data.StoredGroup;
    }

    /* Start logging, then propagate the article. */
    if (CRwithoutLF > 0 || LFwithoutCR > 0) {
	if (CRwithoutLF > 0 && LFwithoutCR == 0)
	    (void)sprintf(buff, "%d article includes CR without LF(%d)", NNTP_REJECTIT_VAL, CRwithoutLF);
	else if (CRwithoutLF == 0 && LFwithoutCR > 0)
	    (void)sprintf(buff, "%d article includes LF without CR(%d)", NNTP_REJECTIT_VAL, LFwithoutCR);
	else
	    (void)sprintf(buff, "%d article includes CR without LF(%d) and LF withtout CR(%d)", NNTP_REJECTIT_VAL, CRwithoutLF, LFwithoutCR);
	ARTlog(&Data, ART_STRSTR, buff);
    }
    ARTlog(&Data, Accepted ? ART_ACCEPT : ART_JUNK, (char *)NULL);
    if ((innconf->nntplinklog) &&
    	(fprintf(Log, " (%s)", Data.Name) == EOF || ferror(Log))) {
	oerrno = errno;
	syslog(L_ERROR, "%s cant write log_nntplink %m", LogName);
	IOError("logging nntplink", oerrno);
	clearerr(Log);
    }
    /* Calculate Max Article Time */
    i = Now.time - cp->ArtBeg;
    if(i > cp->ArtMax)
	cp->ArtMax = i;
    cp->ArtBeg = 0;

    cp->Size += Data.SizeValue;
    if (innconf->logartsize) {
	if (fprintf(Log, " %ld",Data.SizeValue) == EOF || ferror (Log)) {
            oerrno = errno;
	    syslog(L_ERROR, "%s cant write artsize %m", LogName);
	    IOError("logging artsize", oerrno);
	    clearerr(Log);
	}
    }
    
    ARTpropagate(&Data, hops, hopcount, distributions, ControlStore, OverviewCreated);
    if (distributions)
	DISPOSE(distributions);

    /* Now that it's been written, process the control message.  This has
     * a small window, if we get a new article before the newgroup message
     * has been processed.  We could pause ourselves here, but it doesn't
     * seem to be worth it. */
    if (Accepted) {
	if (IsControl) {
	    TMRstart(TMR_ARTCTRL);
	    ARTcontrol(&Data, hash, HDR(_control), cp);
	    TMRstop(TMR_ARTCTRL);
	}
	p = HDR(_supersedes);
	if (*p && ARTidok(p)) {
	    ARTcancel(&Data, p, FALSE);
	}
    }

    /* If we need the overview data, write it. */
    if (NeedOverview && !MadeOverview)
	ARTmakeoverview(&Data, TRUE);

    /* And finally, send to everyone who should get it */
    for (sp = Sites, i = nSites; --i >= 0; sp++)
	if (sp->Sendit) {
    	    TMRstart(TMR_SITESEND);
	    SITEsend(sp, &Data);
    	    TMRstop(TMR_SITESEND);
	}

    return NNTP_TOOKIT;
}

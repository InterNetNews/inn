/*  $Id$
**
**  Article-processing.
*/

#include "config.h"
#include "clibrary.h"
#include <sys/uio.h>

#include "innd.h"
#include "ov.h"
#include "storage.h"

typedef struct iovec	IOVEC;

#define	ARTIOVCNT	16

extern bool DoCancels;

#if	defined(S_IXUSR)
#define EXECUTE_BITS	(S_IXUSR | S_IXGRP | S_IXOTH)
#else
#define EXECUTE_BITS	0111
#endif	/* defined(S_IXUSR) */

/*
**  used to sort Xref, Bytes and Path pointers
*/
typedef struct _HEADERP {
  int   index;                          
  char  *p;
} HEADERP;
  
#define HPCOUNT		4

/*
**  For speed we build a binary tree of the headers, sorted by their
**  name.  We also store the header's Name fields in the tree to avoid
**  doing an extra indirection.
*/
typedef struct _TREE {
  const char	*Name;
  ARTHEADER	*Header;
  struct _TREE	*Before;
  struct _TREE	*After;
} TREE;

static TREE	*ARTheadertree;

/*
**  For doing the overview database, we keep a list of the headers and
**  a flag saying if they're written in brief or full format.
*/
typedef struct _ARTOVERFIELD {
  ARTHEADER	*Header;
  bool		NeedHeader;
} ARTOVERFIELD;

static ARTOVERFIELD	*ARTfields;

/*
**  General newsgroup we care about, and what we put in the Path line.
*/
static char	ARTctl[] = "control";
static char	ARTjnk[] = "junk";
static char	*ARTpathme;

/*
**  Different types of rejected articles.
*/
typedef enum {REJECT_DUPLICATE, REJECT_SITE, REJECT_FILTER, REJECT_DISTRIB,
	      REJECT_GROUP, REJECT_UNAPP, REJECT_OTHER} Reject_type;

/*
**  Flag array, indexed by character.  Character classes for Message-ID's.
*/
static char		ARTcclass[256];
#define CC_MSGID_ATOM	01
#define CC_MSGID_NORM	02
#define CC_HOSTNAME	04
#define ARTnormchar(c)	((ARTcclass[(unsigned char)(c)] & CC_MSGID_NORM) != 0)
#define ARTatomchar(c)	((ARTcclass[(unsigned char)(c)] & CC_MSGID_ATOM) != 0)
#define ARThostchar(c)	((ARTcclass[(unsigned char)(c)] & CC_HOSTNAME) != 0)

#if defined(DO_PERL) || defined(DO_PYTHON)
const char	*filterPath;
#endif /* DO_PERL || DO_PYTHON */



/*
**  Mark that the site gets this article.
*/
static void
SITEmark(SITE *sp, NEWSGROUP *ngp)
{
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
bool
ARTreadschema(void)
{
  static char	*SCHEMA = NULL;
  FILE		*F;
  int		i;
  char		*p;
  ARTOVERFIELD	*fp;
  ARTHEADER	*hp;
  bool		ok;
  char		buff[SMBUF];
  bool		foundxref = FALSE;
  bool		foundxreffull = FALSE;

  if (ARTfields != NULL) {
    DISPOSE(ARTfields);
    ARTfields = NULL;
  }

  /* Open file, count lines. */
  if (SCHEMA == NULL)
    SCHEMA = concatpath(innconf->pathetc, _PATH_SCHEMA);
  if ((F = Fopen(SCHEMA, "r", TEMPORARYOPEN)) == NULL)
    return FALSE;
  for (i = 0; fgets(buff, sizeof buff, F) != NULL; i++)
    continue;
  fseeko(F, 0, SEEK_SET);
  ARTfields = NEW(ARTOVERFIELD, i + 1);

  /* Parse each field. */
  for (ok = TRUE, fp = ARTfields ; fgets(buff, sizeof buff, F) != NULL ;) {
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
    } else
      fp->NeedHeader = FALSE;
    if (caseEQ(buff, "Xref")) {
      foundxref = TRUE;
      foundxreffull = fp->NeedHeader;
    }
    for (hp = ARTheaders; hp < ENDOF(ARTheaders); hp++) {
      if (caseEQ(buff, hp->Name)) {
	fp->Header = hp;
	break;
      }
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
**  This only gets called once, and the tree only has about 37 entries,
**  so we don't bother to unroll the recursion.
*/
static TREE *
ARTbuildtree(ARTHEADER **Table, int lo, int hi)
{
  int	mid;
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
static int
ARTcompare(const void *p1, const void *p2)
{
  return strcasecmp(((const ARTHEADER **)p1)[0]->Name,
    ((const ARTHEADER **)p2)[0]->Name);
}


/*
**  Setup the article processing.
*/
void
ARTsetup(void)
{
  const char *	p;
  ARTHEADER **	table;
  unsigned int	i;

  /* Set up the character class tables.  These are written a
   * little strangely to work around a GCC2.0 bug. */
  memset(ARTcclass, 0, sizeof ARTcclass);
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

  /* Build the header tree. */
  table = NEW(ARTHEADER*, SIZEOF(ARTheaders));
  for (i = 0; i < SIZEOF(ARTheaders); i++)
    table[i] = &ARTheaders[i];
  qsort(table, SIZEOF(ARTheaders), sizeof *table, ARTcompare);
  ARTheadertree = ARTbuildtree(table, 0, SIZEOF(ARTheaders));
  DISPOSE(table);

  /* Get our Path name, kill trailing !. */
  ARTpathme = COPY(Path.Data);
  ARTpathme[Path.Used - 1] = '\0';

  /* Set up database; ignore errors. */
  (void)ARTreadschema();
}


static void
ARTfreetree(TREE *tp)
{
  TREE	*next;

  for ( ; tp != NULL; tp = next) {
    if (tp->Before)
      ARTfreetree(tp->Before);
    next = tp->After;
    DISPOSE(tp);
  }
}


void
ARTclose(void)
{
  if (ARTfields != NULL) {
    DISPOSE(ARTfields);
    ARTfields = NULL;
  }
  ARTfreetree(ARTheadertree);
}

/*
**  Start a log message about an article.
*/
static void
ARTlog(const ARTDATA *data, char code, const char *text)
{
  HDRCONTENT	*hc = data->HdrContent;
  int		i;
  bool		Done;

  TMRstart(TMR_ARTLOG);
  /* We could be a bit faster by not dividing Now.usec by 1000,
   * but who really wants to log at the Microsec level? */
  Done = code == ART_ACCEPT || code == ART_JUNK;
  if (text)
    i = fprintf(Log, "%.15s.%03d %c %s %s %s%s",
      ctime(&Now.time) + 4, (int)(Now.usec / 1000), code, data->Feedsite,
      HDR_FOUND(HDR__MESSAGE_ID) ? HDR(HDR__MESSAGE_ID) : "(null)",
      text, Done ? "" : "\n");
  else
    i = fprintf(Log, "%.15s.%03d %c %s %s%s",
      ctime(&Now.time) + 4, (int)(Now.usec / 1000), code, data->Feedsite,
      HDR_FOUND(HDR__MESSAGE_ID) ? HDR(HDR__MESSAGE_ID) : "(null)",
      Done ? "" : "\n");
  if (i == EOF || (Done && !BufferedLogs && fflush(Log)) || ferror(Log)) {
    i = errno;
    syslog(L_ERROR, "%s cant write log_start %m", LogName);
    IOError("logging article", i);
    clearerr(Log);
  }
  TMRstop(TMR_ARTLOG);
}

/*
**  Parse a Path line, splitting it up into NULL-terminated array of strings.
*/
static int
ARTparsepath(const char *p, int size, LISTBUFFER *list)
{
  int	i;
  char	*q, **hp;

  /* setup buffer */ 
  SetupListBuffer(size, list);

  /* loop over text and copy */
  for (i = 0, q = list->Data, hp = list->List ; *p ; p++, *q++ = '\0') { 
    /* skip leading separators. */
    for (; *p && !ARThostchar(*p) && ISWHITE(*p) ; p++)
      continue;
    if (*p == '\0')
      break;

    if (list->ListLength <= i) {
      list->ListLength += DEFAULTNGBOXSIZE;
      RENEW(list->List, char*, list->ListLength);
      hp = &list->List[i];
    }
    /* mark the start of the host, move to the end of it while copying */  
    for (*hp++ = q, i++ ; *p && ARThostchar(*p) && !ISWHITE(*p) ;)
      *q++ = *p++;
    if (*p == '\0')
      break;
  }
  *q = '\0';
  if (i == list->ListLength) {
    list->ListLength += DEFAULTNGBOXSIZE;
    RENEW(list->List, char *, list->ListLength);
    hp = &list->List[i];
  }
  *hp = NULL;
  return i;
}

/*
**  Sorting pointer where header starts
*/
static int
ARTheaderpcmp(const void *p1, const void *p2)
{
  return (((const HEADERP *)p1)->p - ((const HEADERP *)p2)->p);
}

/* Write an article using the storage api.  Put it together in memory and
   call out to the api. */
static TOKEN
ARTstore(CHANNEL *cp)
{
  BUFFER	*Article = &cp->In;
  ARTDATA	*data = &cp->Data;
  HDRCONTENT	*hc = data->HdrContent;
  const char	*p;
  ARTHANDLE	arth;
  int		i, j, iovcnt = 0;
  long		headersize = 0;
  TOKEN		result;
  BUFFER	*headers = &data->Headers;
  struct iovec	iov[ARTIOVCNT];
  HEADERP	hp[HPCOUNT];

  /* find Path, Bytes and Xref to be prepended/dropped/replaced */
  arth.len = i = 0;
  /* assumes Path header is required header */
  hp[i].p = HDR(HDR__PATH);
  hp[i++].index = HDR__PATH;
  if (HDR_FOUND(HDR__XREF)) {
    hp[i].p = HDR(HDR__XREF);
    hp[i++].index = HDR__XREF;
  }
  if (HDR_FOUND(HDR__BYTES)) {
    hp[i].p = HDR(HDR__BYTES);
    hp[i++].index = HDR__BYTES;
  }
  /* get the order of header appearance */
  qsort(hp, i, sizeof(HEADERP), ARTheaderpcmp);
  /* p always points where the next data should be written from */
  for (p = Article->Data + cp->Start, j = 0 ; j < i ; j++) {
    switch (hp[j].index) {
      case HDR__PATH:
	if (!Hassamepath || AddAlias) {
	  /* write heading data */
	  iov[iovcnt].iov_base = (char *) p;
	  iov[iovcnt++].iov_len = HDR(HDR__PATH) - p;
	  arth.len += HDR(HDR__PATH) - p;
	  /* now append new one */
	  iov[iovcnt].iov_base = Path.Data;
	  iov[iovcnt++].iov_len = Path.Used;
	  arth.len += Path.Used;
	  if (AddAlias) {
	    iov[iovcnt].iov_base = Pathalias.Data;
	    iov[iovcnt++].iov_len = Pathalias.Used;
	    arth.len += Pathalias.Used;
	  }
	  /* next to write */
	  p = HDR(HDR__PATH);
	}
	break;
      case HDR__XREF:
	if (!innconf->xrefslave) {
	  /* write heading data */
	  iov[iovcnt].iov_base = (char *) p;
	  iov[iovcnt++].iov_len = HDR(HDR__XREF) - p;
	  arth.len += HDR(HDR__XREF) - p;
	  /* replace with new one */
	  iov[iovcnt].iov_base = data->Xref;
	  iov[iovcnt++].iov_len = data->XrefLength - 2;
	  arth.len += data->XrefLength - 2;
	  /* next to write */
	  /* this points where trailing "\r\n" of orginal Xref header exists */
	  p = HDR(HDR__XREF) + HDR_LEN(HDR__XREF);
	}
	break;
      case HDR__BYTES:
	/* ditch whole Byte header */
	/* write heading data */
	iov[iovcnt].iov_base = (char *) p;
	iov[iovcnt++].iov_len = data->BytesHeader - p;
	arth.len += data->BytesHeader - p;
	/* next to write */
	/* need to skip trailing "\r\n" of Bytes header */
	p = HDR(HDR__BYTES) + HDR_LEN(HDR__BYTES) + 2;
	break;
      default:
	result.type = TOKEN_EMPTY;
	return result;
    }
  }
  /* in case Xref is not included in orignal article */
  if (!HDR_FOUND(HDR__XREF)) {
    /* write heading data */
    iov[iovcnt].iov_base = (char *) p;
    iov[iovcnt++].iov_len = Article->Data + (data->Body - 2) - p;
    arth.len += Article->Data + (data->Body - 2) - p;
    /* Xref needs to be inserted */
    iov[iovcnt].iov_base = (char *) "Xref: ";
    iov[iovcnt++].iov_len = sizeof("Xref: ") - 1;
    arth.len += sizeof("Xref: ") - 1;
    iov[iovcnt].iov_base = data->Xref;
    iov[iovcnt++].iov_len = data->XrefLength;
    arth.len += data->XrefLength;
    p = Article->Data + (data->Body - 2);
  }
  /* write rest of data */
  iov[iovcnt].iov_base = (char *) p;
  iov[iovcnt++].iov_len = Article->Data + cp->Next - p;
  arth.len += Article->Data + cp->Next - p;

  /* revert trailing '\0\n' to '\r\n' of all system header */
  for (i = 0 ; i < MAX_ARTHEADER ; i++) {
    if (HDR_FOUND(i))
      HDR_PARSE_END(i);
  }

  arth.iov = iov;
  arth.iovcnt = iovcnt;
  arth.arrived = (time_t)0;
  arth.token = (TOKEN *)NULL;
  arth.expires = data->Expires;
  if (innconf->storeonxref) {
    arth.groups = data->Replic;
    arth.groupslen = data->ReplicLength;
  } else {
    arth.groups = HDR(HDR__NEWSGROUPS);
    arth.groupslen = HDR_LEN(HDR__NEWSGROUPS);
  }

  SMerrno = SMERR_NOERROR;
  result = SMstore(arth);
  if (result.type == TOKEN_EMPTY) {
    if (SMerrno == SMERR_NOMATCH)
      ThrottleNoMatchError();
    else if (SMerrno != SMERR_NOERROR)
      IOError("SMstore", SMerrno);
    return result;
  }

  /* calculate stored size */
  for (data->BytesValue = i = 0 ; i < iovcnt ; i++) {
    if (NeedHeaders && (i + 1 == iovcnt)) {
      /* body begins at last iov */
      headersize = data->BytesValue +
	Article->Data + data->Body - (char *) iov[i].iov_base;
    }
    data->BytesValue += iov[i].iov_len;
  }
  /* "\r\n" is counted as 1 byte.  trailing ".\r\n" and body delimitor are also
     substituted */
  data->BytesValue -= (data->HeaderLines + data->Lines + 4);
  /* Figure out how much space we'll need and get it. */
  (void)sprintf(data->Bytes, "Bytes: %ld\r\n", data->BytesValue);
  /* does not include strlen("Bytes: \r\n") */
  data->BytesLength = strlen(data->Bytes) - 9;

  if (!NeedHeaders)
    return result;

  if (headers->Size == 0) {
    headers->Size = headersize;
    headers->Data = NEW(char, headers->Size + 1);
  }

  /* Add the data. */
  BUFFset(headers, data->Bytes, strlen(data->Bytes));
  for (i = 0 ; i < iovcnt ; i++) {
    if (i + 1 == iovcnt)
      BUFFappend(headers, iov[i].iov_base,
	Article->Data+ data->Body - (char *) iov[i].iov_base);
    else
      BUFFappend(headers, iov[i].iov_base, iov[i].iov_len);
  }
  BUFFtrimcr(headers);

  return result;
}

/*
**  Parse a header that starts at header.  size includes trailing "\r\n"
*/
static void
ARTparseheader(CHANNEL *cp, int size)
{
  ARTDATA	*data = &cp->Data;
  char		*header = cp->In.Data + data->CurHeader;
  HDRCONTENT	*hc = cp->Data.HdrContent;
  char		*buff = cp->Error;
  TREE		*tp;
  ARTHEADER	*hp;
  char		c, *p, *colon;
  int		i;

  /* Find first colon */
  if ((colon = memchr(header, ':', size)) == NULL || !ISWHITE(colon[1])) {
    if ((p = memchr(header, '\r', size)) != NULL)
      *p = '\0';
    (void)sprintf(buff, "%d No colon-space in \"%s\" header",
      NNTP_REJECTIT_VAL, MaxLength(header, header));
    if (p != NULL)
      *p = '\r';
    return;
  }

  /* See if this is a system header.  A fairly tightly-coded binary search. */
  c = CTYPE(islower, *header) ? toupper(*header) : *header;
  for (*colon = '\0', tp = ARTheadertree; tp; ) {
    if ((i = c - tp->Name[0]) == 0 && (i = strcasecmp(header, tp->Name)) == 0)
      break;
    if (i < 0)
      tp = tp->Before;
    else
      tp = tp->After;
  }
  *colon = ':';

  if (tp == NULL) {
    /* Not a system header, make sure we have <word><colon><space>. */
    for (p = colon; --p > header; ) {
      if (ISWHITE(*p)) {
	c = *p;
	*p = '\0';
	(void)sprintf(buff, "%d Space before colon in \"%s\" header",
	  NNTP_REJECTIT_VAL, MaxLength(header, header));
	*p = c;
	return;
      }
    }
    return;
  }
  hp = tp->Header;
  i = hp - ARTheaders;
  /* remember to ditch if it's Bytes: */
  if (i == HDR__BYTES)
    cp->Data.BytesHeader = header;
  hc = &hc[i];
  if (hc->Length > 0) {
    /* duplicated */
    hc->Length = -1;
  } else {
    for (p = colon + 1 ; (p < header + size - 2) &&
      (ISWHITE(*p) || *p == '\r' || *p == '\n'); p++);
    if (p < header + size - 2) {
      hc->Value = p;
      /* HDR_LEN() does not include trailing "\r\n" */
      hc->Length = header + size - 2 - p;
    } else {
      (void)sprintf(buff, "%d Body of header is all blanks in \"%s\" header",
      NNTP_REJECTIT_VAL, MaxLength(hp->Name, hp->Name));
    }
  }
  return;
}

/*
**  Check Message-ID format based on RFC 822 grammar, except that (as per
**  RFC 1036) whitespace, non-printing, and '>' characters are excluded.
**  Based on code by Paul Eggert posted to news.software.b on 22-Nov-90
**  in <#*tyo2'~n@twinsun.com>, with additional email discussion.
**  Thanks, Paul.
*/
bool
ARTidok(const char *MessageID)
{
  int		c;
  const char	*p;

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
**  Clean up data field where article informations are stored.
**  This must be called before article processing.
*/
void
ARTprepare(CHANNEL *cp)
{
  ARTDATA	*data = &cp->Data;
  HDRCONTENT	*hc = data->HdrContent;
  int		i;

  for (i = 0 ; i < MAX_ARTHEADER ; i++, hc++) {
    hc->Value = NULL;
    hc->Length = 0;
  }
  data->Lines = data->HeaderLines = data->CRwithoutLF = data->LFwithoutCR = 0;
  data->CurHeader = data->LastTerminator = data->LastCR = cp->Start - 1;
  data->LastCRLF = data->Body = cp->Start - 1;
  data->BytesHeader = NULL;
  data->Feedsite = "?";
  *cp->Error = '\0';
}

/*
**  Clean up an article.  This is mainly copying in-place, stripping bad
**  headers.  Also fill in the article data block with what we can find.
**  Return NULL if the article is okay, or a string describing the error.
**  Parse headers and end of article
**  This is called by NCproc().
*/
void
ARTparse(CHANNEL *cp)
{
  BUFFER	*bp = &cp->In;
  ARTDATA	*data = &cp->Data;
  int		i, limit, hopcount;
  char		**hops;
  HDRCONTENT	*hc = data->HdrContent;

  /* Read through the buffer to find header, body and end of article */
  /* this routine is designed not to refer data so long as possible for
     performance reason, so the code may look redundant at a glance */
  limit = bp->Used;
  i = cp->Next;
  if (cp->State == CSgetheader) {
    /* header processing */
    for (; i < limit ;) {
      if (data->LastCRLF + 1 == i) {
	/* begining of the line */
	switch (bp->Data[i]) {
	  case '.':
	    data->LastTerminator = i;
	    data->NullHeader = FALSE;
	    break;
	  case '\r':
	    data->LastCR = i;
	    data->NullHeader = FALSE;
	    break;
	  case '\n':
	    data->LFwithoutCR++;
	    data->NullHeader = FALSE;
	    break;
	  case '\t':
	  case ' ':
	    /* header is folded.  NullHeader is untouched */
	    break;
	  case '\0':
	    (void)sprintf(cp->Error, "%d Null Header", NNTP_REJECTIT_VAL);
	    data->NullHeader = TRUE;
	    break;
	  default:
	    if (data->CurHeader >= cp->Start) {
	      /* parse previous header */
	      if (!data->NullHeader && (*cp->Error == '\0'))
		/* skip if already got an error */
		ARTparseheader(cp, i - data->CurHeader);
	    }
	    data->CurHeader = i;
	    data->NullHeader = FALSE;
	    break;
	}
	i++;
      }
      for (; i < limit ;) {
	/* rest of the line */
	switch (bp->Data[i]) {
	  case '\0':
	    (void)sprintf(cp->Error, "%d Null Header", NNTP_REJECTIT_VAL);
	    data->NullHeader = TRUE;
	    break;
	  case '\r':
	    if (data->LastCR >= cp->Start)
	      data->CRwithoutLF++;
	    data->LastCR = i;
	    break;
	  case '\n':
	    if (data->LastCR + 1 == i) {
	      /* found CRLF */
	      data->LastCR = cp->Start - 1;
	      if (data->LastTerminator + 2 == i) {
		/* terminated still in header */
		if (cp->Start + 3 == i) {
		  (void)sprintf(cp->Error, "%d Empty article",
		    NNTP_REJECTIT_VAL);
		  cp->State = CSnoarticle;
		} else {
		  (void)sprintf(cp->Error, "%d No body", NNTP_REJECTIT_VAL);
		  cp->State = CSgotarticle;
		}
		cp->Next = ++i;
		goto sizecheck;
	      }
	      if (data->LastCRLF + MAXHEADERSIZE < i)
		sprintf(cp->Error, "%d Too long line in header %d bytes",
		  NNTP_REJECTIT_VAL, i - data->LastCRLF);
	      else if (data->LastCRLF + 2 == i) {
		/* header ends */
		/* parse previous header */
		if (data->CurHeader >= cp->Start) {
		  if (!data->NullHeader && (*cp->Error == '\0'))
		    /* skip if already got an error */
		    ARTparseheader(cp, i - 1 - data->CurHeader);
		} else {
		  (void)sprintf(cp->Error, "%d No header", NNTP_REJECTIT_VAL);
		}
		data->LastCRLF = i++;
		data->Body = i;
		cp->State = CSgetbody;
		goto bodyprocessing;
	      }
	      data->HeaderLines++;
	      data->LastCRLF = i++;
	      goto endofheaderline;
	    } else {
	      data->LFwithoutCR++;
	    }
	    break;
	  default:
	    break;
	}
	i++;
      }
endofheaderline:
      ;
    }
  } else {
bodyprocessing:
    /* body processing, or eating huge article */
    for (; i < limit ;) {
      if (data->LastCRLF + 1 == i) {
        /* begining of the line */
        switch (bp->Data[i]) {
	  case '.':
	    data->LastTerminator = i;
	    break;
	  case '\r':
	    data->LastCR = i;
	    break;
	  case '\n':
	    data->LFwithoutCR++;
	    break;
	  default:
	    break;
        }
        i++;
      }
      for (; i < limit ;) {
	/* rest of the line */
	switch (bp->Data[i]) {
	  case '\r':
	    if (data->LastCR >= cp->Start)
	      data->CRwithoutLF++;
	    data->LastCR = i;
	    break;
	  case '\n':
	    if (data->LastCR + 1 == i) {
	      /* found CRLF */
	      data->LastCR = cp->Start - 1;
	      if (data->LastTerminator + 2 == i) {
		/* found end of article */
		if (cp->State == CSeatarticle) {
		  cp->State = CSgotlargearticle;
		  cp->Next = ++i;
		  return;
		} else
		  cp->State = CSgotarticle;
		i++;
		if (*cp->Error != '\0' && HDR_FOUND(HDR__MESSAGE_ID)) {
		  HDR_PARSE_START(HDR__MESSAGE_ID);
		  if (HDR_FOUND(HDR__PATH)) {
		    /* to record path into news log */
		    HDR_PARSE_START(HDR__PATH);
		    hopcount = ARTparsepath(HDR(HDR__PATH), HDR_LEN(HDR__PATH),
		      &data->Path);
		    HDR_PARSE_END(HDR__PATH);
		    if (hopcount > 0) {
		      hops = data->Path.List;
		      if (innconf->logipaddr) {
			data->Feedsite = RChostname(cp);
			if (data->Feedsite == NULL)
			  data->Feedsite = CHANname(cp);
			if (strcmp("0.0.0.0", data->Feedsite) == 0)
			  data->Feedsite =
			    hops && hops[0] ? hops[0] : CHANname(cp);
		      } else {
			data->Feedsite =
			  hops && hops[0] ? hops[0] : CHANname(cp);
		      }
		    }
		  }
		  ARTlog(data, ART_REJECT, cp->Error);
		  HDR_PARSE_END(HDR__MESSAGE_ID);
		}
		goto sizecheck;
	      }
#if 0 /* this may be examined in the future */
	      if (data->LastCRLF + MAXHEADERSIZE < i)
		(void)sprintf(cp->Error, "%d Too long line in body %d bytes",
		  NNTP_REJECTIT_VAL, i);
#endif
	      data->Lines++;
	      data->LastCRLF = i++;
	      goto endofline;
	    } else {
	      data->LFwithoutCR++;
	    }
	    break;
	  default:
	    break;
	}
	i++;
      }
endofline:
      ;
    }
  }
sizecheck:
  if ((innconf->maxartsize > 0) &&
    (limit - cp->Next + cp->LargeArtSize > innconf->maxartsize)) {
    cp->LargeArtSize += limit - cp->Next;
    cp->State = CSeatarticle;
  }
  cp->Next = i;
  return;
}

/*
**  Clean up an article.  This is mainly copying in-place, stripping bad
**  headers.  Also fill in the article data block with what we can find.
**  Return TRUE if the article has no error, or FALSE which means the error.
*/
static bool
ARTclean(ARTDATA *data, char *buff)
{
  HDRCONTENT	*hc = data->HdrContent;
  ARTHEADER	*hp = ARTheaders;
  int		i;
  char		*p;
  int		delta;

  TMRstart(TMR_ARTCLEAN);
  data->Arrived = Now.time;
  data->Expires = 0;

  /* replace trailing '\r\n' with '\0\n' of all system header to be handled
     easily by str*() functions */
  for (i = 0 ; i < MAX_ARTHEADER ; i++) {
    if (HDR_FOUND(i))
      HDR_PARSE_START(i);
  }

  /* Make sure all the headers we need are there */
  for (i = 0; i < MAX_ARTHEADER ; i++) {
    if (hp[i].Type == HTreq) {
      if (!HDR_FOUND(i)) {
	(void)sprintf(buff, "%d Missing \"%s\" header", NNTP_REJECTIT_VAL,
	  hp[i].Name);
	TMRstop(TMR_ARTCLEAN);
	return FALSE;
      }
    }
  }

  /* assumes Message-ID header is required header */
  if (!ARTidok(HDR(HDR__MESSAGE_ID))) {
    (void)sprintf(buff, "%d Bad \"Message-ID\" header", NNTP_REJECTIT_VAL);
    TMRstop(TMR_ARTCLEAN);
    return FALSE;
  }

  if (innconf->linecountfuzz && HDR_FOUND(HDR__LINES)) {
    p = HDR(HDR__LINES);
    i = data->Lines;
    if ((delta = i - atoi(p)) != 0 && abs(delta) > innconf->linecountfuzz) {
      (void)sprintf(buff, "%d Linecount %s != %d +- %d", NNTP_REJECTIT_VAL,
	MaxLength(p, p), i, innconf->linecountfuzz);
      TMRstop(TMR_ARTCLEAN);
      return FALSE;
    }
  }

  /* Is article too old? */
  /* assumes Date header is required header */
  p = HDR(HDR__DATE);
  if ((data->Posted = parsedate(p, &Now)) == -1) {
    (void)sprintf(buff, "%d Bad \"Date\" header -- \"%s\"", NNTP_REJECTIT_VAL,
      MaxLength(p, p));
    TMRstop(TMR_ARTCLEAN);
    return FALSE;
  }
  if (innconf->artcutoff && data->Posted < Now.time - innconf->artcutoff) {
    (void)sprintf(buff, "%d Too old -- \"%s\"", NNTP_REJECTIT_VAL,
      MaxLength(p, p));
    TMRstop(TMR_ARTCLEAN);
    return FALSE;
  }
  if (data->Posted > Now.time + DATE_FUZZ) {
    (void)sprintf(buff, "%d Article posted in the future -- \"%s\"",
      NNTP_REJECTIT_VAL, MaxLength(p, p));
    TMRstop(TMR_ARTCLEAN);
    return FALSE;
  }
  if (HDR_FOUND(HDR__EXPIRES)) {
    p = HDR(HDR__EXPIRES);
    data->Expires = parsedate(p, &Now);
  }

  /* Colon or whitespace in the Newsgroups header? */
  /* assumes Newsgroups header is required header */
  if ((data->Groupcount =
    NGsplit(HDR(HDR__NEWSGROUPS), HDR_LEN(HDR__NEWSGROUPS),
    &data->Newsgroups)) == 0) {
    TMRstop(TMR_ARTCLEAN);
    (void)sprintf(buff, "%d Unwanted character in \"Newsgroups\" header",
      NNTP_REJECTIT_VAL);
    return FALSE;
  }

  /* Fill in other Data fields. */
  if (HDR_FOUND(HDR__SENDER))
    data->Poster = HDR(HDR__SENDER);
  else
    data->Poster = HDR(HDR__FROM);
  if (HDR_FOUND(HDR__REPLY_TO))
    data->Replyto = HDR(HDR__REPLY_TO);
  else
    data->Replyto = HDR(HDR__FROM);

  TMRstop(TMR_ARTCLEAN);
  return TRUE;
}

/*
**  We are going to reject an article, record the reason and
**  and the article.
*/
static void
ARTreject(Reject_type code, CHANNEL *cp, BUFFER *article UNUSED)
{
  /* Remember why the article was rejected (for the status file) */

  switch (code) {
    case REJECT_DUPLICATE:
      cp->Duplicate++;
      cp->DuplicateSize += cp->Next - cp->Start;
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
static bool
ARTcancelverify(const ARTDATA *data, const char *MessageID, TOKEN *token)
{
  const char	*p;
  char		*q, *q1;
  const char	*local;
  char		buff[SMBUF];
  ARTHANDLE	*art;
  bool		r;

  if (!HISlookup(History, MessageID, NULL, NULL, NULL, token))
    return false;
  if ((art = SMretrieve(*token, RETR_HEAD)) == NULL)
    return false;
  if ((local = HeaderFindMem(art->data, art->len, "Sender", 6)) == NULL
    && (local = HeaderFindMem(art->data, art->len, "From", 4)) == NULL) {
    SMfreearticle(art);
    return false;
  }
  for (p = local; p < art->data + art->len; p++) {
    if (*p == '\r' || *p == '\n')
      break;
  }
  if (p == art->data + art->len) {
    SMfreearticle(art);
    return false;
  }
  q = NEW(char, p - local + 1);
  memcpy(q, local, p - local);
  SMfreearticle(art);
  q[p - local] = '\0';
  HeaderCleanFrom(q);

  /* Compare canonical forms. */
  q1 = COPY(data->Poster);
  HeaderCleanFrom(q1);
  if (!EQ(q, q1)) {
    r = false;
    (void)sprintf(buff, "\"%.50s\" wants to cancel %s by \"%.50s\"",
      q1, MaxLength(MessageID, MessageID), q);
    ARTlog(data, ART_REJECT, buff);
  }
  else {
    r = true;
  }
  DISPOSE(q1);
  DISPOSE(q);
  return r;
}

/*
**  Process a cancel message.
*/
void
ARTcancel(const ARTDATA *data, const char *MessageID, const bool Trusted)
{
  char	buff[SMBUF+16];
  TOKEN	token;
  bool	r;

  TMRstart(TMR_ARTCNCL);
  if (!DoCancels && !Trusted) {
    TMRstop(TMR_ARTCNCL);
    return;
  }

  if (!ARTidok(MessageID)) {
    syslog(L_NOTICE, "%s bad cancel Message-ID %s", data->Feedsite,
      MaxLength(MessageID, MessageID));
    TMRstop(TMR_ARTCNCL);
    return;
  }

  if (!HIScheck(History, MessageID)) {
    /* Article hasn't arrived here, so write a fake entry using
     * most of the information from the cancel message. */
    if (innconf->verifycancels && !Trusted) {
      TMRstop(TMR_ARTCNCL);
      return;
    }
    InndHisRemember(MessageID);
    (void)sprintf(buff, "Cancelling %s", MaxLength(MessageID, MessageID));
    ARTlog(data, ART_CANC, buff);
    TMRstop(TMR_ARTCNCL);
    return;
  }
  if (Trusted || !innconf->verifycancels)
      r = HISlookup(History, MessageID, NULL, NULL, NULL, &token);
  else
      r = ARTcancelverify(data, MessageID, &token);
  if (r == false) {
    TMRstop(TMR_ARTCNCL);
    return;
  }

  /* Get stored message and zap them. */
  if (!SMcancel(token) && SMerrno != SMERR_NOENT && SMerrno != SMERR_UNINIT)
    syslog(L_ERROR, "%s cant cancel %s (SMerrno %d)", LogName,
	TokenToText(token), SMerrno);
  if (innconf->immediatecancel && !SMflushcacheddata(SM_CANCELEDART))
    syslog(L_ERROR, "%s cant cancel cached %s", LogName, TokenToText(token));
  (void)sprintf(buff, "Cancelling %s", MaxLength(MessageID, MessageID));
  ARTlog(data, ART_CANC, buff);
  TMRstop(TMR_ARTCNCL);
}

/*
**  Process a control message.  Cancels are handled here, but any others
**  are passed out to an external program in a specific directory that
**  has the same name as the first word of the control message.
*/
static void
ARTcontrol(ARTDATA *data, char *Control, CHANNEL *cp UNUSED)
{
  char *p, c;

  /* See if it's a cancel message. */
  c = *Control;
  if (c == 'c' && EQn(Control, "cancel", 6)) {
    for (p = &Control[6]; ISWHITE(*p); p++)
      continue;
    if (*p && ARTidok(p))
      ARTcancel(data, p, FALSE);
    return;
  }
}

/*
**  Parse a Distribution line, splitting it up into NULL-terminated array of
**  strings.
*/
static void
ARTparsedist(const char *p, int size, LISTBUFFER *list)
{
  int	i;
  char	*q, **dp;

  /* setup buffer */ 
  SetupListBuffer(size, list);

  /* loop over text and copy */
  for (i = 0, q = list->Data, dp = list->List ; *p ; p++, *q++ = '\0') { 
    /* skip leading separators. */
    for (; *p && ((*p == ',') || ISWHITE(*p)) ; p++)
      continue;
    if (*p == '\0')
      break;

    if (list->ListLength <= i) {
      list->ListLength += DEFAULTNGBOXSIZE;
      RENEW(list->List, char*, list->ListLength);
      dp = &list->List[i];
    }
    /* mark the start of the host, move to the end of it while copying */  
    for (*dp++ = q, i++ ; *p && (*p != ',') && !ISWHITE(*p) ;)
      *q++ = *p++;
    if (*p == '\0')
      break;
  }
  *q = '\0';
  if (i == list->ListLength) {
    list->ListLength += DEFAULTNGBOXSIZE;
    RENEW(list->List, char *, list->ListLength);
    dp = &list->List[i];
  }
  *dp = NULL;
  return;
}

/*
**  A somewhat similar routine, except that this handles negated entries
**  in the list and is used to check the distribution sub-field.
*/
static bool
DISTwanted(char **list, char *p)
{
  char	*q;
  char	c;
  bool	sawbang;

  for (sawbang = FALSE, c = *p; (q = *list) != NULL; list++) {
    if (*q == '!') {
      sawbang = TRUE;
      if (c == *++q && EQ(p, q))
	return FALSE;
    } else if (c == *q && EQ(p, q))
      return TRUE;
  }

  /* If we saw any !foo's and didn't match, then assume they are all negated
     distributions and return TRUE, else return false. */
  return sawbang;
}

/*
**  See if any of the distributions in the article are wanted by the site.
*/
static bool
DISTwantany(char **site, char **article)
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
static void
ARTsendthegroup(char *name)
{
  SITE		*sp;
  int		i;
  NEWSGROUP	*ngp;

  for (ngp = NGfind(ARTctl), sp = Sites, i = nSites; --i >= 0; sp++) {
    if (sp->Name != NULL && SITEwantsgroup(sp, name)) {
      SITEmark(sp, ngp);
    }
  }
}

/*
**  Check if site doesn't want this group even if it's crossposted
**  to a wanted group.
*/
static void
ARTpoisongroup(char *name)
{
  SITE	*sp;
  int	i;

  for (sp = Sites, i = nSites; --i >= 0; sp++) {
    if (sp->Name != NULL && (sp->PoisonEntry || ME.PoisonEntry) &&
      SITEpoisongroup(sp, name))
      sp->Poison = TRUE;
  }
}

/*
** Assign article numbers to the article and create the Xref line.
** If we end up not being able to write the article, we'll get "holes"
** in the directory and active file.
*/
static void
ARTassignnumbers(ARTDATA *data)
{
  char		*p, *q;
  int		i, len, linelen, buflen;
  NEWSGROUP	*ngp;

  if (data->XrefBufLength == 0) {
    data->XrefBufLength = MAXHEADERSIZE * 2 + 1;
    data->Xref = NEW(char, data->XrefBufLength);
    strncpy(data->Xref, Path.Data, Path.Used - 1);
  }
  len = Path.Used - 1;
  p = q = data->Xref + len;
  for (linelen = i = 0; (ngp = GroupPointers[i]) != NULL; i++) {
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
    /*  len  ' ' "news_groupname"  ':' "#" "\r\n" */
    if (len + 1 + ngp->NameLength + 1 + 10 + 2 > data->XrefBufLength) {
      data->XrefBufLength += MAXHEADERSIZE;
      RENEW(data->Xref, char, data->XrefBufLength);
      p = data->Xref + len;
    }
    if (linelen + 1 + ngp->NameLength + 1 + 10 > MAXHEADERSIZE) {
      /* line exceeded */
      (void)sprintf(p, "\r\n %s:%lu", ngp->Name, ngp->Filenum);
      buflen = strlen(p);
      linelen = buflen - 2;
    } else {
      (void)sprintf(p, " %s:%lu", ngp->Name, ngp->Filenum);
      buflen = strlen(p);
      linelen += buflen;
    }
    len += buflen;
    p += buflen;
  }
  /* p[0] is replaced with '\r' to be wireformatted when stored.  p[1] needs to
     be '\n' */
  p[0] = '\r';
  p[1] = '\n';
  /* data->XrefLength includes trailing "\r\n" */
  data->XrefLength = len + 2;
  data->Replic = q + 1;
  data->ReplicLength = len - (q + 1 - data->Xref);
}

/*
**  Parse the data from the xref header and assign the numbers.
**  This involves replacing the GroupPointers entries.
*/
static bool
ARTxrefslave(ARTDATA *data)
{
  char		*p, *q, *name, *next, c;
  NEWSGROUP	*ngp;
  int	        i;
  bool		nogroup = TRUE;
  HDRCONTENT	*hc = data->HdrContent;

  if (!HDR_FOUND(HDR__XREF))
    return FALSE;
  /* skip server name */
  if ((p = strpbrk(HDR(HDR__XREF), " \t\r\n")) == NULL)
    return FALSE;
  /* in case Xref is folded */
  while (*++p == ' ' || *p == '\t' || *p == '\r' || *p == '\n');
  if (*p == '\0')
    return FALSE;
  data->Replic = p;
  data->ReplicLength = HDR_LEN(HDR__XREF) - (p - HDR(HDR__XREF));
  for (i = 0; (*p != '\0') && (p < HDR(HDR__XREF) + HDR_LEN(HDR__XREF)) ; p = next) {
    /* Mark end of this entry and where next one starts. */
    name = p;
    if ((q = next = strpbrk(p, " \t\r\n")) != NULL) {
      c = *q;
      *q = '\0';
      while (*++next == ' ' || *next == '\t' || *next == '\r' || *next == '\n');
    } else {
      q = NULL;
      next = "";
    }

    /* Split into news.group:# */
    if ((p = strchr(p, ':')) == NULL) {
      syslog(L_ERROR, "%s bad_format %s", LogName, name);
      if (q != NULL)
	*q = c;
      continue;
    }
    *p = '\0';
    if ((ngp = NGfind(name)) == NULL) {
      syslog(L_ERROR, "%s bad_newsgroup %s", LogName, name);
      *p = ':';
      if (q != NULL)
	*q = c;
      continue;
    }
    *p = ':';
    ngp->Filenum = atol(p + 1);
    if (q != NULL)
      *q = c;

    /* Update active file if we got a new high-water mark. */
    if (ngp->Last < ngp->Filenum) {
      ngp->Last = ngp->Filenum;
      if (!FormatLong(ngp->LastString, (long)ngp->Last, ngp->Lastwidth)) {
	syslog(L_ERROR, "%s cant update_active %s", LogName, ngp->Name);
	continue;
      }
    }
    /* Mark that this group gets the article. */
    ngp->PostCount++;
    GroupPointers[i++] = ngp;
    nogroup = FALSE;
  }
  if (nogroup)
    return FALSE;
  return TRUE;
}

/*
**  Return TRUE if a list of strings has a specific one.  This is a
**  generic routine, but is used for seeing if a host is in the Path line.
*/
static bool
ListHas(const char **list, const char *p)
{
  const char	*q;
  char		c;

  for (c = *p; (q = *list) != NULL; list++)
    if (caseEQ(p, q))
      return TRUE;
  return FALSE;
}

/*
**  Propagate an article to the sites have "expressed an interest."
*/
static void
ARTpropagate(ARTDATA *data, const char **hops, int hopcount, char **list,
  bool ControlStore, bool OverviewCreated)
{
  HDRCONTENT	*hc = data->HdrContent;
  SITE		*sp, *funnel;
  int		i, j, Groupcount, Followcount, Crosscount;
  char	        *p, *q;
  BUFFER	*bp;
  bool		sendit;

  /* Work out which sites should really get it. */
  Groupcount = data->Groupcount;
  Followcount = data->Followcount;
  Crosscount = Groupcount + Followcount * Followcount;
  for (sp = Sites, i = nSites; --i >= 0; sp++) {
    if ((sp->IgnoreControl && ControlStore) ||
      (sp->NeedOverviewCreation && !OverviewCreated))
      sp->Sendit = FALSE;
    if (sp->Seenit || !sp->Sendit)
      continue;
    sp->Sendit = FALSE;
	
    if (sp->Originator) {
      if (!HDR_FOUND(HDR__XTRACE)) {
	if (!sp->FeedwithoutOriginator)
	  continue;
      } else {
	if ((p = strchr(HDR(HDR__XTRACE), ' ')) != NULL) {
	  *p = '\0';
	  for (j = 0, sendit = FALSE; (q = sp->Originator[j]) != NULL; j++) {
	    if (*q == '@') {
	      if (wildmat(HDR(HDR__XTRACE), &q[1])) {
		*p = ' ';
		sendit = FALSE;
		break;
	      }
	    } else {
	      if (wildmat(HDR(HDR__XTRACE), q))
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

    if (sp->MaxSize && data->BytesValue > sp->MaxSize)
      /* Too big for the site. */
      continue;

    if (sp->MinSize && data->BytesValue < sp->MinSize)
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

    if (list && *list != NULL && sp->Distributions &&
      !DISTwantany(sp->Distributions, list))
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
    if (innconf->logsitename) {
      if (fprintf(Log, " %s", sp->Name) == EOF || ferror(Log)) {
	j = errno;
	syslog(L_ERROR, "%s cant write log_site %m", LogName);
	IOError("logging site", j);
	clearerr(Log);
      }
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
  for (sp = Sites, i = nSites; --i >= 0; sp++) {
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

static int
wvec_freq_cmp(const void *p1, const void *p2)
{
    return ((const struct word_entry *)p2)->count -	/* decreasing sort */
           ((const struct word_entry *)p1)->count;
}

/*
** Wrapper for qsort(3) comparison of word_entry (word length).
*/

static int
wvec_length_cmp(const void *p1, const void *p2)
{
    return ((const struct word_entry *)p2)->length -	/* decreasing sort */
           ((const struct word_entry *)p1)->length;
}

/*
** Wrapper for qsort(3), for pointer-to-pointer strings.
*/

static int
ptr_strcmp(const void *p1, const void *p2)
{
    int cdiff;

    cdiff = (**(const char **)p1) - (**(const char **)p2);
    if (cdiff)
	return cdiff;
    return strcmp((*(const char **)p1)+1, (*(const char **)p2)+1);
}

/*
**  Build new Keywords.
*/

static void
ARTmakekeys(
    HDRCONTENT	*hc,	/* header data */
    char	*body,	/* article body */
    char	*v,	/* old kw value */
    int	l)		/* old kw length */
{

    int		word_count, word_length, bodylen, word_index, distinct_words;
    int		last;
    char	*text, *orig_text, *text_end, *this_word, *chase, *punc;
    static struct word_entry	*word_vec;
    static char		**word;
    static const char	*whitespace  = " \t\r\n";

    /* ---------------------------------------------------------------- */
    /* Prototype setup: Regex match preparation. */
    static	int	regex_lib_init = 0;
    static	regex_t	preg;
    static const char	*elim_regexp = "^\\([-+/0-9][-+/0-9]*\\|.*1st\\|.*2nd\\|.*3rd\\|.*[04-9]th\\|about\\|after\\|ago\\|all\\|already\\|also\\|among\\|and\\|any\\|anybody\\|anyhow\\|anyone\\|anywhere\\|are\\|bad\\|because\\|been\\|before\\|being\\|between\\|but\\|can\\|could\\|did\\|does\\|doing\\|done\\|dont\\|during\\|eight\\|eighth\\|eleven\\|else\\|elsewhere\\|every\\|everywhere\\|few\\|five\\|fifth\\|first\\|for\\|four\\|fourth\\|from\\|get\\|going\\|gone\\|good\\|got\\|had\\|has\\|have\\|having\\|he\\|her\\|here\\|hers\\|herself\\|him\\|himself\\|his\\|how\\|ill\\|into\\|its\\|ive\\|just\\|kn[eo]w\\|least\\|less\\|let\\|like\\|look\\|many\\|may\\|more\\|m[ou]st\\|myself\\|next\\|nine\\|ninth\\|not\\|now\\|off\\|one\\|only\\|onto\\|our\\|out\\|over\\|really\\|said\\|saw\\|says\\|second\\|see\\|set\\|seven\\|seventh\\|several\\|shall\\|she\\|should\\|since\\|six\\|sixth\\|some\\|somehow\\|someone\\|something\\|somewhere\\|such\\|take\\|ten\\|tenth\\|than\\|that\\|the\\|their\\!|them\\|then\\|there\\|therell\\|theres\\|these\\|they\\|thing\\|things\\|third\\|this\\|those\\|three\\|thus\\|together\\|told\\|too\\|twelve\\|two\\|under\\|upon\\|very\\|via\\|want\\|wants\\|was\\|wasnt\\|way\\|were\\|weve\\|what\\|whatever\\|when\\|where\\|wherell\\|wheres\\|whether\\|which\\|while\\|who\\|why\\|will\\|will\\|with\\|would\\|write\\|writes\\|wrote\\|yes\\|yet\\|you\\|your\\|youre\\|yourself\\)$";

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
    hc->Value = malloc(innconf->keylimit+1);
    if ((v != NULL) && (*v != '\0')) {
        strncpy(hc->Value, v, l);
        hc->Value[l] = '\0';
    } else
        *hc->Value = '\0';
    l = hc->Length = strlen(hc->Value);

    /*
     * now figure acceptable extents, and copy body to working string.
     * (Memory-intensive for hefty articles: limit to non-ABSURD articles.)
     */
    bodylen = strlen(body);
    if ((bodylen < 100) || (bodylen > innconf->keyartlimit)) /* too small/big to bother */
	return;

    orig_text = text = xstrdup(body);	/* orig_text is for free() later on */
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
    strcpy(hc->Value + l, ",\377");		/* magic separator, 'ÿ' */
    for (chase = hc->Value + l + 2, word_index = 0;
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

	if (chase - hc->Value > (innconf->keylimit - (MAX_WORD_LENGTH + 4)))
	    break;
    }
    /* note #words we didn't get to add. */
    /* This code can potentially lead to a buffer overflow if the number of
       ignored words is greater than 100, under some circumstances.  It's
       temporarily disabled until fixed. */
    hc->Length = strlen(hc->Value);

out:
    /* We must dispose of the original strdup'd text area. */
    free(orig_text);
}
#endif	/* defined(DO_KEYWORDS) */

/*
**  Build up the overview data.
*/
static void
#if	defined(DO_KEYWORDS)
ARTmakeoverview(CHANNEL *cp)
{
  ARTDATA	*data = &cp->Data;
#else
ARTmakeoverview(ARTDATA *data)
{
#endif	/* defined(DO_KEYWORDS) */
  HDRCONTENT	*hc = data->HdrContent;
  static char	SEP[] = "\t";
  static char	COLONSPACE[] = ": ";
  BUFFER	*overview = &data->Overview;
  ARTOVERFIELD	*fp;
  ARTHEADER	*hp;
  char		*p, *q;
  int		i, j, len;
#if	defined(DO_KEYWORDS)
  char		*key_old_value = NULL;
  int		key_old_length = 0;
#endif	/* defined(DO_KEYWORDS) */


  if (ARTfields == NULL) {
    /* User error. */
    return;
  }

  /* Setup. */
  if (overview->Size == 0) {
    overview->Size = MAXHEADERSIZE;
    overview->Data = NEW(char, overview->Size);
  }

  BUFFset(overview, "", 0);

  /* Write the data, a field at a time. */
  for (fp = ARTfields; fp->Header; fp++) {
    if (fp != ARTfields)
      BUFFappend(overview, SEP, STRLEN(SEP));
    hp = fp->Header;
    j = hp - ARTheaders;

#if	defined(DO_KEYWORDS)
    if (innconf->keywords) {
      /* Ensure that there are Keywords: to shovel. */
      if (hp == &ARTheaders[HDR__KEYWORDS]) {
	key_old_value  = HDR(HDR__KEYWORDS);
	key_old_length = HDR_LEN(HDR__KEYWORDS);
	ARTmakekeys(&hc[HDR__KEYWORDS], cp->In.Data + data->Body, key_old_value,
	  key_old_length);
      }
    }
#endif	/* defined(DO_KEYWORDS) */

    switch (j) {
      case HDR__BYTES:
	p = data->Bytes + 7; /* skip "Bytes: " */
	len = data->BytesLength;
	break;
      case HDR__XREF:
	if (innconf->xrefslave) {
	  p = HDR(j);
	  len = HDR_LEN(j);
	} else {
	  p = data->Xref;
	  len = data->XrefLength - 2;
	}
	break;
      default:
	p = HDR(j);
	len = HDR_LEN(j);
	break;
    }
    if (len == 0)
      continue;
    if (fp->NeedHeader) {
      BUFFappend(overview, hp->Name, hp->Size);
      BUFFappend(overview, COLONSPACE, STRLEN(COLONSPACE));
    }
    if (overview->Used + overview->Left + len > overview->Size) {
        /* Round size up to next 1K */
        overview->Size += (len + 0x3FF) & ~0x3FF;
        RENEW(overview->Data, char, overview->Size);
    }
    for (i = 0, q = &overview->Data[overview->Left] ; i < len ; p++, q++, i++) {
      /* we can replace consecutive '\r', '\n' and '\r' with one ' ' here */
      if (*p == '\t' || *p == '\n' || *p == '\r')
	*q = ' ';
      else
	*q = *p;
    }
    overview->Left += len;

#if	defined(DO_KEYWORDS)
    if (innconf->keywords) {
      if (key_old_value) {
	if (hc->Value)
	  free(hc->Value);		/* malloc'd within */
	hc->Value  = key_old_value;
	hc->Length = key_old_length;
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
bool
ARTpost(CHANNEL *cp)
{
  char		*buff = cp->Error;
  char		*p, **groups, ControlWord[16], tmpbuff[32], **hops;
  int		i, j, *isp, hopcount, oerrno, canpost;
  NEWSGROUP	*ngp, **ngptr;
  SITE		*sp;
  ARTDATA	*data = &cp->Data;
  HDRCONTENT	*hc = data->HdrContent;
  bool		Approved, Accepted, LikeNewgroup, ToGroup, GroupMissing;
  bool		NoHistoryUpdate, artclean;
  bool		MadeOverview = FALSE;
  bool		ControlStore = FALSE;
  bool		NonExist = FALSE;
  bool		OverviewCreated = FALSE;
  bool		IsControl = FALSE;
  bool		Filtered = FALSE;
  BUFFER	*article;
  HASH		hash;
  TOKEN		token;
  char		*groupbuff[2];
#if defined(DO_PERL) || defined(DO_PYTHON)
  char		*filterrc;
#endif /* defined(DO_PERL) || defined(DO_PYTHON) */
  OVADDRESULT	result;

  /* Preliminary clean-ups. */
  article = &cp->In;
  artclean = ARTclean(data, cp->Error);
  if (!artclean)
    return FALSE;

  /* assumes Path header is required header */
  hopcount = ARTparsepath(HDR(HDR__PATH), HDR_LEN(HDR__PATH), &data->Path);
  if (hopcount == 0) {
    sprintf(buff, "%d illegal path element", NNTP_REJECTIT_VAL);
    ARTlog(data, ART_REJECT, buff);
    if (innconf->remembertrash && (Mode == OMrunning) &&
	!InndHisRemember(HDR(HDR__MESSAGE_ID)))
      syslog(L_ERROR, "%s cant write history %s %m", LogName,
	HDR(HDR__MESSAGE_ID));
    ARTreject(REJECT_OTHER, cp, article);
    return FALSE;
  }
  hops = data->Path.List;

  if (innconf->logipaddr) {
    data->Feedsite = RChostname(cp);
    if (data->Feedsite == NULL)
      data->Feedsite = CHANname(cp);
    if (strcmp("0.0.0.0", data->Feedsite) == 0)
      data->Feedsite = hops && hops[0] ? hops[0] : CHANname(cp);
  } else {
    data->Feedsite = hops && hops[0] ? hops[0] : CHANname(cp);
  }
  data->FeedsiteLength = strlen(data->Feedsite);

  hash = HashMessageID(HDR(HDR__MESSAGE_ID));
  data->Hash = &hash;
  if (HIScheck(History, HDR(HDR__MESSAGE_ID))) {
    sprintf(buff, "%d Duplicate", NNTP_REJECTIT_VAL);
    ARTlog(data, ART_REJECT, buff);
    ARTreject(REJECT_DUPLICATE, cp, article);
    return FALSE;
  }

  if (strncmp(Path.Data, hops[0], Path.Used - 1) == 0)
    Hassamepath = TRUE;
  else
    Hassamepath = FALSE;
  if (Pathalias.Data != NULL &&
    !ListHas((const char **)hops, (const char *)innconf->pathalias))
    AddAlias = TRUE;
  else
    AddAlias = FALSE;

  /* And now check the path for unwanted sites -- Andy */
  for(j = 0 ; ME.Exclusions && ME.Exclusions[j] ; j++) {
    if (ListHas((const char **)hops, (const char *)ME.Exclusions[j])) {
      (void)sprintf(buff, "%d Unwanted site %s in path",
	NNTP_REJECTIT_VAL, MaxLength(ME.Exclusions[j], ME.Exclusions[j]));
      ARTlog(data, ART_REJECT, buff);
      if (innconf->remembertrash && (Mode == OMrunning) &&
	  !InndHisRemember(HDR(HDR__MESSAGE_ID)))
	syslog(L_ERROR, "%s cant write history %s %m", LogName,
	  HDR(HDR__MESSAGE_ID));
      ARTreject(REJECT_SITE, cp, article);
      return FALSE;
    }
  }

#if defined(DO_PERL) || defined(DO_PYTHON)
  filterPath = HDR(HDR__PATH);
#endif /* DO_PERL || DO_PYHTON */

#if defined(DO_PYTHON)
  TMRstart(TMR_PYTHON);
  filterrc = PYartfilter(data, article->Data + data->Body,
    cp->Next - data->Body, data->Lines);
  TMRstop(TMR_PYTHON);
  if (filterrc != NULL) {
    if (innconf->dontrejectfiltered) {
      Filtered = TRUE;
    } else {
      (void)sprintf(buff, "%d %.200s", NNTP_REJECTIT_VAL, filterrc);
      syslog(L_NOTICE, "rejecting[python] %s %s", HDR(HDR__MESSAGE_ID), buff);
      ARTlog(data, ART_REJECT, buff);
      if (innconf->remembertrash && (Mode == OMrunning) &&
	  !InndHisRemember(HDR(HDR__MESSAGE_ID)))
	syslog(L_ERROR, "%s cant write history %s %m", LogName,
	  HDR(HDR__MESSAGE_ID));
      ARTreject(REJECT_FILTER, cp, article);
      return FALSE;
    }
  }
#endif /* DO_PYTHON */

  /* I suppose some masochist will run with Python and Perl in together */

#if defined(DO_PERL)
  TMRstart(TMR_PERL);
  filterrc = PLartfilter(data, article->Data + data->Body,
    cp->Next - data->Body, data->Lines);
  TMRstop(TMR_PERL);
  if (filterrc) {
    if (innconf->dontrejectfiltered) {
      Filtered = TRUE;
    } else {
      sprintf(buff, "%d %.200s", NNTP_REJECTIT_VAL, filterrc);
      syslog(L_NOTICE, "rejecting[perl] %s %s", HDR(HDR__MESSAGE_ID), buff);
      ARTlog(data, ART_REJECT, buff);
      if (innconf->remembertrash && (Mode == OMrunning) &&
	  !InndHisRemember(HDR(HDR__MESSAGE_ID)))
	syslog(L_ERROR, "%s cant write history %s %m", LogName,
	  HDR(HDR__MESSAGE_ID));
      ARTreject(REJECT_FILTER, cp, article);
      return FALSE;
    }
  }
#endif /* DO_PERL */

  /* I suppose some masochist will run with both TCL and Perl in together */

#if defined(DO_TCL)
  if (TCLFilterActive) {
    int code;
    ARTHEADER *hp;

    /* make info available to Tcl */

    TCLCurrArticle = article;
    TCLCurrData = data;
    (void)Tcl_UnsetVar(TCLInterpreter, "Body", TCL_GLOBAL_ONLY);
    (void)Tcl_UnsetVar(TCLInterpreter, "Headers", TCL_GLOBAL_ONLY);
    for (i = 0 ; i < MAX_ARTHEADER ; i++, hc++) {
      if (HDR_FOUND(i)) {
	hp = &ARTheaders[i];
	Tcl_SetVar2(TCLInterpreter, "Headers", (char *) hp->Name, HDR(i),
	  TCL_GLOBAL_ONLY);
      }
    }
    Tcl_SetVar(TCLInterpreter, "Body", article->Data + data->Body,
      TCL_GLOBAL_ONLY);
    /* call filter */

    code = Tcl_Eval(TCLInterpreter, "filter_news");
    (void)Tcl_UnsetVar(TCLInterpreter, "Body", TCL_GLOBAL_ONLY);
    (void)Tcl_UnsetVar(TCLInterpreter, "Headers", TCL_GLOBAL_ONLY);
    if (code == TCL_OK) {
      if (strcmp(TCLInterpreter->result, "accept") != 0) {
        if (innconf->dontrejectfiltered) {
	  Filtered = TRUE;
        } else {
	  (void)sprintf(buff, "%d %.200s", NNTP_REJECTIT_VAL,
	    TCLInterpreter->result);
	  syslog(L_NOTICE, "rejecting[tcl] %s %s", HDR(HDR__MESSAGE_ID), buff);
	  ARTlog(data, ART_REJECT, buff);
	  if (innconf->remembertrash && (Mode == OMrunning) &&
	      !InndHisRemember(HDR(HDR__MESSAGE_ID)))
	    syslog(L_ERROR, "%s cant write history %s %m",
	      LogName, HDR(HDR__MESSAGE_ID));
	  ARTreject(REJECT_FILTER, cp, article);
	  return FALSE;
	}
      }
    } else {
      /* the filter failed: complain and then turn off filtering */
      syslog(L_ERROR, "TCL proc filter_news failed: %s",
	TCLInterpreter->result);
      TCLfilter(FALSE);
    }
  }
#endif /* defined(DO_TCL) */

  /* If we limit what distributions we get, see if we want this one. */
  if (HDR_FOUND(HDR__DISTRIBUTION)) {
    if (HDR(HDR__DISTRIBUTION)[0] == ',') {
      (void)sprintf(buff, "%d bogus distribution \"%s\"", NNTP_REJECTIT_VAL,
	MaxLength(HDR(HDR__DISTRIBUTION), HDR(HDR__DISTRIBUTION)));
      ARTlog(data, ART_REJECT, buff);
      if (innconf->remembertrash && Mode == OMrunning &&
	  !InndHisRemember(HDR(HDR__MESSAGE_ID)))
        syslog(L_ERROR, "%s cant write history %s %m", LogName,
	  HDR(HDR__MESSAGE_ID));
      ARTreject(REJECT_DISTRIB, cp, article);
      return FALSE;
    } else {
      ARTparsedist(HDR(HDR__DISTRIBUTION), HDR_LEN(HDR__DISTRIBUTION),
	&data->Distribution);
      if (ME.Distributions &&
	!DISTwantany(ME.Distributions, data->Distribution.List)) {
	(void)sprintf(buff, "%d Unwanted distribution \"%s\"",
	  NNTP_REJECTIT_VAL, MaxLength(data->Distribution.List[0],
	  data->Distribution.List[0]));
	ARTlog(data, ART_REJECT, buff);
        if (innconf->remembertrash && (Mode == OMrunning) &&
	    !InndHisRemember(HDR(HDR__MESSAGE_ID)))
	  syslog(L_ERROR, "%s cant write history %s %m",
	    LogName, HDR(HDR__MESSAGE_ID));
	ARTreject(REJECT_DISTRIB, cp, article);
	return FALSE;
      }
    }
  } else {
    ARTparsedist("", 0, &data->Distribution);
  }

  for (i = nSites, sp = Sites; --i >= 0; sp++) {
    sp->Poison = FALSE;
    sp->Sendit = FALSE;
    sp->Seenit = FALSE;
    sp->FNLnames.Used = 0;
    sp->ng = NULL;
  }

  if (HDR_FOUND(HDR__FOLLOWUPTO)) {
    for (i = 0, p = HDR(HDR__FOLLOWUPTO) ; (p = strchr(p, ',')) != NULL ;
      i++, p++)
      continue;
    data->Followcount = i;
  }
  if (data->Followcount == 0)
    data->Followcount = data->Groupcount;

  groups = data->Newsgroups.List;
  /* Parse the Control header. */
  LikeNewgroup = FALSE;
  if (HDR_FOUND(HDR__CONTROL)) {
    IsControl = TRUE;

    /* Nip off the first word into lowercase. */
    strncpy(ControlWord, HDR(HDR__CONTROL), sizeof ControlWord);
    ControlWord[sizeof ControlWord - 1] = '\0';
    for (p = ControlWord; *p && !ISWHITE(*p); p++)
      if (CTYPE(isupper, *p))
	*p = tolower(*p);
    *p = '\0';
    LikeNewgroup = EQ(ControlWord, "newgroup") || EQ(ControlWord, "rmgroup");

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
      data->Groupcount = 2;
      if (data->Followcount == 0)
	data->Followcount = data->Groupcount;
    }
    /* Control messages to "foo.ctl" are treated as if they were
     * posted to "foo".  I should probably apologize for all the
     * side-effects in the if. */
    for (i = 0; (p = groups[i++]) != NULL; )
      if ((j = strlen(p) - 4) > 0 && *(p += j) == '.'
	&& p[1] == 'c' && p[2] == 't' && p[3] == 'l')
	  *p = '\0';
  }

  /* Loop over the newsgroups, see which ones we want, and get the
   * total space needed for the Xref line.  At the end of this section
   * of code, j will have the needed length, the appropriate site
   * entries will have their Sendit and ng fields set, and GroupPointers
   * will have pointers to the relevant newsgroups. */
  ToGroup = NoHistoryUpdate = FALSE;
  Approved = HDR_FOUND(HDR__APPROVED);
  ngptr = GroupPointers;
  for (GroupMissing = Accepted = FALSE; (p = *groups) != NULL; groups++) {
    if ((ngp = NGfind(p)) == NULL) {
      GroupMissing = TRUE;
      if (LikeNewgroup && Approved) {
	/* Newgroup/rmgroup being sent to a group that doesn't exist.  Assume
	 * it is being sent to the group being created or removed, nd send the
	 * group to all sites that would or would have had the group if it were
	 * created. */
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
	NNTP_REJECTIT_VAL, MaxLength(ngp->Name, ngp->Name));
      ARTlog(data, ART_REJECT, buff);
      if (innconf->remembertrash && (Mode == OMrunning) &&
	  !InndHisRemember(HDR(HDR__MESSAGE_ID)))
	syslog(L_ERROR, "%s cant write history %s %m", LogName,
	  HDR(HDR__MESSAGE_ID));
      ARTreject(REJECT_UNAPP, cp, article);
      return FALSE;
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
    } else if (canpost < 0) {
      (void)sprintf(buff, "%d Won't accept posts in \"%s\"",
	NNTP_REJECTIT_VAL, MaxLength(p, p));
      ARTlog(data, ART_REJECT, buff);
      ARTreject(REJECT_GROUP, cp, article);
      return FALSE;
    }

    /* Valid group, feed it to that group's sites. */
    Accepted = TRUE;
    for (isp = ngp->Sites, i = ngp->nSites; --i >= 0; isp++) {
      if (*isp >= 0) {
	sp = &Sites[*isp];
	if (!sp->Poison)
	  SITEmark(sp, ngp);
      }
    }

    /* If it's excluded, don't file it. */
    if (ngp->Rest[0] == NF_FLAG_EXCLUDED)
      continue;

    /* Expand aliases, mark the article as getting filed in the group. */
    if (ngp->Alias != NULL)
      ngp = ngp->Alias;
    *ngptr++ = ngp;
    ngp->PostCount = 0;
  }

  /* Loop over sites to find Poisons/ControlOnly and undo Sendit flags. */
  for (i = nSites, sp = Sites; --i >= 0; sp++) {
    if (sp->Poison || (sp->ControlOnly && !IsControl)
      || (sp->DontWantNonExist && NonExist))
      sp->Sendit = FALSE;		
  }

  /* Control messages not filed in "to" get filed only in control.name
   * or control. */
  if (IsControl && Accepted && !ToGroup) {
    ControlStore = TRUE;
    FileGlue(tmpbuff, "control", '.', ControlWord);
    if ((ngp = NGfind(tmpbuff)) == NULL)
      ngp = NGfind(ARTctl);
    ngp->PostCount = 0;
    ngptr = GroupPointers;
    *ngptr++ = ngp;
    for (isp = ngp->Sites, i = ngp->nSites; --i >= 0; isp++) {
      if (*isp >= 0) {
	sp = &Sites[*isp];
	SITEmark(sp, ngp);
      }
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
	(void)sprintf(buff, "%d Can't post to \"%s\"", NNTP_REJECTIT_VAL,
	  MaxLength(data->Newsgroups.List[0], data->Newsgroups.List[0]));
      } else {
        (void)sprintf(buff, "%d Unwanted newsgroup \"%s\"", NNTP_REJECTIT_VAL,
	  MaxLength(data->Newsgroups.List[0], data->Newsgroups.List[0]));
      }
      ARTlog(data, ART_REJECT, buff);
      if (!innconf->wanttrash) {
	if (innconf->remembertrash && (Mode == OMrunning) &&
	  !NoHistoryUpdate && !InndHisRemember(HDR(HDR__MESSAGE_ID)))
	  syslog(L_ERROR, "%s cant write history %s %m",
	    LogName, HDR(HDR__MESSAGE_ID));
	ARTreject(REJECT_GROUP, cp, article);
	return FALSE;
      } else {
        /* if !GroupMissing, then all the groups the article was posted
         * to have a flag of "x" in our active file, and therefore
         * we should throw the article away:  if you have set
         * innconf->remembertrash true, then you want all trash except that
         * which you explicitly excluded in your active file. */
  	if (!GroupMissing) {
	  if (innconf->remembertrash && (Mode == OMrunning) &&
	      !NoHistoryUpdate && !InndHisRemember(HDR(HDR__MESSAGE_ID)))
	    syslog(L_ERROR, "%s cant write history %s %m",
	      LogName, HDR(HDR__MESSAGE_ID));
	  ARTreject(REJECT_GROUP, cp, article);
	    return FALSE;
	}
      }
    }
    ngp = NGfind(ARTjnk);
    *ngptr++ = ngp;
    ngp->PostCount = 0;

    /* Junk can be fed to other sites. */
    for (isp = ngp->Sites, i = ngp->nSites; --i >= 0; isp++) {
      if (*isp >= 0) {
	sp = &Sites[*isp];
	if (!sp->Poison && !(sp->ControlOnly && !IsControl))
	  SITEmark(sp, ngp);
      }
    }
  }
  *ngptr = NULL;

  if (innconf->xrefslave) {
    if (ARTxrefslave(data) == FALSE) {
      if (HDR_FOUND(HDR__XREF)) {
	(void)sprintf(buff, "%d Invalid Xref header \"%s\"", NNTP_REJECTIT_VAL,
	  MaxLength(HDR(HDR__XREF), HDR(HDR__XREF)));
      } else {
	(void)sprintf(buff, "%d No Xref header", NNTP_REJECTIT_VAL);
      }
      ARTlog(data, ART_REJECT, buff);
      ARTreject(REJECT_OTHER, cp, article);
      return FALSE;
    }
  } else {
    ARTassignnumbers(data);
  }

  /* Now we can file it. */
  if (++ICDactivedirty >= innconf->icdsynccount) {
    ICDwriteactive();
    ICDactivedirty = 0;
  }
  TMRstart(TMR_ARTWRITE);
  for (i = 0; (ngp = GroupPointers[i]) != NULL; i++)
    ngp->PostCount = 0;

  token = ARTstore(cp);
  /* change trailing '\r\n' to '\0\n' of all system header */
  for (i = 0 ; i < MAX_ARTHEADER ; i++) {
    if (HDR_FOUND(i))
      HDR_PARSE_START(i);
  }
  if (token.type == TOKEN_EMPTY) {
    syslog(L_ERROR, "%s cant store article: %s", LogName, SMerrorstr);
    sprintf(buff, "%d cant store article", NNTP_RESENDIT_VAL);
    ARTlog(data, ART_REJECT, buff);
    if ((Mode == OMrunning) && !InndHisRemember(HDR(HDR__MESSAGE_ID)))
      syslog(L_ERROR, "%s cant write history %s %m", LogName,
	HDR(HDR__MESSAGE_ID));
    ARTreject(REJECT_OTHER, cp, article);
    TMRstop(TMR_ARTWRITE);
    return FALSE;
  }
  TMRstop(TMR_ARTWRITE);
  if (innconf->enableoverview && !innconf->useoverchan || NeedOverview) {
    TMRstart(TMR_OVERV);
#if	defined(DO_KEYWORDS)
    ARTmakeoverview(cp);
#else
    ARTmakeoverview(data);
#endif	/* defined(DO_KEYWORDS) */
    MadeOverview = TRUE;
    if (innconf->enableoverview && !innconf->useoverchan) {
      if ((result = OVadd(token, data->Overview.Data, data->Overview.Left,
	data->Arrived, data->Expires)) == OVADDFAILED) {
	if (OVctl(OVSPACE, (void *)&i) && i == OV_NOSPACE)
	  IOError("creating overview", ENOSPC);
	else
	  IOError("creating overview", 0);
	syslog(L_ERROR, "%s cant store overview for %s", LogName,
	  TokenToText(token));
	OverviewCreated = FALSE;
      } else {
	if (result == OVADDCOMPLETED)
	  OverviewCreated = TRUE;
	else
	  OverviewCreated = FALSE;
      }
    }
    TMRstop(TMR_OVERV);
  }
  strcpy(data->TokenText, TokenToText(token));

  /* Update history if we didn't get too many I/O errors above. */
  if ((Mode != OMrunning) ||
      !InndHisWrite(HDR(HDR__MESSAGE_ID), data->Arrived, data->Posted,
		    data->Expires, &token)) {
    i = errno;
    syslog(L_ERROR, "%s cant write history %s %m", LogName,
      HDR(HDR__MESSAGE_ID));
    (void)sprintf(buff, "%d cant write history, %s", NNTP_RESENDIT_VAL,
      strerror(errno));
    ARTlog(data, ART_REJECT, buff);
    ARTreject(REJECT_OTHER, cp, article);
    return FALSE;
  }

  if (NeedStoredGroup)
    data->StoredGroupLength = strlen(data->Newsgroups.List[0]);

  /* Start logging, then propagate the article. */
  if (data->CRwithoutLF > 0 || data->LFwithoutCR > 0) {
    if (data->CRwithoutLF > 0 && data->LFwithoutCR == 0)
      (void)sprintf(buff, "%d article includes CR without LF(%d)",
	NNTP_REJECTIT_VAL, data->CRwithoutLF);
    else if (data->CRwithoutLF == 0 && data->LFwithoutCR > 0)
      (void)sprintf(buff, "%d article includes LF without CR(%d)",
	NNTP_REJECTIT_VAL, data->LFwithoutCR);
    else
      (void)sprintf(buff,
	"%d article includes CR without LF(%d) and LF withtout CR(%d)",
	NNTP_REJECTIT_VAL, data->CRwithoutLF, data->LFwithoutCR);
    ARTlog(data, ART_STRSTR, buff);
  }
  ARTlog(data, Accepted ? ART_ACCEPT : ART_JUNK, (char *)NULL);
  if ((innconf->nntplinklog) &&
    (fprintf(Log, " (%s)", data->TokenText) == EOF || ferror(Log))) {
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

  cp->Size += data->BytesValue;
  if (innconf->logartsize) {
    if (fprintf(Log, " %ld", data->BytesValue) == EOF || ferror (Log)) {
      oerrno = errno;
      syslog(L_ERROR, "%s cant write artsize %m", LogName);
      IOError("logging artsize", oerrno);
      clearerr(Log);
    }
  }

  ARTpropagate(data, (const char **)hops, hopcount, data->Distribution.List,
    ControlStore, OverviewCreated);

  /* Now that it's been written, process the control message.  This has
   * a small window, if we get a new article before the newgroup message
   * has been processed.  We could pause ourselves here, but it doesn't
   * seem to be worth it. */
  if (Accepted) {
    if (IsControl) {
      ARTcontrol(data, HDR(HDR__CONTROL), cp);
    }
    if (DoCancels && HDR_FOUND(HDR__SUPERSEDES)) {
      if (ARTidok(HDR(HDR__SUPERSEDES)))
	ARTcancel(data, HDR(HDR__SUPERSEDES), FALSE);
    }
  }

  /* And finally, send to everyone who should get it */
  for (sp = Sites, i = nSites; --i >= 0; sp++) {
    if (sp->Sendit) {
      if (!Filtered || !sp->DropFiltered) {
	TMRstart(TMR_SITESEND);
	SITEsend(sp, data);
	TMRstop(TMR_SITESEND);
      }
    }
  }

  return TRUE;
}

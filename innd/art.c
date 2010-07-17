/*  $Id$
**
**  Article-processing.
*/

#include "config.h"
#include "clibrary.h"
#include <sys/uio.h>

#include "inn/innconf.h"
#include "inn/md5.h"
#include "inn/ov.h"
#include "inn/storage.h"
#include "inn/vector.h"
#include "inn/wire.h"
#include "innd.h"

typedef struct iovec	IOVEC;

#define	ARTIOVCNT	16

extern bool DoCancels;

/* Characters used in log messages indicating the disposition of messages. */
#define ART_ACCEPT              '+'
#define ART_CANC                'c'
#define ART_STRSTR              '?'
#define ART_JUNK                'j'
#define ART_REJECT              '-'

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
  const ARTHEADER *Header;
  struct _TREE	*Before;
  struct _TREE	*After;
} TREE;

static TREE	*ARTheadertree;

/*
**  For doing the overview database, we keep a list of the headers and
**  a flag saying if they're written in brief or full format.
*/
typedef struct _ARTOVERFIELD {
  const ARTHEADER *Header;
  bool		NeedHeader;
} ARTOVERFIELD;

static ARTOVERFIELD	*ARTfields;

/*
**  General newsgroup we care about, and what we put in the Path line.
*/
static char	ARTctl[] = "control";
static char	ARTjnk[] = "junk";

/*
**  Flag array, indexed by character.  Character classes for hostnames.
*/
static char             hostcclass[256];
#define ARThostchar(c)  ((hostcclass[(unsigned char)(c)]) != 0)

#if defined(DO_PERL) || defined(DO_PYTHON)
const char	*filterPath;
#endif /* DO_PERL || DO_PYTHON */

/* Prototypes. */
static void ARTerror(CHANNEL *cp, const char *format, ...)
    __attribute__((__format__(printf, 2, 3)));
static void ARTparsebody(CHANNEL *cp);



/*
**  Trim '\r' from buffer.
*/
static void
buffer_trimcr(struct buffer *bp)
{
    char *p, *q;
    int trimmed = 0;

    for (p = q = bp->data ; p < bp->data + bp->left ; p++) {
	if (*p == '\r' && p+1 < bp->data + bp->left && p[1] == '\n') {
	    trimmed++;
	    continue;
	}
	*q++ = *p;
    }
    bp->left -= trimmed;
}

/*
**  Mark that the site gets this article.
*/
static void
SITEmark(SITE *sp, NEWSGROUP *ngp)
{
  SITE	*funnel;

  sp->Sendit = true;
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
  const struct cvector *standardoverview;
  const struct vector  *extraoverview;
  unsigned int         i;
  ARTOVERFIELD         *fp;
  const ARTHEADER      *hp;
  bool                 ok = true;

  if (ARTfields != NULL) {
    free(ARTfields);
    ARTfields = NULL;
  }

  /* Count the number of overview fields and allocate ARTfields. */
  standardoverview = overview_fields();
  extraoverview = overview_extra_fields(true);
  ARTfields = xmalloc((standardoverview->count + extraoverview->count + 1)
                      * sizeof(ARTOVERFIELD));

  /* Parse each field. */
  for (i = 0, fp = ARTfields; i < standardoverview->count; i++) {
    fp->NeedHeader = false;
    for (hp = ARTheaders; hp < ARRAY_END(ARTheaders); hp++) {
      if (strcasecmp(standardoverview->strings[i], hp->Name) == 0) {
	fp->Header = hp;
	break;
      }
    }
    if (hp == ARRAY_END(ARTheaders)) {
      syslog(L_ERROR, "%s bad_schema unknown header \"%s\"",
             LogName, standardoverview->strings[i]);
      ok = false;
      continue;
    }
    fp++;
  }
  for (i = 0; i < extraoverview->count; i++) {
    fp->NeedHeader = true;
    for (hp = ARTheaders; hp < ARRAY_END(ARTheaders); hp++) {
      if (strcasecmp(extraoverview->strings[i], hp->Name) == 0) {
        fp->Header = hp;
        break;
      }
    }
    if (hp == ARRAY_END(ARTheaders)) {
      syslog(L_ERROR, "%s bad_schema unknown header \"%s\"",
             LogName, extraoverview->strings[i]);
      ok = false;
      continue;
    }
    fp++;
  }

  fp->Header = NULL;

  return ok;
}


/*
**  Build a balanced tree for the headers in subscript range [lo..hi).
**  This only gets called once, and the tree only has about 37 entries,
**  so we don't bother to unroll the recursion.
*/
static TREE *
ARTbuildtree(const ARTHEADER **Table, int lo, int hi)
{
  int	mid;
  TREE	*tp;

  mid = lo + (hi - lo) / 2;
  tp = xmalloc(sizeof(TREE));
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
    const char *n1 = ((const ARTHEADER * const *) p1)[0]->Name;
    const char *n2 = ((const ARTHEADER * const *) p2)[0]->Name;

    return strcasecmp(n1, n2);
}


/*
**  Setup the article processing.
*/
void
ARTsetup(void)
{
  const ARTHEADER **	table;
  const unsigned char *p;
  unsigned int	i;

  /* Set up the character class tables.  These are written a
   * little strangely to work around a GCC2.0 bug. */
  memset(hostcclass, 0, sizeof(hostcclass));

  p = (const unsigned char*) "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  while ((i = *p++) != 0) {
    hostcclass[i] = 1;
  }

  /* The RFCs don't require it, but we add underscore to the list
   * of valid hostname characters. */
  hostcclass['.'] = 1;
  hostcclass['-'] = 1;
  hostcclass['_'] = 1;

  /* Also initialize the character class tables for message-IDs. */
  InitializeMessageIDcclass();

  /* Build the header tree. */
  table = xmalloc(ARRAY_SIZE(ARTheaders) * sizeof(ARTHEADER *));
  for (i = 0; i < ARRAY_SIZE(ARTheaders); i++)
    table[i] = &ARTheaders[i];
  qsort(table, ARRAY_SIZE(ARTheaders), sizeof *table, ARTcompare);
  ARTheadertree = ARTbuildtree(table, 0, ARRAY_SIZE(ARTheaders));
  free(table);

  /* Set up database; ignore errors. */
  ARTreadschema();
}


static void
ARTfreetree(TREE *tp)
{
  TREE	*next;

  for ( ; tp != NULL; tp = next) {
    if (tp->Before)
      ARTfreetree(tp->Before);
    next = tp->After;
    free(tp);
  }
}


void
ARTclose(void)
{
  if (ARTfields != NULL) {
    free(ARTfields);
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
  const HDRCONTENT *hc = data->HdrContent;
  int i;
  bool Done;

  TMRstart(TMR_ARTLOG);
  /* We could be a bit faster by not dividing Now.usec by 1000,
   * but who really wants to log at the Microsec level? */
  Done = code == ART_ACCEPT || code == ART_JUNK;
  if (text)
    i = fprintf(Log, "%.15s.%03d %c %s %s %s%s",
      ctime(&Now.tv_sec) + 4, (int)(Now.tv_usec / 1000), code,
      data->Feedsite != NULL ? data->Feedsite : "(null)",
      HDR_FOUND(HDR__MESSAGE_ID) ? HDR(HDR__MESSAGE_ID) : "(null)",
      text, Done ? "" : "\n");
  else
    i = fprintf(Log, "%.15s.%03d %c %s %s%s",
      ctime(&Now.tv_sec) + 4, (int)(Now.tv_usec / 1000), code,
      data->Feedsite != NULL ? data->Feedsite : "(null)",
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
      list->List = xrealloc(list->List, list->ListLength * sizeof(char *));
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
    list->List = xrealloc(list->List, list->ListLength * sizeof(char *));
    hp = &list->List[i];
  }
  *hp = NULL;
  return i;
}

/*
**  We're rejecting an article.  Log a message to the news log about that,
**  including all the interesting article information.
*/
void
ARTlogreject(CHANNEL *cp, const char *text)
{
    ARTDATA *data = &cp->Data;
    HDRCONTENT *hc = data->HdrContent;
    int hopcount;
    char **hops;

    /* We may still haven't received the message-ID of the rejected article. */
    if (HDR_FOUND(HDR__MESSAGE_ID)) {
        HDR_LASTCHAR_SAVE(HDR__MESSAGE_ID);
        HDR_PARSE_START(HDR__MESSAGE_ID);
    }

    /* Set up the headers that we want to use.  We only need to parse the path
       on rejections if logipaddr is false or we can't find a good host. */
    if (innconf->logipaddr) {
        if (cp->Address.ss_family != 0) {
            data->Feedsite = RChostname(cp);
        } else {
            data->Feedsite = "localhost";
        }
    } else {
        if (HDR_FOUND(HDR__PATH)) {
            HDR_LASTCHAR_SAVE(HDR__PATH);
            HDR_PARSE_START(HDR__PATH);
            hopcount =
                ARTparsepath(HDR(HDR__PATH), HDR_LEN(HDR__PATH), &data->Path);
            HDR_PARSE_END(HDR__PATH);
            hops = data->Path.List;
            if (hopcount > 0 && hops != NULL && hops[0] != NULL) {
                data->Feedsite = hops[0];
            } else {
                data->Feedsite = "localhost";
            }
        } else {
            if (cp->Address.ss_family != 0) {
                data->Feedsite = RChostname(cp);
            } else {
                data->Feedsite = "localhost";
            }
        }
    }

    data->FeedsiteLength = strlen(data->Feedsite);

    ARTlog(data, ART_REJECT, text != NULL ? text : cp->Error);

    if (HDR_FOUND(HDR__MESSAGE_ID))
        HDR_PARSE_END(HDR__MESSAGE_ID);
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
  struct buffer	*Article = &cp->In;
  ARTDATA	*data = &cp->Data;
  HDRCONTENT	*hc = data->HdrContent;
  const char	*p;
  ARTHANDLE	arth;
  int		i, j, iovcnt = 0;
  long		headersize = 0;
  TOKEN		result;
  struct buffer	*headers = &data->Headers;
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
  for (p = Article->data + cp->Start, j = 0 ; j < i ; j++) {
    switch (hp[j].index) {
      case HDR__PATH:
	if (!data->Hassamepath || data->AddAlias || Pathcluster.used) {
	  /* write heading data */
	  iov[iovcnt].iov_base = (char *) p;
	  iov[iovcnt++].iov_len = HDR(HDR__PATH) - p;
	  arth.len += HDR(HDR__PATH) - p;
          /* append clusterpath */
          if (Pathcluster.used) {
            iov[iovcnt].iov_base = Pathcluster.data;
            iov[iovcnt++].iov_len = Pathcluster.used;
            arth.len += Pathcluster.used;
          }
	  /* now append new one */
	  iov[iovcnt].iov_base = Path.data;
	  iov[iovcnt++].iov_len = Path.used;
	  arth.len += Path.used;
	  if (data->AddAlias) {
	    iov[iovcnt].iov_base = Pathalias.data;
	    iov[iovcnt++].iov_len = Pathalias.used;
	    arth.len += Pathalias.used;
	  }
	  /* next to write */
	  p = HDR(HDR__PATH);
          if (data->Hassamecluster)
            p += Pathcluster.used;
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
        memset(&result, 0, sizeof(result));
	result.type = TOKEN_EMPTY;
	return result;
    }
  }
  /* In case Xref: is not included in original article. */
  if (!HDR_FOUND(HDR__XREF)) {
    /* Write heading data. */
    iov[iovcnt].iov_base = (char *) p;
    iov[iovcnt++].iov_len = Article->data + (data->Body - 2) - p;
    arth.len += Article->data + (data->Body - 2) - p;
    /* Xref: needs to be inserted. */
    iov[iovcnt].iov_base = (char *) "Xref: ";
    iov[iovcnt++].iov_len = sizeof("Xref: ") - 1;
    arth.len += sizeof("Xref: ") - 1;
    iov[iovcnt].iov_base = data->Xref;
    iov[iovcnt++].iov_len = data->XrefLength;
    arth.len += data->XrefLength;
    p = Article->data + (data->Body - 2);
  }
  /* write rest of data */
  iov[iovcnt].iov_base = (char *) p;
  iov[iovcnt++].iov_len = Article->data + cp->Next - p;
  arth.len += Article->data + cp->Next - p;

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
	Article->data + data->Body - (char *) iov[i].iov_base;
    }
    data->BytesValue += iov[i].iov_len;
  }
  /* Subtract the trailing 3 octets ".\r\n" and the "." characters
   * used for dot-stuffing.  Note that we count a CRLF pair as
   * two octets.  We also count the empty line between headers and body.
   * It is how the count of Bytes: should be done according to
   * RFC 3977. */
  data->BytesValue -= 3 + data->DotStuffedLines;
  /* Figure out how much space we'll need and get it. */
  snprintf(data->Bytes, sizeof(data->Bytes), "Bytes: %ld\r\n",
           data->BytesValue);
  /* does not include strlen("Bytes: \r\n") */
  data->BytesLength = strlen(data->Bytes) - 9;

  if (!NeedHeaders)
    return result;

  /* Add the data. */
  buffer_resize(headers, headersize);
  buffer_set(headers, data->Bytes, strlen(data->Bytes));
  for (i = 0 ; i < iovcnt ; i++) {
    if (i + 1 == iovcnt)
      buffer_append(headers, iov[i].iov_base,
	Article->data + data->Body - (char *) iov[i].iov_base);
    else
      buffer_append(headers, iov[i].iov_base, iov[i].iov_len);
  }
  buffer_trimcr(headers);

  return result;
}

/*
**  Parse, check, and possibly store in the system header table a header that
**  starts at cp->CurHeader.  size includes the trailing "\r\n".
**  Even though an error has already occurred (cp->Error is set), we go on
**  parsing headers (so that we can find the message-ID, the path, etc.).
*/
static void
ARTcheckheader(CHANNEL *cp, int size)
{
  ARTDATA	*data = &cp->Data;
  char		*header = cp->In.data + data->CurHeader;
  HDRCONTENT	*hc = cp->Data.HdrContent;
  TREE		*tp;
  const ARTHEADER *hp;
  char		c, *p, *colon;
  int		i;
  bool          ihave;

  /* Check whether we are receiving the article via IHAVE or TAKETHIS. */
  ihave = (cp->Sendid.size > 3) ? false : true;

  /* Find first colon */
  if ((colon = memchr(header, ':', size)) == NULL || !ISWHITE(colon[1])) {
    if ((p = memchr(header, '\r', size)) != NULL)
      *p = '\0';
    snprintf(cp->Error, sizeof(cp->Error),
             "%d No colon-space in \"%s\" header",
             ihave ? NNTP_FAIL_IHAVE_REJECT : NNTP_FAIL_TAKETHIS_REJECT,
             MaxLength(header, header));
    if (p != NULL)
      *p = '\r';
    return;
  }

  /* See if this is a system header.  A fairly tightly-coded binary search. */
  c = islower((unsigned char) *header) ? toupper(*header) : *header;
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
	snprintf(cp->Error, sizeof(cp->Error),
                 "%d Space before colon in \"%s\" header",
                 ihave ? NNTP_FAIL_IHAVE_REJECT : NNTP_FAIL_TAKETHIS_REJECT,
                 MaxLength(header, header));
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
  if (hc->Length != 0) {
    /* Duplicated. */
    hc->Length = -1;
  } else {
    /* We need to remove leading and trailing spaces for
     * message-IDs; otherwise, history hashes may not be
     * correctly computed. */
    if (i == HDR__MESSAGE_ID || i == HDR__SUPERSEDES) {
      for (p = colon + 1 ; (p < header + size - 2) &&
           (ISWHITE(*p)) ; p++);
      hc->Value = p;
      for (p = header + size - 3 ; (p > hc->Value) &&
           (ISWHITE(*p)) ; p--);
      hc->Length = p - hc->Value + 1;
    } else {
      hc->Value = colon + 2;
      /* HDR_LEN() does not include trailing "\r\n". */
      hc->Length = size - (hc->Value - header) - 2;
    }
  }
  return;
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
    hc->LastChar = '\r';
  }
  data->Lines = data->HeaderLines = data->CRwithoutLF = data->LFwithoutCR = 0;
  data->DotStuffedLines = 0;
  data->CurHeader = data->LastCRLF = data->Body = cp->Start;
  data->BytesHeader = NULL;
  data->Feedsite = "?";
  data->FeedsiteLength = strlen(data->Feedsite);
  *cp->Error = '\0';
}

/*
**  Report a rejection of an article by putting the reason for rejection into
**  the Error field of the supplied channel.
*/
static void
ARTerror(CHANNEL *cp, const char *format, ...)
{
    va_list args;
    bool ihave;
    
    /* Check whether we are receiving the article via IHAVE or TAKETHIS. */
    ihave = (cp->Sendid.size > 3) ? false : true;

    snprintf(cp->Error, sizeof(cp->Error), "%d ",
             ihave ? NNTP_FAIL_IHAVE_REJECT : NNTP_FAIL_TAKETHIS_REJECT);
    va_start(args, format);
    vsnprintf(cp->Error + 4, sizeof(cp->Error) - 4, format, args);
    va_end(args);
}

/*
**  Check to see if an article exceeds the local size limit and set the
**  channel state appropriately.  If the article has been fully received, also
**  update the Error buffer in the channel if needed.
*/
static void
ARTchecksize(CHANNEL *cp)
{
    size_t size;
    HDRCONTENT *hc = cp->Data.HdrContent;
    const char *msgid;

    size = cp->Next - cp->Start;
    if (innconf->maxartsize != 0 && size > innconf->maxartsize) {
        if (cp->State == CSgotarticle || cp->State == CSgotlargearticle)
            cp->State = CSgotlargearticle;
        else
            cp->State = CSeatarticle;
    }
    if (cp->State == CSgotlargearticle) {
        notice("%s internal rejecting huge article (%lu > %lu)", CHANname(cp),
               (unsigned long) size, innconf->maxartsize);
        ARTerror(cp, "Article of %lu bytes exceeds local limit of %lu bytes",
                 (unsigned long) size, innconf->maxartsize);

	/* Write a local cancel entry so nobody else gives it to us. */
	if (HDR_FOUND(HDR__MESSAGE_ID)) {
            HDR_LASTCHAR_SAVE(HDR__MESSAGE_ID);
            HDR_PARSE_START(HDR__MESSAGE_ID);
            msgid = HDR(HDR__MESSAGE_ID);
            /* The article posting time has not been parsed.  We cannot
             * give it to InndHisRemember. */
            if (!HIScheck(History, msgid) && !InndHisRemember(msgid, 0))
                warn("SERVER cant write %s", msgid);
            HDR_PARSE_END(HDR__MESSAGE_ID);
	}
    }
}

/*
**  Parse a section of the header of an article.  This is called by ARTparse()
**  while the channel state is CSgetheader.  If we find the beginning of the
**  body, change the channel state and hand control off to ARTparsebody.
*/
static void
ARTparseheader(CHANNEL *cp)
{
    struct buffer *bp = &cp->In;
    ARTDATA *data = &cp->Data;
    size_t i;
    unsigned long length;

    for (i = cp->Next; i < bp->used; i++) {
        if (bp->data[i] == '\0')
            ARTerror(cp, "Nul character in header");
        if (bp->data[i] == '\n')
            data->LFwithoutCR++;
        if (bp->data[i] != '\r')
            continue;

        /* We saw a \r, which is the beginning of everything interesting.  The
           longest possibly interesting thing we could see is an article
           terminator (five characters).  If we don't have at least five more
           characters, we're guaranteed that the article isn't complete, so
           save ourselves complexity and just return and wait for more
           data.
           We also have to check whether the headers begin with the article
           terminator. */
        if ((i == cp->Start + 1) && (bp->used - i > 1)
            && (memcmp(&bp->data[i - 1], ".\r\n", 3) == 0)) {
            ARTerror(cp, "Empty article");
            cp->State = CSnoarticle;
            cp->Next = i + 2;
            return;
        }

        if (bp->used - i < 5) {
            cp->Next = i;
            return;
        }
        if (memcmp(&bp->data[i], "\r\n.\r\n", 5) == 0) {
            if (i == cp->Start) {
                ARTerror(cp, "Empty headers and body");
                cp->State = CSnoarticle;
            } else {
                ARTerror(cp, "No body");
                cp->State = CSgotarticle;
            }
            cp->Next = i + 5;
            return;
        } else if (bp->data[i + 1] == '\n') {
            length = i - data->LastCRLF - 1;
            if (data->LastCRLF == cp->Start)
                length++;
            /* length includes final CRLF. */
            if (length > MAXARTLINELENGTH)
                ARTerror(cp, "Header line too long (%lu bytes)", length);

            /* Be a little tricky here.  Normally, the headers end at the
               first occurrence of \r\n\r\n, so since we've seen \r\n, we want
               to advance i and then look to see if we have another one.  The
               exception is the degenerate case of an article with no headers.
               In that case, log an error and *don't* advance i so that we'll
               still see the end of headers. */
            if (i == cp->Start) {
                ARTerror(cp, "No headers");
            } else {
                i += 2;
                data->HeaderLines++;
                if (bp->data[i] != ' ' && bp->data[i] != '\t') {
                    ARTcheckheader(cp, i - data->CurHeader);
                    data->CurHeader = i;
                }
            }
            if (bp->data[i] == '\r' && bp->data[i + 1] == '\n') {
                cp->Next = i + 2;
                data->Body = i + 2;
                cp->State = CSgetbody;
                ARTparsebody(cp);
		return;
            }
            data->LastCRLF = i - 1;
        } else {
            data->CRwithoutLF++;
        }
    }
    cp->Next = i;
}

/*
**  Parse a section of the body of an article.  This is called by ARTparse()
**  while the channel state is CSgetbody or CSeatarticle.
*/
static void
ARTparsebody(CHANNEL *cp)
{
    struct buffer *bp = &cp->In;
    ARTDATA *data = &cp->Data;
    size_t i;

    for (i = cp->Next; i < bp->used; i++) {
        if (bp->data[i] == '\0')
            ARTerror(cp, "Nul character in body");
        if (bp->data[i] == '\n')
            data->LFwithoutCR++;
        if (bp->data[i] != '\r')
            continue;

        /* Saw \r.  We're just scanning for the article terminator, so if we
           don't have at least five characters left, we can save effort and
           stop now.
           We also have to check whether the body begins with the article
           terminator. */
        if ((i == data->Body + 1) && (bp->used - i > 1)
            && (memcmp(&bp->data[i - 1], ".\r\n", 3) == 0)) {
            if (cp->State == CSeatarticle)
                cp->State = CSgotlargearticle;
            else
                cp->State = CSgotarticle;
            cp->Next = i + 2;
            return;
        }

        if (bp->used - i < 5) {
            cp->Next = i;
            return;
        }

        if (memcmp(&bp->data[i], "\r\n.\r\n", 5) == 0) {
            if (cp->State == CSeatarticle)
                cp->State = CSgotlargearticle;
            else
                cp->State = CSgotarticle;
            cp->Next = i + 5;
            data->Lines++;
            return;
        } else if (bp->data[i + 1] == '\n') {
            i++;
            data->Lines++;
            /* Check whether the new line begins with a dot. */
            if (bp->data[i + 1] == '.')
                data->DotStuffedLines++;
        } else {
            data->CRwithoutLF++;
        }
    }
    cp->Next = i;
}

/*
**  The external interface to article parsing, called by NCproc.  This
**  function may be called repeatedly as each new block of data arrives.
*/
void
ARTparse(CHANNEL *cp)
{
    if (cp->State == CSgetheader)
        ARTparseheader(cp);
    else
        ARTparsebody(cp);
    ARTchecksize(cp);
    if (cp->State == CSgotarticle || cp->State == CSgotlargearticle)
        if (cp->Error[0] != '\0')
            ARTlogreject(cp, NULL);
}

/*
**  Clean up an article.  This is mainly copying in-place, stripping bad
**  headers.  Also fill in the article data block with what we can find.
**  Return true if the article has no error, or false which means the error.
*/
static bool
ARTclean(ARTDATA *data, char *buff, bool ihave)
{
  HDRCONTENT	*hc = data->HdrContent;
  const ARTHEADER *hp = ARTheaders;
  int		i;
  char		*p;
  int		delta;

  TMRstart(TMR_ARTCLEAN);
  data->Arrived = Now.tv_sec;
  data->Expires = 0;
  data->Posted = 0;

  /* replace trailing '\r\n' with '\0\n' of all system header to be handled
     easily by str*() functions */
  for (i = 0 ; i < MAX_ARTHEADER ; i++) {
    if (HDR_FOUND(i)) {
      HDR_LASTCHAR_SAVE(i);
      HDR_PARSE_START(i);
    }
  }

  /* Make sure all the headers we need are there */
  for (i = 0; i < MAX_ARTHEADER ; i++) {
    if (hp[i].Type == HTreq) {
      if (HDR_FOUND(i))
        continue;
      if (HDR_LEN(i) < 0) {
        sprintf(buff, "%d Duplicate \"%s\" header",
                ihave ? NNTP_FAIL_IHAVE_REJECT : NNTP_FAIL_TAKETHIS_REJECT,
                hp[i].Name);
      } else {
	sprintf(buff, "%d Missing \"%s\" header",
                ihave ? NNTP_FAIL_IHAVE_REJECT : NNTP_FAIL_TAKETHIS_REJECT,
                hp[i].Name);
      }
      TMRstop(TMR_ARTCLEAN);
      return false;
    }
  }

  /* Assumes the Message-ID: header is a required header. */
  if (!IsValidMessageID(HDR(HDR__MESSAGE_ID), true)) {
    HDR_LEN(HDR__MESSAGE_ID) = 0;
    sprintf(buff, "%d Bad \"Message-ID\" header",
            ihave ? NNTP_FAIL_IHAVE_REJECT : NNTP_FAIL_TAKETHIS_REJECT);
    TMRstop(TMR_ARTCLEAN);
    return false;
  }

  if (innconf->linecountfuzz != 0 && HDR_FOUND(HDR__LINES)) {
    p = HDR(HDR__LINES);
    i = data->Lines;
    if ((delta = i - atoi(p)) != 0
        && (unsigned long) abs(delta) > innconf->linecountfuzz) {
      sprintf(buff, "%d Linecount %s != %d +- %lu",
              ihave ? NNTP_FAIL_IHAVE_REJECT : NNTP_FAIL_TAKETHIS_REJECT,
	MaxLength(p, p), i, innconf->linecountfuzz);
      TMRstop(TMR_ARTCLEAN);
      return false;
    }
  }

  /* Is article too old? */
  /* assumes Date header is required header */
  p = HDR(HDR__DATE);
  data->Posted = parsedate_rfc5322_lax(p);
  if (data->Posted == (time_t) -1) {
    sprintf(buff, "%d Bad \"Date\" header -- \"%s\"",
            ihave ? NNTP_FAIL_IHAVE_REJECT : NNTP_FAIL_TAKETHIS_REJECT,
      MaxLength(p, p));
    TMRstop(TMR_ARTCLEAN);
    return false;
  }
  if (innconf->artcutoff != 0) {
      long cutoff = innconf->artcutoff * 24 * 60 * 60;

      if (data->Posted < Now.tv_sec - cutoff) {
          sprintf(buff, "%d Too old -- \"%s\"",
                  ihave ? NNTP_FAIL_IHAVE_REJECT : NNTP_FAIL_TAKETHIS_REJECT,
                  MaxLength(p, p));
          TMRstop(TMR_ARTCLEAN);
          return false;
      }
  }
  if (data->Posted > Now.tv_sec + DATE_FUZZ) {
    sprintf(buff, "%d Article posted in the future -- \"%s\"",
            ihave ? NNTP_FAIL_IHAVE_REJECT : NNTP_FAIL_TAKETHIS_REJECT,
            MaxLength(p, p));
    TMRstop(TMR_ARTCLEAN);
    return false;
  }
  if (HDR_FOUND(HDR__EXPIRES)) {
    p = HDR(HDR__EXPIRES);
    data->Expires = parsedate_rfc5322_lax(p);
    if (data->Expires == (time_t) -1)
      data->Expires = 0;
  }

  /* Colon or whitespace in the Newsgroups: header? */
  /* Assumes Newsgroups: header is required header. */
  if ((data->Groupcount =
    NGsplit(HDR(HDR__NEWSGROUPS), HDR_LEN(HDR__NEWSGROUPS),
    &data->Newsgroups)) == 0) {
    TMRstop(TMR_ARTCLEAN);
    sprintf(buff, "%d Unwanted character in \"Newsgroups\" header",
            ihave ? NNTP_FAIL_IHAVE_REJECT : NNTP_FAIL_TAKETHIS_REJECT);
    return false;
  }

  TMRstop(TMR_ARTCLEAN);
  return true;
}

/*
**  We are going to reject an article, record the reason and
**  and the article.
*/
void
ARTreject(Reject_type code, CHANNEL *cp)
{
  /* Remember why the article was rejected (for the status file) */

  cp->Rejected++;
  cp->RejectSize += cp->Next - cp->Start;

  /* Streaming is used.  Increase the reject counter for TAKETHIS. */
  if (cp->Sendid.size > 3) {
    cp->Takethis_Err++;
  }

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
**  Verify if a cancel message is valid.  Unless at least one group in the 
**  cancel message's Newsgroups: line can be found in the Newsgroups: line 
**  of the article to be cancelled, the cancel is considered bogus and 
**  false is returned.
*/
static bool
ARTcancelverify(const ARTDATA *data, const char *MessageID, TOKEN *token)
{
  const char	*p;
  char		*q, *q1;
  char          **gp;
  const char	*local;
  char		buff[SMBUF];
  ARTHANDLE	*art;
  bool		r;

  if (!HISlookup(History, MessageID, NULL, NULL, NULL, token))
    return false;
  if ((art = SMretrieve(*token, RETR_HEAD)) == NULL)
    return false;

  /* Copy Newsgroups: from article be to cancelled to q.
   * Double-terminate q (sentinel). */
  local = wire_findheader(art->data, art->len, "Newsgroups", true);
  if (local == NULL) {
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
  q = xmalloc(p - local + 2);
  memcpy(q, local, p - local);
  SMfreearticle(art);
  q[p - local] = '\0';
  q[p - local + 1] = '\0';

  /* Replace separator ',' by '\0'. */
  for (q1 = q; *q1; q1++) {
    if (NG_ISSEP(*q1)) {
      *q1 = '\0';
    }
  }

  r = false;
  for (gp = data->Newsgroups.List; *gp && !r; gp++) {
    for (q1 = q; *q1; q1 += strlen(q1) + 1) {
      if (strcmp(q1, *gp) == 0) {
        r = true;
        break;
      }
    }
  }

  free(q);

  if (!r) {
    sprintf(buff, "No matching newsgroups in cancel %s",
            MaxLength(MessageID, MessageID));
    ARTlog(data, ART_REJECT, buff);
  }
      
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

  if (!IsValidMessageID(MessageID, true)) {
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
    InndHisRemember(MessageID, data->Posted);
    snprintf(buff, sizeof(buff), "Cancelling %s",
             MaxLength(MessageID, MessageID));
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
  if (innconf->enableoverview)
    OVcancel(token);
  if (!SMcancel(token) && SMerrno != SMERR_NOENT && SMerrno != SMERR_UNINIT)
    syslog(L_ERROR, "%s cant cancel %s (SMerrno %d)", LogName,
           TokenToText(token), SMerrno);
  if (innconf->immediatecancel && !SMflushcacheddata(SM_CANCELLEDART))
    syslog(L_ERROR, "%s cant cancel cached %s", LogName, TokenToText(token));
  snprintf(buff, sizeof(buff), "Cancelling %s",
           MaxLength(MessageID, MessageID));
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
  if ((c == 'c' || c == 'C') && strncasecmp(Control, "cancel", 6) == 0) {
    for (p = &Control[6]; ISWHITE(*p); p++)
      continue;
    if (*p && IsValidMessageID(p, true))
      ARTcancel(data, p, false);
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
      list->List = xrealloc(list->List, list->ListLength * sizeof(char *));
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
    list->List = xrealloc(list->List, list->ListLength * sizeof(char *));
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

  for (sawbang = false, c = *p; (q = *list) != NULL; list++) {
    if (*q == '!') {
      sawbang = true;
      if (c == *++q && strcasecmp(p, q) == 0)
	return false;
    } else if (c == *q && strcasecmp(p, q) == 0)
      return true;
  }

  /* If we saw any !foo's and didn't match, then assume they are all negated
     distributions and return true, else return false. */
  return sawbang;
}

/*
**  See if any of the distributions in the article are wanted by the site.
*/
static bool
DISTwantany(char **site, char **article)
{
  /* An empty distributions list is allowed. */
  if (*article == NULL) {
    return true;
  }

  for ( ; *article; article++)
    if (DISTwanted(site, *article))
      return true;
  return false;
}

/*
**  Send the current article to all sites that would get it (used
**  for instance to propagate newgroup control articles or junk articles).
*/
static void
ARTsendthegroup(char *name, char *fromgroup)
{
  SITE		*sp;
  int		i;
  NEWSGROUP	*ngp;

  for (ngp = NGfind(fromgroup), sp = Sites, i = nSites; --i >= 0; sp++) {
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
      sp->Poison = true;
  }
}

/*
** Assign article numbers to the article and create the Xref: header field.
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
    data->XrefBufLength = MED_BUFFER * 2 + 1;
    data->Xref = xmalloc(data->XrefBufLength);
    strncpy(data->Xref, Path.data, Path.used - 1);
  }
  len = Path.used - 1;
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
    /*  len  ' ' "news_groupname"  ':' "#" "\r\n"
        plus an extra 2 bytes for "\r\n" in case of a continuation line. */
    if (len + 1 + ngp->NameLength + 1 + 10 + 2 + 2 > data->XrefBufLength) {
      data->XrefBufLength += MED_BUFFER;
      data->Xref = xrealloc(data->Xref, data->XrefBufLength);
      p = data->Xref + len;
    }
    /* Trailing CRLF is counted in the maximum length. */
    if (linelen + 1 + ngp->NameLength + 1 + 10 + 2 > MAXARTLINELENGTH) {
      /* Line exceeded. */
      sprintf(p, "\r\n %s:%lu", ngp->Name, ngp->Filenum);
      buflen = strlen(p);
      linelen = buflen - 2;
    } else {
      sprintf(p, " %s:%lu", ngp->Name, ngp->Filenum);
      buflen = strlen(p);
      linelen += buflen;
    }
    len += buflen;
    p += buflen;
  }
  /* p[0] is replaced with '\r' to be wireformatted when stored.  p[1] needs to
     be '\n'.  We have enough place to modify p here (checked during the
     reallocation above). */
  p[0] = '\r';
  p[1] = '\n';
  /* data->XrefLength includes trailing "\r\n". */
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
  char		*p, *q, *name, *next, c = 0;
  NEWSGROUP	*ngp;
  int	        i;
  bool		nogroup = true;
  HDRCONTENT	*hc = data->HdrContent;

  if (!HDR_FOUND(HDR__XREF))
    return false;
  /* skip server name */
  if ((p = strpbrk(HDR(HDR__XREF), " \t\r\n")) == NULL)
    return false;
  /* in case Xref is folded */
  while (*++p == ' ' || *p == '\t' || *p == '\r' || *p == '\n');
  if (*p == '\0')
    return false;
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
      next = (char *) "";
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

    /* Update active file if we got a new high water mark. */
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
    nogroup = false;
  }
  GroupPointers[i] = NULL;
  if (nogroup)
    return false;
  return true;
}

/*
**  Return true if a list of strings has a specific one.  This is a
**  generic routine, but is used for seeing if a host is in the Path line.
*/
static bool
ListHas(const char **list, const char *p)
{
  const char	*q;
  char		c;

  for (c = *p; (q = *list) != NULL; list++)
    if (strcasecmp(p, q) == 0)
      return true;
  return false;
}

/*
**  Even though we have already calculated the Message-ID MD5sum,
**  we have to do it again since unfortunately HashMessageID()
**  lowercases the Message-ID first.  We also need to remain
**  compatible with Diablo's hashfeed.
*/
static unsigned int
HashFeedMD5(char *MessageID, unsigned int offset)
{
  static char LastMessageID[128];
  static char *LastMessageIDPtr;
  static struct md5_context context;
  unsigned int ret;

  if (offset > 12)
    return 0;

  /* Some light caching. */
  if (MessageID != LastMessageIDPtr ||
      strcmp(MessageID, LastMessageID) != 0) {
    md5_init(&context);
    md5_update(&context, (unsigned char *)MessageID, strlen(MessageID));
    md5_final(&context);
    LastMessageIDPtr = MessageID;
    strncpy(LastMessageID, MessageID, sizeof(LastMessageID) - 1);
    LastMessageID[sizeof(LastMessageID) - 1] = 0;
  }

  memcpy(&ret, &context.digest[12 - offset], 4);

  return ntohl(ret);
}

/*
** Old-style Diablo (< 5.1) quickhash.
*/
static unsigned int
HashFeedQH(char *MessageID, unsigned int *tmp)
{
  unsigned char *p;
  int n;

  if (*tmp != (unsigned int)-1)
    return *tmp;

  p = (unsigned char *)MessageID;
  n = 0;
  while (*p)
    n += *p++;
  *tmp = (unsigned int)n;

  return *tmp;
}

/*
**  Return true if an element of the HASHFEEDLIST matches
**  the hash of the Message-ID.
*/
static bool
HashFeedMatch(HASHFEEDLIST *hf, char *MessageID)
{
  unsigned int qh = (unsigned int)-1;
  unsigned int h;

  while (hf) {
    if (hf->type == HASHFEED_MD5)
      h = HashFeedMD5(MessageID, hf->offset);
    else if (hf->type == HASHFEED_QH)
      h = HashFeedQH(MessageID, &qh);
    else
      continue;

    if ((h % hf->mod + 1) >= hf->begin &&
        (h % hf->mod + 1) <= hf->end)
      return true;
    hf = hf->next;
  }

  return false;
}

/*
**  Propagate an article to the sites have "expressed an interest."
*/
static void
ARTpropagate(ARTDATA *data, const char **hops, int hopcount, char **list,
  bool ControlStore, bool OverviewCreated, bool Filtered)
{
  HDRCONTENT	*hc = data->HdrContent;
  SITE		*sp, *funnel;
  int		i, j, Groupcount, Followcount, Crosscount;
  char	        *p, *q, *begin, savec;
  struct buffer	*bp;
  bool		sendit;

  /* Work out which sites should really get it. */
  Groupcount = data->Groupcount;
  Followcount = data->Followcount;
  Crosscount = Groupcount + Followcount * Followcount;
  for (sp = Sites, i = nSites; --i >= 0; sp++) {
    if ((sp->IgnoreControl && ControlStore) ||
      (sp->NeedOverviewCreation && !OverviewCreated))
      sp->Sendit = false;
    if (sp->Seenit || !sp->Sendit)
      continue;
    sp->Sendit = false;
	
    if (sp->Originator) {
      if (!HDR_FOUND(HDR__XTRACE) && !HDR_FOUND(HDR__INJECTION_INFO)) {
	if (!sp->FeedwithoutOriginator)
	  continue;
      } else if (HDR_FOUND(HDR__INJECTION_INFO)) {
        begin = (char *) skip_cfws(HDR(HDR__INJECTION_INFO));

        if (begin == '\0')
          continue;

        /* The path identity ends with ';' or CFWS. */
        for (p = begin; *p != ';' && *p != ' ' && *p != '\t' && *p != '\r'
                        && *p != '\0'; p++) ;
        savec = *p;
        *p = '\0';

        for (j = 0, sendit = false; (q = sp->Originator[j]) != NULL; j++) {
          if (*q == '@') {
            if (uwildmat(begin, &q[1])) {
              *p = savec;
              sendit = false;
              break;
            }
          } else {
            if (uwildmat(begin, q))
              sendit = true;
          }
        }
        *p = savec;
        if (!sendit)
          continue;
      } else if (HDR_FOUND(HDR__XTRACE)) {
	if ((p = strchr(HDR(HDR__XTRACE), ' ')) != NULL) {
	  *p = '\0';
	  for (j = 0, sendit = false; (q = sp->Originator[j]) != NULL; j++) {
	    if (*q == '@') {
	      if (uwildmat(HDR(HDR__XTRACE), &q[1])) {
		*p = ' ';
		sendit = false;
		break;
	      }
	    } else {
	      if (uwildmat(HDR(HDR__XTRACE), q))
		sendit = true;
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

    if (sp->HashFeedList
      && !HashFeedMatch(sp->HashFeedList, HDR(HDR__MESSAGE_ID)))
      /* Hashfeed doesn't match. */
      continue;

    if (list && *list != NULL && sp->Distributions &&
      !DISTwantany(sp->Distributions, list))
      /* Not in the site's desired list of distributions. */
      continue;
    if (sp->DistRequired && (list == NULL || *list == NULL))
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

    /* Handle dontrejectfiltered. */
    if (Filtered && sp->DropFiltered)
      continue;

    /* Write that the site is getting it, and flag to send it. */
    if (innconf->logsitename) {
      if (fprintf(Log, " %s", sp->Name) == EOF || ferror(Log)) {
	j = errno;
	syslog(L_ERROR, "%s cant write log_site %m", LogName);
	IOError("logging site", j);
	clearerr(Log);
      }
    }
    sp->Sendit = true;
    sp->Seenit = true;
    if (sp->Master != NOSITE)
      Sites[sp->Master].Seenit = true;
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
      sp->Sendit = false;
      funnel = &Sites[sp->Funnel];
      funnel->Sendit = true;
      if (funnel->FNLwantsnames) {
	bp = &funnel->FNLnames;
	p = &bp->data[bp->used];
	if (bp->used) {
	  *p++ = ' ';
	  bp->used++;
	}
	bp->used += strlcpy(p, sp->Name, bp->size - bp->used);
      }
    }
  }
}

/*
**  Build up the overview data.
*/
static void
ARTmakeoverview(CHANNEL *cp)
{
  ARTDATA	*data = &cp->Data;
  HDRCONTENT	*hc = data->HdrContent;
  static char	SEP[] = "\t";
  static char	COLONSPACE[] = ": ";
  struct buffer	*overview = &data->Overview;
  ARTOVERFIELD	*fp;
  const ARTHEADER *hp;
  char		*p, *q;
  int		i, j, len;
  bool          keywords_generated = false;

  if (ARTfields == NULL) {
    /* User error. */
    return;
  }

  /* Setup. */
  buffer_resize(overview, MED_BUFFER);
  buffer_set(overview, "", 0);

  /* Write the data, a field at a time. */
  for (fp = ARTfields; fp->Header; fp++) {
    if (fp != ARTfields)
      buffer_append(overview, SEP, strlen(SEP));
    hp = fp->Header;
    j = hp - ARTheaders;

    /* If requested, generate keywords from the body of the article and patch
       them into the apparent value of the Keywords: header so that they make
       it into overview. */
    if (DO_KEYWORDS && innconf->keywords) {
      /* Ensure that there are Keywords: to shovel. */
      if (hp == &ARTheaders[HDR__KEYWORDS] && HDR(HDR__KEYWORDS) == NULL) {
        keywords_generated = true;
        KEYgenerate(&hc[HDR__KEYWORDS], cp->In.data + data->Body,
                    cp->Next - data->Body);
        /* Do not memorize an empty Keywords: header. */
        if (HDR_LEN(HDR__KEYWORDS) == 0) {
          if (HDR(HDR__KEYWORDS) != NULL)
              free(HDR(HDR__KEYWORDS)); /* malloc'd within. */
          HDR(HDR__KEYWORDS) = NULL;
          keywords_generated = false;
        }
      }
    }

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
      case HDR__LINES:
        snprintf(data->LinesBuffer, sizeof(data->LinesBuffer),
                 "%d", data->Lines);
        p = data->LinesBuffer;
        len = strlen(data->LinesBuffer);
        break;
      default:
	p = HDR(j);
	len = HDR_LEN(j);
	break;
    }
    if (len == 0)
      continue;
    if (fp->NeedHeader) {
      buffer_append(overview, hp->Name, hp->Size);
      buffer_append(overview, COLONSPACE, strlen(COLONSPACE));
    }
    if (overview->used + overview->left + len > overview->size)
        buffer_resize(overview, overview->size + len);
    for (i = 0, q = overview->data + overview->left; i < len; p++, i++) {
        if (*p == '\r' && i < len - 1 && p[1] == '\n') {
            p++;
            i++;
            continue;
        }
        if (*p == '\0' || *p == '\t' || *p == '\n' || *p == '\r')
            *q++ = ' ';
        else
            *q++ = *p;
        overview->left++;
    }

    /* Patch the old keywords back in. */
    if (DO_KEYWORDS && innconf->keywords) {
      if (keywords_generated) {
        if (HDR(HDR__KEYWORDS) != NULL)
          free(HDR(HDR__KEYWORDS)); /* malloc'd within. */
        HDR(HDR__KEYWORDS) = NULL;
        HDR_LEN(HDR__KEYWORDS) = 0;
        keywords_generated = false;
      }
    }
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
  char		*p, **groups, ControlWord[SMBUF], **hops, *controlgroup;
  int		i, j, *isp, hopcount, oerrno, canpost;
  size_t        n;
  NEWSGROUP	*ngp, **ngptr;
  NEWSGROUP     *ngpjunk;
  SITE		*sp;
  ARTDATA	*data = &cp->Data;
  HDRCONTENT	*hc = data->HdrContent;
  bool		Approved, Accepted, LikeNewgroup, ToGroup, GroupMissing;
  bool		NoHistoryUpdate, artclean;
  bool		ControlStore = false;
  bool		NonExist = false;
  bool		OverviewCreated = false;
  bool		IsControl = false;
  bool		Filtered = false;
  bool          ihave;
  struct buffer	*article;
  HASH		hash;
  TOKEN		token;
  char		*groupbuff[2];
#if defined(DO_PERL) || defined(DO_PYTHON)
  char		*filterrc;
#endif /* defined(DO_PERL) || defined(DO_PYTHON) */
  OVADDRESULT	result;

  /* Check whether we are receiving the article via IHAVE or TAKETHIS. */
  ihave = (cp->Sendid.size > 3) ? false : true;

  /* Preliminary clean-ups. */
  article = &cp->In;
  artclean = ARTclean(data, cp->Error, ihave);

  /* We have not parsed the Path: header yet.  We do not check for logipaddr
   * right now (it will be done afterwards and change data->Feedsite
   * in consequence).  We assign a feed site for the next call to ARTlog(). */
  if (cp->Address.ss_family != 0) {
    data->Feedsite = RChostname(cp);
  } else {
    data->Feedsite = "localhost";
  }
  if (data->Feedsite == NULL)
    data->Feedsite = CHANname(cp);
  data->FeedsiteLength = strlen(data->Feedsite);

  /* If we don't have Path: or Message-ID:, we can't continue. */
  if (!artclean && (!HDR_FOUND(HDR__PATH) || !HDR_FOUND(HDR__MESSAGE_ID))) {
    /* cp->Error is set since Path: and Message-ID: are required headers and one
     * of them is not found during ARTclean().
     * We do not remember the message-ID of this article because another
     * peer may send it with a good Path: header. */
    ARTlog(data, ART_REJECT, cp->Error);
    ARTreject(REJECT_OTHER, cp);
    return false;
  }
  hopcount = ARTparsepath(HDR(HDR__PATH), HDR_LEN(HDR__PATH), &data->Path);
  if (hopcount == 0) {
    snprintf(cp->Error, sizeof(cp->Error), "%d illegal path element",
             ihave ? NNTP_FAIL_IHAVE_REJECT : NNTP_FAIL_TAKETHIS_REJECT);
    /* We do not remember the message-ID of this article because another
     * peer may send it with a good Path: header. */
    ARTlog(data, ART_REJECT, cp->Error);
    ARTreject(REJECT_OTHER, cp);
    return false;
  }
  hops = data->Path.List;

  if (innconf->logipaddr) {
    if (strcmp("0.0.0.0", data->Feedsite) == 0 || data->Feedsite[0] == '\0')
      data->Feedsite = hops && hops[0] ? hops[0] : CHANname(cp);
  } else {
    data->Feedsite = hops && hops[0] ? hops[0] : CHANname(cp);
  }
  data->FeedsiteLength = strlen(data->Feedsite);

  hash = HashMessageID(HDR(HDR__MESSAGE_ID));
  data->Hash = &hash;
  if (HIScheck(History, HDR(HDR__MESSAGE_ID))) {
    snprintf(cp->Error, sizeof(cp->Error), "%d Duplicate",
             ihave ? NNTP_FAIL_IHAVE_REJECT : NNTP_FAIL_TAKETHIS_REJECT);
    ARTlog(data, ART_REJECT, cp->Error);
    ARTreject(REJECT_DUPLICATE, cp);
    return false;
  }
  if (!artclean) {
    ARTlog(data, ART_REJECT, cp->Error);
    /* If the article posting time has not been properly parsed, data->Posted
     * will be negative or zero. */
    if (innconf->remembertrash && (Mode == OMrunning) &&
	!InndHisRemember(HDR(HDR__MESSAGE_ID), data->Posted))
      syslog(L_ERROR, "%s cant write history %s %m", LogName,
	HDR(HDR__MESSAGE_ID));
    ARTreject(REJECT_OTHER, cp);
    return false;
  }

  n = strlen(hops[0]);
  if (n == Path.used - 1 &&
    strncasecmp(Path.data, hops[0], Path.used - 1) == 0)
    data->Hassamepath = true;
  else
    data->Hassamepath = false;
  if (Pathcluster.data != NULL &&
    n == Pathcluster.used - 1 &&
    strncasecmp(Pathcluster.data, hops[0], Pathcluster.used - 1) == 0)
    data->Hassamecluster = true;
  else
    data->Hassamecluster = false;
  if (Pathalias.data != NULL &&
    !ListHas((const char **)hops, (const char *)innconf->pathalias))
    data->AddAlias = true;
  else
    data->AddAlias = false;

  /* And now check the path for unwanted sites -- Andy */
  for(j = 0 ; ME.Exclusions && ME.Exclusions[j] ; j++) {
    if (ListHas((const char **)hops, (const char *)ME.Exclusions[j])) {
      snprintf(cp->Error, sizeof(cp->Error), "%d Unwanted site %s in path",
	       ihave ? NNTP_FAIL_IHAVE_REJECT : NNTP_FAIL_TAKETHIS_REJECT,
               MaxLength(ME.Exclusions[j], ME.Exclusions[j]));
      ARTlog(data, ART_REJECT, cp->Error);
      if (innconf->remembertrash && (Mode == OMrunning) &&
	  !InndHisRemember(HDR(HDR__MESSAGE_ID), data->Posted))
	syslog(L_ERROR, "%s cant write history %s %m", LogName,
	  HDR(HDR__MESSAGE_ID));
      ARTreject(REJECT_SITE, cp);
      return false;
    }
  }

#if defined(DO_PERL) || defined(DO_PYTHON)
  filterPath = HDR(HDR__PATH);
#endif /* DO_PERL || DO_PYHTON */

#if defined(DO_PYTHON)
  TMRstart(TMR_PYTHON);
  filterrc = PYartfilter(data, article->data + data->Body,
    cp->Next - data->Body, data->Lines);
  TMRstop(TMR_PYTHON);
  if (filterrc != NULL) {
    if (innconf->dontrejectfiltered) {
      Filtered = true;
      syslog(L_NOTICE, "rejecting[python] %s %d %.200s (with dontrejectfiltered)",
             HDR(HDR__MESSAGE_ID),
             ihave ? NNTP_OK_IHAVE : NNTP_OK_TAKETHIS,
             filterrc);
    } else {
      snprintf(cp->Error, sizeof(cp->Error), "%d %.200s",
               ihave ? NNTP_FAIL_IHAVE_REJECT : NNTP_FAIL_TAKETHIS_REJECT,
               filterrc);
      syslog(L_NOTICE, "rejecting[python] %s %s", HDR(HDR__MESSAGE_ID),
             cp->Error);
      ARTlog(data, ART_REJECT, cp->Error);
      if (innconf->remembertrash && (Mode == OMrunning) &&
	  !InndHisRemember(HDR(HDR__MESSAGE_ID), data->Posted))
	syslog(L_ERROR, "%s cant write history %s %m", LogName,
	  HDR(HDR__MESSAGE_ID));
      ARTreject(REJECT_FILTER, cp);
      return false;
    }
  }
#endif /* DO_PYTHON */

  /* I suppose some masochist will run with Python and Perl in together */

#if defined(DO_PERL)
  TMRstart(TMR_PERL);
  filterrc = PLartfilter(data, article->data + data->Body,
    cp->Next - data->Body, data->Lines);
  TMRstop(TMR_PERL);
  if (filterrc) {
    if (innconf->dontrejectfiltered) {
      Filtered = true;
      syslog(L_NOTICE, "rejecting[perl] %s %d %.200s (with dontrejectfiltered)",
             HDR(HDR__MESSAGE_ID),
             ihave ? NNTP_OK_IHAVE : NNTP_OK_TAKETHIS,
             filterrc);
    } else {
      snprintf(cp->Error, sizeof(cp->Error), "%d %.200s",
               ihave ? NNTP_FAIL_IHAVE_REJECT : NNTP_FAIL_TAKETHIS_REJECT,
               filterrc);
      syslog(L_NOTICE, "rejecting[perl] %s %s", HDR(HDR__MESSAGE_ID),
             cp->Error);
      ARTlog(data, ART_REJECT, cp->Error);
      if (innconf->remembertrash && (Mode == OMrunning) &&
	  !InndHisRemember(HDR(HDR__MESSAGE_ID), data->Posted))
	syslog(L_ERROR, "%s cant write history %s %m", LogName,
	  HDR(HDR__MESSAGE_ID));
      ARTreject(REJECT_FILTER, cp);
      return false;
    }
  }
#endif /* DO_PERL */

  /* If we limit what distributions we get, see if we want this one. */
  if (HDR_FOUND(HDR__DISTRIBUTION)) {
    if (HDR(HDR__DISTRIBUTION)[0] == ',') {
      snprintf(cp->Error, sizeof(cp->Error), "%d bogus distribution \"%s\"",
               ihave ? NNTP_FAIL_IHAVE_REJECT : NNTP_FAIL_TAKETHIS_REJECT,
               MaxLength(HDR(HDR__DISTRIBUTION), HDR(HDR__DISTRIBUTION)));
      ARTlog(data, ART_REJECT, cp->Error);
      if (innconf->remembertrash && Mode == OMrunning &&
	  !InndHisRemember(HDR(HDR__MESSAGE_ID), data->Posted))
        syslog(L_ERROR, "%s cant write history %s %m", LogName,
	  HDR(HDR__MESSAGE_ID));
      ARTreject(REJECT_DISTRIB, cp);
      return false;
    } else {
      ARTparsedist(HDR(HDR__DISTRIBUTION), HDR_LEN(HDR__DISTRIBUTION),
	&data->Distribution);
      if (ME.Distributions && data->Distribution.List != NULL
          && *data->Distribution.List != NULL
	  && !DISTwantany(ME.Distributions, data->Distribution.List)) {
	snprintf(cp->Error, sizeof(cp->Error),
                 "%d Unwanted distribution \"%s\"",
                 ihave ? NNTP_FAIL_IHAVE_REJECT : NNTP_FAIL_TAKETHIS_REJECT,
                 MaxLength(data->Distribution.List[0],
                           data->Distribution.List[0]));
	ARTlog(data, ART_REJECT, cp->Error);
        if (innconf->remembertrash && (Mode == OMrunning) &&
	    !InndHisRemember(HDR(HDR__MESSAGE_ID), data->Posted))
	  syslog(L_ERROR, "%s cant write history %s %m",
	    LogName, HDR(HDR__MESSAGE_ID));
	ARTreject(REJECT_DISTRIB, cp);
	return false;
      }
    }
  } else {
    ARTparsedist("", 0, &data->Distribution);
  }

  for (i = nSites, sp = Sites; --i >= 0; sp++) {
    sp->Poison = false;
    sp->Sendit = false;
    sp->Seenit = false;
    sp->FNLnames.used = 0;
    sp->ng = NULL;
  }

  if (HDR_FOUND(HDR__FOLLOWUPTO)) {
    /* Count the number of commas without syntactically parsing the header. */
    for (i = 1, p = HDR(HDR__FOLLOWUPTO) ; (p = strchr(p, ',')) != NULL ;
         i++, p++) ;

    data->Followcount = i;

    /* When "poster" is the only value, then it is not a followup. */
    if (i == 1) {
      /* Skip leading whitespaces. */
      p = (char *) skip_fws(HDR(HDR__FOLLOWUPTO));

      /* Check for an empty header field or "poster". */
      if (*p == '\0' || (strncasecmp(p, "poster", 6) == 0
          && (p[6] == ' ' || p[6] == '\t' || p[6] == '\0'
              || p[6] == '\r' || p[6] == '\n'))) {
        data->Followcount = 0;
      }
    }
  } else {
    data->Followcount = data->Groupcount;
  }

  groups = data->Newsgroups.List;
  /* Parse the Control: header. */
  LikeNewgroup = false;
  if (HDR_FOUND(HDR__CONTROL)) {
    IsControl = true;

    /* Nip off the first word into lowercase. */
    strlcpy(ControlWord, HDR(HDR__CONTROL), sizeof(ControlWord));
    for (p = ControlWord; *p && !ISWHITE(*p); p++)
      if (isupper((unsigned char) *p))
	*p = tolower(*p);
    *p = '\0';
    LikeNewgroup = (strcasecmp(ControlWord, "newgroup") == 0
                    || strcasecmp(ControlWord, "rmgroup") == 0);

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
    
    LikeNewgroup = (LikeNewgroup || strcasecmp(ControlWord, "checkgroups") == 0);
    
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
  ToGroup = NoHistoryUpdate = false;
  Approved = HDR_FOUND(HDR__APPROVED);
  ngptr = GroupPointers;
  for (GroupMissing = Accepted = false; (p = *groups) != NULL; groups++) {
    if ((ngp = NGfind(p)) == NULL) {
      GroupMissing = true;
      if (LikeNewgroup && Approved) {
        /* Checkgroups/newgroup/rmgroup being sent to a group that doesn't
         * exist.  Assume it is being sent to the group being created or
         * removed (or to the admin group to which the checkgroups is posted),
         * and send it to all sites that would or would have had the group
         * if it were created. */
        ARTsendthegroup(*groups, ARTctl);
        Accepted = true;
      } else if (innconf->wanttrash && !innconf->verifygroups) {
        /* Don't set Accepted in this case, because we may still end
         * up filing the article in the junk group. */
        for (ngpjunk = NGfind(ARTjnk), sp = Sites, i = nSites; --i >= 0; sp++) {
            if (sp->Name != NULL && sp->FeedTrash && SITEwantsgroup(sp, *groups)) {
                SITEmark(sp, ngpjunk);
            }
        }
        NonExist = true;
      } else {
        NonExist = true;
        /* Check if all the newsgroups listed in the Newsgroups: header of
         * the article exist on the news server. */
        if (innconf->verifygroups) {
          snprintf(cp->Error, sizeof(cp->Error),
                   "%d Nonexistent newsgroup \"%s\"",
                   ihave ? NNTP_FAIL_IHAVE_REJECT : NNTP_FAIL_TAKETHIS_REJECT,
                   MaxLength(p, p));
          ARTlog(data, ART_REJECT, cp->Error);
          if (innconf->remembertrash && (Mode == OMrunning) &&
              !InndHisRemember(HDR(HDR__MESSAGE_ID), data->Posted))
              syslog(L_ERROR, "%s cant write history %s %m",
                     LogName, HDR(HDR__MESSAGE_ID));
          ARTreject(REJECT_GROUP, cp);
          return false;
        }
      }

      ARTpoisongroup(*groups);

      if (innconf->mergetogroups) {
	/* Try to collapse all "to" newsgroups. */
	if (*p != 't' || *++p != 'o' || *++p != '.' || *++p == '\0')
	  continue;
	ngp = NGfind("to");
	ToGroup = true;
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
	  Sites[*isp].Poison = true;
      continue;
    }

    /* Basic validity check. */
    if (ngp->Rest[0] == NF_FLAG_MODERATED && !Approved) {
      snprintf(cp->Error, sizeof(cp->Error), "%d Unapproved for \"%s\"",
               ihave ? NNTP_FAIL_IHAVE_REJECT : NNTP_FAIL_TAKETHIS_REJECT,
               MaxLength(ngp->Name, ngp->Name));
      ARTlog(data, ART_REJECT, cp->Error);
      if (innconf->remembertrash && (Mode == OMrunning) &&
	  !InndHisRemember(HDR(HDR__MESSAGE_ID), data->Posted))
	syslog(L_ERROR, "%s cant write history %s %m", LogName,
	  HDR(HDR__MESSAGE_ID));
      ARTreject(REJECT_UNAPP, cp);
      return false;
    }

    /* See if any of this group's sites considers this group poison. */
    for (isp = ngp->Poison, i = ngp->nPoison; --i >= 0; isp++)
      if (*isp >= 0)
	Sites[*isp].Poison = true;

    /* Check if we accept articles in this group from this peer, after
       poisoning.  This means that articles that we accept from them will
       be handled correctly if they're crossposted. */
    canpost = RCcanpost(cp, p);
    if (!canpost) {  /* At least one group cannot be fed by this peer.
		        If we later reject the post as unwanted group,
			don't remember it.  If we accept, do remember */
      NoHistoryUpdate = true;
      continue;
    } else if (canpost < 0) {
      snprintf(cp->Error, sizeof(cp->Error),
               "%d Won't accept posts in \"%s\"",
               ihave ? NNTP_FAIL_IHAVE_REJECT : NNTP_FAIL_TAKETHIS_REJECT,
               MaxLength(p, p));
      ARTlog(data, ART_REJECT, cp->Error);
      ARTreject(REJECT_GROUP, cp);
      return false;
    }

    /* Valid group, feed it to that group's sites. */
    Accepted = true;
    for (isp = ngp->Sites, i = ngp->nSites; --i >= 0; isp++) {
      if (*isp >= 0) {
	sp = &Sites[*isp];
	if (!sp->Poison)
	  SITEmark(sp, ngp);
      }
    }

    /* If it's junk, don't file it. */
    if (ngp->Rest[0] == NF_FLAG_JUNK)
      continue;

    /* Expand aliases, mark the article as getting filed in the group. */
    if (ngp->Alias != NULL)
      ngp = ngp->Alias;
    *ngptr++ = ngp;
    ngp->PostCount = 0;
  }

  /* Control messages not filed in "to" get filed only in control.name
   * or control. */
  if (IsControl && Accepted && !ToGroup) {
    ControlStore = true;
    controlgroup = concat("control.", ControlWord, (char *) 0);
    if ((ngp = NGfind(controlgroup)) == NULL)
      ngp = NGfind(ARTctl);
    free(controlgroup);
    ngp->PostCount = 0;
    ngptr = GroupPointers;
    *ngptr++ = ngp;
    for (isp = ngp->Sites, i = ngp->nSites; --i >= 0; isp++) {
      if (*isp >= 0) {
        /* Checkgroups/newgroup/rmgroup posted to local.example
         * will still be sent with the newsfeeds patterns
         * "*,!local.*" and "*,@local.*".  So as not to propagate
         * them, "!control,!control.*" should be added. */
        sp = &Sites[*isp];
        SITEmark(sp, ngp);
      }
    }
  }

  /* If !Accepted, then none of the article's newgroups exist in our
   * active file.  Proper action is to drop the article on the floor.
   * If ngptr == GroupPointers, then all the new articles newsgroups are
   * "j" entries in the active file.  In that case, we have to file it
   * under junk so that downstream feeds can get it. */
  if (!Accepted || ngptr == GroupPointers) {
    if (!Accepted) {
      if (NoHistoryUpdate) {
	snprintf(cp->Error, sizeof(cp->Error), "%d Can't post to \"%s\"",
                 ihave ? NNTP_FAIL_IHAVE_REJECT : NNTP_FAIL_TAKETHIS_REJECT,
                 MaxLength(data->Newsgroups.List[0],
                           data->Newsgroups.List[0]));
      } else {
        snprintf(cp->Error, sizeof(cp->Error),
                 "%d Unwanted newsgroup \"%s\"",
                 ihave ? NNTP_FAIL_IHAVE_REJECT : NNTP_FAIL_TAKETHIS_REJECT,
                 MaxLength(data->Newsgroups.List[0],
                           data->Newsgroups.List[0]));
      }

      if (innconf->logtrash) {
          /* Log the previous reject. */
          ARTlog(data, ART_REJECT, cp->Error);
      }

      if (!innconf->wanttrash) {
	if (innconf->remembertrash && (Mode == OMrunning) &&
	  !NoHistoryUpdate && !InndHisRemember(HDR(HDR__MESSAGE_ID),
                                               data->Posted))
	  syslog(L_ERROR, "%s cant write history %s %m",
	    LogName, HDR(HDR__MESSAGE_ID));
	ARTreject(REJECT_GROUP, cp);
	return false;
      } else {
        /* if !GroupMissing, then all the groups the article was posted
         * to have a flag of "x" in our active file, and therefore
         * we should throw the article away:  if you have set
         * innconf->remembertrash true, then you want all trash except that
         * which you explicitly excluded in your active file. */
  	if (!GroupMissing) {
	  if (innconf->remembertrash && (Mode == OMrunning) &&
	      !NoHistoryUpdate && !InndHisRemember(HDR(HDR__MESSAGE_ID),
                                                   data->Posted))
	    syslog(L_ERROR, "%s cant write history %s %m",
	      LogName, HDR(HDR__MESSAGE_ID));
	  ARTreject(REJECT_GROUP, cp);
	  return false;
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

  /* Loop over sites to find Poisons/ControlOnly and undo Sendit flags. */
  for (i = nSites, sp = Sites; --i >= 0; sp++) {
    if (sp->Poison || (sp->ControlOnly && !IsControl)
        || (sp->DontWantNonExist && NonExist))
      sp->Sendit = false;
  }

  if (innconf->xrefslave) {
    if (ARTxrefslave(data) == false) {
      if (HDR_FOUND(HDR__XREF)) {
	snprintf(cp->Error, sizeof(cp->Error),
                 "%d Xref: header \"%s\" invalid in xrefslave mode",
                 ihave ? NNTP_FAIL_IHAVE_REJECT : NNTP_FAIL_TAKETHIS_REJECT,
                 MaxLength(HDR(HDR__XREF), HDR(HDR__XREF)));
      } else {
	snprintf(cp->Error, sizeof(cp->Error),
                 "%d Xref: header required in xrefslave mode",
                 ihave ? NNTP_FAIL_IHAVE_REJECT : NNTP_FAIL_TAKETHIS_REJECT);
      }
      ARTlog(data, ART_REJECT, cp->Error);
      if (innconf->remembertrash && (Mode == OMrunning) &&
          !InndHisRemember(HDR(HDR__MESSAGE_ID), data->Posted))
          syslog(L_ERROR, "%s cant write history %s %m",
                 LogName, HDR(HDR__MESSAGE_ID));
      ARTreject(REJECT_OTHER, cp);
      return false;
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
  /* Change trailing '\r\n' to '\0\n' of all system header. */
  for (i = 0 ; i < MAX_ARTHEADER ; i++) {
    if (HDR_FOUND(i)) {
      HDR_LASTCHAR_SAVE(i);
      HDR_PARSE_START(i);
    }
  }
  if (token.type == TOKEN_EMPTY) {
    syslog(L_ERROR, "%s cant store article: %s", LogName, SMerrorstr);
    snprintf(cp->Error, sizeof(cp->Error), "%d cant store article",
             ihave ? NNTP_FAIL_IHAVE_DEFER : NNTP_FAIL_ACTION);
    /* Do not remember the message-ID of the article because we want
     * it to be received again later. */
    ARTlog(data, ART_REJECT, cp->Error);
    ARTreject(REJECT_OTHER, cp);
    TMRstop(TMR_ARTWRITE);
    return false;
  }
  TMRstop(TMR_ARTWRITE);
  if ((innconf->enableoverview && !innconf->useoverchan) || NeedOverview) {
    TMRstart(TMR_OVERV);
    ARTmakeoverview(cp);
    if (innconf->enableoverview && !innconf->useoverchan) {
      if ((result = OVadd(token, data->Overview.data, data->Overview.left,
	data->Arrived, data->Expires)) == OVADDFAILED) {
	if (OVctl(OVSPACE, (void *)&i) && i == OV_NOSPACE)
	  IOError("creating overview", ENOSPC);
	else
	  IOError("creating overview", 0);
	syslog(L_ERROR, "%s cant store overview for %s", LogName,
	  TokenToText(token));
	OverviewCreated = false;
      } else {
	if (result == OVADDCOMPLETED)
	  OverviewCreated = true;
	else
	  OverviewCreated = false;
      }
    }
    TMRstop(TMR_OVERV);
  }
  strlcpy(data->TokenText, TokenToText(token), sizeof(data->TokenText));

  /* Update history if we didn't get too many I/O errors above. */
  if ((Mode != OMrunning) ||
      !InndHisWrite(HDR(HDR__MESSAGE_ID), data->Arrived, data->Posted,
		    data->Expires, &token)) {
    i = errno;
    syslog(L_ERROR, "%s cant write history %s %m", LogName,
      HDR(HDR__MESSAGE_ID));
    snprintf(cp->Error, sizeof(cp->Error), "%d cant write history, %s",
             ihave ? NNTP_FAIL_IHAVE_DEFER : NNTP_FAIL_ACTION,
             strerror(errno));
    ARTlog(data, ART_REJECT, cp->Error);
    ARTreject(REJECT_OTHER, cp);
    return false;
  }

  if (NeedStoredGroup)
    data->StoredGroupLength = strlen(data->Newsgroups.List[0]);

  /* Start logging, then propagate the article. */
  if (data->CRwithoutLF > 0 || data->LFwithoutCR > 0) {
    if (data->CRwithoutLF > 0 && data->LFwithoutCR == 0)
      snprintf(cp->Error, sizeof(cp->Error),
               "Article accepted but includes CR without LF(%d)",
               data->CRwithoutLF);
    else if (data->CRwithoutLF == 0 && data->LFwithoutCR > 0)
      snprintf(cp->Error, sizeof(cp->Error),
               "Article accepted but includes LF without CR(%d)",
               data->LFwithoutCR);
    else
      snprintf(cp->Error, sizeof(cp->Error),
               "Article accepted but includes CR without LF(%d) and LF withtout CR(%d)",
               data->CRwithoutLF, data->LFwithoutCR);
    /* We have another ARTlog() for the same article just after. */
    ARTlog(data, ART_STRSTR, cp->Error);
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
  i = Now.tv_sec - cp->ArtBeg;
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
    ControlStore, OverviewCreated, Filtered);

  /* Now that it's been written, process the control message.  This has
   * a small window, if we get a new article before the newgroup message
   * has been processed.  We could pause ourselves here, but it doesn't
   * seem to be worth it. */
  if (Accepted) {
    if (IsControl) {
      ARTcontrol(data, HDR(HDR__CONTROL), cp);
    }
    if (DoCancels && HDR_FOUND(HDR__SUPERSEDES)) {
      if (IsValidMessageID(HDR(HDR__SUPERSEDES), true))
	ARTcancel(data, HDR(HDR__SUPERSEDES), false);
    }
  }

  /* And finally, send to everyone who should get it.
   * sp->Sendit is false for funnel sites:  ARTpropagate()
   * transferred it to the corresponding funnel. */
  for (sp = Sites, i = nSites; --i >= 0; sp++) {
    if (sp->Sendit) {
      TMRstart(TMR_SITESEND);
      SITEsend(sp, data);
      TMRstop(TMR_SITESEND);
    }
  }

  return true;
}

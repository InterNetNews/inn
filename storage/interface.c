/*  $Id$
**
**  Storage Manager interface
*/
#include "config.h"
#include "clibrary.h"
#include <ctype.h>
#include <errno.h>
#include <syslog.h>
#include <time.h>

#include "conffile.h"
#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/wire.h"
#include "interface.h"
#include "libinn.h"
#include "methods.h"
#include "paths.h"

typedef enum {INIT_NO, INIT_DONE, INIT_FAIL} INITTYPE;
typedef struct {
    INITTYPE		initialized;
    bool		configured;
    bool		selfexpire;
    bool		expensivestat;
} METHOD_DATA;

METHOD_DATA method_data[NUM_STORAGE_METHODS];

static STORAGE_SUB      *subscriptions = NULL;
static unsigned int     typetoindex[256];
int                     SMerrno;
char                    *SMerrorstr = NULL;
static bool             Initialized = false;
bool			SMopenmode = false;
bool			SMpreopen = false;

/*
** Checks to see if the token is valid
*/
bool IsToken(const char *text) {
    const char          *p;
    
    if (!text)
	return false;
    
    if (strlen(text) != (sizeof(TOKEN) * 2) + 2)
	return false;
    
    if (text[0] != '@')
	return false;

    if (text[(sizeof(TOKEN) * 2) + 1] != '@')
	return false;

    for (p = text + 1; *p != '@'; p++)
	if (!isxdigit((int)*p))
	    return false;
    
    return true;
}

/*
** Converts a token to a textual representation for error messages
** and the like.
*/
char *
TokenToText(const TOKEN token)
{
    static const char   hex[] = "0123456789ABCDEF";
    static char         result[(sizeof(TOKEN) * 2) + 3];
    const char          *p;
    char                *q;
    size_t              i;

    
    result[0] = '@';
    for (q = result + 1, p = (const char *) &token, i = 0; i < sizeof(TOKEN);
         i++, p++) {
	*q++ = hex[(*p & 0xF0) >> 4];
	*q++ = hex[*p & 0x0F];
    }
    *q++ = '@';
    *q++ = '\0';
    return result;
    
}

/*
** Converts a hex digit and converts it to a int
*/
static int hextodec(const int c) {
    return isdigit(c) ? (c - '0') : ((c - 'A') + 10);
}

/*
** Converts a textual representation of a token back to a native
** representation
*/
TOKEN TextToToken(const char *text) {
    const char          *p;
    char                *q;
    int                 i;
    TOKEN               token;

    if (text[0] == '@')
	p = &text[1];
    else
	p = text;

    for (q = (char *)&token, i = 0; i != sizeof(TOKEN); i++) {
	q[i] = (hextodec(*p) << 4) + hextodec(*(p + 1));
	p += 2;
    }
    return token;
}

/*
** Given an article and length in non-wire format, return a malloced region
** containing the article in wire format.  Set *newlen to the length of the
** new article.
*/ 
char *
ToWireFmt(const char *article, size_t len, size_t *newlen)
{
    size_t bytes;
    char *newart;
    const char *p;
    char  *dest;
    bool atstartofline=true;

    /* First go thru article and count number of bytes we need. */
    for (bytes = 0, p=article ; p < &article[len] ; ++p) {
	if (*p == '.' && atstartofline) ++bytes; /* 1 byte for escaping . */
	++bytes;
	if (*p == '\n') {
	    ++bytes; /* need another byte for CR */
	    atstartofline = true; /* next char starts new line */
	} else {
	    atstartofline = false;
	}
    }
    bytes += 3; /* for .\r\n */
    newart = xmalloc(bytes + 1);
    *newlen = bytes;

    /* now copy the article, making changes */
    atstartofline = true;
    for (p=article, dest=newart ; p < &article[len] ; ++p) {
	if (*p == '\n') {
	    *dest++ = '\r';
	    *dest++ = '\n';
	    atstartofline = true;
	} else {
	    if (atstartofline && *p == '.') *dest++ = '.'; /* add extra . */
	    *dest++ = *p;
	    atstartofline = false;
	}
    }
    *dest++ = '.';
    *dest++ = '\r';
    *dest++ = '\n';
    *dest = '\0';
    return newart;
}

char *
FromWireFmt(const char *article, size_t len, size_t *newlen)
{
    size_t bytes;
    char *newart;
    const char *p;
    char *dest;
    bool atstartofline = true;

    /* First go thru article and count number of bytes we need */
    for (bytes = 0, p=article ; p < &article[len] ; ) {
	/* check for terminating .\r\n and if so break */
	if (p == &article[len-3] && *p == '.' && p[1] == '\r' && p[2] == '\n')
	    break;
	/* check for .. at start-of-line */
	if (atstartofline && p < &article[len-1] && *p == '.' && p[1] == '.') {
	    bytes++; /* only output 1 byte */
	    p+=2; 
	    atstartofline = false;
	} else if (p < &article[len-1] && *p == '\r' && p[1] == '\n') { 
	    bytes++; /* \r\n counts as only one byte in output */
	    p += 2;
	    atstartofline = true;
	} else {
	    bytes++;
	    p++;
	    atstartofline = false;
	}
    }
    newart = xmalloc(bytes + 1);
    *newlen = bytes;
    for (p = article, dest = newart ; p < &article[len]; ) {
	/* check for terminating .\r\n and if so break */
	if (p == &article[len-3] && *p == '.' && p[1] == '\r' && p[2] == '\n')
	    break;
	if (atstartofline && p < &article[len-1] && *p == '.' && p[1] == '.') {
	    *dest++ = '.';
	    p += 2;
	    atstartofline = false;
	} else if (p < &article[len-1] && *p == '\r' && p[1] == '\n') {
	    *dest++ = '\n';
	    p += 2;
	    atstartofline = true;
	} else {
	    *dest++ = *p++;
	    atstartofline = false;
	}
    }
    *dest = '\0';
    return newart;
}

/*
**  get Xref header without pathhost
*/
static char *
GetXref(ARTHANDLE *art)
{
  const char *p, *p1;
  const char *q;
  char	*buff;
  bool	Nocr = false;

  p = wire_findheader(art->data, art->len, "xref");
  if (p == NULL)
    return NULL;
  q = p;
  for (p1 = NULL; p < art->data + art->len; p++) {
    if (p1 != (char *)NULL && *p1 == '\r' && *p == '\n') {
      Nocr = false;
      break;
    }
    if (*p == '\n') {
      Nocr = true;
      break;
    }
    p1 = p;
  }
  if (p >= art->data + art->len)
    return NULL;
  if (!Nocr)
    p = p1;
  /* skip pathhost */
  for (; (*q == ' ') && (q < p); q++);
  if (q == p)
    return NULL;
  if ((q = memchr(q, ' ', p - q)) == NULL)
    return NULL;
  for (q++; (*q == ' ') && (q < p); q++);
  if (q == p)
    return NULL;
  buff = xmalloc(p - q + 1);
  memcpy(buff, q, p - q);
  buff[p - q] = '\0';
  return buff;
}

/*
**  Split newsgroup and returns artnum
**  or 0 if there are no newsgroup.
*/
static ARTNUM GetGroups(char *Xref) {
  char	*p;

  if ((p = strchr(Xref, ':')) == NULL)
    return 0;
  *p++ = '\0';
  return ((ARTNUM)atoi(p));
}

STORAGE_SUB *SMGetConfig(STORAGETYPE type, STORAGE_SUB *sub) {
    if (sub == (STORAGE_SUB *)NULL)
	sub = subscriptions;
    else
	sub = sub->next;
    for (;sub != NULL; sub = sub->next) {
	if (sub->type == type) {
	    return sub;
	}
    }
    return (STORAGE_SUB *)NULL;
}

static time_t ParseTime(char *tmbuf)
{
    char *startnum;
    time_t ret;
    int tmp;

    ret = 0;
    startnum = tmbuf;
    while (*tmbuf) {
	if (!isdigit((int)*tmbuf)) {
	    tmp = atol(startnum);
	    switch (*tmbuf) {
	      case 'M':
		ret += tmp*60*60*24*31;
		break;
	      case 'd':
		ret += tmp*60*60*24;
		break;
	      case 'h':
		ret += tmp*60*60;
		break;
	      case 'm':
		ret += tmp*60;
		break;
	      case 's':
		ret += tmp;
		break;
	      default:
		return(0);
	    }
	    startnum = tmbuf+1;
	}
	tmbuf++;
    }
    return(ret);
}

#define SMlbrace  1
#define SMrbrace  2
#define SMmethod  10
#define SMgroups  11
#define SMsize    12
#define SMclass   13
#define SMexpire  14
#define SMoptions 15
#define SMexactmatch 16

static CONFTOKEN smtoks[] = {
  { SMlbrace,	"{" },
  { SMrbrace,	"}" },
  { SMmethod,	"method" },
  { SMgroups,	"newsgroups:" },
  { SMsize,	"size:" },
  { SMclass,	"class:" },
  { SMexpire,	"expires:" },
  { SMoptions,	"options:" },
  { SMexactmatch,	"exactmatch:" },
  { 0, 0 }
};

/* Open the config file and parse it, generating the policy data */
static bool
SMreadconfig(void)
{
    CONFFILE            *f;
    CONFTOKEN           *tok;
    int			type;
    int                 i;
    char                *p;
    char                *q;
    char                *path;
    char                *method = NULL;
    char                *pattern = NULL;
    size_t              minsize = 0;
    size_t              maxsize = 0;
    time_t		minexpire = 0;
    time_t		maxexpire = 0;
    int                 class = 0;
    STORAGE_SUB         *sub = NULL;
    STORAGE_SUB         *prev = NULL;
    char		*options = 0;
    int			inbrace;
    bool		exactmatch = false;

    /* if innconf isn't already read in, do so. */
    if (innconf == NULL) {
        if (!innconf_read(NULL)) {
	    SMseterror(SMERR_INTERNAL, "ReadInnConf() failed");
	    return false;
	}
    }

    for (i = 0; i < NUM_STORAGE_METHODS; i++) {
	method_data[i].initialized = INIT_NO;
	method_data[i].configured = false;
    }
    path = concatpath(innconf->pathetc, _PATH_STORAGECTL);
    f = CONFfopen(path);
    if (f == NULL) {
	SMseterror(SMERR_UNDEFINED, NULL);
        syswarn("SM cant open %s", path);
        free(path);
	return false;
    }
    free(path);
    
    inbrace = 0;
    while ((tok = CONFgettoken(smtoks, f)) != NULL) {
	if (!inbrace) {
	    if (tok->type != SMmethod) {
		SMseterror(SMERR_CONFIG, "Expected 'method' keyword");
		syslog(L_ERROR, "SM expected 'method' keyword, line %d", f->lineno);
		return false;
	    }
	    if ((tok = CONFgettoken(0, f)) == NULL) {
		SMseterror(SMERR_CONFIG, "Expected method name");
		syslog(L_ERROR, "SM expected method name, line %d", f->lineno);
		return false;
	    }
	    method = xstrdup(tok->name);
	    if ((tok = CONFgettoken(smtoks, f)) == NULL || tok->type != SMlbrace) {
		SMseterror(SMERR_CONFIG, "Expected '{'");
		syslog(L_ERROR, "SM Expected '{', line %d", f->lineno);
		return false;
	    }
	    inbrace = 1;
	    /* initialize various params to defaults. */
	    minsize = 0;
	    maxsize = 0; /* zero means no limit */
	    class = 0;
            pattern = NULL;
	    options = NULL;
	    minexpire = 0;
	    maxexpire = 0;
	    exactmatch = false;

	} else {
	    type = tok->type;
	    if (type == SMrbrace)
		inbrace = 0;
	    else {
		if ((tok = CONFgettoken(0, f)) == NULL) {
		    SMseterror(SMERR_CONFIG, "Keyword with no value");
		    syslog(L_ERROR, "SM keyword with no value, line %d", f->lineno);
		    return false;
		}
		p = tok->name;
		switch(type) {
		  case SMgroups:
		    if (pattern)
			free(pattern);
		    pattern = xstrdup(tok->name);
		    break;
		  case SMsize:
		    minsize = strtoul(p, NULL, 10);
		    if ((p = strchr(p, ',')) != NULL) {
			p++;
			maxsize = strtoul(p, NULL, 10);
		    }
		    break;
		  case SMclass:
		    class = atoi(p);
		    break;
		  case SMexpire:
		    q = strchr(p, ',');
		    if (q)
			*q++ = 0;
		    minexpire = ParseTime(p);
		    if (q)
			maxexpire = ParseTime(q);
		    break;
		  case SMoptions:
		    if (options)
			free(options);
		    options = xstrdup(p);
		    break;
		  case SMexactmatch:
		    if (strcasecmp(p, "true") == 0
                        || strcasecmp(p, "yes") == 0
                        || strcasecmp(p, "on") == 0)
			exactmatch = true;
		    break;
		  default:
		    SMseterror(SMERR_CONFIG, "Unknown keyword in method declaration");
		    syslog(L_ERROR, "SM Unknown keyword in method declaration, line %d: %s", f->lineno, tok->name);
		    free(method);
		    return false;
		    break;
		}
	    }
	}
	if (!inbrace) {
	    /* just finished a declaration */
	    sub = xmalloc(sizeof(STORAGE_SUB));
	    sub->type = TOKEN_EMPTY;
	    for (i = 0; i < NUM_STORAGE_METHODS; i++) {
		if (!strcasecmp(method, storage_methods[i].name)) {
		    sub->type = storage_methods[i].type;
		    method_data[i].configured = true;
		    break;
		}
	    }
	    if (sub->type == TOKEN_EMPTY) {
		SMseterror(SMERR_CONFIG, "Invalid storage method name");
		syslog(L_ERROR, "SM no configured storage methods are named '%s'", method);
		free(options);
		free(sub);
		return false;
	    }
	    if (!pattern) {
		SMseterror(SMERR_CONFIG, "pattern not defined");
		syslog(L_ERROR, "SM no pattern defined");
		free(options);
		free(sub);
		return false;
	    }
            sub->pattern = pattern;
	    sub->minsize = minsize;
	    sub->maxsize = maxsize;
	    sub->class = class;
	    sub->options = options;
	    sub->minexpire = minexpire;
	    sub->maxexpire = maxexpire;
	    sub->exactmatch = exactmatch;

	    free(method);
	    method = 0;

	    if (!prev)
		subscriptions = sub;
	    if (prev)
		prev->next = sub;
	    prev = sub;
	    sub->next = NULL;
	}
    }
    
    CONFfclose(f);

    return true;
}

/*
** setup storage api environment (open mode etc.)
*/
bool SMsetup(SMSETUP type, void *value) {
    if (Initialized)    
	return false;
    switch (type) {
    case SM_RDWR:
	SMopenmode = *(bool *)value;
	break;
    case SM_PREOPEN:
	SMpreopen = *(bool *)value;
	break;
    default:
	return false;
    }
    return true;
}

/*
** Calls the setup function for all of the configured methods and returns
** true if they all initialize ok, false if they don't
*/
bool SMinit(void) {
    int                 i;
    bool		allok = true;
    static		bool once = false;
    SMATTRIBUTE		smattr;

    if (Initialized)
	return true;
    
    Initialized = true;
    
    if (!SMreadconfig()) {
	SMshutdown();
	Initialized = false;
	return false;
    }

    for (i = 0; i < NUM_STORAGE_METHODS; i++) {
	if (method_data[i].configured) {
	    if (method_data[i].configured && storage_methods[i].init(&smattr)) {
		method_data[i].initialized = INIT_DONE;
		method_data[i].selfexpire = smattr.selfexpire;
		method_data[i].expensivestat = smattr.expensivestat;
	    } else {
		method_data[i].initialized = INIT_FAIL;
		method_data[i].selfexpire = false;
		method_data[i].expensivestat = true;
		syslog(L_ERROR, "SM storage method '%s' failed initialization", storage_methods[i].name);
		allok = false;
	    }
	}
	typetoindex[storage_methods[i].type] = i;
    }
    if (!allok) {
	SMshutdown();
	Initialized = false;
	SMseterror(SMERR_UNDEFINED, "one or more storage methods failed initialization");
	syslog(L_ERROR, "SM one or more storage methods failed initialization");
	return false;
    }
    if (!once && atexit(SMshutdown) < 0) {
	SMshutdown();
	Initialized = false;
	SMseterror(SMERR_UNDEFINED, NULL);
	return false;
    }
    once = true;
    return true;
}

static bool InitMethod(STORAGETYPE method) {
    SMATTRIBUTE		smattr;

    if (!Initialized)
	if (!SMreadconfig()) {
	    Initialized = false;
	    return false;
	}
    Initialized = true;
    
    if (method_data[method].initialized == INIT_DONE)
	return true;

    if (method_data[method].initialized == INIT_FAIL)
	return false;

    if (!method_data[method].configured) {
	method_data[method].initialized = INIT_FAIL;
	SMseterror(SMERR_UNDEFINED, "storage method is not configured.");
	return false;
    }
    if (!storage_methods[method].init(&smattr)) {
	method_data[method].initialized = INIT_FAIL;
	method_data[method].selfexpire = false;
	method_data[method].expensivestat = true;
	SMseterror(SMERR_UNDEFINED, "Could not initialize storage method late.");
	return false;
    }
    method_data[method].initialized = INIT_DONE;
    method_data[method].selfexpire = smattr.selfexpire;
    method_data[method].expensivestat = smattr.expensivestat;
    return true;
}

static bool
MatchGroups(const char *g, int len, const char *pattern, bool exactmatch)
{
    char *group, *groups, *q;
    int i, lastwhite;
    enum uwildmat matched;
    bool wanted = false;

    q = groups = xmalloc(len + 1);
    for (lastwhite = -1,  i = 0 ; i < len ; i++) {
	/* trim white chars */
	if (g[i] == '\r' || g[i] == '\n' || g[i] == ' ' || g[i] == '\t') {
	    if (lastwhite + 1 != i)
	        *q++ = ' ';
	    lastwhite = i;
	} else
	    *q++ = g[i];
    }
    *q = '\0';

    group = strtok(groups, " ,");
    while (group != NULL) {
        q = strchr(group, ':');
        if (q != NULL)
            *q = '\0';
        matched = uwildmat_poison(group, pattern);
        if (matched == UWILDMAT_POISON || (exactmatch && !matched)) {
	    free(groups);
	    return false;
	}
        if (matched == UWILDMAT_MATCH)
            wanted = true;
        group = strtok(NULL, " ,");
    }

    free(groups);
    return wanted;
}

STORAGE_SUB *SMgetsub(const ARTHANDLE article) {
    STORAGE_SUB         *sub;

    if (article.len == 0) {
	SMseterror(SMERR_BADHANDLE, NULL);
	return NULL;
    }

    if (article.groups == NULL)
	return NULL;

    for (sub = subscriptions; sub != NULL; sub = sub->next) {
	if (!(method_data[typetoindex[sub->type]].initialized == INIT_FAIL) &&
	    (article.len >= sub->minsize) &&
	    (!sub->maxsize || (article.len <= sub->maxsize)) &&
	    (!sub->minexpire || article.expires >= sub->minexpire) &&
	    (!sub->maxexpire || (article.expires <= sub->maxexpire)) &&
	    MatchGroups(article.groups, article.groupslen, sub->pattern,
                        sub->exactmatch)) {
	    if (InitMethod(typetoindex[sub->type]))
		return sub;
	}
    }
    errno = 0;
    SMseterror(SMERR_NOMATCH, "no matching entry in storage.conf");
    return NULL;
}

TOKEN SMstore(const ARTHANDLE article) {
    STORAGE_SUB         *sub;
    TOKEN               result;

    if (!SMopenmode) {
	result.type = TOKEN_EMPTY;
	SMseterror(SMERR_INTERNAL, "read only storage api");
	return result;
    }
    result.type = TOKEN_EMPTY;
    if ((sub = SMgetsub(article)) == NULL) {
	return result;
    }
    return storage_methods[typetoindex[sub->type]].store(article, sub->class);
}

ARTHANDLE *SMretrieve(const TOKEN token, const RETRTYPE amount) {
    ARTHANDLE           *art;

    if (method_data[typetoindex[token.type]].initialized == INIT_FAIL) {
	SMseterror(SMERR_UNINIT, NULL);
	return NULL;
    }
    if (method_data[typetoindex[token.type]].initialized == INIT_NO && !InitMethod(typetoindex[token.type])) {
	syslog(L_ERROR, "SM could not find token type or method was not initialized (%d)",
	       token.type);
	SMseterror(SMERR_UNINIT, NULL);
	return NULL;
    }
    art = storage_methods[typetoindex[token.type]].retrieve(token, amount);
    if (art)
	art->nextmethod = 0;
    return art;

}

ARTHANDLE *SMnext(const ARTHANDLE *article, const RETRTYPE amount) {
    unsigned char       i;
    int                 start;
    ARTHANDLE           *newart;

    if (article == NULL)
	start = 0;
    else
	start= article->nextmethod;

    if (method_data[start].initialized == INIT_FAIL) {
	SMseterror(SMERR_UNINIT, NULL);
	return NULL;
    }
    if (method_data[start].initialized == INIT_NO && method_data[start].configured
      && !InitMethod(start)) {
	SMseterror(SMERR_UNINIT, NULL);
	return NULL;
    }

    for (i = start, newart = NULL; i < NUM_STORAGE_METHODS; i++) {
	if (method_data[i].configured && (newart = storage_methods[i].next(article, amount)) != (ARTHANDLE *)NULL) {
	    newart->nextmethod = i;
	    break;
	} else
	    article = NULL;
    }

    return newart;
}

void SMfreearticle(ARTHANDLE *article) {
    if (method_data[typetoindex[article->type]].initialized == INIT_FAIL) {
	return;
    }
    if (method_data[typetoindex[article->type]].initialized == INIT_NO && !InitMethod(typetoindex[article->type])) {
	syslog(L_ERROR, "SM can't free article with uninitialized method");
	return;
    }
    storage_methods[typetoindex[article->type]].freearticle(article);
}

bool SMcancel(TOKEN token) {
    if (!SMopenmode) {
	SMseterror(SMERR_INTERNAL, "read only storage api");
	return false;
    }
    if (method_data[typetoindex[token.type]].initialized == INIT_FAIL) {
	SMseterror(SMERR_UNINIT, NULL);
	return false;
    }
    if (method_data[typetoindex[token.type]].initialized == INIT_NO && !InitMethod(typetoindex[token.type])) {
	SMseterror(SMERR_UNINIT, NULL);
	syslog(L_ERROR, "SM can't cancel article with uninitialized method");
	return false;
    }
    return storage_methods[typetoindex[token.type]].cancel(token);
}

bool SMprobe(PROBETYPE type, TOKEN *token, void *value) {
    struct artngnum	*ann;
    ARTHANDLE		*art;

    switch (type) {
    case SELFEXPIRE:
	return (method_data[typetoindex[token->type]].selfexpire);
    case SMARTNGNUM:
	if (method_data[typetoindex[token->type]].initialized == INIT_FAIL) {
	    SMseterror(SMERR_UNINIT, NULL);
	    return false;
	}
	if (method_data[typetoindex[token->type]].initialized == INIT_NO && !InitMethod(typetoindex[token->type])) {
	    SMseterror(SMERR_UNINIT, NULL);
	    syslog(L_ERROR, "SM can't cancel article with uninitialized method");
	    return false;
	}
	if ((ann = (struct artngnum *)value) == NULL)
	    return false;
	ann->groupname = NULL;
	if (storage_methods[typetoindex[token->type]].ctl(type, token, value)) {
	    if (ann->artnum != 0) {
		/* set by storage method */
		return true;
	    } else {
		art = storage_methods[typetoindex[token->type]].retrieve(*token, RETR_HEAD);
		if (art == NULL) {
		    if (ann->groupname != NULL)
			free(ann->groupname);
		    storage_methods[typetoindex[token->type]].freearticle(art);
		    return false;
		}
		if ((ann->groupname = GetXref(art)) == NULL) {
		    if (ann->groupname != NULL)
			free(ann->groupname);
		    storage_methods[typetoindex[token->type]].freearticle(art);
		    return false;
		}
		storage_methods[typetoindex[token->type]].freearticle(art);
		if ((ann->artnum = GetGroups(ann->groupname)) == 0) {
		    if (ann->groupname != NULL)
			free(ann->groupname);
		    return false;
		}
		return true;
	    }
	} else {
	    return false;
	}
    case EXPENSIVESTAT:
	return (method_data[typetoindex[token->type]].expensivestat);
    default:
	return false;
    }
}

bool SMflushcacheddata(FLUSHTYPE type) {
    int		i;

    for (i = 0; i < NUM_STORAGE_METHODS; i++) {
	if (method_data[i].initialized == INIT_DONE &&
	    !storage_methods[i].flushcacheddata(type))
	    syslog(L_ERROR, "SM can't flush cached data method '%s'", storage_methods[i].name);
    }
    return true;
}

void SMprintfiles(FILE *file, TOKEN token, char **xref, int ngroups) {
    if (method_data[typetoindex[token.type]].initialized == INIT_FAIL)
	return;
    if (method_data[typetoindex[token.type]].initialized == INIT_NO
        && !InitMethod(typetoindex[token.type])) {
	SMseterror(SMERR_UNINIT, NULL);
	syslog(L_ERROR, "SM can't print files for article with uninitialized method");
	return;
    }
    storage_methods[typetoindex[token.type]].printfiles(file, token, xref, ngroups);
}

void SMshutdown(void) {
    int                 i;
    STORAGE_SUB         *old;

    if (!Initialized)
	return;

    for (i = 0; i < NUM_STORAGE_METHODS; i++)
	if (method_data[i].initialized == INIT_DONE) {
	    storage_methods[i].shutdown();
	    method_data[i].initialized = INIT_NO;
	    method_data[i].configured = false;
	}
    while (subscriptions) {
	old = subscriptions;
	subscriptions = subscriptions->next;
	free(old->pattern);
	free(old->options);
	free(old);
    }
    Initialized = false;
}

void
SMseterror(int errornum, const char *error)
{
    if (SMerrorstr != NULL)
        free(SMerrorstr);
    
    if (errornum == SMERR_UNDEFINED && errno == ENOENT)
	errornum = SMERR_NOENT;
    SMerrno = errornum;

    if (error == NULL) {
	switch (SMerrno) {
	case SMERR_UNDEFINED:
            error = strerror(errno);
	    break;
	case SMERR_INTERNAL:
	    error = "Internal error";
	    break;
	case SMERR_NOENT:
	    error = "Token not found";
	    break;
	case SMERR_TOKENSHORT:
	    error = "Configured token size too small";
	    break;
	case SMERR_NOBODY:
	    error = "No article body found";
	    break;
	case SMERR_UNINIT:
	    error = "Storage manager is not initialized";
	    break;
	case SMERR_CONFIG:
	    error = "Error reading config file";
	    break;
	case SMERR_BADHANDLE:
	    error = "Bad article handle";
	    break;
	case SMERR_BADTOKEN:
	    error = "Bad token";
	    break;
	case SMERR_NOMATCH:
	    error = "No matching entry in storage.conf";
	    break;
	default:
	    error = "Undefined error";
	}
    }
    SMerrorstr = xstrdup(error);
}

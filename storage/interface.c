/*  $Id$
**
**  Storage Manager interface
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <macros.h>
#include <configdata.h>
#include <clibrary.h>
#include <libinn.h>
#include <syslog.h> 
#include <paths.h>
#include <methods.h>
#include <interface.h>
#include <errno.h>
#include <conffile.h>

typedef enum {INIT_NO, INIT_DONE, INIT_FAIL} INITTYPE;
typedef struct {
    INITTYPE		initialized;
    BOOL		configured;
    BOOL		selfexpire;
} METHOD_DATA;

METHOD_DATA method_data[NUM_STORAGE_METHODS];

STATIC STORAGE_SUB      *subscriptions = NULL;
STATIC unsigned int     typetoindex[256];
int                     SMerrno;
char                    *SMerrorstr = NULL;
STATIC BOOL             ErrorAlloc = FALSE;
STATIC BOOL             Initialized = FALSE;
BOOL			SMopenmode = FALSE;
BOOL			SMpreopen = FALSE;

/*
** Checks to see if the token is valid
*/
BOOL IsToken(const char *text) {
    const char          *p;
    
    if (!text)
	return FALSE;
    
    if (strlen(text) != (sizeof(TOKEN) * 2) + 2)
	return FALSE;
    
    if (text[0] != '@')
	return FALSE;

    if (text[(sizeof(TOKEN) * 2) + 1] != '@')
	return FALSE;

    for (p = text + 1; *p != '@'; p++)
	if (!isxdigit(*p))
	    return FALSE;
    
    return TRUE;
}

/*
** Converts a token to a textual representation for error messages
** and the like.
*/
char *TokenToText(const TOKEN token) {
    static char         hex[] = "0123456789ABCDEF";
    static char         result[(sizeof(TOKEN) * 2) + 3];
    char                *p;
    char                *q;
    char                i;

    result[0] = '@';
    for (q = result + 1, p = (char *)&token, i = 0; i < sizeof(TOKEN); i++, p++) {
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
STATIC int hextodec(const char c) {
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
**  Searches through the given string and find the begining of the
**  message body and returns that if it finds it.  If not, it returns
**  NULL.
*/
char *SMFindBody(char *article, int len) {
    char                *p;

    for (p = article; p < (article + len - 4); p++) {
	if (!memcmp(p, "\r\n\r\n", 4))
	    return p+4;
    }
    return NULL;
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

static time_t parse_time(char *tmbuf)
{
    char *startnum;
    time_t ret;
    int tmp;

    ret = 0;
    startnum = tmbuf;
    while (*tmbuf) {
	if (!isdigit(*tmbuf)) {
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

static CONFTOKEN smtoks[] = {
  { SMlbrace,	"{" },
  { SMrbrace,	"}" },
  { SMmethod,	"method" },
  { SMgroups,	"newsgroups:" },
  { SMsize,	"size:" },
  { SMclass,	"class:" },
  { SMexpire,	"expires:" },
  { SMoptions,	"options:" },
  { 0, 0 }
};

/* Open the config file and parse it, generating the policy data */
static BOOL SMreadconfig(void) {
    CONFFILE            *f;
    CONFTOKEN           *tok;
    int			type;
    char                line[1024];
    int                 i;
    char                *p;
    char                *q;
    char                *method;
    char                *patterns = NULL;
    int                 minsize;
    int                 maxsize;
    time_t		minexpire;
    time_t		maxexpire;
    int                 class;
    STORAGE_SUB         *checksub;
    STORAGE_SUB         *sub = NULL;
    STORAGE_SUB         *prev = NULL;
    char		*options = 0;
    int			inbrace;

    /* if innconf isn't already read in, do so. */
    if (innconf == NULL) {
	if (ReadInnConf() < 0) {
	    SMseterror(SMERR_INTERNAL, "ReadInnConf() failed");
	    return FALSE;
	}
    }

    for (i = 0; i < NUM_STORAGE_METHODS; i++) {
	method_data[i].initialized = INIT_NO;
	method_data[i].configured = FALSE;
    }
    if ((f = CONFfopen(cpcatpath(innconf->pathetc, _PATH_STORAGECTL))) == NULL) {
	SMseterror(SMERR_UNDEFINED, NULL);
	syslog(L_ERROR, "SM Could not open %s: %m",
			cpcatpath(innconf->pathetc, _PATH_STORAGECTL));
	return FALSE;
    }
    
    inbrace = 0;
    while ((tok = CONFgettoken(smtoks, f)) != NULL) {
	if (!inbrace) {
	    if (tok->type != SMmethod) {
		SMseterror(SMERR_CONFIG, "Expected 'method' keyword");
		syslog(L_ERROR, "SM expected 'method' keyword, line %d", f->lineno);
		return FALSE;
	    }
	    if ((tok = CONFgettoken(0, f)) == NULL) {
		SMseterror(SMERR_CONFIG, "Expected method name");
		syslog(L_ERROR, "SM expected method name, line %d", f->lineno);
		return FALSE;
	    }
	    method = COPY(tok->name);
	    if ((tok = CONFgettoken(smtoks, f)) == NULL || tok->type != SMlbrace) {
		SMseterror(SMERR_CONFIG, "Expected '{'");
		syslog(L_ERROR, "SM Expected '{', line %d", f->lineno);
		return FALSE;
	    }
	    inbrace = 1;
	    /* initialize various params to defaults. */
	    minsize = 0;
	    maxsize = 0; /* zero means no limit */
	    class = 0;
	    options = (char *)NULL;
	    minexpire = 0;
	    maxexpire = 0;

	} else {
	    type = tok->type;
	    if (type == SMrbrace)
		inbrace = 0;
	    else {
		if ((tok = CONFgettoken(0, f)) == NULL) {
		    SMseterror(SMERR_CONFIG, "Keyword with no value");
		    syslog(L_ERROR, "SM keyword with no value, line %d", f->lineno);
		    return FALSE;
		}
		p = tok->name;
		switch(type) {
		  case SMgroups:
		    if (patterns)
			DISPOSE(patterns);
		    patterns = COPY(tok->name);
		    break;
		  case SMsize:
		    minsize = atoi(p);
		    if ((p = strchr(p, ',')) != NULL) {
			p++;
			maxsize = atoi(p);
		    }
		    break;
		  case SMclass:
		    class = atoi(p);
		    break;
		  case SMexpire:
		    q = strchr(p, ',');
		    if (q)
			*q++ = 0;
		    minexpire = parse_time(p);
		    if (q)
			maxexpire = parse_time(q);
		    break;
		  case SMoptions:
		    if (options)
			DISPOSE(options);
		    options = COPY(p);
		    break;
		  default:
		    SMseterror(SMERR_CONFIG, "Unknown keyword in method declaration");
		    syslog(L_ERROR, "SM Unknown keyword in method declaration, line %d: %s", f->lineno, tok->name);
		    DISPOSE(method);
		    return FALSE;
		    break;
		}
	    }
	}
	if (!inbrace) {
	    /* just finished a declaration */
	    sub = NEW(STORAGE_SUB, 1);
	    sub->type = TOKEN_EMPTY;
	    for (i = 0; i < NUM_STORAGE_METHODS; i++) {
		if (!strcasecmp(method, storage_methods[i].name)) {
		    sub->type = storage_methods[i].type;
		    method_data[i].configured = TRUE;
		    break;
		}
	    }
	    if (sub->type == TOKEN_EMPTY) {
		SMseterror(SMERR_CONFIG, "Invalid storage method name");
		syslog(L_ERROR, "SM no configured storage methods are named '%s'", method);
		DISPOSE(options);
		DISPOSE(sub);
		return FALSE;
	    }
	    if (!patterns) {
		SMseterror(SMERR_CONFIG, "patterns not defined");
		syslog(L_ERROR, "SM no patterns defined");
		DISPOSE(options);
		DISPOSE(sub);
		return FALSE;
	    }
	    sub->minsize = minsize;
	    sub->maxsize = maxsize;
	    sub->class = class;
	    sub->options = options;
	    sub->minexpire = minexpire;
	    sub->maxexpire = maxexpire;

	    DISPOSE(method);
	    method = 0;
	    
	
	    /* Count the number of patterns and allocate space*/
	    for (i = 1, p = patterns; *p && (p = strchr(p+1, ',')); i++);

	    sub->numpatterns = i;
	    sub->patterns = NEW(char *, i);
	    if (!prev)
		subscriptions = sub;

	    /* Store the patterns. */
	    for (i = 0, p = strtok(patterns, ","); p != NULL; i++, p = strtok(NULL, ","))
		sub->patterns[i] = COPY(p);
	    DISPOSE(patterns);
	    patterns = NULL;

	    if (prev)
		prev->next = sub;
	    prev = sub;
	    sub->next = NULL;
	}
    }
    
    (void)CONFfclose(f);

    return TRUE;
}

/*
** setup storage api environment (open mode etc.)
*/
BOOL SMsetup(SMSETUP type, void *value) {
    if (Initialized)    
	return FALSE;
    switch (type) {
    case SM_RDWR:
	SMopenmode = *(BOOL *)value;
	break;
    case SM_PREOPEN:
	SMpreopen = *(BOOL *)value;
	break;
    default:
	return FALSE;
    }
    return TRUE;
}

/*
** Calls the setup function for all of the configured methods and returns
** TRUE if they all initialize ok, FALSE if they don't
*/
BOOL SMinit(void) {
    int                 i;
    BOOL		allok = TRUE;
    BOOL		selfexpire;
    static		BOOL once = FALSE;

    if (Initialized)
	return TRUE;
    
    Initialized = TRUE;
    
    if (!SMreadconfig()) {
	SMshutdown();
	Initialized = FALSE;
	return FALSE;
    }

    for (i = 0; i < NUM_STORAGE_METHODS; i++) {
	if (method_data[i].configured) {
	    if (method_data[i].configured && storage_methods[i].init(&selfexpire)) {
		method_data[i].initialized = INIT_DONE;
		method_data[i].selfexpire = selfexpire;
	    } else {
		method_data[i].initialized = INIT_FAIL;
		method_data[i].selfexpire = FALSE;
		syslog(L_ERROR, "SM storage method '%s' failed initialization", storage_methods[i].name);
		allok = FALSE;
	    }
	}
	typetoindex[storage_methods[i].type] = i;
    }
    if (!allok) {
	SMshutdown();
	Initialized = FALSE;
	SMseterror(SMERR_UNDEFINED, "one or more storage methods failed initialization");
	syslog(L_ERROR, "SM one or more storage methods failed initialization");
	return FALSE;
    }
    if (!once && atexit(SMshutdown) < 0) {
	SMshutdown();
	Initialized = FALSE;
	SMseterror(SMERR_UNDEFINED, NULL);
	return FALSE;
    }
    once = TRUE;
    return TRUE;
}

static BOOL InitMethod(STORAGETYPE method) {
    BOOL		selfexpire;

    if (!Initialized)
	if (!SMreadconfig()) {
	    Initialized = FALSE;
	    return FALSE;
	}
    Initialized = TRUE;
    
    if (method_data[method].initialized == INIT_DONE)
	return TRUE;

    if (method_data[method].initialized == INIT_FAIL)
	return FALSE;

    if (!method_data[method].configured) {
	method_data[method].initialized = INIT_FAIL;
	SMseterror(SMERR_UNDEFINED, "storage method is not configured.");
	return FALSE;
    }
    if (!storage_methods[method].init(&selfexpire)) {
	method_data[method].initialized = INIT_FAIL;
	method_data[method].selfexpire = FALSE;
	SMseterror(SMERR_UNDEFINED, "Could not initialize storage method late.");
	return FALSE;
    }
    method_data[method].initialized = INIT_DONE;
    method_data[method].selfexpire = selfexpire;
    return TRUE;
}

static BOOL MatchGroups(const char *g, int num, char **patterns) {
    char                *group;
    char                *groups;
    char		*groupsep, *q;
    const char          *p;
    int                 i;
    BOOL                wanted = FALSE;

    /* Find the end of the line */
    for (p = g+1; (*p != '\n') && (*(p - 1) != '\r'); p++);

    groups = NEW(char, p - g);
    memcpy(groups, g, p - g - 1);
    groups[p - g - 1] = '\0';

    if (innconf->storeonxref)
	groupsep = " ";
    else
	groupsep = ",";
    for (group = strtok(groups, groupsep); group != NULL; group = strtok(NULL, groupsep)) {
	if (innconf->storeonxref && ((q = strchr(group, ':')) != (char *)NULL))
	    *q = '\0';
	for (i = 0; i < num; i++) {
	    switch (patterns[i][0]) {
	    case '!':
		if (!wanted && wildmat(group, &patterns[i][1]))
		    break;
	    case '@':
		if (wildmat(group, &patterns[i][1])) {
		    DISPOSE(groups);
		    return FALSE;
		}
	    default:
		if (wildmat(group, patterns[i]))
		    wanted = TRUE;
	    }
	}
    }

    DISPOSE(groups);
    return wanted;
}

TOKEN SMstore(const ARTHANDLE article) {
    STORAGE_SUB         *sub;
    TOKEN               result;
    char                *groups;
    char		*expire;
    time_t		expiretime;

    if (!SMopenmode) {
	result.type = TOKEN_EMPTY;
	SMseterror(SMERR_INTERNAL, "read only storage api");
	return result;
    }
    result.type = TOKEN_EMPTY;
    if (!article.data || !article.len) {
	SMseterror(SMERR_BADHANDLE, NULL);
	return result;
    }

    if (innconf->storeonxref) {
	if ((groups = (char *)HeaderFindMem(article.data, article.len, "Xref", 4)) == NULL) {
	    SMseterror(SMERR_UNDEFINED, "Could not find Xref header");
	    return result;
	}
	/* skip pathhost */
	if ((groups = strchr(groups, ' ')) == NULL) {
	    SMseterror(SMERR_UNDEFINED, "Could not find pathhost in Xref header");
	    return result;
	}
	for (groups++; *groups == ' '; groups++);
    } else {
	if ((groups = (char *)HeaderFindMem(article.data, article.len, "Newsgroups", 10)) == NULL) {
	    SMseterror(SMERR_UNDEFINED, "Could not find Newsgroups header");
	    return result;
	}
    }

    expiretime = 0;
    if (expire = (char *)HeaderFindMem(article.data, article.len, "Expires", 7)) {
	/* optionally parse expire header */
	char *x, *p;
	for (p = expire+1; (*p != '\n') && (*(p - 1) != '\r'); p++);
	x = NEW(char, p - expire);
    	memcpy(x, expire, p - expire - 1);
    	x[p - expire - 1] = '\0';
	
	expiretime = parsedate(x, NULL);
	if (expiretime == -1)
	    expiretime = 0;
	else
	    expiretime -= time(0);
	DISPOSE(x);
    }

    for (sub = subscriptions; sub != NULL; sub = sub->next) {
	if (!(method_data[typetoindex[sub->type]].initialized == INIT_FAIL) &&
	    (article.len >= sub->minsize) &&
	    (!sub->maxsize || (article.len <= sub->maxsize)) &&
	    (!sub->minexpire || expiretime >= sub->minexpire) &&
	    (!sub->maxexpire || (expiretime <= sub->maxexpire)) &&
	    MatchGroups(groups, sub->numpatterns, sub->patterns)) {
	    if (InitMethod(typetoindex[sub->type]))
		return storage_methods[typetoindex[sub->type]].store(article, sub->class);
	}
    }

    return result;
}

ARTHANDLE *SMretrieve(const TOKEN token, const RETRTYPE amount) {
    ARTHANDLE           *art;

    if (method_data[typetoindex[token.type]].initialized == INIT_FAIL) {
	SMseterror(SMERR_UNINIT, NULL);
	return NULL;
    }
    if (method_data[typetoindex[token.type]].initialized == INIT_NO && !InitMethod(typetoindex[token.type])) {
	syslog(L_ERROR, "SM could not find token type or method was not initialized");
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

BOOL SMcancel(TOKEN token) {
    if (!SMopenmode) {
	SMseterror(SMERR_INTERNAL, "read only storage api");
	return FALSE;
    }
    if (method_data[typetoindex[token.type]].initialized == INIT_FAIL) {
	SMseterror(SMERR_UNINIT, NULL);
	return FALSE;
    }
    if (method_data[typetoindex[token.type]].initialized == INIT_NO && !InitMethod(typetoindex[token.type])) {
	SMseterror(SMERR_UNINIT, NULL);
	syslog(L_ERROR, "SM can't cancel article with uninitialized method");
	return FALSE;
    }
    return storage_methods[typetoindex[token.type]].cancel(token);
}

BOOL SMprobe(PROBETYPE type, TOKEN *token) {
    switch (type) {
    case SELFEXPIRE:
	return (method_data[typetoindex[token->type]].selfexpire);
    default:
	return FALSE;
    }
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
	    method_data[i].configured = FALSE;
	}
    while (subscriptions) {
	old = subscriptions;
	subscriptions = subscriptions->next;
	DISPOSE(old->patterns);
	DISPOSE(old->options);
	DISPOSE(old);
    }
    Initialized = FALSE;
}

void SMseterror(int errornum, char *error) {
    if (ErrorAlloc)
	DISPOSE(SMerrorstr);

    ErrorAlloc = FALSE;
    
    if ((errornum == SMERR_UNDEFINED) && (errno == ENOENT))
	errornum = SMERR_NOENT;
	    
    SMerrno = errornum;

    if (error == NULL) {
	switch (SMerrno) {
	case SMERR_UNDEFINED:
	    SMerrorstr = COPY(strerror(errno));
	    ErrorAlloc = TRUE;
	    break;
	case SMERR_INTERNAL:
	    SMerrorstr = "Internal error";
	    break;
	case SMERR_NOENT:
	    SMerrorstr = "Token not found";
	    break;
	case SMERR_TOKENSHORT:
	    SMerrorstr = "Configured token size too small";
	    break;
	case SMERR_NOBODY:
	    SMerrorstr = "No article body found";
	    break;
	case SMERR_UNINIT:
	    SMerrorstr = "Storage manager is not initialized";
	    break;
	case SMERR_CONFIG:
	    SMerrorstr = "Error reading config file";
	    break;
	case SMERR_BADHANDLE:
	    SMerrorstr = "Bad article handle";
	    break;
	case SMERR_BADTOKEN:
	    SMerrorstr = "Bad token";
	    break;
	default:
	    SMerrorstr = "Undefined error";
	}
    } else {
	SMerrorstr = COPY(error);
	ErrorAlloc = TRUE;
    }
}


/*  $Id$
**
**  How to figure out where a user comes from, and what that user can do once
**  we know who sie is.
*/
#include "config.h"
#include "clibrary.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>

#include "conffile.h"
#include "nnrpd.h"

/* wait portability mess.  Per autoconf, #define macros ourself. */
#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
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
#ifndef WTERMSIG
# define WTERMSIG(status)       (((unsigned)(status)) & 0x7F)
#endif

/* Error returns from inet_addr. */
#ifndef INADDR_NONE
# define INADDR_NONE 0xffffffff
#endif

/* Needed on AIX 4.1 to get fd_set and friends. */
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif

/* data types */
typedef struct _CONFCHAIN {
    CONFFILE *f;
    struct _CONFCHAIN *parent;
} CONFCHAIN;

typedef struct _METHOD {
    char *name;
    char *program;
    char *users;	/* only used for auth_methods, not for res_methods. */
    char **extra_headers;
    char **extra_logs;
} METHOD;

typedef struct _AUTHGROUP {
    char *name;
    char *key;
    char *hosts;
    METHOD **res_methods;
    METHOD **auth_methods;
    char *default_user;
    char *default_domain;
} AUTHGROUP;

typedef struct _GROUP {
    char *name;
    struct _GROUP *above;
    AUTHGROUP *auth;
    ACCESSGROUP *access;
} GROUP;

/* function declarations */
static void PERMreadfile(char *filename);
static void authdecl_parse(AUTHGROUP*, CONFFILE*, CONFTOKEN*);
static void accessdecl_parse(ACCESSGROUP*, CONFFILE*, CONFTOKEN*);
static void method_parse(METHOD*, CONFFILE*, CONFTOKEN*, int);

static void add_authgroup(AUTHGROUP*);
static void add_accessgroup(ACCESSGROUP*);
static void strip_accessgroups();

static METHOD *copy_method(METHOD*);
static void free_method(METHOD*);
static AUTHGROUP *copy_authgroup(AUTHGROUP*);
static void free_authgroup(AUTHGROUP*);
static ACCESSGROUP *copy_accessgroup(ACCESSGROUP*);
static void free_accessgroup(ACCESSGROUP*);

static void CompressList(char*);
static int MatchClient(AUTHGROUP*);
static int MatchUser(char*, char*);
static char *ResolveUser(AUTHGROUP*);
static char *AuthenticateUser(AUTHGROUP*, char*, char*);

static void GrowArray(void***, void*);

/* global variables */
static AUTHGROUP **auth_realms;
static AUTHGROUP *success_auth;
static ACCESSGROUP **access_realms;

static char	*ConfigBit;
static int	ConfigBitsize;

#define PERMlbrace		1
#define PERMrbrace		2
#define PERMgroup		3
#define PERMauth		4
#define PERMaccess		5
#define PERMhost		6
#define PERMauthprog		7
#define PERMresolv		8
#define PERMresprog		9
#define PERMdefuser		10
#define PERMdefdomain		11
#define PERMusers		12
#define PERMnewsgroups		13
#define PERMread		14
#define PERMpost		15
#define PERMaccessrp		16
#define PERMheader		17
#define PERMalsolog		18
#define PERMprogram		19
#define PERMinclude		20
#define PERMkey			21
#define PERMlocaltime		22
#define PERMstrippath		23
#define PERMnnrpdperlfilter	24
#define PERMnnrpdpythonfilter	25
#define PERMfromhost		26
#define PERMpathhost		27
#define PERMorganization	28
#define PERMmoderatormailer	29
#define PERMdomain		30
#define PERMcomplaints		31
#define PERMspoolfirst		32
#define PERMcheckincludedtext	33
#define PERMclienttimeout	34
#define PERMlocalmaxartsize	35
#define PERMreadertrack		36
#define PERMstrippostcc		37
#define PERMaddnntppostinghost	38
#define PERMaddnntppostingdate	39
#define PERMnnrpdposthost	40
#define PERMnnrpdpostport	41
#define PERMnnrpdoverstats	42
#define PERMbackoff_auth	43
#define PERMbackoff_db		44
#define PERMbackoff_k		45
#define PERMbackoff_postfast	46
#define PERMbackoff_postslow	47
#define PERMbackoff_trigger	48
#define PERMnnrpdcheckart	49
#define PERMnnrpdauthsender	50
#define PERMvirtualhost		51
#define PERMnewsmaster		52
#define PERMMAX			53

#define TEST_CONFIG(a, b) \
    { \
	int byte, offset; \
	offset = a % 8; \
	byte = (a - offset) / 8; \
	b = ((ConfigBit[byte] & (1 << offset)) != 0) ? TRUE : FALSE; \
    }
#define SET_CONFIG(a) \
    { \
	int byte, offset; \
	offset = a % 8; \
	byte = (a - offset) / 8; \
	ConfigBit[byte] |= (1 << offset); \
    }
#define CLEAR_CONFIG(a) \
    { \
	int byte, offset; \
	offset = a % 8; \
	byte = (a - offset) / 8; \
	ConfigBit[byte] &= ~(1 << offset); \
    }

static CONFTOKEN PERMtoks[] = {
  { PERMlbrace, "{" },
  { PERMrbrace, "}" },
  { PERMgroup, "group" },
  { PERMauth, "auth" },
  { PERMaccess, "access" },
  { PERMhost, "hosts:" },
  { PERMauthprog, "auth:" },
  { PERMresolv, "res" },
  { PERMresprog, "res:" },
  { PERMdefuser, "default:" },
  { PERMdefdomain, "default-domain:" },
  { PERMusers, "users:" },
  { PERMnewsgroups, "newsgroups:" },
  { PERMread, "read:" },
  { PERMpost, "post:" },
  { PERMaccessrp, "access:" },
  { PERMheader, "header:" },
  { PERMalsolog, "log:" },
  { PERMprogram, "program:" },
  { PERMinclude, "include" },
  { PERMkey, "key:" },
  { PERMlocaltime, "localtime:" },
  { PERMstrippath, "strippath:" },
  { PERMnnrpdperlfilter, "perlfilter:" },
  { PERMnnrpdpythonfilter, "pythonfilter:" },
  { PERMfromhost, "fromhost:" },
  { PERMpathhost, "pathhost:" },
  { PERMorganization, "organization:" },
  { PERMmoderatormailer, "moderatormailer:" },
  { PERMdomain, "domain:" },
  { PERMcomplaints, "complaints:" },
  { PERMspoolfirst, "spoolfirst:" },
  { PERMcheckincludedtext, "checkincludedtext:" },
  { PERMclienttimeout, "clienttimeout:" },
  { PERMlocalmaxartsize, "localmaxartsize:" },
  { PERMreadertrack, "readertrack:" },
  { PERMstrippostcc, "strippostcc:" },
  { PERMaddnntppostinghost, "addnntppostinghost:" },
  { PERMaddnntppostingdate, "addnntppostingdate:" },
  { PERMnnrpdposthost, "nnrpdposthost:" },
  { PERMnnrpdpostport, "nnrpdpostport:" },
  { PERMnnrpdoverstats, "nnrpdoverstats:" },
  { PERMbackoff_auth, "backoff_auth:" },
  { PERMbackoff_db, "backoff_db:" },
  { PERMbackoff_k, "backoff_k:" },
  { PERMbackoff_postfast, "backoff_postfast:" },
  { PERMbackoff_postslow, "backoff_postslow:" },
  { PERMbackoff_trigger, "backoff_trigger:" },
  { PERMnnrpdcheckart, "nnrpdcheckart:" },
  { PERMnnrpdauthsender, "nnrpdauthsender:" },
  { PERMvirtualhost, "virtualhost:" },
  { PERMnewsmaster, "newsmaster:" },
  { 0, 0 }
};

/* function definitions */
static void GrowArray(void ***array, void *el)
{
    int i;

    if (!*array) {
	*array = NEW(void*, 2);
	i = 0;
    } else {
	for (i = 0; (*array)[i]; i++)
	    ;
	*array = RENEW(*array, void*, i+2);
    }
    (*array)[i++] = el;
    (*array)[i] = 0;
}

static METHOD *copy_method(METHOD *orig)
{
    METHOD *ret;
    int i;

    ret = NEW(METHOD, 1);
    memset(ConfigBit, '\0', ConfigBitsize);

    ret->name = COPY(orig->name);
    ret->program = COPY(orig->program);
    if (orig->users)
	ret->users = COPY(orig->users);
    else
	ret->users = 0;

    ret->extra_headers = 0;
    if (orig->extra_headers) {
	for (i = 0; orig->extra_headers[i]; i++)
	    GrowArray((void***) &ret->extra_headers,
	      (void*) COPY(orig->extra_headers[i]));
    }

    ret->extra_logs = 0;
    if (orig->extra_logs) {
	for (i = 0; orig->extra_logs[i]; i++)
	    GrowArray((void***) &ret->extra_logs,
	      (void*) COPY(orig->extra_logs[i]));
    }

    return(ret);
}

static void free_method(METHOD *del)
{
    int j;

    if (del->extra_headers) {
	for (j = 0; del->extra_headers[j]; j++)
	    DISPOSE(del->extra_headers[j]);
	DISPOSE(del->extra_headers);
    }
    if (del->extra_logs) {
	for (j = 0; del->extra_logs[j]; j++)
	    DISPOSE(del->extra_logs[j]);
	DISPOSE(del->extra_logs);
    }
    if (del->program)
	DISPOSE(del->program);
    if (del->users)
	DISPOSE(del->users);
    DISPOSE(del->name);
    DISPOSE(del);
}

static AUTHGROUP *copy_authgroup(AUTHGROUP *orig)
{
    AUTHGROUP *ret;
    int i;

    if (!orig)
	return(0);
    ret = NEW(AUTHGROUP, 1);
    memset(ConfigBit, '\0', ConfigBitsize);

    if (orig->name)
	ret->name = COPY(orig->name);
    else
	ret->name = 0;

    if (orig->key)
	ret->key = COPY(orig->key);
    else
	ret->key = 0;

    if (orig->hosts)
	ret->hosts = COPY(orig->hosts);
    else
	ret->hosts = 0;

    ret->res_methods = 0;
    if (orig->res_methods) {
	for (i = 0; orig->res_methods[i]; i++)
	    GrowArray((void***) &ret->res_methods,
	      (void*) copy_method(orig->res_methods[i]));;
    }

    ret->auth_methods = 0;
    if (orig->auth_methods) {
	for (i = 0; orig->auth_methods[i]; i++)
	    GrowArray((void***) &ret->auth_methods,
	      (void*) copy_method(orig->auth_methods[i]));
    }

    if (orig->default_user)
	ret->default_user = COPY(orig->default_user);
    else
	ret->default_user = 0;

    if (orig->default_domain)
	ret->default_domain = COPY(orig->default_domain);
    else
	ret->default_domain = 0;

    return(ret);
}

static ACCESSGROUP *copy_accessgroup(ACCESSGROUP *orig)
{
    ACCESSGROUP *ret;

    if (!orig)
	return(0);
    ret = NEW(ACCESSGROUP, 1);
    memset(ConfigBit, '\0', ConfigBitsize);
    /* copy all anyway, and update for local strings */
    *ret = *orig;

    if (orig->name)
	ret->name = COPY(orig->name);
    if (orig->key)
	ret->key = COPY(orig->key);
    if (orig->read)
	ret->read = COPY(orig->read);
    if (orig->post)
	ret->post = COPY(orig->post);
    if (orig->users)
	ret->users = COPY(orig->users);
    if (orig->fromhost)
	ret->fromhost = COPY(orig->fromhost);
    if (orig->pathhost)
	ret->pathhost = COPY(orig->pathhost);
    if (orig->organization)
	ret->organization = COPY(orig->organization);
    if (orig->moderatormailer)
	ret->moderatormailer = COPY(orig->moderatormailer);
    if (orig->domain)
	ret->domain = COPY(orig->domain);
    if (orig->complaints)
	ret->complaints = COPY(orig->complaints);
    if (orig->nnrpdposthost)
	ret->nnrpdposthost = COPY(orig->nnrpdposthost);
    if (orig->backoff_db)
	ret->backoff_db = COPY(orig->backoff_db);
    if (orig->newsmaster)
	ret->newsmaster = COPY(orig->newsmaster);
    return(ret);
}

void SetDefaultAccess(ACCESSGROUP *curaccess)
{
    curaccess->allownewnews = innconf->allownewnews;
    curaccess->locpost = FALSE;
    curaccess->allowapproved = FALSE;
    curaccess->localtime = FALSE;
    curaccess->strippath = FALSE;
    curaccess->nnrpdperlfilter = TRUE;
    curaccess->nnrpdpythonfilter = TRUE;
    curaccess->fromhost = NULL;
    if (innconf->fromhost)
	curaccess->fromhost = COPY(innconf->fromhost);
    curaccess->pathhost = NULL;
    if (innconf->pathhost)
	curaccess->pathhost = COPY(innconf->pathhost);
    curaccess->organization = NULL;
    if (innconf->organization)
	curaccess->organization = COPY(innconf->organization);
    curaccess->moderatormailer = NULL;
    if (innconf->moderatormailer)
	curaccess->moderatormailer = COPY(innconf->moderatormailer);
    curaccess->domain = NULL;
    if (innconf->domain)
	curaccess->domain = COPY(innconf->domain);
    curaccess->complaints = NULL;
    if (innconf->complaints)
	curaccess->complaints = COPY(innconf->complaints);
    curaccess->spoolfirst = innconf->spoolfirst;
    curaccess->checkincludedtext = innconf->checkincludedtext;
    curaccess->clienttimeout = innconf->clienttimeout;
    curaccess->localmaxartsize = innconf->localmaxartsize;
    curaccess->readertrack = innconf->readertrack;
    curaccess->strippostcc = innconf->strippostcc;
    curaccess->addnntppostinghost = innconf->addnntppostinghost;
    curaccess->addnntppostingdate = innconf->addnntppostingdate;
    curaccess->nnrpdposthost = innconf->nnrpdposthost;
    curaccess->nnrpdpostport = innconf->nnrpdpostport;
    curaccess->nnrpdoverstats = innconf->nnrpdoverstats;
    curaccess->backoff_auth = innconf->backoff_auth;
    curaccess->backoff_db = NULL;
    if (innconf->backoff_db && *innconf->backoff_db != '\0')
	curaccess->backoff_db = COPY(innconf->backoff_db);
    curaccess->backoff_k = innconf->backoff_k;
    curaccess->backoff_postfast = innconf->backoff_postfast;
    curaccess->backoff_postslow = innconf->backoff_postslow;
    curaccess->backoff_trigger = innconf->backoff_trigger;
    curaccess->nnrpdcheckart = innconf->nnrpdcheckart;
    curaccess->nnrpdauthsender = innconf->nnrpdauthsender;
    curaccess->virtualhost = FALSE;
    curaccess->newsmaster = NULL;
}

static void free_authgroup(AUTHGROUP *del)
{
    int i;

    if (del->name)
	DISPOSE(del->name);
    if (del->key)
	DISPOSE(del->key);
    if (del->hosts)
	DISPOSE(del->hosts);
    if (del->res_methods) {
	for (i = 0; del->res_methods[i]; i++)
	    free_method(del->res_methods[i]);
	DISPOSE(del->res_methods);
    }
    if (del->auth_methods) {
	for (i = 0; del->auth_methods[i]; i++)
	    free_method(del->auth_methods[i]);
	DISPOSE(del->auth_methods);
    }
    if (del->default_user)
	DISPOSE(del->default_user);
    if (del->default_domain)
	DISPOSE(del->default_domain);
    DISPOSE(del);
}

static void free_accessgroup(ACCESSGROUP *del)
{
    if (del->name)
	DISPOSE(del->name);
    if (del->key)
	DISPOSE(del->key);
    if (del->read)
	DISPOSE(del->read);
    if (del->post)
	DISPOSE(del->post);
    if (del->users)
	DISPOSE(del->users);
    if (del->fromhost)
	DISPOSE(del->fromhost);
    if (del->pathhost)
	DISPOSE(del->pathhost);
    if (del->organization)
	DISPOSE(del->organization);
    if (del->moderatormailer)
	DISPOSE(del->moderatormailer);
    if (del->domain)
	DISPOSE(del->domain);
    if (del->complaints)
	DISPOSE(del->complaints);
    if (del->nnrpdposthost)
	DISPOSE(del->nnrpdposthost);
    if (del->backoff_db)
	DISPOSE(del->backoff_db);
    if (del->newsmaster)
	DISPOSE(del->newsmaster);
    DISPOSE(del);
}

static void ReportError(CONFFILE *f, char *err)
{
    syslog(L_ERROR, "%s syntax error in %s(%d), %s", ClientHost,
      f->filename, f->lineno, err);
    Reply("%d NNTP server unavailable. Try later.\r\n", NNTP_TEMPERR_VAL);
    ExitWithStats(1, TRUE);
}

static void method_parse(METHOD *method, CONFFILE *f, CONFTOKEN *tok, int auth)
{
    int oldtype;

    oldtype = tok->type;
    tok = CONFgettoken(0, f);

    if (tok == NULL) {
	ReportError(f, "Expected value.");
    }

    switch (oldtype) {
      case PERMheader:
	GrowArray((void***) &method->extra_headers, (void*) COPY(tok->name));
	break;
      case PERMalsolog:
	GrowArray((void***) &method->extra_logs, (void*) COPY(tok->name));
	break;
      case PERMusers:

	if (!auth) {
	    ReportError(f, "Unexpected users: directive in file.");
	} else if (method->users) {
	    ReportError(f, "Multiple users: directive in file.");
	}

	method->users = COPY(tok->name);
	break;
      case PERMprogram:
	if (method->program) {
	    ReportError(f, "Multiple program: directives in auth/res decl."); 
	}

	method->program = COPY(tok->name);
	break;
    }
}

static void authdecl_parse(AUTHGROUP *curauth, CONFFILE *f, CONFTOKEN *tok)
{
    int oldtype;
    METHOD *m;
    BOOL bit;
    char buff[SMBUF], *oldname, *p;

    oldtype = tok->type;
    oldname = tok->name;

    tok = CONFgettoken(PERMtoks, f);

    if (tok == NULL) {
	ReportError(f, "Expected value.");
    }
    TEST_CONFIG(oldtype, bit);
    if (bit) {
	sprintf(buff, "Duplicated '%s' field in authgroup.", oldname);
	ReportError(f, buff);
    }

    switch (oldtype) {
      case PERMkey:
	curauth->key = COPY(tok->name);
	SET_CONFIG(PERMkey);
	break;
      case PERMhost:
	curauth->hosts = COPY(tok->name);
	CompressList(curauth->hosts);
	SET_CONFIG(PERMhost);

        /* nnrpd.c downcases the names of connecting hosts.  We should
           therefore also downcase the wildmat patterns to make sure there
           aren't any surprises.  DNS is case-insensitive. */
        for (p = curauth->hosts; *p; p++)
            if (CTYPE(isupper, (unsigned char) *p))
                *p = tolower((unsigned char) *p);

	break;
      case PERMdefdomain:
	curauth->default_domain = COPY(tok->name);
	SET_CONFIG(PERMdefdomain);
	break;
      case PERMdefuser:
	curauth->default_user = COPY(tok->name);
	SET_CONFIG(PERMdefuser);
	break;
      case PERMresolv:
      case PERMresprog:
	m = NEW(METHOD, 1);
	(void) memset((POINTER) m, 0, sizeof(METHOD));
	memset(ConfigBit, '\0', ConfigBitsize);
	GrowArray((void***) &curauth->res_methods, (void*) m);

	if (oldtype == PERMresprog)
	    m->program = COPY(tok->name);
	else {
	    m->name = COPY(tok->name);
	    tok = CONFgettoken(PERMtoks, f);
	    if (tok == NULL || tok->type != PERMlbrace) {
		ReportError(f, "Expected '{' after 'res'");
	    }

	    tok = CONFgettoken(PERMtoks, f);

	    while (tok != NULL && tok->type != PERMrbrace) {
		method_parse(m, f, tok, 0);
		tok = CONFgettoken(PERMtoks, f);
	    }

	    if (tok == NULL) {
		ReportError(f, "Unexpected EOF.");
	    }
	}
	break;
      case PERMauth:
      case PERMauthprog:
	m = NEW(METHOD, 1);
	(void) memset((POINTER) m, 0, sizeof(METHOD));
	memset(ConfigBit, '\0', ConfigBitsize);
	GrowArray((void***) &curauth->auth_methods, (void*) m);
	if (oldtype == PERMauthprog)
	    m->program = COPY(tok->name);
	else {
	    m->name = COPY(tok->name);
	    tok = CONFgettoken(PERMtoks, f);

	    if (tok == NULL || tok->type != PERMlbrace) {
		ReportError(f, "Expected '{' after 'auth'");
	    }

	    tok = CONFgettoken(PERMtoks, f);

	    while (tok != NULL && tok->type != PERMrbrace) {
		method_parse(m, f, tok, 1);
		tok = CONFgettoken(PERMtoks, f);
	    }

	    if (tok == NULL) {
		ReportError(f, "Unexpected EOF.");
	    }
	}

	break;
      default:
	ReportError(f, "Unexpected token.");
	break;
    }
}

static void accessdecl_parse(ACCESSGROUP *curaccess, CONFFILE *f, CONFTOKEN *tok)
{
    int oldtype, boolval;
    BOOL bit;
    char buff[SMBUF], *oldname;

    oldtype = tok->type;
    oldname = tok->name;

    tok = CONFgettoken(0, f);

    if (tok == NULL) {
	ReportError(f, "Expected value.");
    }
    TEST_CONFIG(oldtype, bit);
    if (bit) {
	sprintf(buff, "Duplicated '%s' field in accessgroup.", oldname);
	ReportError(f, buff);
    }
    if (caseEQ(tok->name, "on") || caseEQ(tok->name, "true") || caseEQ(tok->name, "yes"))
	boolval = TRUE;
    else if (caseEQ(tok->name, "off") || caseEQ(tok->name, "false") || caseEQ(tok->name, "no"))
	boolval = FALSE;
    else
	boolval = -1;

    switch (oldtype) {
      case PERMkey:
	curaccess->key = COPY(tok->name);
	SET_CONFIG(oldtype);
	break;
      case PERMusers:
	curaccess->users = COPY(tok->name);
	CompressList(curaccess->users);
	SET_CONFIG(oldtype);
	break;
      case PERMnewsgroups:
	TEST_CONFIG(PERMread, bit);
	if (bit) {
	    /* syntax error..  can't set read: or post: _and_ use
	     * newsgroups: */
	    ReportError(f, "read: newsgroups already set.");
	}
	TEST_CONFIG(PERMpost, bit);
	if (bit) {
	    /* syntax error..  can't set read: or post: _and_ use
	     * newsgroups: */
	    ReportError(f, "post: newsgroups already set.");
	}

	curaccess->read = COPY(tok->name);
	CompressList(curaccess->read);
	curaccess->post = COPY(tok->name);
	CompressList(curaccess->post);
	SET_CONFIG(oldtype);
	SET_CONFIG(PERMread);
	SET_CONFIG(PERMpost);
	break;
      case PERMread:
	curaccess->read = COPY(tok->name);
	CompressList(curaccess->read);
	SET_CONFIG(oldtype);
	break;
      case PERMpost:
	curaccess->post = COPY(tok->name);
	CompressList(curaccess->post);
	SET_CONFIG(oldtype);
	break;
      case PERMaccessrp:
	TEST_CONFIG(PERMread, bit);
	if (bit && strchr(tok->name, 'R') == NULL) {
	    DISPOSE(curaccess->read);
	    curaccess->read = 0;
	    CLEAR_CONFIG(PERMread);
	}
	TEST_CONFIG(PERMpost, bit);
	if (bit && strchr(tok->name, 'P') == NULL) {
	    DISPOSE(curaccess->post);
	    curaccess->post = 0;
	    CLEAR_CONFIG(PERMpost);
	}
	curaccess->allowapproved = (strchr(tok->name, 'A') != NULL);
	curaccess->allownewnews = (strchr(tok->name, 'N') != NULL);
	curaccess->locpost = (strchr(tok->name, 'L') != NULL);
	SET_CONFIG(oldtype);
	break;
      case PERMlocaltime:
	if (boolval != -1) curaccess->localtime = boolval;
	SET_CONFIG(oldtype);
	break;
      case PERMstrippath:
	if (boolval != -1) curaccess->strippath = boolval;
	SET_CONFIG(oldtype);
	break;
      case PERMnnrpdperlfilter:
	if (boolval != -1) curaccess->nnrpdperlfilter = boolval;
	SET_CONFIG(oldtype);
	break;
      case PERMnnrpdpythonfilter:
	if (boolval != -1) curaccess->nnrpdpythonfilter = boolval;
	SET_CONFIG(oldtype);
	break;
      case PERMfromhost:
	if (curaccess->fromhost)
	    DISPOSE(curaccess->fromhost);
	curaccess->fromhost = COPY(tok->name);
	SET_CONFIG(oldtype);
	break;
      case PERMpathhost:
	if (curaccess->pathhost)
	    DISPOSE(curaccess->pathhost);
	curaccess->pathhost = COPY(tok->name);
	SET_CONFIG(oldtype);
	break;
      case PERMorganization:
	if (curaccess->organization)
	    DISPOSE(curaccess->organization);
	curaccess->organization = COPY(tok->name);
	SET_CONFIG(oldtype);
	break;
      case PERMmoderatormailer:
	if (curaccess->moderatormailer)
	    DISPOSE(curaccess->moderatormailer);
	curaccess->moderatormailer = COPY(tok->name);
	SET_CONFIG(oldtype);
	break;
      case PERMdomain:
	if (curaccess->domain)
	    DISPOSE(curaccess->domain);
	curaccess->domain = COPY(tok->name);
	SET_CONFIG(oldtype);
	break;
      case PERMcomplaints:
	if (curaccess->complaints)
	    DISPOSE(curaccess->complaints);
	curaccess->complaints = COPY(tok->name);
	SET_CONFIG(oldtype);
	break;
      case PERMspoolfirst:
	if (boolval != -1) curaccess->spoolfirst = boolval;
	SET_CONFIG(oldtype);
	break;
      case PERMcheckincludedtext:
	if (boolval != -1) curaccess->checkincludedtext = boolval;
	SET_CONFIG(oldtype);
	break;
      case PERMclienttimeout:
	curaccess->clienttimeout = atoi(tok->name);
	SET_CONFIG(oldtype);
	break;
      case PERMlocalmaxartsize:
	curaccess->localmaxartsize = atol(tok->name);
	SET_CONFIG(oldtype);
	break;
      case PERMreadertrack:
	if (boolval != -1) curaccess->readertrack = boolval;
	SET_CONFIG(oldtype);
	break;
      case PERMstrippostcc:
	if (boolval != -1) curaccess->strippostcc = boolval;
	SET_CONFIG(oldtype);
	break;
      case PERMaddnntppostinghost:
	if (boolval != -1) curaccess->addnntppostinghost = boolval;
	SET_CONFIG(oldtype);
	break;
      case PERMaddnntppostingdate:
	if (boolval != -1) curaccess->addnntppostingdate = boolval;
	SET_CONFIG(oldtype);
	break;
      case PERMnnrpdposthost:
	if (curaccess->nnrpdposthost)
	    DISPOSE(curaccess->nnrpdposthost);
	curaccess->nnrpdposthost = COPY(tok->name);
	SET_CONFIG(oldtype);
	break;
      case PERMnnrpdpostport:
	curaccess->nnrpdpostport = atoi(tok->name);
	SET_CONFIG(oldtype);
	break;
      case PERMnnrpdoverstats:
	if (boolval != -1) curaccess->nnrpdoverstats = boolval;
	SET_CONFIG(oldtype);
	break;
      case PERMbackoff_auth:
	if (boolval != -1) curaccess->backoff_auth = boolval;
	SET_CONFIG(oldtype);
	break;
      case PERMbackoff_db:
	if (curaccess->backoff_db)
	    DISPOSE(curaccess->backoff_db);
	curaccess->backoff_db = COPY(tok->name);
	SET_CONFIG(oldtype);
	break;
      case PERMbackoff_k:
	curaccess->backoff_k = atol(tok->name);
	SET_CONFIG(oldtype);
	break;
      case PERMbackoff_postfast:
	curaccess->backoff_postfast = atol(tok->name);
	SET_CONFIG(oldtype);
	break;
      case PERMbackoff_postslow:
	curaccess->backoff_postslow = atol(tok->name);
	SET_CONFIG(oldtype);
	break;
      case PERMbackoff_trigger:
	curaccess->backoff_trigger = atol(tok->name);
	SET_CONFIG(oldtype);
	break;
      case PERMnnrpdcheckart:
	if (boolval != -1) curaccess->nnrpdcheckart = boolval;
	SET_CONFIG(oldtype);
	break;
      case PERMnnrpdauthsender:
	if (boolval != -1) curaccess->nnrpdauthsender = boolval;
	SET_CONFIG(oldtype);
	break;
      case PERMvirtualhost:
	if (boolval != -1) curaccess->virtualhost = boolval;
	SET_CONFIG(oldtype);
	break;
      case PERMnewsmaster:
	if (curaccess->newsmaster)
	    DISPOSE(curaccess->newsmaster);
	curaccess->newsmaster = COPY(tok->name);
	SET_CONFIG(oldtype);
	break;
      default:
	ReportError(f, "Unexpected token.");
	break;
    }
}

static void PERMreadfile(char *filename)
{
    CONFCHAIN	*cf	    = NULL,
		*hold	    = NULL;
    CONFTOKEN	*tok	    = NULL;
    int		inwhat;
    GROUP	*curgroup   = NULL,
		*newgroup   = NULL;
    ACCESSGROUP *curaccess  = NULL;
    AUTHGROUP	*curauth    = NULL;
    int		oldtype;
    char	*str	    = NULL;

    if(filename != NULL) {
	syslog(L_TRACE, "Reading access from %s", 
	       filename == NULL ? "(NULL)" : filename);
    }

    cf		= NEW(CONFCHAIN, 1);
    if ((cf->f = CONFfopen(filename)) == NULL) {
	syslog(L_ERROR, "%s cannot open %s: %m", ClientHost, filename);
	Reply("%d NNTP server unavailable. Try later.\r\n", NNTP_TEMPERR_VAL);
	ExitWithStats(1, TRUE);
    }
    cf->parent	= 0;

    /* are we editing an AUTH or ACCESS group? */

    inwhat	= 0;
    newgroup	= curgroup = 0;

    tok		= CONFgettoken(PERMtoks, cf->f);

    while (tok != NULL) {
	if (inwhat == 0) {
	    /* top-level parser */

	    switch (tok->type) {
		/* include a child file */

	      case PERMinclude:
		tok = CONFgettoken(0, cf->f);

		if (tok == NULL) {
		    ReportError(cf->f, "Expected filename after 'include'.");
		}

		hold		= NEW(CONFCHAIN, 1);
		hold->parent	= cf;

		/* unless the filename's path is fully qualified, open it
		 * relative to /news/etc */

		if (*tok->name == '/')
		    hold->f = CONFfopen(tok->name);
		else
		    hold->f = CONFfopen(cpcatpath(innconf->pathetc, tok->name));

		if (hold->f == NULL) {
		    ReportError(cf->f, "Couldn't open 'include' filename.");
		}

		cf = hold;
		goto again;
		break;

		/* nested group declaration. */
	      case PERMgroup:
		tok = CONFgettoken(PERMtoks, cf->f);

		if (tok == NULL) {
		    ReportError(cf->f, "Unexpected EOF at group name");
		}

		newgroup	= NEW(GROUP, 1);
		newgroup->above = curgroup;
		newgroup->name	= COPY(tok->name);
		memset(ConfigBit, '\0', ConfigBitsize);

		tok = CONFgettoken(PERMtoks, cf->f);

		if (tok == NULL || tok->type != PERMlbrace) {
		    ReportError(cf->f, "Expected '{' after group name");
		}

		/* nested group declaration */
		if (curgroup) {
		    newgroup->auth = copy_authgroup(curgroup->auth);
		    newgroup->access = copy_accessgroup(curgroup->access);
		} else {
		    newgroup->auth = 0;
		    newgroup->access = 0;
		}

		curgroup = newgroup;
		break;

		/* beginning of an auth or access group decl */
	      case PERMauth:
	      case PERMaccess:
		oldtype = tok->type;

		if ((tok = CONFgettoken(PERMtoks, cf->f)) == NULL) {
		    ReportError(cf->f, "Expected identifier.");
		}

		str = COPY(tok->name);

		tok = CONFgettoken(PERMtoks, cf->f);

		if (tok == NULL || tok->type != PERMlbrace) {
		    ReportError(cf->f, "Expected '{'");
		}

		switch (oldtype) {
		  case PERMauth:
		    if (curgroup && curgroup->auth)
			curauth = copy_authgroup(curgroup->auth);
		    else {
			curauth = NEW(AUTHGROUP, 1);
			memset((POINTER) curauth, 0, sizeof(AUTHGROUP));
			memset(ConfigBit, '\0', ConfigBitsize);
		    }

		    curauth->name = str;
		    inwhat = 1;
		    break;

		  case PERMaccess:
		    if (curgroup && curgroup->access)
			curaccess = copy_accessgroup(curgroup->access);
		    else {
			curaccess = NEW(ACCESSGROUP, 1);
			memset((POINTER) curaccess, 0, sizeof(ACCESSGROUP));
			memset(ConfigBit, '\0', ConfigBitsize);
			SetDefaultAccess(curaccess);
		    }
		    curaccess->name = str;
		    inwhat = 2;
		    break;
		}

		break;

		/* end of a group declaration */

	      case PERMrbrace:
		if (curgroup == NULL) {
		    ReportError(cf->f, "Unmatched '}'");
		}

		newgroup = curgroup;
		curgroup = curgroup->above;
		if (newgroup->auth)
		    free_authgroup(newgroup->auth);
		if (newgroup->access)
		    free_accessgroup(newgroup->access);
		DISPOSE(newgroup->name);
		DISPOSE(newgroup);
		break;

		/* stuff that belongs in an authgroup */
	      case PERMhost:
	      case PERMauthprog:
	      case PERMresprog:
	      case PERMdefuser:
	      case PERMdefdomain:
		if (curgroup == NULL) {
		    curgroup = NEW(GROUP, 1);
		    memset((POINTER) curgroup, 0, sizeof(GROUP));
		    memset(ConfigBit, '\0', ConfigBitsize);
		}
		if (curgroup->auth == NULL) {
		    curgroup->auth = NEW(AUTHGROUP, 1);
		    (void)memset((POINTER)curgroup->auth, 0, sizeof(AUTHGROUP));
		    memset(ConfigBit, '\0', ConfigBitsize);
		}

		authdecl_parse(curgroup->auth, cf->f, tok);
		break;

		/* stuff that belongs in an accessgroup */
	      case PERMusers:
	      case PERMnewsgroups:
	      case PERMread:
	      case PERMpost:
	      case PERMaccessrp:
	      case PERMlocaltime:
	      case PERMstrippath:
	      case PERMnnrpdperlfilter:
	      case PERMnnrpdpythonfilter:
	      case PERMfromhost:
	      case PERMpathhost:
	      case PERMorganization:
	      case PERMmoderatormailer:
	      case PERMdomain:
	      case PERMcomplaints:
	      case PERMspoolfirst:
	      case PERMcheckincludedtext:
	      case PERMclienttimeout:
	      case PERMlocalmaxartsize:
	      case PERMreadertrack:
	      case PERMstrippostcc:
	      case PERMaddnntppostinghost:
	      case PERMaddnntppostingdate:
	      case PERMnnrpdposthost:
	      case PERMnnrpdpostport:
	      case PERMnnrpdoverstats:
	      case PERMbackoff_auth:
	      case PERMbackoff_db:
	      case PERMbackoff_k:
	      case PERMbackoff_postfast:
	      case PERMbackoff_postslow:
	      case PERMbackoff_trigger:
	      case PERMnnrpdcheckart:
	      case PERMnnrpdauthsender:
	      case PERMvirtualhost:
	      case PERMnewsmaster:
		if (!curgroup) {
		    curgroup = NEW(GROUP, 1);
		    memset((POINTER) curgroup, 0, sizeof(GROUP));
		    memset(ConfigBit, '\0', ConfigBitsize);
		}
		if (!curgroup->access) {
		    curgroup->access = NEW(ACCESSGROUP, 1);
		    (void)memset((POINTER)curgroup->access, 0,
		      sizeof(ACCESSGROUP));
		    memset(ConfigBit, '\0', ConfigBitsize);
		    SetDefaultAccess(curgroup->access);
		}
		accessdecl_parse(curgroup->access, cf->f, tok);
		break;
	      default:
		ReportError(cf->f, "Unexpected token.");
		break;
	    }
	} else if (inwhat == 1) {
	    /* authgroup parser */
	    if (tok->type == PERMrbrace) {
		inwhat = 0;

		if (curauth->name && MatchClient(curauth)) {
		    add_authgroup(curauth);
		} else if (curauth->name) {
		    syslog(L_TRACE, "Auth strategy '%s' does not match client.  Removing.",
			   curauth->name == NULL ? "(NULL)" : curauth->name);
		    free_authgroup(curauth);
		}

		goto again;
	    }

	    authdecl_parse(curauth, cf->f, tok);
	} else if (inwhat == 2) {
	    /* accessgroup parser */
	    if (tok->type == PERMrbrace) {
		inwhat = 0;

		if (curaccess->name) {
		    add_accessgroup(curaccess);
		}

		goto again;
	    }

	    accessdecl_parse(curaccess, cf->f, tok);
	} else {
	    /* should never happen */
	    syslog(L_TRACE, "SHOULD NEVER HAPPEN!");
	}
again:
	/* go back up the 'include' chain. */
	tok = CONFgettoken(PERMtoks, cf->f);

	while (tok == NULL && cf) {
	    hold = cf;
	    cf = hold->parent;
	    CONFfclose(hold->f);
	    DISPOSE(hold);
	    if (cf) {
		tok = CONFgettoken(PERMtoks, cf->f);
	    }
	}
    }

    return;
}

void PERMgetaccess(void)
{
    int i;
    char *uname;
    int canauthenticate;

    auth_realms	    = NULL;
    access_realms   = NULL;
    success_auth    = NULL;

    PERMcanread	    = PERMcanpost   = FALSE;
    PERMreadlist    = PERMpostlist  = FALSE;
    PERMaccessconf = NULL;

    if (ConfigBit == NULL) {
	if (PERMMAX % 8 == 0)
	    ConfigBitsize = PERMMAX/8;
	else
	    ConfigBitsize = (PERMMAX - (PERMMAX % 8))/8 + 1;
	ConfigBit = NEW(char, ConfigBitsize);
	memset(ConfigBit, '\0', ConfigBitsize);
    }
    PERMreadfile(cpcatpath(innconf->pathetc, _PATH_NNRPACCESS));

    strip_accessgroups();

    if (auth_realms == NULL) {
	/* no one can talk, empty file */
	syslog(L_NOTICE, "%s no_permission", ClientHost);
	Printf("%d You have no permission to talk.  Goodbye.\r\n",
	  NNTP_ACCESS_VAL);
	ExitWithStats(1, TRUE);
    }

    /* auth_realms are all expected to match the user. */
    canauthenticate = 0;
    for (i = 0; auth_realms[i]; i++)
	if (auth_realms[i]->auth_methods)
	    canauthenticate = 1;
    uname = 0;
    while (!uname && i--) {
	if ((uname = ResolveUser(auth_realms[i])) != NULL)
	    PERMauthorized = TRUE;
	if (!uname && auth_realms[i]->default_user)
	    uname = auth_realms[i]->default_user;
    }
    if (uname) {
	strcpy(PERMuser, uname);
	uname = strchr(PERMuser, '@');
	if (!uname && auth_realms[i]->default_domain) {
	    /* append the default domain to the username */
	    strcat(PERMuser, "@");
	    strcat(PERMuser, auth_realms[i]->default_domain);
	}
	PERMneedauth = FALSE;
	success_auth = auth_realms[i];
	syslog(L_TRACE, "%s res %s", ClientHost, PERMuser);
    } else if (!canauthenticate) {
	/* couldn't resolve the user. */
	syslog(L_NOTICE, "%s no_user", ClientHost);
	Printf("%d Could not get your access name.  Goodbye.\r\n",
	  NNTP_ACCESS_VAL);
	ExitWithStats(1, TRUE);
    } else {
	PERMneedauth = TRUE;
    }
    /* check maximum allowed permissions for any host that matches (for
     * the greeting string) */
    for (i = 0; access_realms[i]; i++) {
	if (!PERMcanread)
	    PERMcanread = (access_realms[i]->read != NULL);
	if (!PERMcanpost)
	    PERMcanpost = (access_realms[i]->post != NULL);
    }
    if (!i) {
	/* no applicable access groups. Zeroing all these makes INN 
	 * return permission denied to client. */
	PERMcanread = PERMcanpost = PERMneedauth = FALSE;
    }
}

void PERMlogin(char *uname, char *pass)
{
    int i   = 0;
    char *runame;

    if (ConfigBit == NULL) {
	if (PERMMAX % 8 == 0)
	    ConfigBitsize = PERMMAX/8;
	else
	    ConfigBitsize = (PERMMAX - (PERMMAX % 8))/8 + 1;
	ConfigBit = NEW(char, ConfigBitsize);
	memset(ConfigBit, '\0', ConfigBitsize);
    }
    /* The check in CMDauthinfo uses the value of PERMneedauth to know if
     * authentication succeeded or not.  By default, authentication doesn't
     * succeed. */
    PERMneedauth = TRUE;

    if(auth_realms != NULL) {
	for (i = 0; auth_realms[i]; i++) {
	    ;
	}
    }

    runame  = NULL;

    while (runame == NULL && i--)
	runame = AuthenticateUser(auth_realms[i], uname, pass);
    if (runame) {
	strcpy(PERMuser, runame);
	uname = strchr(PERMuser, '@');
	if (!uname && auth_realms[i]->default_domain) {
	    /* append the default domain to the username */
	    strcat(PERMuser, "@");
	    strcat(PERMuser, auth_realms[i]->default_domain);
	}
	PERMneedauth = FALSE;
	PERMauthorized = TRUE;
	success_auth = auth_realms[i];
    }
}

static int MatchUser(char *pat, char *user)
{
    char *cp, **list;
    char *userlist[2];
    int ret;

    if (!pat)
	return(1);
    if (!user || !*user)
	return(0);
    cp = COPY(pat);
    list = 0;
    NGgetlist(&list, cp);
    userlist[0] = user;
    userlist[1] = 0;
    ret = PERMmatch(list, userlist);
    DISPOSE(cp);
    DISPOSE(list);
    return(ret);
}

void PERMgetpermissions()
{
    int i;
    char *cp, **list;
    char *user[2];
    static ACCESSGROUP *noaccessconf;

    if (ConfigBit == NULL) {
	if (PERMMAX % 8 == 0)
	    ConfigBitsize = PERMMAX/8;
	else
	    ConfigBitsize = (PERMMAX - (PERMMAX % 8))/8 + 1;
	ConfigBit = NEW(char, ConfigBitsize);
	memset(ConfigBit, '\0', ConfigBitsize);
    }
    if (!success_auth) {
	/* if we haven't successfully authenticated, we can't do anything. */
	syslog(L_TRACE, "%s no_success_auth", ClientHost);
	if (!noaccessconf)
	    noaccessconf = NEW(ACCESSGROUP, 1);
	PERMaccessconf = noaccessconf;
	SetDefaultAccess(PERMaccessconf);
	return;
    }
    for (i = 0; access_realms[i]; i++)
	;
    user[0] = PERMuser;
    user[1] = 0;
    while (i--) {
	if ((!success_auth->key && !access_realms[i]->key) ||
	  (access_realms[i]->key && success_auth->key &&
	   strcmp(access_realms[i]->key, success_auth->key) == 0)) {
	    if (!access_realms[i]->users)
		break;
	    else if (!*PERMuser)
		continue;
	    cp = COPY(access_realms[i]->users);
	    list = 0;
	    NGgetlist(&list, cp);
	    if (PERMmatch(list, user)) {
		syslog(L_TRACE, "%s match_user %s %s", ClientHost,
		  PERMuser, access_realms[i]->users);
		DISPOSE(cp);
		DISPOSE(list);
		break;
	    } else
		syslog(L_TRACE, "%s no_match_user %s %s", ClientHost,
		  PERMuser, access_realms[i]->users);
	    DISPOSE(cp);
	    DISPOSE(list);
	}
    }
    if (i >= 0) {
	/* found the right access group */
	if (access_realms[i]->read) {
	    cp = COPY(access_realms[i]->read);
	    PERMspecified = NGgetlist(&PERMreadlist, cp);
	    PERMcanread = TRUE;
	} else {
	    syslog(L_TRACE, "%s no_read %s", ClientHost, access_realms[i]->name);
	    PERMcanread = FALSE;
	}
	if (access_realms[i]->post) {
	    cp = COPY(access_realms[i]->post);
	    NGgetlist(&PERMpostlist, cp);
	    PERMcanpost = TRUE;
	} else {
	    syslog(L_TRACE, "%s no_post %s", ClientHost, access_realms[i]->name);
	    PERMcanpost = FALSE;
	}
	PERMaccessconf = access_realms[i];
	if (PERMaccessconf->virtualhost) {
	    if (PERMaccessconf->domain == NULL) {
		syslog(L_ERROR, "%s virtualhost needs domain parameter(%s)",
		    ClientHost, PERMaccessconf->name);
		Reply("%d NNTP server unavailable. Try later.\r\n", NNTP_TEMPERR_VAL);
		ExitWithStats(1, TRUE);
	    }
	    if (VirtualPath)
		DISPOSE(VirtualPath);
	    if (EQ(innconf->pathhost, PERMaccessconf->pathhost)) {
		/* use domain, if pathhost in access relm matches one in
		   inn.conf to differentiate virtual host */
		if (innconf->domain != NULL && EQ(innconf->domain, PERMaccessconf->domain)) {
		    syslog(L_ERROR, "%s domain parameter(%s) in readers.conf must be different from the one in inn.conf",
			ClientHost, PERMaccessconf->name);
		    Reply("%d NNTP server unavailable. Try later.\r\n", NNTP_TEMPERR_VAL);
		    ExitWithStats(1, TRUE);
		}
		VirtualPathlen = strlen(PERMaccessconf->domain) + strlen("!");
		VirtualPath = NEW(char, VirtualPathlen + 1);
		sprintf(VirtualPath, "%s!", PERMaccessconf->domain);
	    } else {
		VirtualPathlen = strlen(PERMaccessconf->pathhost) + strlen("!");
		VirtualPath = NEW(char, VirtualPathlen + 1);
		sprintf(VirtualPath, "%s!", PERMaccessconf->pathhost);
	    }
	} else
	    VirtualPathlen = 0;
    } else {
	if (!noaccessconf)
	    noaccessconf = NEW(ACCESSGROUP, 1);
	PERMaccessconf = noaccessconf;
	SetDefaultAccess(PERMaccessconf);
	syslog(L_TRACE, "%s no_access_realm", ClientHost);
    }
}

/* strip blanks out of a string */
static void CompressList(char *list)
{
    char *cpto;

    /* XXX
     * I should make it check for preceding ',' characters before stripping a
     * space.
     */
    for (cpto = list; *list; ) {
	if ((*list == '\n') || (*list == ' ') || (*list == '\t'))
	    list++;
	else
	    *cpto++ = *list++;
    }
    *cpto = '\0';
}

static int MatchClient(AUTHGROUP *group)
{
    char    **list;
    int	    ret	    = 0;
    char    *cp;
    int	    iter;
    char    *pat, 
	    *p;

    /*	If no hosts are specified, by default they match.   */

    if (group->hosts == NULL) {
	return(1);
    }

    list    = 0;
    cp	    = COPY(group->hosts);

    NGgetlist(&list, cp);

    /* default is no access */
    for (iter = 0; list[iter]; iter++) {
	;
    }

    while (iter-- > 0) {
	pat = list[iter];
	if (*pat == '!')
	    pat++;
	ret = wildmat(ClientHost, pat);
	if (!ret && *ClientIp) {
	    ret = wildmat(ClientIp, pat);
	    if (!ret && (p = strchr(pat, '/')) != (char *)NULL) {
		int bits, c;
		struct in_addr ia, net;
		unsigned int mask;

		*p = '\0';
		ia.s_addr = inet_addr(ClientIp);
		net.s_addr = inet_addr(pat);
		if (ia.s_addr != INADDR_NONE && net.s_addr != INADDR_NONE) {
		    if (strchr(p+1, '.') == (char *)NULL) {
			mask = atoi(p+1);
			for (bits = c = 0; c < mask && c < 32; c++)
			    bits |= (1 << (31 - c));
			mask = htonl(bits);
		    } else {
		        mask = inet_addr(p+1);
		    }
		    if ((ia.s_addr & mask) == (net.s_addr & mask))
			ret = 1;
		}
	    }
        }
	if (ret)
	    break;
    }
    if (ret && list[iter][0] == '!')
	ret = 0;
    DISPOSE(list);
    DISPOSE(cp);
    return(ret);
}

static void add_authgroup(AUTHGROUP *group)
{
    int i;

    if (auth_realms == NULL) {
	i = 0;
	auth_realms = NEW(AUTHGROUP*, 2);
    } else {
	for (i = 0; auth_realms[i]; i++)
	    ;
	RENEW(auth_realms, AUTHGROUP*, i+2);
    }
    auth_realms[i] = group;
    auth_realms[i+1] = 0;
}

static void add_accessgroup(ACCESSGROUP *group)
{
    int i;

    if (access_realms == NULL) {
	i = 0;
	access_realms = NEW(ACCESSGROUP*, 2);
    } else {
	for (i = 0; access_realms[i]; i++)
	    ;
	RENEW(access_realms, ACCESSGROUP*, i+2);
    }
    access_realms[i] = group;
    access_realms[i+1] = 0;
}

/* clean out access groups that don't apply to any of our auth groups. */

static void strip_accessgroups()
{
    int i, j;

    /* flag the access group as used or not */

    if(access_realms != NULL) {
	for (j = 0; access_realms[j] != NULL; j++) {
	    access_realms[j]->used = 0;
	}
    } else {
	syslog(L_TRACE, "No access realms to check!");
    }

    /*	If there are auth realms to check...	*/

    if(auth_realms != NULL) {
	/*  ... Then for each auth realm...	*/

	for (i = 0; auth_realms[i] != NULL; i++) {

	    /*	... for each access realm...	*/

	    for (j = 0; access_realms[j] != NULL; j++) {

		/*  If the access realm isn't already in use...	*/

		if (! access_realms[j]->used) {
		    /*	Check to see if both the access_realm key and 
			auth_realm key are NULL...			*/

		    if (!access_realms[j]->key && !auth_realms[i]->key) {
			/*  If so, mark the realm in use and continue on... */

			access_realms[j]->used = 1;
		    } else { 
			/*  If not, check to see if both the access_realm and 
			    auth_realm are NOT _both_ NULL, and see if they are
			    equal...						*/

			if (access_realms[j]->key && auth_realms[i]->key &&
			    strcmp(access_realms[j]->key, auth_realms[i]->key) == 0) {

			    /*	And if so, mark the realm in use.   */

			    access_realms[j]->used = 1;
			}
		    }
		}
	    }
	}
    } else {
	syslog(L_TRACE, "No auth realms to check!");
    }

    /* strip out unused access groups */
    i = j = 0;

    while (access_realms[i] != NULL) {
	if (access_realms[i]->used)
	    access_realms[j++] = access_realms[i];
	else
	    syslog(L_TRACE, "%s removing irrelevant access group %s", 
		   ClientHost, access_realms[i]->name);
	i++;
    }
    access_realms[j] = 0;
}

typedef struct _EXECSTUFF {
    PID_T pid;
    int rdfd, errfd, wrfd;
} EXECSTUFF;

static EXECSTUFF *ExecProg(char *arg0, char **args)
{
    EXECSTUFF *ret;
    int rdfd[2], errfd[2], wrfd[2];
    PID_T pid;

    pipe(rdfd);
    pipe(errfd);
    pipe(wrfd);
    switch (pid = FORK()) {
      case -1:
	close(rdfd[0]);
	close(rdfd[1]);
	close(errfd[0]);
	close(errfd[1]);
	close(wrfd[0]);
	close(wrfd[1]);
	return(0);
      case 0:
	close(rdfd[0]);
	dup2(rdfd[1], 1);
	close(errfd[0]);
	dup2(errfd[1], 2);
	close(wrfd[1]);
	dup2(wrfd[0], 0);
	execv(arg0, args);
	/* if we got here, there was an error */
	syslog(L_ERROR, "%s perm could not exec %s: %m", ClientHost, arg0);
	exit(1);
    }
    close(rdfd[1]);
    close(errfd[1]);
    close(wrfd[0]);
    ret = NEW(EXECSTUFF, 1);
    ret->pid = pid;
    ret->rdfd = rdfd[0];
    ret->errfd = errfd[0];
    ret->wrfd = wrfd[1];
    return(ret);
}

static void GetConnInfo(METHOD *method, char *buf)
{
    struct sockaddr_in cli, loc;
    int gotsin;
    int i;
    ARGTYPE j;

    j = sizeof(cli);
    gotsin = (getpeername(0, (struct sockaddr*)&cli, &j) == 0);
    if (gotsin)
	getsockname(0, (struct sockaddr*)&loc, &j);
    buf[0] = '\0';
    if (*ClientHost)
	sprintf(buf, "ClientHost: %s\r\n", ClientHost);
    if (*ClientIp)
	sprintf(buf+strlen(buf), "ClientIP: %s\r\n", ClientIp);
    if (gotsin) {
	sprintf(buf+strlen(buf), "ClientPort: %d\r\n", ntohs(cli.sin_port));
	sprintf(buf+strlen(buf), "LocalIP: %s\r\n", inet_ntoa(loc.sin_addr));
	sprintf(buf+strlen(buf), "LocalPort: %d\r\n", ntohs(loc.sin_port));
    }
    /* handle this here, since we only get here when we're about to exec
     * something. */
    if (method->extra_headers) {
	for (i = 0; method->extra_headers[i]; i++)
	    sprintf(buf+strlen(buf), "%s\r\n", method->extra_headers[i]);
    }
}

static char ubuf[SMBUF];

typedef void (*LineFunc)(char*);

/* messages from a program's stdout */
static void HandleProgLine(char *ln)
{
    if (caseEQn(ln, "User:", strlen("User:")))
	strcpy(ubuf, ln+strlen("User:"));
}

/* messages from a programs stderr */
static void HandleErrorLine(char *ln)
{
    syslog(L_NOTICE, "%s auth_err %s", ClientHost, ln);
}

static int HandleProgInput(int fd, char *buf, int buflen, LineFunc f)
{
    char *nl;
    char *start;
    int curpos, got;

    /* read the data */
    curpos = strlen(buf);
    if (curpos >= buflen-1) {
	/* data overflow (on one line!) */
	return(-1);
    }
    got = read(fd, buf+curpos, buflen-curpos-1);
    if (got <= 0)
	return(got);
    buf[curpos+got] = '\0';

    /* break what we got up into lines */
    start = nl = buf;
    while ((nl = strchr(nl, '\n')) != NULL) {
	if (nl != buf && *(nl-1) == '\r')
	    *(nl-1) = '\0';
	*nl++ = '\0';
	f(start);
	start = nl;
    }

    /* delete all the lines we've read from the buffer. */
    /* 'start' points to the end of the last unterminated string */
    nl = start;
    start = buf;
    if (nl == start) {
	return(0);
    }

    while (*nl) {
	*start++ = *nl++;
    }

    *start = '\0';

    return(got);
}

static void GetProgInput(EXECSTUFF *prog)
{
    fd_set rfds, tfds;
    int maxfd;
    int got;
    struct timeval tmout;
    pid_t tmp;
    int status;
    char rdbuf[BIG_BUFFER], errbuf[BIG_BUFFER];

    FD_ZERO(&rfds);
    FD_SET(prog->rdfd, &rfds);
    FD_SET(prog->errfd, &rfds);
    tfds = rfds;
    maxfd = prog->rdfd > prog->errfd ? prog->rdfd : prog->errfd;
    tmout.tv_sec = 5;
    tmout.tv_usec = 0;
    rdbuf[0] = errbuf[0] = '\0';
    while ((got = select(maxfd+1, &tfds, 0, 0, &tmout)) >= 0) {
	tmout.tv_sec = 5;
	tmout.tv_usec = 0;
	if (got > 0) {
	    if (FD_ISSET(prog->rdfd, &tfds)) {
		got = HandleProgInput(prog->rdfd, rdbuf, sizeof(rdbuf), HandleProgLine);
		if (got <= 0) {
		    close(prog->rdfd);
		    FD_CLR(prog->rdfd, &tfds);
		    kill(prog->pid, SIGTERM);
		}
	    }
	    if (FD_ISSET(prog->errfd, &tfds)) {
		got = HandleProgInput(prog->errfd, errbuf, sizeof(errbuf), HandleErrorLine);
		if (got <= 0) {
		    close(prog->errfd);
		    FD_CLR(prog->errfd, &tfds);
		    kill(prog->pid, SIGTERM);
		}
	    }
	}
	tfds = rfds;
    }
    /* wait for it if he's toast. */
    do {
	tmp = waitpid(prog->pid, &status, 0);
    } while ((tmp >= 0 || (tmp < 0 && errno == EINTR)) &&
      !WIFEXITED(status) && !WIFSIGNALED(status));
    if (WIFSIGNALED(status)) {
	ubuf[0] = '\0';
	syslog(L_NOTICE, "%s bad_hook program caught signal %d", ClientHost,
	  WTERMSIG(status));
    } else if (WIFEXITED(status)) {
	if (WEXITSTATUS(status) != 0) {
	    ubuf[0] = '\0';
	    syslog(L_TRACE, "%s bad_hook program exited with status %d",
	      ClientHost, WEXITSTATUS(status));
	}
    } else {
	syslog(L_ERROR, "%s bad_hook waitpid failed: %m", ClientHost);
	ubuf[0] = '\0';
    }
}

/* execute a series of resolvers to get the remote username */
static char *ResolveUser(AUTHGROUP *auth)
{
    int i, j;
    char *cp;
    char **args;
    char *arg0;
    char *resdir;
    EXECSTUFF *foo;
    int done	    = 0;
    char buf[BIG_BUFFER];

    if (!auth->res_methods)
	return(0);

    resdir = NEW(char, strlen(cpcatpath(innconf->pathbin, _PATH_AUTHDIR)) +
      1 + strlen(_PATH_AUTHDIR_NOPASS) + 1 + 1);
    sprintf(resdir, "%s/%s/", cpcatpath(innconf->pathbin, _PATH_AUTHDIR),
      _PATH_AUTHDIR_NOPASS);

    ubuf[0] = '\0';
    for (i = 0; auth->res_methods[i]; i++) {
	/* build the command line */
	syslog(L_TRACE, "%s res starting resolver %s", ClientHost, auth->res_methods[i]->program);
	if (auth->res_methods[i]->extra_logs) {
	    for (j = 0; auth->res_methods[i]->extra_logs[j]; j++)
		syslog(L_NOTICE, "%s res also-log: %s", ClientHost,
		  auth->res_methods[i]->extra_logs[j]);
	}
	cp = COPY(auth->res_methods[i]->program);
	args = 0;
	Argify(cp, &args);
	arg0 = NEW(char, strlen(resdir)+strlen(args[0])+1);
	sprintf(arg0, "%s%s", resdir, args[0]);
	/* exec the resolver */
	foo = ExecProg(arg0, args);
	if (foo) {
	    GetConnInfo(auth->res_methods[i], buf);
	    strcat(buf, ".\r\n");
	    xwrite(foo->wrfd, buf, strlen(buf));
	    close(foo->wrfd);

	    GetProgInput(foo);
	    done = (ubuf[0] != '\0');
	    if (done)
		syslog(L_TRACE, "%s res resolver successful, user %s", ClientHost, ubuf);
	    else
		syslog(L_TRACE, "%s res resolver failed", ClientHost);
	    DISPOSE(foo);
	} else
	    syslog(L_ERROR, "%s res couldnt start resolver: %m", ClientHost);
	/* clean up */
	DISPOSE(arg0);
	DISPOSE(args);
	DISPOSE(cp);
	if (done)
	    /* this resolver succeeded */
	    break;
    }
    DISPOSE(resdir);
    if (ubuf[0])
	return(ubuf);
    return(0);
}

/* execute a series of authenticators to get the remote username */
static char *AuthenticateUser(AUTHGROUP *auth, char *username, char *password)
{
    int i, j;
    char *cp;
    char **args;
    char *arg0;
    char *resdir;
    EXECSTUFF *foo;
    int done	    = 0;
    char buf[BIG_BUFFER];

    if (!auth->auth_methods)
	return(0);

    resdir = NEW(char, strlen(cpcatpath(innconf->pathbin, _PATH_AUTHDIR)) +
      1 + strlen(_PATH_AUTHDIR_PASSWD) + 1 + 1);
    sprintf(resdir, "%s/%s/", cpcatpath(innconf->pathbin, _PATH_AUTHDIR),
      _PATH_AUTHDIR_PASSWD);

    ubuf[0] = '\0';
    for (i = 0; auth->auth_methods[i]; i++) {
	if (auth->auth_methods[i]->users &&
	  !MatchUser(auth->auth_methods[i]->users, username))
	    continue;

	/* build the command line */
	syslog(L_TRACE, "%s auth starting authenticator %s", ClientHost, auth->auth_methods[i]->program);
	if (auth->auth_methods[i]->extra_logs) {
	    for (j = 0; auth->auth_methods[i]->extra_logs[j]; j++)
		syslog(L_NOTICE, "%s auth also-log: %s", ClientHost,
		  auth->auth_methods[i]->extra_logs[j]);
	}
	cp = COPY(auth->auth_methods[i]->program);
	args = 0;
	Argify(cp, &args);
	arg0 = NEW(char, strlen(resdir)+strlen(args[0])+1);
	sprintf(arg0, "%s%s", resdir, args[0]);
	/* exec the authenticator */
	foo = ExecProg(arg0, args);
	if (foo) {
	    GetConnInfo(auth->auth_methods[i], buf);
	    sprintf(buf+strlen(buf), "ClientAuthname: %s\r\n", username);
	    sprintf(buf+strlen(buf), "ClientPassword: %s\r\n", password);
	    strcat(buf, ".\r\n");
	    xwrite(foo->wrfd, buf, strlen(buf));
	    close(foo->wrfd);

	    GetProgInput(foo);
	    done = (ubuf[0] != '\0');
	    if (done)
		syslog(L_TRACE, "%s auth authenticator successful, user %s", ClientHost, ubuf);
	    else
		syslog(L_TRACE, "%s auth authenticator failed", ClientHost);
	    DISPOSE(foo);
	} else
	    syslog(L_ERROR, "%s auth couldnt start authenticator: %m", ClientHost);
	/* clean up */
	DISPOSE(arg0);
	DISPOSE(args);
	DISPOSE(cp);
	if (done)
	    /* this authenticator succeeded */
	    break;
    }
    DISPOSE(resdir);
    if (ubuf[0])
	return(ubuf);
    return(0);
}

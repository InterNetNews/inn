/*  $Id$
**
**  How to figure out where a user comes from, and what that user can do once
**  we know who sie is.
*/

#include "config.h"
#include "clibrary.h"
#include "portable/wait.h"
#include <netdb.h>
#include <signal.h>

#include "conffile.h"
#include "innperl.h"
#include "nnrpd.h"

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
    int  type;          /* for seperating perl_auth from auth */
    char *users;	/* only used for auth_methods, not for res_methods. */
    char **extra_headers;
    char **extra_logs;
} METHOD;

typedef struct _AUTHGROUP {
    char *name;
    char *key;
#ifdef HAVE_SSL
    int require_ssl;
#endif
    char *hosts;
    METHOD **res_methods;
    METHOD **auth_methods;
    char *default_user;
    char *default_domain;
    char *localaddress;
    char *perl_access;
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
static void accessdecl_parse(ACCESSGROUP *curaccess, CONFFILE *f, CONFTOKEN *tok);
static void method_parse(METHOD*, CONFFILE*, CONFTOKEN*, int);

static void add_authgroup(AUTHGROUP*);
static void add_accessgroup(ACCESSGROUP*);
static void strip_accessgroups(void);

static METHOD *copy_method(METHOD*);
static void free_method(METHOD*);
static AUTHGROUP *copy_authgroup(AUTHGROUP*);
static void free_authgroup(AUTHGROUP*);
static ACCESSGROUP *copy_accessgroup(ACCESSGROUP*);
static void free_accessgroup(ACCESSGROUP*);

static void CompressList(char*);
static bool MatchHost(char*, char*, char*);
static int MatchUser(char*, char*);
static char *ResolveUser(AUTHGROUP*);
static char *AuthenticateUser(AUTHGROUP*, char*, char*, char*);

static void GrowArray(void***, void*);
static void PERMvectortoaccess(ACCESSGROUP *acc, const char *name, struct vector *acccess_vec);

/* global variables */
static AUTHGROUP **auth_realms;
static AUTHGROUP *success_auth;
static ACCESSGROUP **access_realms;

static char	*ConfigBit;
static int	ConfigBitsize;

extern int LLOGenable;
extern bool PerlLoaded;

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
#define PERMlocaladdress	53
#define PERMrejectwith		54
#define PERMmaxbytespersecond	55
#define PERMperl_auth           56
#define PERMperl_access         57
#ifdef HAVE_SSL
#define PERMrequire_ssl		58
#define PERMMAX			59
#else
#define PERMMAX			58
#endif

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
  { PERMlocaladdress, "localaddress:" },
  { PERMrejectwith, "reject_with:" },
  { PERMmaxbytespersecond, "max_rate:" },
  { PERMperl_auth, "perl_auth:" },
  { PERMperl_access, "perl_access:" },
#ifdef HAVE_SSL
  { PERMrequire_ssl, "require_ssl:" },
#endif
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

    ret->type = orig->type;

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

#ifdef HAVE_SSL
    ret->require_ssl = orig->require_ssl;
#endif

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

    if (orig->localaddress)
	ret->localaddress = COPY(orig->localaddress);
    else
	ret->localaddress = 0;

    if (orig->perl_access)
        ret->perl_access = COPY(orig->perl_access);
    else
        ret->perl_access = 0;

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
    if (orig->rejectwith)
	ret->rejectwith = COPY(orig->rejectwith);
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

static void SetDefaultAuth(AUTHGROUP *curauth UNUSED)
{
#ifdef HAVE_SSL
        curauth->require_ssl = FALSE;
#endif
}

void SetDefaultAccess(ACCESSGROUP *curaccess)
{
    curaccess->allownewnews = innconf->allownewnews;;
    curaccess->allowihave = false;
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
    if (innconf->backoff_db)
	curaccess->backoff_db = COPY(innconf->backoff_db);
    curaccess->backoff_k = innconf->backoff_k;
    curaccess->backoff_postfast = innconf->backoff_postfast;
    curaccess->backoff_postslow = innconf->backoff_postslow;
    curaccess->backoff_trigger = innconf->backoff_trigger;
    curaccess->nnrpdcheckart = innconf->nnrpdcheckart;
    curaccess->nnrpdauthsender = innconf->nnrpdauthsender;
    curaccess->virtualhost = FALSE;
    curaccess->newsmaster = NULL;
    curaccess->maxbytespersecond = 0;
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
    if (del->localaddress)
	DISPOSE(del->localaddress);
    if (del->perl_access)
        DISPOSE(del->perl_access);
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
    if (del->rejectwith)
	DISPOSE(del->rejectwith);
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

static void ReportError(CONFFILE *f, const char *err)
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
    int oldtype,boolval;
    METHOD *m;
    bool bit;
    char buff[SMBUF], *oldname, *p;

    oldtype = tok->type;
    oldname = tok->name;

    tok = CONFgettoken(PERMtoks, f);

    if (tok == NULL) {
	ReportError(f, "Expected value.");
    }
    TEST_CONFIG(oldtype, bit);
    if (bit) {
	snprintf(buff, sizeof(buff), "Duplicated '%s' field in authgroup.",
                 oldname);
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
	curauth->key = COPY(tok->name);
	SET_CONFIG(PERMkey);
	break;
#ifdef HAVE_SSL
      case PERMrequire_ssl:
        if (boolval != -1) curauth->require_ssl = boolval;
        SET_CONFIG(PERMrequire_ssl);
        break;
#endif
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
	memset(m, 0, sizeof(METHOD));
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
      case PERMperl_auth:
      case PERMauthprog:
	m = NEW(METHOD, 1);
	memset(m, 0, sizeof(METHOD));
	memset(ConfigBit, '\0', ConfigBitsize);
	GrowArray((void***) &curauth->auth_methods, (void*) m);
	if (oldtype == PERMauthprog) {
            m->type = PERMauthprog;
	    m->program = COPY(tok->name);
	} else if (oldtype == PERMperl_auth) {
#ifdef DO_PERL
            m->type = PERMperl_auth;
	    m->program = COPY(tok->name);
#else
            ReportError(f, "perl_auth can not be used in readers.conf: inn not compiled with perl support enabled.");
#endif
        } else {
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
      case PERMperl_access:
#ifdef DO_PERL
        curauth->perl_access = COPY(tok->name);
#else
        ReportError(f, "perl_access can not be used in readers.conf: inn not compiled with perl support enabled.");
#endif
        break;
      case PERMlocaladdress:
	curauth->localaddress = COPY(tok->name);
	CompressList(curauth->localaddress);
	SET_CONFIG(PERMlocaladdress);
	break;
      default:
	snprintf(buff, sizeof(buff), "Unexpected token: %s", tok->name);
	ReportError(f, buff);
	break;
    }
}

static void accessdecl_parse(ACCESSGROUP *curaccess, CONFFILE *f, CONFTOKEN *tok)
{
    int oldtype, boolval;
    bool bit;
    char buff[SMBUF], *oldname;

    oldtype = tok->type;
    oldname = tok->name;

    tok = CONFgettoken(0, f);

    if (tok == NULL) {
	ReportError(f, "Expected value.");
    }
    TEST_CONFIG(oldtype, bit);
    if (bit) {
	snprintf(buff, sizeof(buff), "Duplicated '%s' field in accessgroup.",
                 oldname);
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
      case PERMrejectwith:
	curaccess->rejectwith = COPY(tok->name);
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
	curaccess->allowihave = (strchr(tok->name, 'I') != NULL);
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
      case PERMmaxbytespersecond:
	curaccess->maxbytespersecond = atol(tok->name);
	SET_CONFIG(oldtype);
	break;
      default:
	snprintf(buff, sizeof(buff), "Unexpected token: %s", tok->name);
	ReportError(f, buff);
	break;
    }
}

static void PERMvectortoaccess(ACCESSGROUP *acc, const char *name, struct vector *access_vec) {
    CONFTOKEN	*tok	    = NULL;
    CONFFILE    *file;
    char        *str;
    unsigned int i;

    file	= NEW(CONFFILE, 1);
    memset(file, 0, sizeof(CONFFILE));
    file->array = access_vec->strings;
    file->array_len = access_vec->count;
 
    memset(ConfigBit, '\0', ConfigBitsize);

    SetDefaultAccess(acc);
    str = COPY(name);
    acc->name = str;

    for (i = 0; i <= access_vec->count; i++) {
      tok = CONFgettoken(PERMtoks, file);

      if (tok != NULL) {
        accessdecl_parse(acc, file, tok);
      }
    }
    DISPOSE(file);
    return;       
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
    char        *path       = NULL;
    char buff[SMBUF];

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

                path = concatpath(innconf->pathetc, tok->name);
                hold->f = CONFfopen(path);
                free(path);

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
			memset(curauth, 0, sizeof(AUTHGROUP));
			memset(ConfigBit, '\0', ConfigBitsize);
                        SetDefaultAuth(curauth);
		    }

		    curauth->name = str;
		    inwhat = 1;
		    break;

		  case PERMaccess:
		    if (curgroup && curgroup->access)
			curaccess = copy_accessgroup(curgroup->access);
		    else {
			curaccess = NEW(ACCESSGROUP, 1);
			memset(curaccess, 0, sizeof(ACCESSGROUP));
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
#ifdef HAVE_SSL
              case PERMrequire_ssl:
#endif
	      case PERMauthprog:
	      case PERMresprog:
	      case PERMdefuser:
	      case PERMdefdomain:
		if (curgroup == NULL) {
		    curgroup = NEW(GROUP, 1);
		    memset(curgroup, 0, sizeof(GROUP));
		    memset(ConfigBit, '\0', ConfigBitsize);
		}
		if (curgroup->auth == NULL) {
		    curgroup->auth = NEW(AUTHGROUP, 1);
		    memset(curgroup->auth, 0, sizeof(AUTHGROUP));
		    memset(ConfigBit, '\0', ConfigBitsize);
                    SetDefaultAuth(curgroup->auth);
		}

		authdecl_parse(curgroup->auth, cf->f, tok);
		break;

		/* stuff that belongs in an accessgroup */
	      case PERMusers:
	      case PERMrejectwith:
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
		    memset(curgroup, 0, sizeof(GROUP));
		    memset(ConfigBit, '\0', ConfigBitsize);
		}
		if (!curgroup->access) {
		    curgroup->access = NEW(ACCESSGROUP, 1);
		    memset(curgroup->access, 0, sizeof(ACCESSGROUP));
		    memset(ConfigBit, '\0', ConfigBitsize);
		    SetDefaultAccess(curgroup->access);
		}
		accessdecl_parse(curgroup->access, cf->f, tok);
		break;
	      default:
		snprintf(buff, sizeof(buff), "Unexpected token: %s", tok->name);
		ReportError(cf->f, buff);
		break;
	    }
	} else if (inwhat == 1) {
	    /* authgroup parser */
	    if (tok->type == PERMrbrace) {
		inwhat = 0;

		if (curauth->name
#ifdef HAVE_SSL
		    && ((curauth->require_ssl == FALSE) || (ClientSSL == TRUE))
#endif
		    && MatchHost(curauth->hosts, ClientHost, ClientIpString)) {
		    if (!MatchHost(curauth->localaddress, ServerHost, ServerIpString)) {
			syslog(L_TRACE, "Auth strategy '%s' does not match localhost.  Removing.",
			   curauth->name == NULL ? "(NULL)" : curauth->name);
			free_authgroup(curauth);
		    } else
			add_authgroup(curauth);
		} else {
		    syslog(L_TRACE, "Auth strategy '%s' does not match client.  Removing.",
			   curauth->name == NULL ? "(NULL)" : curauth->name);
		    free_authgroup(curauth);
		}
                curauth = NULL;
		goto again;
	    }

	    authdecl_parse(curauth, cf->f, tok);
	} else if (inwhat == 2) {
	    /* accessgroup parser */
	    if (tok->type == PERMrbrace) {
		inwhat = 0;

		if (curaccess->name)
		    add_accessgroup(curaccess);
		else
		    free_accessgroup(curaccess);
		curaccess = NULL;
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

void PERMgetaccess(char *nnrpaccess)
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
    PERMreadfile(nnrpaccess);

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
	strncpy(PERMuser, uname, sizeof(PERMuser) - 1);
        PERMuser[sizeof(PERMuser) - 1] = '\0';
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

void PERMlogin(char *uname, char *pass, char *errorstr)
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
	runame = AuthenticateUser(auth_realms[i], uname, pass, errorstr);
    if (runame) {
	strncpy(PERMuser, runame, sizeof(PERMuser) - 1);
        PERMuser[sizeof(PERMuser) - 1] = '\0';
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
    DISPOSE(list[0]);
    DISPOSE(list);
    return(ret);
}

void PERMgetpermissions()
{
    int i;
    char *cp, **list;
    char *user[2];
    static ACCESSGROUP *noaccessconf;
    char *uname;
    char *cpp, *perl_path;
    char **args;
    struct vector *access_vec;

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
#ifdef DO_PERL
    } else if (success_auth->perl_access != NULL) {
      i = 0;
      cpp = COPY(success_auth->perl_access);
      args = 0;
      Argify(cpp, &args);
      perl_path = concat(args[0], (char *) 0);
      if ((perl_path != NULL) && (strlen(perl_path) > 0)) {
        if(!PerlLoaded) {
          loadPerl();
        }
        PERLsetup(NULL, perl_path, "access");
        free(perl_path);

        uname = COPY(PERMuser);
        DISPOSE(args);        
        
        access_vec = vector_new();

        perlAccess(ClientHost, ClientIpString, ServerHost, uname, access_vec);
        DISPOSE(uname);

        access_realms[0] = NEW(ACCESSGROUP, 1);
        memset(access_realms[0], 0, sizeof(ACCESSGROUP));

        PERMvectortoaccess(access_realms[0], "perl-dyanmic", access_vec);

        vector_free(access_vec);
      } else {
        syslog(L_ERROR, "No script specified in auth method.\n");
        Reply("%d NNTP server unavailable. Try later.\r\n", NNTP_TEMPERR_VAL);
        ExitWithStats(1, TRUE);
      }
      DISPOSE(cpp);
      DISPOSE(args);
#endif /* DO_PERL */
    } else {
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
            DISPOSE(list[0]);
            DISPOSE(list);
            break;
          } else
            syslog(L_TRACE, "%s no_match_user %s %s", ClientHost,
                   PERMuser, access_realms[i]->users);
          DISPOSE(cp);
	  DISPOSE(list[0]);
          DISPOSE(list);
	}
      }
    }
    if (i >= 0) {
	/* found the right access group */
	if (access_realms[i]->rejectwith) {
	    syslog(L_ERROR, "%s rejected by rule (%s)",
		ClientHost, access_realms[i]->rejectwith);
	    Reply("%d Permission denied:  %s\r\n",
		NNTP_ACCESS_VAL, access_realms[i]->rejectwith);
	    ExitWithStats(1, TRUE);
	}
	if (access_realms[i]->read) {
	    cp = COPY(access_realms[i]->read);
	    PERMspecified = NGgetlist(&PERMreadlist, cp);
	    DISPOSE(cp);
	    PERMcanread = TRUE;
	} else {
	    syslog(L_TRACE, "%s no_read %s", ClientHost, access_realms[i]->name);
	    PERMcanread = FALSE;
	}
	if (access_realms[i]->post) {
	    cp = COPY(access_realms[i]->post);
	    NGgetlist(&PERMpostlist, cp);
	    DISPOSE(cp);
	    PERMcanpost = TRUE;
	} else {
	    syslog(L_TRACE, "%s no_post %s", ClientHost, access_realms[i]->name);
	    PERMcanpost = FALSE;
	}
	PERMaccessconf = access_realms[i];
	MaxBytesPerSecond = PERMaccessconf->maxbytespersecond;
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
                VirtualPath = concat(PERMaccessconf->domain, "!", (char *) 0);
	    } else {
                VirtualPath = concat(PERMaccessconf->pathhost, "!",
                                     (char *) 0);
	    }
            VirtualPathlen = strlen(VirtualPath);
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
    bool inword = FALSE;

    for (cpto = list; *list; ) {
	if (strchr("\n \t,", *list) != NULL) {
	    list++;
	    if(inword) {
		*cpto++ = ',';
		inword = FALSE;
	    }
	} else {
	    *cpto++ = *list++;
	    inword = TRUE;
	}
    }
    *cpto = '\0';
}

static bool MatchHost(char *hostlist, char *host, char *ip)
{
    char    **list;
    bool    ret	= FALSE;
    char    *cp;
    int	    iter;
    char    *pat, 
	    *p;

    /*	If no hostlist are specified, by default they match.   */

    if (hostlist == NULL) {
	return(TRUE);
    }

    list    = 0;
    cp	    = COPY(hostlist);

    NGgetlist(&list, cp);

    /* default is no access */
    for (iter = 0; list[iter]; iter++) {
	;
    }

    while (iter-- > 0) {
	pat = list[iter];
	if (*pat == '!')
	    pat++;
	ret = uwildmat(host, pat);
	if (!ret && *ip) {
	    ret = uwildmat(ip, pat);
	    if (!ret && (p = strchr(pat, '/')) != (char *)NULL) {
		unsigned int bits, c;
		struct in_addr ia, net, tmp;
#ifdef HAVE_INET6
		struct in6_addr ia6, net6;
		unsigned char bits8;
#endif
		unsigned int mask;

		*p = '\0';
                if (inet_aton(ip, &ia) && inet_aton(pat, &net)) {
		    if (strchr(p+1, '.') == (char *)NULL) {
			/* string following / is a masklength */
			mask = atoi(p+1);
			for (bits = c = 0; c < mask && c < 32; c++)
			    bits |= (1 << (31 - c));
			mask = htonl(bits);
		    } else {	/* or it may be a dotted quad bitmask */
                        if (inet_aton(p+1, &tmp))
                            mask = tmp.s_addr;
                        else	/* otherwise skip it */
                            continue;
		    }
		    if ((ia.s_addr & mask) == (net.s_addr & mask))
			ret = TRUE;
		}
#ifdef HAVE_INET6
                else if (inet_pton(AF_INET6, ip, &ia6) && 
			 inet_pton(AF_INET6, pat, &net6)) {
		    mask = atoi(p+1);
		    ret = TRUE;
		    /* do a prefix match byte by byte */
		    for (c = 0; c*8 < mask && c < sizeof(ia6); c++) {
			if ( (c+1)*8 <= mask &&
			    ia6.s6_addr[c] != net6.s6_addr[c] ) {
			    ret = FALSE;
			    break;
			} else if ( (c+1)*8 > mask ) {
			    for (bits8 = b = 0; b < (mask % 8); b++)
				bits8 |= (1 << (7 - b));
			    if ((ia6.s6_addr[c] & bits8) !=
			    	(net6.s6_addr[c] & bits8) ) {
				ret = FALSE;
				break;
			    }
			}
		    }
		}
#endif
	    }
        }
	if (ret)
	    break;
    }
    if (ret && list[iter][0] == '!')
	ret = FALSE;
    DISPOSE(list[0]);
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

static void strip_accessgroups(void)
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
    pid_t pid;
    int rdfd, errfd, wrfd;
} EXECSTUFF;

static EXECSTUFF *ExecProg(char *arg0, char **args)
{
    EXECSTUFF *ret;
    int rdfd[2], errfd[2], wrfd[2];
    pid_t pid;
    struct stat stb;

#if !defined(S_IXUSR) && defined(_S_IXUSR)
#define S_IXUSR _S_IXUSR
#endif /* !defined(S_IXUSR) && defined(_S_IXUSR) */

#if !defined(S_IXUSR) && defined(S_IEXEC)
#define S_IXUSR S_IEXEC
#endif  /* !defined(S_IXUSR) && defined(S_IEXEC) */

    if (stat(arg0, &stb) || !(stb.st_mode&S_IXUSR))
	return(0);

    pipe(rdfd);
    pipe(errfd);
    pipe(wrfd);
    switch (pid = fork()) {
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
    int i;

    buf[0] = '\0';
    if (*ClientHost)
	sprintf(buf, "ClientHost: %s\r\n", ClientHost);
    if (*ClientIpString)
	sprintf(buf+strlen(buf), "ClientIP: %s\r\n", ClientIpString);
    if (ClientPort)
	sprintf(buf+strlen(buf), "ClientPort: %d\r\n", ClientPort);
    if (*ServerIpString)
	sprintf(buf+strlen(buf), "LocalIP: %s\r\n", ServerIpString);
    if (ServerPort)
	sprintf(buf+strlen(buf), "LocalPort: %d\r\n", ServerPort);
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
    if (caseEQn(ln, "User:", strlen("User:"))) {
	strncpy(ubuf, ln+strlen("User:"), sizeof(ubuf) - 1);
        ubuf[sizeof(ubuf) - 1] = '\0';
    }
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
    struct timeval tmout, stv, etv;
    pid_t tmp;
    int status;
    char rdbuf[BIG_BUFFER], errbuf[BIG_BUFFER];
    TIMEINFO		Start, End;

    FD_ZERO(&rfds);
    FD_SET(prog->rdfd, &rfds);
    FD_SET(prog->errfd, &rfds);
    tfds = rfds;
    maxfd = prog->rdfd > prog->errfd ? prog->rdfd : prog->errfd;
    tmout.tv_sec = 5;
    tmout.tv_usec = 0;
    rdbuf[0] = errbuf[0] = '\0';
    gettimeofday(&stv, NULL);
    while ((got = select(maxfd+1, &tfds, 0, 0, &tmout)) >= 0) {
	gettimeofday(&etv, NULL);
	Start.time = stv.tv_sec;
	Start.usec = stv.tv_usec;
	End.time = etv.tv_sec;
	End.usec = etv.tv_usec;
	IDLEtime += TIMEINFOasDOUBLE(End) - TIMEINFOasDOUBLE(Start);
	stv = etv;
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
    gettimeofday(&etv, NULL);
    Start.time = stv.tv_sec;
    Start.usec = stv.tv_usec;
    End.time = etv.tv_sec;
    End.usec = etv.tv_usec;
    IDLEtime += TIMEINFOasDOUBLE(End) - TIMEINFOasDOUBLE(Start);
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
    char *tmp;
    EXECSTUFF *foo;
    int done	    = 0;
    char buf[BIG_BUFFER];

    if (!auth->res_methods)
	return(0);

    tmp = concatpath(innconf->pathbin, _PATH_AUTHDIR);
    resdir = concatpath(tmp, _PATH_AUTHDIR_NOPASS);
    free(tmp);

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
        arg0 = concat(resdir, "/", args[0], (char *) 0);
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
static char *AuthenticateUser(AUTHGROUP *auth, char *username, char *password, char *errorstr)
{
    int i, j;
    char *cp;
    char **args;
    char *arg0;
    char *resdir;
    char *tmp;
    char *perl_path;
    EXECSTUFF *foo;
    int done	    = 0;
    int code;
    char buf[BIG_BUFFER];

    if (!auth->auth_methods)
	return(0);

    tmp = concatpath(innconf->pathbin, _PATH_AUTHDIR);
    resdir = concatpath(tmp, _PATH_AUTHDIR_PASSWD);
    free(tmp);

    ubuf[0] = '\0';
    for (i = 0; auth->auth_methods[i]; i++) {
#ifdef DO_PERL
      if (auth->auth_methods[i]->type == PERMperl_auth) {
            cp = COPY(auth->auth_methods[i]->program);
            args = 0;
            Argify(cp, &args);
            perl_path = concat(args[0], (char *) 0);
            if ((perl_path != NULL) && (strlen(perl_path) > 0)) {
                if(!PerlLoaded) {
                    loadPerl();
                }
                PERLsetup(NULL, perl_path, "authenticate");
                free(perl_path);
                perlAuthInit();
          
                code = perlAuthenticate(ClientHost, ClientIpString, ServerHost, username, password, errorstr);
                if (code == NNTP_AUTH_OK_VAL) {
                    syslog(L_NOTICE, "%s user %s", ClientHost, username);
                    if (LLOGenable) {
                        fprintf(locallog, "%s user %s\n", ClientHost, username);
                        fflush(locallog);
                    }
              
                    /* save these values in case you need them later */
                    strcpy(ubuf, username);
                    break;
                } else {
                    syslog(L_NOTICE, "%s bad_auth", ClientHost);
                }            
            } else {
              syslog(L_ERROR, "No script specified in auth method.\n");
            }
      } else if (auth->auth_methods[i]->type == PERMauthprog) {
#endif	/* DO_PERL */    
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
        arg0 = concat(resdir, "/", args[0], (char *) 0);
	/* exec the authenticator */
	foo = ExecProg(arg0, args);
	if (foo) {
	    GetConnInfo(auth->auth_methods[i], buf);
	    snprintf(buf+strlen(buf), sizeof(buf) - strlen(buf) - 3,
                     "ClientAuthname: %s\r\n", username);
	    snprintf(buf+strlen(buf), sizeof(buf) - strlen(buf) - 3,
                     "ClientPassword: %s\r\n", password);
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
#ifdef DO_PERL
      }
#endif /* DO_PERL */
    }
    DISPOSE(resdir);
    if (ubuf[0])
	return(ubuf);
    return(0);
}

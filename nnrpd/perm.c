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
#include "inn/network.h"
#include "inn/innconf.h"
#include "innperl.h"
#include "nnrpd.h"

/* Needed on AIX 4.1 to get fd_set and friends. */
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif

#ifdef HAVE_OPENSSL
extern bool nnrpd_starttls_done;
#endif /* HAVE_OPENSSL */

/* Data types. */
typedef struct _CONFCHAIN {
    CONFFILE *f;
    struct _CONFCHAIN *parent;
} CONFCHAIN;

typedef struct _METHOD {
    char *name;
    char *program;
    int  type;          /* Type of auth (perl, python or external)/ */
    char *users;	/* Only used for auth_methods, not for res_methods. */
    char **extra_logs;
} METHOD;

typedef struct _AUTHGROUP {
    char *name;
    char *key;
#ifdef HAVE_OPENSSL
    int require_ssl;
#endif
    char *hosts;
    METHOD **res_methods;
    METHOD **auth_methods;
    char *default_user;
    char *default_domain;
    char *localaddress;
    char *access_script;
    int  access_type; /* Type of access (Perl or Python). */
    char *dynamic_script;
    int  dynamic_type; /* Type of dynamic authorization (Python only). */
} AUTHGROUP;

typedef struct _GROUP {
    char *name;
    struct _GROUP *above;
    AUTHGROUP *auth;
    ACCESSGROUP *access;
} GROUP;

/* Function declarations. */
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
static char *AuthenticateUser(AUTHGROUP*, char*, char*, int*, char*);

static void GrowArray(void*, void*);
static void PERMvectortoaccess(ACCESSGROUP *acc, const char *name, struct vector *acccess_vec) UNUSED;

/* Global variables. */
static AUTHGROUP **auth_realms;
static AUTHGROUP *success_auth;
static ACCESSGROUP **access_realms;

static char	*ConfigBit;
static int	ConfigBitsize;

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
#define PERMaddinjectiondate	38
#define PERMaddinjectionpostinghost	39
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
#define PERMpython_auth         57
#define PERMperl_access         58
#define PERMpython_access       59
#define PERMpython_dynamic      60
#ifdef HAVE_OPENSSL
#define PERMrequire_ssl                61
#define PERMMAX                        62
#else
#define PERMMAX			61
#endif

#define TEST_CONFIG(a, b) \
    { \
	int byte, offset; \
	offset = a % 8; \
	byte = (a - offset) / 8; \
	b = ((ConfigBit[byte] & (1 << offset)) != 0) ? true : false; \
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
    { PERMlbrace,               (char *) "{"                    },
    { PERMrbrace,               (char *) "}"                    },
    { PERMgroup,                (char *) "group"                },
    { PERMauth,                 (char *) "auth"                 },
    { PERMaccess,               (char *) "access"               },
    { PERMhost,                 (char *) "hosts:"               },
    { PERMauthprog,             (char *) "auth:"                },
    { PERMresolv,               (char *) "res"                  },
    { PERMresprog,              (char *) "res:"                 },
    { PERMdefuser,              (char *) "default:"             },
    { PERMdefdomain,            (char *) "default-domain:"      },
    { PERMusers,                (char *) "users:"               },
    { PERMnewsgroups,           (char *) "newsgroups:"          },
    { PERMread,                 (char *) "read:"                },
    { PERMpost,                 (char *) "post:"                },
    { PERMaccessrp,             (char *) "access:"              },
    { PERMalsolog,              (char *) "log:"                 },
    { PERMprogram,              (char *) "program:"             },
    { PERMinclude,              (char *) "include"              },
    { PERMkey,                  (char *) "key:"                 },
    { PERMlocaltime,            (char *) "localtime:"           },
    { PERMstrippath,            (char *) "strippath:"           },
    { PERMnnrpdperlfilter,      (char *) "perlfilter:"          },
    { PERMnnrpdpythonfilter,    (char *) "pythonfilter:"        },
    { PERMfromhost,             (char *) "fromhost:"            },
    { PERMpathhost,             (char *) "pathhost:"            },
    { PERMorganization,         (char *) "organization:"        },
    { PERMmoderatormailer,      (char *) "moderatormailer:"     },
    { PERMdomain,               (char *) "domain:"              },
    { PERMcomplaints,           (char *) "complaints:"          },
    { PERMspoolfirst,           (char *) "spoolfirst:"          },
    { PERMcheckincludedtext,    (char *) "checkincludedtext:"   },
    { PERMclienttimeout,        (char *) "clienttimeout:"       },
    { PERMlocalmaxartsize,      (char *) "localmaxartsize:"     },
    { PERMreadertrack,          (char *) "readertrack:"         },
    { PERMstrippostcc,          (char *) "strippostcc:"         },
    { PERMaddinjectiondate,     (char *) "addinjectiondate:"    },
    { PERMaddinjectionpostinghost, (char *) "addinjectionpostinghost:" },
    { PERMnnrpdposthost,        (char *) "nnrpdposthost:"       },
    { PERMnnrpdpostport,        (char *) "nnrpdpostport:"       },
    { PERMnnrpdoverstats,       (char *) "nnrpdoverstats:"      },
    { PERMbackoff_auth,         (char *) "backoff_auth:"        },
    { PERMbackoff_db,           (char *) "backoff_db:"          },
    { PERMbackoff_k,            (char *) "backoff_k:"           },
    { PERMbackoff_postfast,     (char *) "backoff_postfast:"    },
    { PERMbackoff_postslow,     (char *) "backoff_postslow:"    },
    { PERMbackoff_trigger,      (char *) "backoff_trigger:"     },
    { PERMnnrpdcheckart,        (char *) "nnrpdcheckart:"       },
    { PERMnnrpdauthsender,      (char *) "nnrpdauthsender:"     },
    { PERMvirtualhost,          (char *) "virtualhost:"         },
    { PERMnewsmaster,           (char *) "newsmaster:"          },
    { PERMlocaladdress,         (char *) "localaddress:"        },
    { PERMrejectwith,           (char *) "reject_with:"         },
    { PERMmaxbytespersecond,    (char *) "max_rate:"            },
    { PERMperl_auth,            (char *) "perl_auth:"           },
    { PERMpython_auth,          (char *) "python_auth:"         },
    { PERMperl_access,          (char *) "perl_access:"         },
    { PERMpython_access,        (char *) "python_access:"       },
    { PERMpython_dynamic,       (char *) "python_dynamic:"      },
#ifdef HAVE_OPENSSL
    { PERMrequire_ssl,          (char *) "require_ssl:"         },
#endif
    { 0,                        (char *) NULL                   }
};

/* Function definitions. */
static void
GrowArray(void *data, void *el)
{
    int i;
    void ***array = data;

    if (*array == NULL) {
	*array = xmalloc(2 * sizeof(void *));
	i = 0;
    } else {
	for (i = 0; (*array)[i]; i++)
	    ;
	*array = xrealloc(*array, (i + 2) * sizeof(void *));
    }
    (*array)[i++] = el;
    (*array)[i] = 0;
}

static METHOD *
copy_method(METHOD *orig)
{
    METHOD *ret;
    int i;

    ret = xmalloc(sizeof(METHOD));
    memset(ConfigBit, '\0', ConfigBitsize);

    ret->name = xstrdup(orig->name);
    if (orig->program != NULL) {
        ret->program = xstrdup(orig->program);
    } else {
        ret->program = NULL;
    }
    if (orig->users)
	ret->users = xstrdup(orig->users);
    else
	ret->users = 0;

    ret->extra_logs = 0;
    if (orig->extra_logs) {
	for (i = 0; orig->extra_logs[i]; i++)
	    GrowArray(&ret->extra_logs, xstrdup(orig->extra_logs[i]));
    }

    ret->type = orig->type;

    return(ret);
}

static void
free_method(METHOD *del)
{
    int j;

    if (del->extra_logs) {
	for (j = 0; del->extra_logs[j]; j++)
	    free(del->extra_logs[j]);
	free(del->extra_logs);
    }
    if (del->program)
	free(del->program);
    if (del->users)
	free(del->users);
    free(del->name);
    free(del);
}

static AUTHGROUP *
copy_authgroup(AUTHGROUP *orig)
{
    AUTHGROUP *ret;
    int i;

    if (!orig)
	return(0);
    ret = xmalloc(sizeof(AUTHGROUP));
    memset(ConfigBit, '\0', ConfigBitsize);

    if (orig->name)
	ret->name = xstrdup(orig->name);
    else
	ret->name = 0;

    if (orig->key)
	ret->key = xstrdup(orig->key);
    else
	ret->key = 0;

    if (orig->hosts)
	ret->hosts = xstrdup(orig->hosts);
    else
	ret->hosts = 0;

#ifdef HAVE_OPENSSL
    ret->require_ssl = orig->require_ssl;
#endif

    ret->res_methods = 0;
    if (orig->res_methods) {
	for (i = 0; orig->res_methods[i]; i++)
	    GrowArray(&ret->res_methods, copy_method(orig->res_methods[i]));;
    }

    ret->auth_methods = 0;
    if (orig->auth_methods) {
	for (i = 0; orig->auth_methods[i]; i++)
	    GrowArray(&ret->auth_methods, copy_method(orig->auth_methods[i]));
    }

    if (orig->default_user)
	ret->default_user = xstrdup(orig->default_user);
    else
	ret->default_user = 0;

    if (orig->default_domain)
	ret->default_domain = xstrdup(orig->default_domain);
    else
	ret->default_domain = 0;

    if (orig->localaddress)
	ret->localaddress = xstrdup(orig->localaddress);
    else
	ret->localaddress = 0;

    if (orig->access_script)
        ret->access_script = xstrdup(orig->access_script);
    else
        ret->access_script = 0;

    if (orig->access_type)
        ret->access_type = orig->access_type;
    else
        ret->access_type = 0;

    if (orig->dynamic_script)
        ret->dynamic_script = xstrdup(orig->dynamic_script);
    else
        ret->dynamic_script = 0;

    if (orig->dynamic_type)
        ret->dynamic_type = orig->dynamic_type;
    else
        ret->dynamic_type = 0;

    return(ret);
}

static ACCESSGROUP *
copy_accessgroup(ACCESSGROUP *orig)
{
    ACCESSGROUP *ret;

    if (!orig)
	return(0);
    ret = xmalloc(sizeof(ACCESSGROUP));
    memset(ConfigBit, '\0', ConfigBitsize);
    /* Copy all anyway, and update for local strings. */
    *ret = *orig;

    if (orig->name)
	ret->name = xstrdup(orig->name);
    if (orig->key)
	ret->key = xstrdup(orig->key);
    if (orig->read)
	ret->read = xstrdup(orig->read);
    if (orig->post)
	ret->post = xstrdup(orig->post);
    if (orig->users)
	ret->users = xstrdup(orig->users);
    if (orig->rejectwith)
	ret->rejectwith = xstrdup(orig->rejectwith);
    if (orig->fromhost)
	ret->fromhost = xstrdup(orig->fromhost);
    if (orig->pathhost)
	ret->pathhost = xstrdup(orig->pathhost);
    if (orig->organization)
	ret->organization = xstrdup(orig->organization);
    if (orig->moderatormailer)
	ret->moderatormailer = xstrdup(orig->moderatormailer);
    if (orig->domain)
	ret->domain = xstrdup(orig->domain);
    if (orig->complaints)
	ret->complaints = xstrdup(orig->complaints);
    if (orig->nnrpdposthost)
	ret->nnrpdposthost = xstrdup(orig->nnrpdposthost);
    if (orig->backoff_db)
	ret->backoff_db = xstrdup(orig->backoff_db);
    if (orig->newsmaster)
	ret->newsmaster = xstrdup(orig->newsmaster);
    return(ret);
}

static void
SetDefaultAuth(AUTHGROUP *curauth UNUSED)
{
#ifdef HAVE_OPENSSL
        curauth->require_ssl = false;
#endif
}

void
SetDefaultAccess(ACCESSGROUP *curaccess)
{
    curaccess->allownewnews = innconf->allownewnews;;
    curaccess->allowihave = false;
    curaccess->locpost = false;
    curaccess->allowapproved = false;
    curaccess->localtime = false;
    curaccess->strippath = false;
    curaccess->nnrpdperlfilter = true;
    curaccess->nnrpdpythonfilter = true;
    curaccess->fromhost = NULL;
    if (innconf->fromhost)
	curaccess->fromhost = xstrdup(innconf->fromhost);
    curaccess->pathhost = NULL;
    if (innconf->pathhost)
	curaccess->pathhost = xstrdup(innconf->pathhost);
    curaccess->organization = NULL;
    if (innconf->organization)
	curaccess->organization = xstrdup(innconf->organization);
    curaccess->moderatormailer = NULL;
    if (innconf->moderatormailer)
	curaccess->moderatormailer = xstrdup(innconf->moderatormailer);
    curaccess->domain = NULL;
    if (innconf->domain)
	curaccess->domain = xstrdup(innconf->domain);
    curaccess->complaints = NULL;
    if (innconf->complaints)
	curaccess->complaints = xstrdup(innconf->complaints);
    curaccess->spoolfirst = innconf->spoolfirst;
    curaccess->checkincludedtext = innconf->checkincludedtext;
    curaccess->clienttimeout = innconf->clienttimeout;
    curaccess->localmaxartsize = innconf->localmaxartsize;
    curaccess->readertrack = innconf->readertrack;
    curaccess->strippostcc = innconf->strippostcc;
    curaccess->addinjectiondate = innconf->addinjectiondate;
    curaccess->addinjectionpostinghost = innconf->addinjectionpostinghost;
    curaccess->nnrpdposthost = innconf->nnrpdposthost;
    curaccess->nnrpdpostport = innconf->nnrpdpostport;
    curaccess->nnrpdoverstats = innconf->nnrpdoverstats;
    curaccess->backoff_auth = innconf->backoffauth;
    curaccess->backoff_db = NULL;
    if (innconf->backoffdb && *innconf->backoffdb != '\0')
	curaccess->backoff_db = xstrdup(innconf->backoffdb);
    curaccess->backoff_k = innconf->backoffk;
    curaccess->backoff_postfast = innconf->backoffpostfast;
    curaccess->backoff_postslow = innconf->backoffpostslow;
    curaccess->backoff_trigger = innconf->backofftrigger;
    curaccess->nnrpdcheckart = innconf->nnrpdcheckart;
    curaccess->nnrpdauthsender = innconf->nnrpdauthsender;
    curaccess->virtualhost = false;
    curaccess->newsmaster = NULL;
    curaccess->maxbytespersecond = 0;
}

static void
free_authgroup(AUTHGROUP *del)
{
    int i;

    if (del->name)
	free(del->name);
    if (del->key)
	free(del->key);
    if (del->hosts)
	free(del->hosts);
    if (del->res_methods) {
	for (i = 0; del->res_methods[i]; i++)
	    free_method(del->res_methods[i]);
	free(del->res_methods);
    }
    if (del->auth_methods) {
	for (i = 0; del->auth_methods[i]; i++)
	    free_method(del->auth_methods[i]);
	free(del->auth_methods);
    }
    if (del->default_user)
	free(del->default_user);
    if (del->default_domain)
	free(del->default_domain);
    if (del->localaddress)
	free(del->localaddress);
    if (del->access_script)
        free(del->access_script);
    if (del->dynamic_script)
        free(del->dynamic_script);
    free(del);
}

static void
free_accessgroup(ACCESSGROUP *del)
{
    if (del->name)
	free(del->name);
    if (del->key)
	free(del->key);
    if (del->read)
	free(del->read);
    if (del->post)
	free(del->post);
    if (del->users)
	free(del->users);
    if (del->rejectwith)
	free(del->rejectwith);
    if (del->fromhost)
	free(del->fromhost);
    if (del->pathhost)
	free(del->pathhost);
    if (del->organization)
	free(del->organization);
    if (del->moderatormailer)
	free(del->moderatormailer);
    if (del->domain)
	free(del->domain);
    if (del->complaints)
	free(del->complaints);
    if (del->nnrpdposthost)
	free(del->nnrpdposthost);
    if (del->backoff_db)
	free(del->backoff_db);
    if (del->newsmaster)
	free(del->newsmaster);
    free(del);
}

static void
ReportError(CONFFILE *f, const char *err)
{
    syslog(L_ERROR, "%s syntax error in %s(%d), %s", Client.host,
      f->filename, f->lineno, err);
    Reply("%d NNTP server unavailable.  Try later!\r\n", NNTP_FAIL_TERMINATING);
    ExitWithStats(1, true);
}

static void
method_parse(METHOD *method, CONFFILE *f, CONFTOKEN *tok, int auth)
{
    int oldtype;

    oldtype = tok->type;
    tok = CONFgettoken(0, f);

    if (tok == NULL) {
	ReportError(f, "Expected value.");
    }

    switch (oldtype) {
      case PERMalsolog:
	GrowArray(&method->extra_logs, xstrdup(tok->name));
	break;
      case PERMusers:

	if (!auth) {
	    ReportError(f, "Unexpected users: directive in file.");
	} else if (method->users) {
	    ReportError(f, "Multiple users: directive in file.");
	}

	method->users = xstrdup(tok->name);
	break;
      case PERMprogram:
	if (method->program) {
	    ReportError(f, "Multiple program: directives in auth/res declaration.");
	}

	method->program = xstrdup(tok->name);
	break;
    }
}

static void
authdecl_parse(AUTHGROUP *curauth, CONFFILE *f, CONFTOKEN *tok)
{
    int oldtype;
#ifdef HAVE_OPENSSL
    int boolval;
#endif
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
	snprintf(buff, sizeof(buff), "Duplicated '%s' field in auth group.",
                 oldname);
	ReportError(f, buff);
    }

#ifdef HAVE_OPENSSL
    if (strcasecmp(tok->name, "on") == 0
        || strcasecmp(tok->name, "true") == 0
        || strcasecmp(tok->name, "yes") == 0)
	boolval = true;
    else if (strcasecmp(tok->name, "off") == 0
             || strcasecmp(tok->name, "false") == 0
             || strcasecmp(tok->name, "no") == 0)
	boolval = false;
    else
	boolval = -1;
#endif

    switch (oldtype) {
      case PERMkey:
	curauth->key = xstrdup(tok->name);
	SET_CONFIG(PERMkey);
	break;
#ifdef HAVE_OPENSSL
      case PERMrequire_ssl:
        if (boolval != -1)
            curauth->require_ssl = boolval;
        SET_CONFIG(PERMrequire_ssl);
        break;
#endif
      case PERMhost:
	curauth->hosts = xstrdup(tok->name);
	CompressList(curauth->hosts);
	SET_CONFIG(PERMhost);

        /* nnrpd.c downcases the names of connecting hosts.  We should
         * therefore also downcase the wildmat patterns to make sure there
         * aren't any surprises.  DNS is case-insensitive. */
        for (p = curauth->hosts; *p; p++)
            if (isupper((unsigned char) *p))
                *p = tolower((unsigned char) *p);

	break;
      case PERMdefdomain:
	curauth->default_domain = xstrdup(tok->name);
	SET_CONFIG(PERMdefdomain);
	break;
      case PERMdefuser:
	curauth->default_user = xstrdup(tok->name);
	SET_CONFIG(PERMdefuser);
	break;
      case PERMresolv:
      case PERMresprog:
        m = xcalloc(1, sizeof(METHOD));
	memset(ConfigBit, '\0', ConfigBitsize);
	GrowArray(&curauth->res_methods, m);

	if (oldtype == PERMresprog)
	    m->program = xstrdup(tok->name);
	else {
	    m->name = xstrdup(tok->name);
	    tok = CONFgettoken(PERMtoks, f);
	    if (tok == NULL || tok->type != PERMlbrace) {
		ReportError(f, "Expected '{' after 'res'.");
	    }

	    tok = CONFgettoken(PERMtoks, f);

	    while (tok != NULL && tok->type != PERMrbrace) {
		method_parse(m, f, tok, 0);
		tok = CONFgettoken(PERMtoks, f);
	    }

        if (m->program == NULL) {
            ReportError(f, "Missing 'program:' key.");
        }

	    if (tok == NULL) {
		ReportError(f, "Unexpected EOF.");
	    }
	}
	break;
      case PERMauth:
      case PERMperl_auth:
      case PERMpython_auth:
      case PERMauthprog:
        m = xcalloc(1, sizeof(METHOD));
	memset(ConfigBit, '\0', ConfigBitsize);
	GrowArray(&curauth->auth_methods, m);
	if (oldtype == PERMauthprog) {
            m->type = PERMauthprog;
	    m->program = xstrdup(tok->name);
	} else if (oldtype == PERMperl_auth) {
#ifdef DO_PERL
            m->type = PERMperl_auth;
	    m->program = xstrdup(tok->name);
#else
            ReportError(f, "perl_auth can not be used in readers.conf: INN not compiled with Perl support enabled.");
#endif
        } else if (oldtype == PERMpython_auth) {
#ifdef DO_PYTHON
            m->type = PERMpython_auth;
            m->program = xstrdup(tok->name);
#else
            ReportError(f, "python_auth can not be used in readers.conf: INN not compiled with Python support enabled.");
#endif
        } else {
	    m->name = xstrdup(tok->name);
	    tok = CONFgettoken(PERMtoks, f);

	    if (tok == NULL || tok->type != PERMlbrace) {
		ReportError(f, "Expected '{' after 'auth'.");
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
        curauth->access_script = xstrdup(tok->name);
        curauth->access_type = PERMperl_access;
#else
        ReportError(f, "perl_access can not be used in readers.conf: INN not compiled with Perl support enabled.");
#endif
        break;
      case PERMpython_access:
#ifdef DO_PYTHON
        curauth->access_script = xstrdup(tok->name);
        curauth->access_type = PERMpython_access;
#else
        ReportError(f, "python_access can not be used in readers.conf: INN not compiled with Python support enabled.");
#endif
        break;
      case PERMpython_dynamic:
#ifdef DO_PYTHON
        curauth->dynamic_script = xstrdup(tok->name);
        curauth->dynamic_type = PERMpython_dynamic;
#else
        ReportError(f, "python_dynamic can not be used in readers.conf: INN not compiled with Python support enabled.");
#endif
       break;
      case PERMlocaladdress:
	curauth->localaddress = xstrdup(tok->name);
	CompressList(curauth->localaddress);
	SET_CONFIG(PERMlocaladdress);
	break;
      default:
	snprintf(buff, sizeof(buff), "Unexpected token '%s'.", tok->name);
	ReportError(f, buff);
	break;
    }
}

static void
accessdecl_parse(ACCESSGROUP *curaccess, CONFFILE *f, CONFTOKEN *tok)
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
	snprintf(buff, sizeof(buff), "Duplicated '%s' field in access group.",
                 oldname);
	ReportError(f, buff);
    }
    if (strcasecmp(tok->name, "on") == 0
        || strcasecmp(tok->name, "true") == 0
        || strcasecmp(tok->name, "yes") == 0)
	boolval = true;
    else if (strcasecmp(tok->name, "off") == 0
             || strcasecmp(tok->name, "false") == 0
             || strcasecmp(tok->name, "no") == 0)
	boolval = false;
    else
	boolval = -1;

    switch (oldtype) {
      case PERMkey:
	curaccess->key = xstrdup(tok->name);
	SET_CONFIG(oldtype);
	break;
      case PERMusers:
	curaccess->users = xstrdup(tok->name);
	CompressList(curaccess->users);
	SET_CONFIG(oldtype);
	break;
      case PERMrejectwith:
	curaccess->rejectwith = xstrdup(tok->name);
	SET_CONFIG(oldtype);
	break;
      case PERMnewsgroups:
	TEST_CONFIG(PERMread, bit);
	if (bit) {
	    /* Syntax error...  can't set read: or post: _and_ use
	     * newsgroups:. */
	    ReportError(f, "read: newsgroups already set.");
	}
	TEST_CONFIG(PERMpost, bit);
	if (bit) {
	    /* Syntax error...  can't set read: or post: _and_ use
	     * newsgroups:. */
	    ReportError(f, "post: newsgroups already set.");
	}

	curaccess->read = xstrdup(tok->name);
	CompressList(curaccess->read);
	curaccess->post = xstrdup(tok->name);
	CompressList(curaccess->post);
	SET_CONFIG(oldtype);
	SET_CONFIG(PERMread);
	SET_CONFIG(PERMpost);
	break;
      case PERMread:
	curaccess->read = xstrdup(tok->name);
	CompressList(curaccess->read);
	SET_CONFIG(oldtype);
	break;
      case PERMpost:
	curaccess->post = xstrdup(tok->name);
	CompressList(curaccess->post);
	SET_CONFIG(oldtype);
	break;
      case PERMaccessrp:
	TEST_CONFIG(PERMread, bit);
	if (bit && strchr(tok->name, 'R') == NULL) {
	    free(curaccess->read);
	    curaccess->read = 0;
	    CLEAR_CONFIG(PERMread);
	}
	TEST_CONFIG(PERMpost, bit);
	if (bit && strchr(tok->name, 'P') == NULL) {
	    free(curaccess->post);
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
	    free(curaccess->fromhost);
	curaccess->fromhost = xstrdup(tok->name);
	SET_CONFIG(oldtype);
	break;
      case PERMpathhost:
	if (curaccess->pathhost)
	    free(curaccess->pathhost);
	curaccess->pathhost = xstrdup(tok->name);
	SET_CONFIG(oldtype);
	break;
      case PERMorganization:
	if (curaccess->organization)
	    free(curaccess->organization);
	curaccess->organization = xstrdup(tok->name);
	SET_CONFIG(oldtype);
	break;
      case PERMmoderatormailer:
	if (curaccess->moderatormailer)
	    free(curaccess->moderatormailer);
	curaccess->moderatormailer = xstrdup(tok->name);
	SET_CONFIG(oldtype);
	break;
      case PERMdomain:
	if (curaccess->domain)
	    free(curaccess->domain);
	curaccess->domain = xstrdup(tok->name);
	SET_CONFIG(oldtype);
	break;
      case PERMcomplaints:
	if (curaccess->complaints)
	    free(curaccess->complaints);
	curaccess->complaints = xstrdup(tok->name);
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
	curaccess->localmaxartsize = strtoul(tok->name, NULL, 10);
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
      case PERMaddinjectiondate:
	if (boolval != -1)
            curaccess->addinjectiondate = boolval;
	SET_CONFIG(oldtype);
	break;
      case PERMaddinjectionpostinghost:
	if (boolval != -1)
            curaccess->addinjectionpostinghost = boolval;
	SET_CONFIG(oldtype);
	break;
      case PERMnnrpdposthost:
	if (curaccess->nnrpdposthost)
	    free(curaccess->nnrpdposthost);
	curaccess->nnrpdposthost = xstrdup(tok->name);
	SET_CONFIG(oldtype);
	break;
      case PERMnnrpdpostport:
	curaccess->nnrpdpostport = strtoul(tok->name, NULL, 10);
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
	    free(curaccess->backoff_db);
	curaccess->backoff_db = xstrdup(tok->name);
	SET_CONFIG(oldtype);
	break;
      case PERMbackoff_k:
	curaccess->backoff_k = strtoul(tok->name, NULL, 10);
	SET_CONFIG(oldtype);
	break;
      case PERMbackoff_postfast:
	curaccess->backoff_postfast = strtoul(tok->name, NULL, 10);
	SET_CONFIG(oldtype);
	break;
      case PERMbackoff_postslow:
	curaccess->backoff_postslow = strtoul(tok->name, NULL, 10);
	SET_CONFIG(oldtype);
	break;
      case PERMbackoff_trigger:
	curaccess->backoff_trigger = strtoul(tok->name, NULL, 10);
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
	    free(curaccess->newsmaster);
	curaccess->newsmaster = xstrdup(tok->name);
	SET_CONFIG(oldtype);
	break;
      case PERMmaxbytespersecond:
	curaccess->maxbytespersecond = atol(tok->name);
	SET_CONFIG(oldtype);
	break;
      default:
	snprintf(buff, sizeof(buff), "Unexpected token '%s'.", tok->name);
	ReportError(f, buff);
	break;
    }
}

static void
PERMvectortoaccess(ACCESSGROUP *acc, const char *name,
                   struct vector *access_vec)
{
    CONFTOKEN	*tok	    = NULL;
    CONFFILE    *file;
    char        *str;
    unsigned int i;

    file = xcalloc(1, sizeof(CONFFILE));
    file->array = access_vec->strings;
    file->array_len = access_vec->count;

    memset(ConfigBit, '\0', ConfigBitsize);

    SetDefaultAccess(acc);
    str = xstrdup(name);
    acc->name = str;
    file->filename = str;

    for (i = 0; i <= access_vec->count; i++) {
      tok = CONFgettoken(PERMtoks, file);

      if (tok != NULL) {
        accessdecl_parse(acc, file, tok);
      }
    }

    /* No need to free str here; it is used by acc, and will be
     * free'd when the access block is free'd. */
    free(file);
    return;
}

static void
PERMreadfile(char *filename)
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

    cf		= xmalloc(sizeof(CONFCHAIN));
    if ((cf->f = CONFfopen(filename)) == NULL) {
	syslog(L_ERROR, "%s cannot open %s: %m", Client.host, filename);
	Reply("%d NNTP server unavailable.  Try later!\r\n", NNTP_FAIL_TERMINATING);
	ExitWithStats(1, true);
    }
    cf->parent	= 0;

    /* Are we editing an auth or access group? */

    inwhat	= 0;
    newgroup	= curgroup = 0;

    tok		= CONFgettoken(PERMtoks, cf->f);

    while (tok != NULL) {
	if (inwhat == 0) {
	    /* Top-level parser. */

	    switch (tok->type) {
		/* Include a child file. */

	      case PERMinclude:
		tok = CONFgettoken(0, cf->f);

		if (tok == NULL) {
		    ReportError(cf->f, "Expected filename after 'include'.");
		}

		hold		= xmalloc(sizeof(CONFCHAIN));
		hold->parent	= cf;

		/* Unless the filename's path is fully qualified, open it
		 * relative to <pathetc>. */
                path = concatpath(innconf->pathetc, tok->name);
                hold->f = CONFfopen(path);
                free(path);

		if (hold->f == NULL) {
		    ReportError(cf->f, "Could not open 'include' filename.");
		}

		cf = hold;
		goto again;
		break;

		/* Nested group declaration. */
	      case PERMgroup:
		tok = CONFgettoken(PERMtoks, cf->f);

		if (tok == NULL) {
		    ReportError(cf->f, "Unexpected EOF at group name.");
		}

		newgroup	= xmalloc(sizeof(GROUP));
		newgroup->above = curgroup;
		newgroup->name	= xstrdup(tok->name);
		memset(ConfigBit, '\0', ConfigBitsize);

		tok = CONFgettoken(PERMtoks, cf->f);

		if (tok == NULL || tok->type != PERMlbrace) {
		    ReportError(cf->f, "Expected '{' after group name.");
		}

		/* Nested group declaration. */
		if (curgroup) {
		    newgroup->auth = copy_authgroup(curgroup->auth);
		    newgroup->access = copy_accessgroup(curgroup->access);
		} else {
		    newgroup->auth = 0;
		    newgroup->access = 0;
		}

		curgroup = newgroup;
		break;

		/* Beginning of an auth or access group declaration. */
	      case PERMauth:
	      case PERMaccess:
		oldtype = tok->type;

		if ((tok = CONFgettoken(PERMtoks, cf->f)) == NULL) {
		    ReportError(cf->f, "Expected identifier.");
		}

		str = xstrdup(tok->name);

		tok = CONFgettoken(PERMtoks, cf->f);

		if (tok == NULL || tok->type != PERMlbrace) {
		    ReportError(cf->f, "Expected '{'.");
		}

		switch (oldtype) {
		  case PERMauth:
		    if (curgroup && curgroup->auth) {
			curauth = copy_authgroup(curgroup->auth);
                    } else {
			curauth = xcalloc(1, sizeof(AUTHGROUP));
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
			curaccess = xcalloc(1, sizeof(ACCESSGROUP));
			memset(ConfigBit, '\0', ConfigBitsize);
			SetDefaultAccess(curaccess);
		    }
		    curaccess->name = str;
		    inwhat = 2;
		    break;
		}

		break;

		/* End of a group declaration. */

	      case PERMrbrace:
		if (curgroup == NULL) {
		    ReportError(cf->f, "Unmatched '}'.");
		}

		newgroup = curgroup;
		curgroup = curgroup->above;
		if (newgroup->auth)
		    free_authgroup(newgroup->auth);
		if (newgroup->access)
		    free_accessgroup(newgroup->access);
		free(newgroup->name);
		free(newgroup);
		break;

		/* Stuff that belongs to an auth group. */
	      case PERMhost:
#ifdef HAVE_OPENSSL
              case PERMrequire_ssl:
#endif
	      case PERMauthprog:
	      case PERMresprog:
	      case PERMdefuser:
	      case PERMdefdomain:
		if (curgroup == NULL) {
		    curgroup = xcalloc(1, sizeof(GROUP));
		    memset(ConfigBit, '\0', ConfigBitsize);
		}
		if (curgroup->auth == NULL) {
		    curgroup->auth = xcalloc(1, sizeof(AUTHGROUP));
		    memset(ConfigBit, '\0', ConfigBitsize);
                    SetDefaultAuth(curgroup->auth);
		}
		authdecl_parse(curgroup->auth, cf->f, tok);
		break;

		/* Stuff that belongs to an access group. */
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
	      case PERMaddinjectiondate:
	      case PERMaddinjectionpostinghost:
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
		    curgroup = xcalloc(1, sizeof(GROUP));
		    memset(ConfigBit, '\0', ConfigBitsize);
		}
		if (!curgroup->access) {
		    curgroup->access = xcalloc(1, sizeof(ACCESSGROUP));
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
	    /* Auth group parser. */
	    if (tok->type == PERMrbrace) {
		inwhat = 0;

		if (curauth->name
		    && MatchHost(curauth->hosts, Client.host, Client.ip)) {
		    if (!MatchHost(curauth->localaddress, Client.serverhost, Client.serverip)) {
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
	    /* Access group parser. */
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
	    /* Should never happen. */
	    syslog(L_TRACE, "SHOULD NEVER HAPPEN!");
	}
again:


        /* Go back up the 'include' chain. */
	tok = CONFgettoken(PERMtoks, cf->f);

	while (tok == NULL && cf) {
	    hold = cf;
	    cf = hold->parent;
	    CONFfclose(hold->f);
	    free(hold);
	    if (cf) {
		tok = CONFgettoken(PERMtoks, cf->f);
	    }
	}
    }

    return;
}

void
PERMgetaccess(char *nnrpaccess)
{
    int i;
    char *uname;

    auth_realms	    = NULL;
    access_realms   = NULL;
    success_auth    = NULL;

#ifdef HAVE_SASL
    PERMcanauthenticate = true;
#else
    PERMcanauthenticate = false;
#endif
#ifdef HAVE_OPENSSL
    PERMcanauthenticatewithoutSSL = false;
#endif
    PERMgroupmadeinvalid = false;
    PERMcanpostgreeting = false;
    PERMcanread	    = PERMcanpost   = false;
    PERMreadlist    = PERMpostlist  = false;
    PERMaccessconf = NULL;

    if (ConfigBit == NULL) {
	if (PERMMAX % 8 == 0)
	    ConfigBitsize = PERMMAX/8;
	else
	    ConfigBitsize = (PERMMAX - (PERMMAX % 8))/8 + 1;
	ConfigBit = xcalloc(ConfigBitsize, 1);
    }
    PERMreadfile(nnrpaccess);

    strip_accessgroups();

    if (auth_realms == NULL) {
	/* No one can talk, empty file. */
	syslog(L_NOTICE, "%s no_permission", Client.host);
	Reply("%d You have no permission to talk.  Goodbye!\r\n",
	  NNTP_ERR_ACCESS);
	ExitWithStats(1, true);
    }

    /* auth_realms are all expected to match the user.
     * Be careful whether SSL is required, though. */
    for (i = 0; auth_realms[i]; i++) {
	if (auth_realms[i]->auth_methods != NULL) {
	    PERMcanauthenticate = true;
#ifdef HAVE_OPENSSL
            if (auth_realms[i]->require_ssl == false)
                PERMcanauthenticatewithoutSSL = true;
#endif
        }
        /* We assume that an access or dynamic script will allow
         * the user to post when authenticated, so that a 200 greeting
         * code can be sent. */
        if (auth_realms[i]->access_script != NULL
            || auth_realms[i]->dynamic_script != NULL)
            PERMcanpostgreeting = true;
    }
    uname = 0;
    while (!uname && i--) {
#ifdef HAVE_OPENSSL
        /* If SSL is required, check that the connection is encrypted. */
        if ((auth_realms[i]->require_ssl == true) && !nnrpd_starttls_done)
            continue;
#endif
	if ((uname = ResolveUser(auth_realms[i])) != NULL)
	    PERMauthorized = true;
	if (!uname && auth_realms[i]->default_user)
	    uname = xstrdup(auth_realms[i]->default_user);
    }
    if (uname) {
	strlcpy(PERMuser, uname, sizeof(PERMuser));
        free(uname);
	uname = strchr(PERMuser, '@');
	if (!uname && auth_realms[i]->default_domain) {
	    /* Append the default domain to the username. */
	    strlcat(PERMuser, "@", sizeof(PERMuser));
	    strlcat(PERMuser, auth_realms[i]->default_domain,
                    sizeof(PERMuser));
	}
	PERMneedauth = false;
	success_auth = auth_realms[i];
	syslog(L_TRACE, "%s res %s", Client.host, PERMuser);
    } else if (!PERMcanauthenticate) {
	/* Couldn't resolve the user. */
	syslog(L_NOTICE, "%s no_user", Client.host);
	Reply("%d Could not get your access name.  Goodbye!\r\n",
	  NNTP_ERR_ACCESS);
	ExitWithStats(1, true);
    } else {
	PERMneedauth = true;
    }
    /* Check maximum allowed permissions for any host that matches (for
     * the greeting string). */
    for (i = 0; access_realms[i]; i++) {
	if (!PERMcanread)
	    PERMcanread = (access_realms[i]->read != NULL);
	if (!PERMcanpost)
	    PERMcanpost = (access_realms[i]->post != NULL);
        if (!PERMcanpostgreeting)
            PERMcanpostgreeting = (access_realms[i]->post != NULL);
    }
    if (!i) {
	/* No applicable access groups.  Zeroing all these makes INN
	 * return permission denied to client. */
	PERMcanread = PERMcanpost = PERMneedauth = false;
    }
}

void
PERMlogin(char *uname, char *pass, int *code, char *errorstr)
{
    int i   = 0;
    char *runame;

    if (ConfigBit == NULL) {
	if (PERMMAX % 8 == 0)
	    ConfigBitsize = PERMMAX/8;
	else
	    ConfigBitsize = (PERMMAX - (PERMMAX % 8))/8 + 1;
	ConfigBit = xcalloc(ConfigBitsize, 1);
    }
    /* The check in CMDauthinfo uses the value of PERMneedauth to know if
     * authentication succeeded or not.  By default, authentication doesn't
     * succeed. */
    PERMneedauth = true;

    if(auth_realms != NULL) {
	for (i = 0; auth_realms[i]; i++) {
	    ;
	}
    }

    runame  = NULL;

    while (runame == NULL && i--)
	runame = AuthenticateUser(auth_realms[i], uname, pass, code, errorstr);
    if (runame) {
	strlcpy(PERMuser, runame, sizeof(PERMuser));
        free(runame);
	uname = strchr(PERMuser, '@');
	if (!uname && auth_realms[i]->default_domain) {
	    /* Append the default domain to the username. */
	    strlcat(PERMuser, "@", sizeof(PERMuser));
	    strlcat(PERMuser, auth_realms[i]->default_domain,
                    sizeof(PERMuser));
	}
	PERMneedauth = false;
	PERMauthorized = true;
        PERMcanauthenticate = false;
	success_auth = auth_realms[i];
    }
}

static int
MatchUser(char *pat, char *user)
{
    char *cp, **list;
    char *userlist[2];
    int ret;

    if (!pat)
	return(1);
    if (!user || !*user)
	return(0);
    cp = xstrdup(pat);
    list = 0;
    NGgetlist(&list, cp);
    userlist[0] = user;
    userlist[1] = 0;
    ret = PERMmatch(list, userlist);
    free(cp);
    free(list[0]);
    free(list);
    return(ret);
}

void
PERMgetpermissions(void)
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
	ConfigBit = xcalloc(ConfigBitsize, 1);
    }
    if (!success_auth) {
	/* If we haven't successfully authenticated, we can't do anything. */
	syslog(L_TRACE, "%s no_success_auth", Client.host);
	if (!noaccessconf)
	    noaccessconf = xmalloc(sizeof(ACCESSGROUP));
	PERMaccessconf = noaccessconf;
	SetDefaultAccess(PERMaccessconf);
	return;
#ifdef DO_PERL
    } else if ((success_auth->access_script != NULL) && (success_auth->access_type == PERMperl_access)) {
      char *uname;
      char *cpp, *script_path;
      char **args;
      struct vector *access_vec;

      i = 0;
      cpp = xstrdup(success_auth->access_script);
      args = 0;
      Argify(cpp, &args);
      script_path = concat(args[0], (char *) 0);
      if ((script_path != NULL) && (strlen(script_path) > 0)) {
        if(!PerlLoaded) {
          loadPerl();
        }
        PERLsetup(NULL, script_path, "access");
        free(script_path);

        uname = xstrdup(PERMuser);

        access_vec = vector_new();

        perlAccess(uname, access_vec);
        free(uname);

        access_realms[0] = xcalloc(1, sizeof(ACCESSGROUP));

        PERMvectortoaccess(access_realms[0], "perl_access-block", access_vec);

        vector_free(access_vec);
      } else {
        syslog(L_ERROR, "No script specified in perl_access method.\n");
        Reply("%d NNTP server unavailable.  Try later!\r\n", NNTP_FAIL_TERMINATING);
        ExitWithStats(1, true);
      }
      free(cpp);
      free(args);
#endif /* DO_PERL */
#ifdef DO_PYTHON
    } else if ((success_auth->access_script != NULL) && (success_auth->access_type == PERMpython_access)) {
        char *uname;
        char *cpp, *script_path;
        char **args;
        struct vector *access_vec;

        i = 0;
        cpp = xstrdup(success_auth->access_script);
        args = 0;
        Argify(cpp, &args);
        script_path = concat(args[0], (char *) 0);
        if ((script_path != NULL) && (strlen(script_path) > 0)) {
            uname = xstrdup(PERMuser);
            access_vec = vector_new();

            PY_access(script_path, access_vec, uname);
            free(script_path);
            free(uname);
            free(args);

            access_realms[0] = xcalloc(1, sizeof(ACCESSGROUP));
            memset(access_realms[0], 0, sizeof(ACCESSGROUP));

            PERMvectortoaccess(access_realms[0], "python_access-block", access_vec);

            vector_free(access_vec);
        } else {
            syslog(L_ERROR, "No script specified in python_access method.\n");
            Reply("%d NNTP server unavailable.  Try later!\r\n", NNTP_FAIL_TERMINATING);
            ExitWithStats(1, true);
        }
        free(cpp);
#endif /* DO_PYTHON */
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
          cp = xstrdup(access_realms[i]->users);
          list = 0;
          NGgetlist(&list, cp);
          if (PERMmatch(list, user)) {
            syslog(L_TRACE, "%s match_user %s %s", Client.host,
                   PERMuser, access_realms[i]->users);
            free(cp);
            free(list[0]);
            free(list);
            break;
          } else
            syslog(L_TRACE, "%s no_match_user %s %s", Client.host,
                   PERMuser, access_realms[i]->users);
          free(cp);
	  free(list[0]);
          free(list);
	}
      }
    }
    if (i >= 0) {
	/* Found the right access group. */
	if (access_realms[i]->rejectwith) {
	    syslog(L_ERROR, "%s rejected by rule (%s)",
		Client.host, access_realms[i]->rejectwith);
	    Reply("%d Permission denied:  %s\r\n",
		NNTP_FAIL_TERMINATING, access_realms[i]->rejectwith);
	    ExitWithStats(1, true);
	}
	if (access_realms[i]->read) {
	    cp = xstrdup(access_realms[i]->read);
	    PERMspecified = NGgetlist(&PERMreadlist, cp);
	    free(cp);
	    PERMcanread = true;
	} else {
	    syslog(L_TRACE, "%s no_read %s", Client.host, access_realms[i]->name);
	    PERMcanread = false;
	}
	if (access_realms[i]->post) {
	    cp = xstrdup(access_realms[i]->post);
	    NGgetlist(&PERMpostlist, cp);
	    free(cp);
	    PERMcanpost = true;
	} else {
	    syslog(L_TRACE, "%s no_post %s", Client.host, access_realms[i]->name);
	    PERMcanpost = false;
	}
	PERMaccessconf = access_realms[i];
	MaxBytesPerSecond = PERMaccessconf->maxbytespersecond;
	if (PERMaccessconf->virtualhost) {
	    if (PERMaccessconf->domain == NULL) {
		syslog(L_ERROR, "%s virtualhost needs domain parameter (%s).",
		    Client.host, PERMaccessconf->name);
		Reply("%d NNTP server unavailable.  Try later!\r\n", NNTP_FAIL_TERMINATING);
		ExitWithStats(1, true);
	    }
	    if (VirtualPath)
		free(VirtualPath);
	    if (strcasecmp(innconf->pathhost, PERMaccessconf->pathhost) == 0) {
		/* Use domain, if pathhost in access realm matches one in
		 * inn.conf to differentiate virtual host. */
		if (innconf->domain != NULL && strcasecmp(innconf->domain, PERMaccessconf->domain) == 0) {
		    syslog(L_ERROR, "%s domain parameter (%s) in readers.conf must be different from the one in inn.conf.",
			Client.host, PERMaccessconf->name);
		    Reply("%d NNTP server unavailable.  Try later!\r\n", NNTP_FAIL_TERMINATING);
		    ExitWithStats(1, true);
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
	    noaccessconf = xmalloc(sizeof(ACCESSGROUP));
	PERMaccessconf = noaccessconf;
	SetDefaultAccess(PERMaccessconf);
	syslog(L_TRACE, "%s no_access_realm", Client.host);
    }
    /* Check if dynamic access control is enabled; if so, init it. */
#ifdef DO_PYTHON
    if ((success_auth->dynamic_type == PERMpython_dynamic) && success_auth->dynamic_script) {
      PY_dynamic_init(success_auth->dynamic_script);
    }
#endif /* DO_PYTHON */
}

/* Strip blanks out of a string. */
static void
CompressList(char *list)
{
    char *cpto;
    bool inword = false;

    for (cpto = list; *list; ) {
	if (strchr("\n \t,", *list) != NULL) {
	    list++;
	    if(inword) {
		*cpto++ = ',';
		inword = false;
	    }
	} else {
	    *cpto++ = *list++;
	    inword = true;
	}
    }
    *cpto = '\0';
}

static bool
MatchHost(char *hostlist, char *host, char *ip)
{
    int i;
    char *cp, *pat, *mask;
    char **list = NULL;
    bool match = false;

    /* If no hostlist are specified, by default they match. */
    if (hostlist == NULL)
        return true;

    cp = xstrdup(hostlist);
    NGgetlist(&list, cp);

    /* Start searching from the end of the list.  The default is no access. */
    for (i = 0; list[i] != NULL; i++)
        ;
    while (!match && i-- > 0) {
        pat = list[i];
        match = uwildmat(host, pat);
        if (!match && ip != NULL && *ip != '\0') {
            match = uwildmat(ip, pat);
            if (match)
                break;
            mask = strchr(pat, '/');
            if (mask == NULL)
                continue;
            *mask = '\0';
            mask++;
            match = network_addr_match(ip, pat, mask);
        }
    }
    free(list[0]);
    free(list);
    free(cp);
    return match;
}

static void
add_authgroup(AUTHGROUP *group)
{
    int i;

    if (auth_realms == NULL) {
	i = 0;
	auth_realms = xmalloc(2 * sizeof(AUTHGROUP *));
    } else {
	for (i = 0; auth_realms[i]; i++)
	    ;
        auth_realms = xrealloc(auth_realms, (i + 2) * sizeof(AUTHGROUP *));
    }
    auth_realms[i] = group;
    auth_realms[i+1] = 0;
}

static void
add_accessgroup(ACCESSGROUP *group)
{
    int i;

    if (access_realms == NULL) {
	i = 0;
	access_realms = xmalloc(2 * sizeof(ACCESSGROUP *));
    } else {
	for (i = 0; access_realms[i]; i++)
	    ;
        access_realms = xrealloc(access_realms, (i + 2) * sizeof(ACCESSGROUP *));
    }
    access_realms[i] = group;
    access_realms[i+1] = 0;
}


/*
**  Clean out access groups that don't apply to any of our auth groups.
*/
static void
strip_accessgroups(void)
{
    int i, j;

    /* Flag the access group as used or not. */
    if(access_realms != NULL) {
	for (j = 0; access_realms[j] != NULL; j++) {
	    access_realms[j]->used = 0;
	}
    } else {
	syslog(L_TRACE, "No access realms to check!");
    }

    /* If there are auth realms to check... */
    if(auth_realms != NULL) {
	/* ... Then for each auth realm... */
	for (i = 0; auth_realms[i] != NULL; i++) {
	    /* ... for each access realm... */
	    for (j = 0; access_realms[j] != NULL; j++) {
		/* If the access realm isn't already in use... */
		if (! access_realms[j]->used) {
		    /* Check to see if both the access_realm key and
                     * auth_realm key are NULL... */
		    if (!access_realms[j]->key && !auth_realms[i]->key) {
			/* If so, mark the realm in use and continue on... */
			access_realms[j]->used = 1;
		    } else {
			/* If not, check to see if both the access_realm and
			   auth_realm are NOT _both_ NULL, and see if they are
			   equal... */
			if (access_realms[j]->key && auth_realms[i]->key &&
			    strcmp(access_realms[j]->key, auth_realms[i]->key) == 0) {
			    /* And if so, mark the realm in use. */
			    access_realms[j]->used = 1;
			}
		    }
		}
	    }
	}
    } else {
	syslog(L_TRACE, "No auth realms to check!");
    }

    /* Strip out unused access groups. */
    i = j = 0;

    while (access_realms[i] != NULL) {
	if (access_realms[i]->used)
	    access_realms[j++] = access_realms[i];
	else
	    syslog(L_TRACE, "%s removing irrelevant access group %s",
		   Client.host, access_realms[i]->name);
	i++;
    }
    access_realms[j] = 0;
}


/*
**  Execute a series of resolvers to get the remote username.  If one is
**  found, return it in newly allocated space; otherwise, return NULL.
*/
static char *
ResolveUser(AUTHGROUP *auth)
{
    int i, j;
    char *command;
    char *user = NULL;
    char *resdir;
    char *tmp;

    if (auth->res_methods == NULL)
        return NULL;

#ifdef HAVE_OPENSSL
    /* If SSL is required, check that the connection is encrypted. */
    if ((auth->require_ssl == true) && !nnrpd_starttls_done)
        return NULL;
#endif

    tmp = concatpath(innconf->pathbin, INN_PATH_AUTHDIR);
    resdir = concatpath(tmp, INN_PATH_AUTHDIR_NOPASS);
    free(tmp);

    for (i = 0; auth->res_methods[i]; i++) {
        command = auth->res_methods[i]->program;
	syslog(L_TRACE, "%s res starting resolver %s", Client.host, command);
	if (auth->res_methods[i]->extra_logs) {
	    for (j = 0; auth->res_methods[i]->extra_logs[j]; j++)
		syslog(L_NOTICE, "%s res also-log: %s", Client.host,
                       auth->res_methods[i]->extra_logs[j]);
	}
        user = auth_external(&Client, command, resdir, NULL, NULL);
        if (user == NULL)
            syslog(L_TRACE, "%s res resolver failed", Client.host);
        else {
            syslog(L_TRACE, "%s res resolver successful, user %s",
                   Client.host, user);
            break;
        }
    }
    free(resdir);
    return user;
}


/*
**  Execute a series of authenticators to get the remote username.  If one is
**  found, return it in newly allocated space; otherwise, return NULL.  Also
**  handles running the Perl and Python authentication functions.
*/
static char *
AuthenticateUser(AUTHGROUP *auth, char *username, char *password,
                 int *code UNUSED, char *errorstr UNUSED)
{
    int i, j;
    char *command;
    char *user = NULL;
    char *resdir;
    char *tmp;

    if (auth->auth_methods == NULL)
        return NULL;

#ifdef HAVE_OPENSSL
    /* If SSL is required, check that the connection is encrypted. */
    if ((auth->require_ssl == true) && !nnrpd_starttls_done)
        return NULL;
#endif

    tmp = concatpath(innconf->pathbin, INN_PATH_AUTHDIR);
    resdir = concatpath(tmp, INN_PATH_AUTHDIR_PASSWD);
    free(tmp);

    for (i = 0; auth->auth_methods[i]; i++) {
        if (auth->auth_methods[i]->type == PERMperl_auth) {
#ifdef DO_PERL
            char *script_path, *cp;
            char **args;
            char newUser[BIG_BUFFER];

            cp = xstrdup(auth->auth_methods[i]->program);
            args = 0;
            Argify(cp, &args);
            script_path = concat(args[0], (char *) 0);
            if ((script_path != NULL) && (strlen(script_path) > 0)) {
                if (!PerlLoaded)
                    loadPerl();
                PERLsetup(NULL, script_path, "authenticate");
                free(script_path);
                perlAuthInit();

                newUser[0] = '\0';
                perlAuthenticate(username, password, code, errorstr, newUser);
                if (*code == NNTP_OK_AUTHINFO) {
                    if (newUser[0] != '\0')
                        user = xstrdup(newUser);
                    else
                        user = xstrdup(username);
                    syslog(L_NOTICE, "%s user %s", Client.host, user);
                    if (LLOGenable) {
                        fprintf(locallog, "%s user %s\n", Client.host, user);
                        fflush(locallog);
                    }
                    break;
                } else {
                    syslog(L_NOTICE, "%s bad_auth", Client.host);
                }
            } else {
                syslog(L_ERROR, "no script specified in auth method");
            }
#endif	/* DO_PERL */
        } else if (auth->auth_methods[i]->type == PERMpython_auth) {
#ifdef DO_PYTHON
            char *script_path, *cp;
            char **args;
            char newUser[BIG_BUFFER];

            cp = xstrdup(auth->auth_methods[i]->program);
            args = 0;
            Argify(cp, &args);
            script_path = concat(args[0], (char *) 0);
            if ((script_path != NULL) && (strlen(script_path) > 0)) {
                newUser[0] = '\0';
                PY_authenticate(script_path, username, password,
                                code, errorstr, newUser);
                free(script_path);
                if (*code == NNTP_OK_AUTHINFO) {
                    if (newUser[0] != '\0')
                        user = xstrdup(newUser);
                    else
                        user = xstrdup(username);
                    syslog(L_NOTICE, "%s user %s", Client.host, user);
                    if (LLOGenable) {
                        fprintf(locallog, "%s user %s\n", Client.host, user);
                        fflush(locallog);
                    }
                    break;
                } else if (*code < 0) {
                    syslog(L_NOTICE, "PY_authenticate(): authentication skipped due to no Python authentication method defined.");
                } else {
                    syslog(L_NOTICE, "%s bad_auth", Client.host);
                }
            } else {
                syslog(L_ERROR, "no script specified in auth method");
            }
#endif /* DO_PYTHON */
      } else {
            if (auth->auth_methods[i]->users &&
                !MatchUser(auth->auth_methods[i]->users, username))
                continue;

            command = auth->auth_methods[i]->program;
            syslog(L_TRACE, "%s res starting authenticator %s", Client.host,
                   command);
            if (auth->auth_methods[i]->extra_logs) {
                for (j = 0; auth->auth_methods[i]->extra_logs[j]; j++)
                    syslog(L_NOTICE, "%s auth also-log: %s", Client.host,
                           auth->auth_methods[i]->extra_logs[j]);
            }
            user = auth_external(&Client, command, resdir, username, password);
            if (user == NULL)
                syslog(L_TRACE, "%s auth authenticator failed", Client.host);
            else {
                syslog(L_TRACE, "%s auth authenticator successful, user %s",
                       Client.host, user);
                break;
            }
        }
    }
    free(resdir);
    return user;
}

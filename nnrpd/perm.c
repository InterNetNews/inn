/*  $Id$
**
**  How to figure out where a user comes from, and what that user can do once
**  we know who sie is.
*/
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "configdata.h"
#include "clibrary.h"
#include "nnrpd.h"
#include "conffile.h"
#include <netdb.h>
#include <sys/wait.h>
#include <signal.h>

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

typedef struct _ACCESSGROUP {
    char *name;
    char *key;
    char *read;
    char *post;
    char *users;
    int newnews;
    int locpost;
    int used;
} ACCESSGROUP;

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

    if (orig->name)
	ret->name = COPY(orig->name);
    else
	ret->name = 0;

    if (orig->key)
	ret->key = COPY(orig->key);
    else
	ret->key = 0;

    if (orig->read)
	ret->read = COPY(orig->read);
    else
	ret->read = 0;

    if (orig->post)
	ret->post = COPY(orig->post);
    else
	ret->post = 0;

    if (orig->users)
	ret->users = COPY(orig->users);
    else
	ret->users = 0;

    return(ret);
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
    DISPOSE(del);
}

static void ReportError(CONFFILE *f, char *err)
{
    syslog(L_NOTICE, "%s syntax error in %s(%d), %s", ClientHost,
      f->filename, f->lineno, err);
    Reply("%d NNTP server unavailable. Try later.\r\n", NNTP_TEMPERR_VAL);
    ExitWithStats(1);
}

#define PERMlbrace	1
#define PERMrbrace	2
#define PERMgroup	3
#define PERMauth	4
#define PERMaccess	5
#define PERMhost	6
#define PERMauthprog	7
#define PERMresolv	8
#define PERMresprog	9
#define PERMdefuser	10
#define PERMdefdomain	11
#define PERMusers	12
#define PERMnewsgroups	13
#define PERMread	14
#define PERMpost	15
#define PERMaccessrp	16
#define PERMheader	17
#define PERMalsolog	18
#define PERMprogram	19
#define PERMinclude	20
#define PERMkey		21

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
  { 0, 0 }
};

static void method_parse(METHOD *method, CONFFILE *f, CONFTOKEN *tok, int auth)
{
    int oldtype;
    int i;

    oldtype = tok->type;
    tok = CONFgettoken(0, f);
    if (!tok)
	ReportError(f, "Expected value.");
    switch (oldtype) {
      case PERMheader:
	GrowArray((void***) &method->extra_headers, (void*) COPY(tok->name));
	break;
      case PERMalsolog:
	GrowArray((void***) &method->extra_logs, (void*) COPY(tok->name));
	break;
      case PERMusers:
	if (!auth)
	    ReportError(f, "Unexpected users: directive in file.");
	else if (method->users)
	    ReportError(f, "Multiple users: directive in file.");
	method->users = COPY(tok->name);
	break;
      case PERMprogram:
	if (method->program)
	    ReportError(f, "Multiple program: directives in auth/res decl.");
	method->program = COPY(tok->name);
	break;
    }
}

static void authdecl_parse(AUTHGROUP *curauth, CONFFILE *f, CONFTOKEN *tok)
{
    int oldtype;
    int i;
    METHOD *m;

    oldtype = tok->type;
    tok = CONFgettoken(PERMtoks, f);
    if (!tok)
	ReportError(f, "Expected value.");
    switch (oldtype) {
      case PERMkey:
	if (curauth->key)
	    ReportError(f, "Duplicated 'key:' field in authgroup.");
	curauth->key = COPY(tok->name);
	break;
      case PERMhost:
	if (curauth->hosts)
	    ReportError(f, "Duplicated 'hosts:' line in authgroup.");
	curauth->hosts = COPY(tok->name);
	CompressList(curauth->hosts);
	break;
      case PERMdefdomain:
	if (curauth->default_domain)
	    ReportError(f, "Duplicated 'default-domain:' line in authgroup.");
	curauth->default_domain = COPY(tok->name);
	break;
      case PERMdefuser:
	if (curauth->default_user)
	    ReportError(f, "Duplicated 'default:' user in authgroup.");
	curauth->default_user = COPY(tok->name);
	break;
      case PERMresolv:
      case PERMresprog:
	m = NEW(METHOD, 1);
	(void) memset((POINTER) m, 0, sizeof(METHOD));
	GrowArray((void***) &curauth->res_methods, (void*) m);

	if (oldtype == PERMresprog)
	    m->program = COPY(tok->name);
	else {
	    m->name = COPY(tok->name);
	    tok = CONFgettoken(PERMtoks, f);
	    if (!tok || tok->type != PERMlbrace)
		ReportError(f, "Expected '{' after 'res'");
	    tok = CONFgettoken(PERMtoks, f);
	    while (tok && tok->type != PERMrbrace) {
		method_parse(m, f, tok, 0);
		tok = CONFgettoken(PERMtoks, f);
	    }
	    if (!tok)
		ReportError(f, "Unexpected EOF.");
	}
	break;
      case PERMauth:
      case PERMauthprog:
	m = NEW(METHOD, 1);
	(void) memset((POINTER) m, 0, sizeof(METHOD));
	GrowArray((void***) &curauth->auth_methods, (void*) m);
	if (oldtype == PERMauthprog)
	    m->program = COPY(tok->name);
	else {
	    m->name = COPY(tok->name);
	    tok = CONFgettoken(PERMtoks, f);
	    if (!tok || tok->type != PERMlbrace)
		ReportError(f, "Expected '{' after 'auth'");
	    tok = CONFgettoken(PERMtoks, f);
	    while (tok && tok->type != PERMrbrace) {
		method_parse(m, f, tok, 1);
		tok = CONFgettoken(PERMtoks, f);
	    }
	    if (!tok)
		ReportError(f, "Unexpected EOF.");
	}
	break;
      default:
	ReportError(f, "Unexpected token.");
	break;
    }
}

static void accessdecl_parse(ACCESSGROUP *curaccess, CONFFILE *f, CONFTOKEN *tok)
{
    int oldtype;

    oldtype = tok->type;
    tok = CONFgettoken(0, f);
    if (!tok)
	ReportError(f, "Expected value.");
    switch (oldtype) {
      case PERMkey:
	if (curaccess->key)
	    ReportError(f, "Duplicated 'key:' field in accessgroup.");
	curaccess->key = COPY(tok->name);
	break;
      case PERMusers:
	if (curaccess->users)
	    ReportError(f, "Duplicated 'users:' field in accessgroup.");
	curaccess->users = COPY(tok->name);
	CompressList(curaccess->users);
	break;
      case PERMnewsgroups:
	if (curaccess->read || curaccess->post)
	    /* syntax error..  can't set read: or post: _and_ use
	     * newsgroups: */
	    ReportError(f, "read: or post: newsgroups already set.");
	curaccess->read = COPY(tok->name);
	CompressList(curaccess->read);
	curaccess->post = COPY(tok->name);
	CompressList(curaccess->post);
	break;
      case PERMread:
	if (curaccess->read)
	    ReportError(f, "read: newsgroups already set.");
	curaccess->read = COPY(tok->name);
	CompressList(curaccess->read);
	break;
      case PERMpost:
	if (curaccess->post)
	    ReportError(f, "post: newsgroups already set.");
	curaccess->post = COPY(tok->name);
	CompressList(curaccess->post);
	break;
      case PERMaccessrp:
	if (curaccess->read && strchr(tok->name, 'R') == NULL) {
	    DISPOSE(curaccess->read);
	    curaccess->read = 0;
	}
	if (curaccess->post && strchr(tok->name, 'P') == NULL) {
	    DISPOSE(curaccess->post);
	    curaccess->post = 0;
	}
	curaccess->newnews = (strchr(tok->name, 'N') != NULL);
	curaccess->locpost = (strchr(tok->name, 'L') != NULL);
	break;
      default:
	ReportError(f, "Unexpected token.");
	break;
    }
}

static void PERMreadfile(char *filename)
{
    CONFCHAIN *cf, *hold;
    CONFTOKEN *tok;
    int inwhat;
    GROUP *curgroup, *newgroup;
    ACCESSGROUP *curaccess;
    AUTHGROUP *curauth;
    int oldtype;
    char *str;
    int i;
    char errorstr[SMBUF];

    cf = NEW(CONFCHAIN, 1);
    cf->f = CONFfopen(filename);
    cf->parent = 0;
    /* are we editing an AUTH or ACCESS group? */
    inwhat = 0;
    newgroup = curgroup = 0;
    tok = CONFgettoken(PERMtoks, cf->f);
    while (tok != NULL) {
	if (inwhat == 0) {
	    /* top-level parser */
	    switch (tok->type) {
		/* include a child file */
	      case PERMinclude:
		tok = CONFgettoken(0, cf->f);
		if (!tok)
		    ReportError(cf->f, "Expected filename after 'include'.");
		hold = NEW(CONFCHAIN, 1);
		hold->parent = cf;
		/* unless the filename's path is fully qualified, open it
		 * relative to /news/etc */
		if (*tok->name == '/')
		    hold->f = CONFfopen(tok->name);
		else
		    hold->f = CONFfopen(cpcatpath(innconf->pathetc, tok->name));
		if (!hold->f)
		    ReportError(cf->f, "Couldn't open 'include' filename.");
		cf = hold;
		goto again;
		break;

		/* nested group declaration. */
	      case PERMgroup:
		tok = CONFgettoken(PERMtoks, cf->f);
		if (!tok)
		    ReportError(cf->f, "Unexpected EOF at group name");
		newgroup = NEW(GROUP, 1);
		newgroup->above = curgroup;
		newgroup->name = COPY(tok->name);
		tok = CONFgettoken(PERMtoks, cf->f);
		if (!tok || tok->type != PERMlbrace)
		    ReportError(cf->f, "Expected '{' after group name");
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
		if ((tok = CONFgettoken(PERMtoks, cf->f)) == 0)
		    ReportError(cf->f, "Expected identifier.");
		str = COPY(tok->name);
		tok = CONFgettoken(PERMtoks, cf->f);
		if (tok->type != PERMlbrace)
		    ReportError(cf->f, "Expected '{'");
		switch (oldtype) {
		  case PERMauth:
		    if (curgroup && curgroup->auth)
			curauth = copy_authgroup(curgroup->auth);
		    else {
			curauth = NEW(AUTHGROUP, 1);
			memset((POINTER) curauth, 0, sizeof(AUTHGROUP));
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
		    }
		    curaccess->name = str;
		    inwhat = 2;
		    break;
		}
		break;

		/* end of a group declaration */
	      case PERMrbrace:
		if (!curgroup)
		    ReportError(cf->f, "Unmatched '}'");
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
		if (!curgroup) {
		    curgroup = NEW(GROUP, 1);
		    memset((POINTER) curgroup, 0, sizeof(GROUP));
		}
		if (!curgroup->auth) {
		    curgroup->auth = NEW(AUTHGROUP, 1);
		    (void)memset((POINTER)curgroup->auth, 0, sizeof(AUTHGROUP));
		}
		authdecl_parse(curgroup->auth, cf->f, tok);
		break;

		/* stuff that belongs in an accessgroup */
	      case PERMusers:
	      case PERMnewsgroups:
	      case PERMread:
	      case PERMpost:
	      case PERMaccessrp:
		if (!curgroup) {
		    curgroup = NEW(GROUP, 1);
		    memset((POINTER) curgroup, 0, sizeof(GROUP));
		}
		if (!curgroup->access) {
		    curgroup->access = NEW(ACCESSGROUP, 1);
		    (void)memset((POINTER)curgroup->access, 0,
		      sizeof(ACCESSGROUP));
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

		if (curauth->name && MatchClient(curauth))
		    add_authgroup(curauth);
		else if (curauth->name)
		    free_authgroup(curauth);
		goto again;
	    }
	    authdecl_parse(curauth, cf->f, tok);
	} else if (inwhat == 2) {
	    /* accessgroup parser */
	    if (tok->type == PERMrbrace) {
		inwhat = 0;
		if (curaccess->name)
		    add_accessgroup(curaccess);
		goto again;
	    }
	    accessdecl_parse(curaccess, cf->f, tok);
	} else
	    /* should never happen */
	    ;
again:
	/* go back up the 'include' chain. */
	tok = CONFgettoken(PERMtoks, cf->f);
	while (!tok && cf) {
	    hold = cf;
	    cf = hold->parent;
	    CONFfclose(hold->f);
	    DISPOSE(hold);
	    if (cf)
		tok = CONFgettoken(PERMtoks, cf->f);
	}
    }
    return;
}

void PERMgetaccess(void)
{
    int i;
    char *uname;
    int canauthenticate;

    auth_realms = 0;
    access_realms = 0;
    success_auth = 0;
    PERMcanread = PERMcanpost = PERMlocpost = 0;
    PERMreadlist = PERMpostlist = 0;
    PERMreadfile(cpcatpath(innconf->pathetc, _PATH_NNRPACCESS));
    strip_accessgroups();
    if (!auth_realms) {
	/* no one can talk, empty file */
	syslog(L_NOTICE, "%s no_permission", ClientHost);
	Printf("%d You have no permission to talk.  Goodbye.\r\n",
	  NNTP_ACCESS_VAL);
	ExitWithStats(1);
    }
    /* auth_realms are all expected to match the user. */
    canauthenticate = 0;
    for (i = 0; auth_realms[i]; i++)
	if (auth_realms[i]->auth_methods)
	    canauthenticate = 1;
    uname = 0;
    while (!uname && i--) {
	uname = ResolveUser(auth_realms[i]);
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
	PERMneedauth = 0;
	success_auth = auth_realms[i];
	syslog(L_TRACE, "%s res %s", ClientHost, PERMuser);
    } else if (!canauthenticate) {
	/* couldn't resolve the user. */
	syslog(L_NOTICE, "%s no_user", ClientHost);
	Printf("%d Could not get your access name.  Goodbye.\r\n",
	  NNTP_ACCESS_VAL);
	ExitWithStats(1);
    } else
	PERMneedauth = 1;
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
	PERMcanread = PERMcanpost = PERMneedauth = 0;
    }
}

void PERMlogin(char *uname, char *pass)
{
    int i;
    char *runame;

    /* The check in CMDauthinfo uses the value of PERMneedauth to know if
     * authentication succeeded or not.  By default, authentication doesn't
     * succeed. */
    PERMneedauth = 1;

    for (i = 0; auth_realms[i]; i++)
	;
    runame = 0;
    while (!runame && i--)
	runame = AuthenticateUser(auth_realms[i], uname, pass);
    if (runame) {
	strcpy(PERMuser, runame);
	uname = strchr(PERMuser, '@');
	if (!uname && auth_realms[i]->default_domain) {
	    /* append the default domain to the username */
	    strcat(PERMuser, "@");
	    strcat(PERMuser, auth_realms[i]->default_domain);
	}
	PERMneedauth = 0;
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

    if (!success_auth) {
	/* if we haven't successfully authenticated, we can't do anything. */
	syslog(L_TRACE, "%s no_success_auth", ClientHost);
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
	    PERMcanread = 1;
	} else {
	    syslog(L_TRACE, "%s no_read %s", ClientHost, access_realms[i]->name);
	    PERMcanread = 0;
	}
	if (access_realms[i]->post) {
	    cp = COPY(access_realms[i]->post);
	    NGgetlist(&PERMpostlist, cp);
	    PERMcanpost = 1;
	} else {
	    syslog(L_TRACE, "%s no_post %s", ClientHost, access_realms[i]->name);
	    PERMcanpost = 0;
	}
	PERMnewnews = access_realms[i]->newnews;
	PERMlocpost = access_realms[i]->locpost;
    } else
	syslog(L_TRACE, "%s no_access_realm", ClientHost);
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
    char **list;
    int ret;
    char *cp;
    int iter;
    char *pat, *p;

    if (!group->hosts)
	return(1);
    list = 0;
    cp = COPY(group->hosts);
    NGgetlist(&list, cp);
    /* default is no access */
    for (iter = 0; list[iter]; iter++)
	;
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
		if (ia.s_addr != (unsigned int)INADDR_NONE &&
		    net.s_addr != (unsigned int)INADDR_NONE) {
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

    if (!auth_realms) {
	i = 0;
	auth_realms = NEW(AUTHGROUP*, 2);
    } else {
	for (i = 0; auth_realms[i]; i++)
	    ;
	auth_realms = RENEW(auth_realms, AUTHGROUP*, i+2);
    }
    auth_realms[i] = group;
    auth_realms[i+1] = 0;
}

static void add_accessgroup(ACCESSGROUP *group)
{
    int i;

    if (!access_realms) {
	i = 0;
	access_realms = NEW(ACCESSGROUP*, 2);
    } else {
	for (i = 0; access_realms[i]; i++)
	    ;
	access_realms = RENEW(access_realms, ACCESSGROUP*, i+2);
    }
    access_realms[i] = group;
    access_realms[i+1] = 0;
}

/* clean out access groups that don't apply to any of our auth groups. */
static void strip_accessgroups()
{
    int i, j;

    /* flag the access group as used or not */
    for (j = 0; access_realms[j]; j++)
	access_realms[j]->used = 0;
    for (i = 0; auth_realms[i]; i++) {
	for (j = 0; access_realms[j]; j++)
	    if (! access_realms[j]->used) {
		if (!access_realms[j]->key && !auth_realms[i]->key)
		    access_realms[j]->used = 1;
		else if (access_realms[j]->key && auth_realms[i]->key &&
		  strcmp(access_realms[j]->key, auth_realms[i]->key) == 0)
		    access_realms[j]->used = 1;
	    }
    }
    /* strip out unused access groups */
    i = j = 0;
    while (access_realms[i]) {
	if (access_realms[i]->used)
	    access_realms[j++] = access_realms[i];
	else
	    syslog(L_TRACE, "%s removing access group %s", access_realms[i]->name);
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

    i = sizeof(cli);
    gotsin = (getpeername(0, (struct sockaddr*)&cli, &i) == 0);
    if (gotsin)
	getsockname(0, (struct sockaddr*)&loc, &i);
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
    if (!strncmp(ln, "User:", strlen("User:")))
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
    while (nl = strchr(nl, '\n')) {
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
    if (nl == start)
	return;
    while (*nl)
	*start++ = *nl++;
    *start = '\0';
    return(got);
}

static void GetProgInput(EXECSTUFF *prog)
{
    fd_set rfds, tfds;
    int maxfd;
    int got, curpos;
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
	    syslog(L_NOTICE, "%s bad_hook program died with status %d",
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
    int tmp, status;
    int done;
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
		syslog(L_TRACE, "%s res resolver succesful, user %s", ClientHost, ubuf);
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
    int tmp, status;
    int done;
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
		syslog(L_TRACE, "%s auth authenticator succesful, user %s", ClientHost, ubuf);
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

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
typedef struct _AUTHGROUP {
    char *name;
    char *hosts;
    char **res_methods;
    char **auth_methods;
    char *default_user;
    char *default_domain;
} AUTHGROUP;

typedef struct _ACCESSGROUP {
    char *name;
    char *read;
    char *post;
    char *users;
    int newnews;
    int locpost;
} ACCESSGROUP;

typedef struct _GROUP {
    struct _GROUP *above;
    AUTHGROUP *auth;
    ACCESSGROUP *access;
} GROUP;

/* function declarations */
static void add_authgroup(AUTHGROUP*);
static void add_accessgroup(ACCESSGROUP*);
static void strip_accessgroups();

static AUTHGROUP *copy_authgroup(AUTHGROUP*);
static void free_authgroup(AUTHGROUP*);
static ACCESSGROUP *copy_accessgroup(ACCESSGROUP*);
static void free_accessgroup(ACCESSGROUP*);

static void CompressList(char*);
static int MatchClient(AUTHGROUP*);
static char *ResolveUser(AUTHGROUP*);
static char *AuthenticateUser(AUTHGROUP*, char*, char*);

/* global variables */
static AUTHGROUP **auth_realms;
static AUTHGROUP *success_auth;
static ACCESSGROUP **access_realms;

/* function definitions */
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

    if (orig->hosts)
	ret->hosts = COPY(orig->hosts);
    else
	ret->hosts = 0;

    if (orig->res_methods) {
	for (i = 0; orig->res_methods[i]; i++)
	    ;
	ret->res_methods = NEW(char*, i+1);
	for (i = 0; orig->res_methods[i]; i++)
	    ret->res_methods[i] = COPY(orig->res_methods[i]);
	ret->res_methods[i] = 0;
    } else
	ret->res_methods = 0;

    if (orig->auth_methods) {
	for (i = 0; orig->auth_methods[i]; i++)
	    ;
	ret->auth_methods = NEW(char*, i+1);
	for (i = 0; orig->auth_methods[i]; i++)
	    ret->auth_methods[i] = COPY(orig->auth_methods[i]);
	ret->auth_methods[i] = 0;
    } else
	ret->auth_methods = 0;

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
    if (del->hosts)
	DISPOSE(del->hosts);
    if (del->res_methods) {
	for (i = 0; del->res_methods[i]; i++)
	    DISPOSE(del->res_methods[i]);
	DISPOSE(del->res_methods);
    }
    if (del->auth_methods) {
	for (i = 0; del->auth_methods[i]; i++)
	    DISPOSE(del->auth_methods[i]);
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
    syslog(L_NOTICE, "%s syntax error in readers.conf(%d), %s", ClientHost,
      f->lineno, err);
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
#define PERMresprog	8
#define PERMdefuser	9
#define PERMdefdomain	10
#define PERMusers	11
#define PERMnewsgroups	12
#define PERMread	13
#define PERMpost	14
#define PERMaccessrp	15

static CONFTOKEN PERMtoks[] = {
  { PERMlbrace, "{" },
  { PERMrbrace, "}" },
#if 0	/* group declarations not ready yet */
  { PERMgroup, "group" },
#endif
  { PERMauth, "auth" },
  { PERMaccess, "access" },
  { PERMhost, "hosts:" },
  { PERMauthprog, "auth:" },
  { PERMresprog, "res:" },
  { PERMdefuser, "default:" },
  { PERMdefdomain, "default-domain:" },
  { PERMusers, "users:" },
  { PERMnewsgroups, "newsgroups:" },
  { PERMread, "read:" },
  { PERMpost, "post:" },
  { PERMaccessrp, "access:" },
  { 0, 0 }
};

static void PERMreadfile(char *filename)
{
    CONFFILE *f;
    CONFTOKEN *tok;
    int inwhat;
    GROUP *curgroup, *newgroup;
    ACCESSGROUP *curaccess;
    AUTHGROUP *curauth;
    int oldtype;
    char *str;
    int i;
    char errorstr[SMBUF];

    f = CONFfopen(filename);
    /* are we editing an AUTH or ACCESS group? */
    inwhat = 0;
    newgroup = curgroup = 0;
    while ((tok = CONFgettoken(PERMtoks, f)) != NULL) {
	if (inwhat == 0) {
	    /* top-level parser */
	    switch (tok->type) {
	      case PERMgroup:
		/* nested group declaration. */
		tok = CONFgettoken(PERMtoks, f);
		if (!tok)
		    ReportError(f, "Unexpected EOF");
		switch(tok->type) {
		  case PERMlbrace:
		    /* nested group declaration */
		    newgroup = NEW(GROUP, 1);
		    newgroup->above = curgroup;
		    if (curgroup) {
			newgroup->auth = copy_authgroup(curgroup->auth);
			newgroup->access = copy_accessgroup(curgroup->access);
		    } else {
			newgroup->auth = 0;
			newgroup->access = 0;
		    }
		    curgroup = newgroup;
		    break;
		  case PERMauth:
		    /* authentication info for the current group */
		    tok = CONFgettoken(PERMtoks, f);
		    if (tok->type != PERMlbrace)
			ReportError(f, "Expected '{'");
		    if (!curgroup->auth) {
		        curgroup->auth = NEW(AUTHGROUP, 1);
		        curgroup->auth->hosts = 0;
		        curgroup->auth->res_methods = 0;
		        curgroup->auth->auth_methods = 0;
		        curgroup->auth->name = 0;
		        curgroup->auth->default_user = 0;
		        curgroup->auth->default_domain = 0;
		    }
		    curauth = curgroup->auth;
		    inwhat = 1;
		    break;
		  case PERMaccess:
		    /* access info for the current group */
		    tok = CONFgettoken(PERMtoks, f);
		    if (tok->type != PERMlbrace)
			ReportError(f, "Expected '{'");
		    if (!curgroup->access) {
		        curgroup->access = NEW(ACCESSGROUP, 1);
		        curgroup->access->users = 0;
		        curgroup->access->read = 0;
		        curgroup->access->post = 0;
		        curgroup->access->name = 0;
		    }
		    curaccess = curgroup->access;
		    inwhat = 2;
		    break;
		  default:
		    ReportError(f, "Unexpected token in group declaration.");
		    break;
		}
		break;
	      case PERMauth:
	      case PERMaccess:
		oldtype = tok->type;
		if ((tok = CONFgettoken(PERMtoks, f)) == 0)
		    ReportError(f, "Expected identifier.");
		str = COPY(tok->name);
		tok = CONFgettoken(PERMtoks, f);
		if (tok->type != PERMlbrace)
		    ReportError(f, "Expected '{'");
		switch (oldtype) {
		  case PERMauth:
		    if (curgroup && curgroup->auth)
			curauth = copy_authgroup(curgroup->auth);
		    else {
			curauth = NEW(AUTHGROUP, 1);
			curauth->hosts = 0;
			curauth->res_methods = 0;
			curauth->auth_methods = 0;
			curauth->default_user = 0;
			curauth->default_domain = 0;
		    }
		    curauth->name = str;
		    inwhat = 1;
		    break;
		  case PERMaccess:
		    if (curgroup && curgroup->access)
			curaccess = copy_accessgroup(curgroup->access);
		    else {
			curaccess = NEW(ACCESSGROUP, 1);
			curaccess->users = 0;
			curaccess->post = 0;
			curaccess->read = 0;
			curaccess->newnews = innconf->allownewnews;
			curaccess->locpost = 0;
		    }
		    curaccess->name = str;
		    inwhat = 2;
		    break;
		}
		break;
	      case PERMrbrace:
		/* end of a group declaration */
		if (!curgroup)
		    ReportError(f, "Unmatched '}'");
		newgroup = curgroup;
		curgroup = curgroup->above;
		if (newgroup->auth)
		    free_authgroup(newgroup->auth);
		if (newgroup->access)
		    free_accessgroup(newgroup->access);
		free((void*) newgroup);
		break;
	      default:
		ReportError(f, "Unexpected token.");
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
		continue;
	    }
	    oldtype = tok->type;
	    tok = CONFgettoken(0, f);
	    if (!tok)
		ReportError(f, "Expected value.");
	    switch (oldtype) {
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
	      case PERMresprog:
		if (!curauth->res_methods) {
		    curauth->res_methods = NEW(char*, 2);
		    i = 0;
		} else {
		    for (i = 0; curauth->res_methods[i]; i++)
			;
		    curauth->res_methods = RENEW(curauth->res_methods, char*, i+2);
		}
		curauth->res_methods[i] = COPY(tok->name);
		curauth->res_methods[i+1] = 0;
		break;
	      case PERMauthprog:
		if (!curauth->auth_methods) {
		    curauth->auth_methods = NEW(char*, 2);
		    i = 0;
		} else {
		    for (i = 0; curauth->auth_methods[i]; i++)
			;
		    curauth->auth_methods = RENEW(curauth->auth_methods, char*, i+2);
		}
		curauth->auth_methods[i] = COPY(tok->name);
		curauth->auth_methods[i+1] = 0;
		break;
	      default:
		ReportError(f, "Unexpected token.");
		break;
	    }
	} else if (inwhat == 2) {
	    /* accessgroup parser */
	    if (tok->type == PERMrbrace) {
		inwhat = 0;
		if (curaccess->name)
		    add_accessgroup(curaccess);
		continue;
	    }
	    oldtype = tok->type;
	    tok = CONFgettoken(0, f);
	    if (!tok)
		ReportError(f, "Expected value.");
	    switch (oldtype) {
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
	} else
	    /* should never happen */
	    ;
    }
    CONFfclose(f);
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
}

void PERMlogin(char *uname, char *pass)
{
    int i;
    char *runame;

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

void PERMgetpermissions()
{
    int i;
    char **list, *cp;
    char *user[2];

    if (!success_auth) {
	/* if we haven't successfully authenticated, we can't do anything. */
	return;
    }
    user[0] = PERMuser;
    user[1] = 0;
    for (i = 0; access_realms[i]; i++)
	;
    while (i--) {
	if (!strcmp(access_realms[i]->name, success_auth->name)) {
	    if (!access_realms[i]->users)
		break;
	    else if (!*PERMuser)
		continue;
	    cp = COPY(access_realms[i]->users);
	    list = 0;
	    NGgetlist(&list, cp);
	    if (PERMmatch(list, user)) {
		DISPOSE(cp);
		DISPOSE(list);
		break;
	    }
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
	} else
	    PERMcanread = 0;
	if (access_realms[i]->post) {
	    cp = COPY(access_realms[i]->post);
	    NGgetlist(&PERMpostlist, cp);
	    PERMcanpost = 1;
	} else
	    PERMcanpost = 0;
	PERMnewnews = access_realms[i]->newnews;
	PERMlocpost = access_realms[i]->locpost;
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

static void strip_accessgroups()
{
    int from, to;
    /* whatever.  I'll get around to this function sooner or later, but it's
     * not important right now. */
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

static void GetConnInfo(char *buf)
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
    int i;
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
	syslog(L_TRACE, "%s res starting resolver %s", ClientHost, auth->res_methods[i]);
	cp = COPY(auth->res_methods[i]);
	args = 0;
	Argify(cp, &args);
	arg0 = NEW(char, strlen(resdir)+strlen(args[0])+1);
	sprintf(arg0, "%s%s", resdir, args[0]);
	/* exec the resolver */
	foo = ExecProg(arg0, args);
	if (foo) {
	    GetConnInfo(buf);
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
    int i;
    char *cp;
    char **args;
    char *arg0;
    char *resdir;
    EXECSTUFF *foo;
    struct sockaddr_in sin;
    int gotsin;
    int tmp, status;
    int done;
    char buf[BIG_BUFFER];

    if (!auth->auth_methods)
	return(0);

    resdir = NEW(char, strlen(cpcatpath(innconf->pathbin, _PATH_AUTHDIR)) +
      1 + strlen(_PATH_AUTHDIR_PASSWD) + 1 + 1);
    sprintf(resdir, "%s/%s/", cpcatpath(innconf->pathbin, _PATH_AUTHDIR),
      _PATH_AUTHDIR_PASSWD);

    i = sizeof(sin);
    gotsin = (getpeername(0, (struct sockaddr*)&sin, &i) == 0);
    ubuf[0] = '\0';
    for (i = 0; auth->auth_methods[i]; i++) {
	/* build the command line */
	syslog(L_TRACE, "%s auth starting authenticator %s", ClientHost, auth->auth_methods[i]);
	cp = COPY(auth->auth_methods[i]);
	args = 0;
	Argify(cp, &args);
	arg0 = NEW(char, strlen(resdir)+strlen(args[0])+1);
	sprintf(arg0, "%s%s", resdir, args[0]);
	/* exec the authenticator */
	foo = ExecProg(arg0, args);
	if (foo) {
	    GetConnInfo(buf);
	    sprintf(buf+strlen(buf), "ClientAuthname: %s\r\n", username);
	    sprintf(buf+strlen(buf), "ClientPassword: %s\r\n", password);
	    strcat(buf, ".\r\n");
	    xwrite(foo->wrfd, buf, strlen(buf));
	    close(foo->wrfd);

	    GetProgInput(foo);
	    if (done)
		syslog(L_TRACE, "%s auth authenticator succesful, user %s", ClientHost, ubuf);
	    else
		syslog(L_TRACE, "%s auth authenticator failed", ClientHost);
	    done = (ubuf[0] != '\0');
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

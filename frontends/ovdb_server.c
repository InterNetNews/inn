/*
 * ovdb_server.c
 * ovdb read server
 */

#include "config.h"
#include "clibrary.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif
#include "libinn.h"
#include "macros.h"
#include "paths.h"
#include "storage.h"
#include "ov.h"

#ifndef USE_BERKELEY_DB

int
main(int argc, char **argv)
{
    fprintf(stderr, "Error: BerkeleyDB not compiled in.\n");
    exit(1);
}

#else /* USE_BERKELEY_DB */

#include <db.h>
#include "../storage/ovdb/ovdb.h"
#include "../storage/ovdb/ovdb-private.h"

#ifdef HAVE_INN_VERSION_H
#include "inn/version.h"
#else
/* 2.3.0 doesn't have version.h */
#define INN_VERSION_MAJOR 2
#define INN_VERSION_MINOR 3
#define INN_VERSION_PATCH 0
#endif

#if INN_VERSION_MINOR == 3
#define nonblocking SetNonBlocking
#endif

#ifdef _HPUX_SOURCE
#include <sys/pstat.h>
#else
#if !defined(HAVE_SETPROCTITLE)
static char     *TITLEstart;
static char     *TITLEend;
#endif
#endif

static void TITLEset(char *what)
{
#if defined(HAVE_SETPROCTITLE)
    setproctitle("%s", what);
#else
#if defined(_HPUX_SOURCE)
    char                buff[BUFSIZ];
    union pstun un;

    un.pst_command = what;
    (void)pstat(PSTAT_SETCMD, un, strlen(buff), 0, 0);

#else	/* defined(_HPUX_SOURCE) */
    register char       *p;
    register int        i;
    char                buff[BUFSIZ];

    p = TITLEstart;

    strcpy(buff, what);
    i = strlen(buff);
    if (i > TITLEend - p - 2) {
        i = TITLEend - p - 2;
        buff[i] = '\0';
    }
    (void)strcpy(p, buff);
    for (p += i; p < TITLEend; )
        *p++ = ' ';

#endif	/* defined(_HPUX_SOURCE) */
#endif  /* defined(HAVE_SETPROCTITLE) */
}

typedef void (*SIG_HANDLER_T)(int);

#ifdef HAVE_SIGACTION

/* like xsignal, but does not set SA_RESTART */

SIG_HANDLER_T
xrsignal(int signum, SIG_HANDLER_T sigfunc)
{
    struct sigaction act, oact;

    act.sa_handler = sigfunc;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;

    if (sigaction(signum, &act, &oact) < 0)
        return SIG_ERR;
    return oact.sa_handler;
}

/* If there is no SA_RESTART, then maybe we can't set signal handler
   semantics to NOT restart.  So to be safe, we'll assume the select
   won't exit with EINTR when signalled and give it a timeout. */
#ifndef SA_RESTART
#define SELECT_TIMEOUT 30
#endif

#else /* HAVE_SIGACTION */

#define xrsignal xsignal
#define SELECT_TIMEOUT 30

#endif /* HAVE_SIGACTION */



/* This will work unless user sets a larger clienttimeout
   in readers.conf */
#define CLIENT_TIMEOUT (innconf->clienttimeout + 60)
/*#define CLIENT_TIMEOUT 3600*/


static int listensock;

#define MODE_READ   0
#define MODE_WRITE  1
#define MODE_CLOSED 2
#define STATE_READCMD 0
#define STATE_READGROUP 1
struct reader {
    int fd;
    int mode;
    int state;
    int buflen;
    int bufpos;
    void *buf;
    time_t lastactive;
};

static struct reader **readertab;
static int readertablen;
static int numreaders;
static time_t now;

static int signalled = 0;
static void
sigfunc(int sig)
{
    signalled = 1;
}

static int putpid(char *path)
{
    char buf[30];
    int fd = open(path, O_WRONLY|O_TRUNC|O_CREAT, 0664);
    if(!fd) {
        syslog(L_FATAL, "can't open %s: %m", path);
        return -1;
    }
    sprintf(buf, "%d\n", getpid());
    if(write(fd, buf, strlen(buf)) < 0) {
        syslog(L_FATAL, "can't write to %s: %m", path);
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

static void
newclient(int fd)
{
    struct reader *r;

    nonblocking(fd, 1);

    r = NEW(struct reader, 1);
    r->fd = fd;
    r->mode = MODE_WRITE;
    r->buflen = sizeof(OVDB_SERVER_BANNER);
    r->bufpos = 0;
    r->buf = COPY(OVDB_SERVER_BANNER);
    r->lastactive = now;

    if(numreaders >= readertablen) {
    	readertablen += 50;
	RENEW(readertab, struct reader *, readertablen);
    }
    readertab[numreaders] = r;
    numreaders++;
}

static void
delclient(int which)
{
    int i;
    struct reader *r = readertab[which];

    if(r->mode != MODE_CLOSED)
    	close(r->fd);

    if(r->buf != NULL) {
    	DISPOSE(r->buf);
    }
    DISPOSE(r);
    numreaders--;
    for(i = which; i < numreaders; i++)
    	readertab[i] = readertab[i+1];
}

static void
do_groupstats(struct reader *r)
{
    struct rs_cmd *cmd = r->buf;
    struct rs_groupstats *reply;
    char *group = (char *)(r->buf) + sizeof(struct rs_cmd);
    reply = NEW(struct rs_groupstats, 1);

    /*syslog(LOG_DEBUG, "OVDB: rs: do_groupstats '%s'", group);*/
    if(ovdb_groupstats(group, &reply->lo, &reply->hi, &reply->count, &reply->flag)) {
	reply->status = CMD_GROUPSTATS;
    	reply->aliaslen = 0;
    } else {
	reply->status = CMD_GROUPSTATS | RPLY_ERROR;
    }
    DISPOSE(r->buf);
    r->buf = reply;
    r->buflen = sizeof(struct rs_groupstats);
    r->bufpos = 0;
    r->mode = MODE_WRITE;
}

static void
do_opensrch(struct reader *r)
{
    struct rs_cmd *cmd = r->buf;
    struct rs_opensrch *reply;
    char *group = (char *)(r->buf) + sizeof(struct rs_cmd);
    reply = NEW(struct rs_opensrch, 1);

    /*syslog(LOG_DEBUG, "OVDB: rs: do_opensrch '%s' %d %d", group, cmd->artlo, cmd->arthi);*/

    reply->handle = ovdb_opensearch(group, cmd->artlo, cmd->arthi);
    if(reply->handle == NULL) {
    	reply->status = CMD_OPENSRCH | RPLY_ERROR;
    } else {
    	reply->status = CMD_OPENSRCH;
    }
    DISPOSE(r->buf);
    r->buf = reply;
    r->buflen = sizeof(struct rs_opensrch);
    r->bufpos = 0;
    r->mode = MODE_WRITE;
}

static void
do_srch(struct reader *r)
{
    struct rs_cmd *cmd = r->buf;
    struct rs_srch *reply;
    ARTNUM artnum;
    TOKEN token;
    time_t arrived;
    int len;
    char *data;

    if(ovdb_search(cmd->handle, &artnum, &data, &len, &token, &arrived)) {
    	reply = NEW(char, (sizeof(struct rs_srch) + len));
	reply->status = CMD_SRCH;
	reply->artnum = artnum;
	reply->token = token;
	reply->arrived = arrived;
	reply->len = len;
	memcpy((char *)reply + sizeof(struct rs_srch), data, len);
	r->buflen = sizeof(struct rs_srch) + len;
    } else {
    	reply = NEW(struct rs_srch, 1);
	reply->status = CMD_SRCH | RPLY_ERROR;
	r->buflen = sizeof(struct rs_srch);
    }
    DISPOSE(r->buf);
    r->buf = reply;
    r->bufpos = 0;
    r->mode = MODE_WRITE;
}

static void
do_closesrch(struct reader *r)
{
    struct rs_cmd *cmd = r->buf;

    ovdb_closesearch(cmd->handle);
    DISPOSE(r->buf);
    r->buf = NULL;
    r->bufpos = r->buflen = 0;
    r->mode = MODE_READ;
}

static void
do_artinfo(struct reader *r)
{
    struct rs_cmd *cmd = r->buf;
    struct rs_artinfo *reply;
    char *group = (char *)(r->buf) + sizeof(struct rs_cmd);
    TOKEN token;
    char *data;
    int len;

    /*syslog(LOG_DEBUG, "OVDB: rs: do_artinfo: '%s' %d", group, cmd->artlo);*/
    if(ovdb_getartinfo(group, cmd->artlo, &data, &len, &token)) {
    	reply = NEW(char, (sizeof(struct rs_artinfo) + len));
	reply->status = CMD_ARTINFO;
	reply->len = len;
	reply->token = token;
	memcpy((char *)reply + sizeof(struct rs_artinfo), data, len);
	r->buflen = sizeof(struct rs_artinfo) + len;
    } else {
    	reply = NEW(struct rs_artinfo, 1);
	reply->status = CMD_ARTINFO | RPLY_ERROR;
	r->buflen = sizeof(struct rs_artinfo);
    }
    DISPOSE(r->buf);
    r->buf = reply;
    r->bufpos = 0;
    r->mode = MODE_WRITE;
}


static int
process_cmd(struct reader *r)
{
    struct rs_cmd *cmd = r->buf;

    if(r->state == STATE_READCMD) {
	switch(cmd->what) {
	case CMD_GROUPSTATS:
	case CMD_OPENSRCH:
	case CMD_ARTINFO:
	    r->state = STATE_READGROUP;
	    r->buflen += cmd->grouplen;
	    RENEW(r->buf, char, r->buflen);
	    return 1;
	}
    }

    switch(cmd->what) {
    case CMD_GROUPSTATS:
	do_groupstats(r);
	break;
    case CMD_OPENSRCH:
	do_opensrch(r);
	break;
    case CMD_SRCH:
	do_srch(r);
	break;
    case CMD_CLOSESRCH:
	do_closesrch(r);
	break;
    case CMD_ARTINFO:
	do_artinfo(r);
	break;
    case CMD_QUIT:
	r->mode = MODE_CLOSED;
	close(r->fd);
	DISPOSE(r->buf);
	r->buf = NULL;
	break;
    }

    return 0;
}

static void
handle_read(struct reader *r)
{
    int n;
    r->lastactive = now;

    if(!r->buflen) {
    	r->state = STATE_READCMD;
    	r->buf = NEW(struct rs_cmd, 1);
	r->buflen = sizeof(struct rs_cmd);
	r->bufpos = 0;
    }
again:
    n = read(r->fd, (char *)(r->buf) + r->bufpos, r->buflen - r->bufpos);
    if(n <= 0) {
	if(n < 0 && (errno == EAGAIN || errno == EINTR))
	    return;
    	r->mode = MODE_CLOSED;
	close(r->fd);
	DISPOSE(r->buf);
	r->buf = NULL;
    }
    r->bufpos += n;

    if(r->bufpos >= r->buflen)
	if(process_cmd(r))
	    goto again;
}

static void
handle_write(struct reader *r)
{
    int n;
    r->lastactive = now;

    if(r->buf == NULL)	/* shouldn't happen */
	return;

    n = write(r->fd, (char *)(r->buf) + r->bufpos, r->buflen - r->bufpos);
    if(n <= 0) {
	if(n < 0 && (errno == EAGAIN || errno == EINTR))
	    return;
    	r->mode = MODE_CLOSED;
	close(r->fd);
	DISPOSE(r->buf);
	r->buf = NULL;
    }
    r->bufpos += n;

    if(r->bufpos >= r->buflen) {
    	r->buf = NULL;
	r->bufpos = r->buflen = 0;
	r->mode = MODE_READ;
    }
}

static pid_t
serverproc(void)
{
    fd_set rdset, wrset;
    int i, ret, count, lastfd, salen, lastnumreaders;
    struct sockaddr_in sa;
#ifdef SELECT_TIMEOUT
    struct timeval tv;
#endif
    char string[50];
    pid_t pid;

    if(pid = fork())
	return pid;

    if(!ovdb_open(OV_READ|OVDB_SERVER)) {
	syslog(L_FATAL, "ovdb_server: cant open overview");
	exit(1);
    }
    xrsignal(SIGINT, sigfunc);
    xrsignal(SIGTERM, sigfunc);
    xrsignal(SIGHUP, sigfunc);

    nonblocking(listensock, 1);

    if(listen(listensock, MAXLISTEN) < 0) {
	syslog(L_FATAL, "ovdb_server: cant listen: %m");
	exit(1);
    }

    numreaders = lastnumreaders = 0;
    if(ovdb_conf.maxrsconn) {
	readertablen = ovdb_conf.maxrsconn;
    } else {
    	readertablen = 50;
    }
    readertab = NEW(struct reader *, ovdb_conf.maxrsconn);    
    lastfd = listensock;
    TITLEset("ovdb_server: 0 clients");

    /* main loop */
    while(!signalled) {
	FD_ZERO(&rdset);
	FD_ZERO(&wrset);
	if(!ovdb_conf.maxrsconn || numreaders < ovdb_conf.maxrsconn) {
	    FD_SET(listensock, &rdset);
	    lastfd = listensock;
	} else {
	    lastfd = 0;
	}

	for(i = 0; i < numreaders; i++) {
	    switch(readertab[i]->mode) {
	    case MODE_READ:
	    	FD_SET(readertab[i]->fd, &rdset);
		break;
	    case MODE_WRITE:
	    	FD_SET(readertab[i]->fd, &wrset);
		break;
	    default:
	    	continue;
	    }
	    if(readertab[i]->fd > lastfd)
	    	lastfd = readertab[i]->fd;
	}
#ifdef SELECT_TIMEOUT
	tv.tv_usec = 0;
	tv.tv_sec = SELECT_TIMEOUT;
#endif

#ifdef SELECT_TIMEOUT
	count = select(lastfd + 1, &rdset, &wrset, NULL, &tv);
#else
	count = select(lastfd + 1, &rdset, &wrset, NULL, NULL);
#endif
	if(count < 0)
	    continue;
	if(signalled)
	    break;

	now = time(NULL);

	if(FD_ISSET(listensock, &rdset)) {
	    if(!ovdb_conf.maxrsconn || numreaders < ovdb_conf.maxrsconn) {
		salen = sizeof(sa);
	    	ret = accept(listensock, &sa, &salen);
		if(ret >= 0) {
		    newclient(ret);
		    FD_SET(readertab[numreaders-1]->fd, &wrset);
		}
	    }
	}

	for(i = 0; i < numreaders; i++) {
	    switch(readertab[i]->mode) {
	    case MODE_READ:
	    	if(FD_ISSET(readertab[i]->fd, &rdset))
		    handle_read(readertab[i]);
	        break;
	    case MODE_WRITE:
	    	if(FD_ISSET(readertab[i]->fd, &wrset))
		    handle_write(readertab[i]);
		break;
	    }
	    /* this is not in the switch because the connection
		may have been closed in handle_read */
	    if(readertab[i]->mode == MODE_CLOSED) {
	        delclient(i);
		i--;
	    }
	}

	for(i = 0; i < numreaders; i++) {
	    if(readertab[i]->lastactive + CLIENT_TIMEOUT < now) {
	    	delclient(i);
		i--;
	    }
	}

	if(numreaders != lastnumreaders) {
	    lastnumreaders = numreaders;
	    sprintf(string, "ovdb_server: %d client%s", numreaders,
		    numreaders == 1 ? "" : "s");
	    TITLEset(string);
	}
    }

    ovdb_close();
    exit(0);
}

int
main(int argc, char *argv[])
{
    int i;
    struct sockaddr_in sa;
    pid_t *children;

    if(argc != 2 || strcmp(argv[1], SPACES)) {
        fprintf(stderr, "Use ovdb_init to start me\n");
        exit(1);
    }
#if     !defined(_HPUX_SOURCE) && !defined(HAVE_SETPROCTITLE)
    /* Save start and extent of argv for TITLEset. */
    TITLEstart = argv[0];
    TITLEend = argv[argc - 1] + strlen(argv[argc - 1]) - 1;
#endif

    openlog("ovdb_server", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);

    if(ReadInnConf() < 0)
	exit(1);

    if(strcmp(innconf->ovmethod, "ovdb")) {
        syslog(L_FATAL, "ovmethod not set to ovdb");
        exit(1);
    }

    read_ovdb_conf();

    listensock = socket(AF_INET, SOCK_STREAM, 0);
    if(listensock < 0) {
	fprintf(stderr, "ovdb_server: socket: %s\n", strerror(errno));
	exit(1);
    }
    sa.sin_family = AF_INET;
    sa.sin_port = htons(OVDB_SERVER_PORT);
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    
    if(bind(listensock, (struct sockaddr *)&sa, sizeof sa) != 0) {
	fprintf(stderr, "ovdb_server: bind: %s\n", strerror(errno));
	exit(1);
    }

    if(putpid(cpcatpath(innconf->pathrun, OVDB_SERVER_PIDFILE)))
        exit(1);

    xrsignal(SIGINT, sigfunc);
    xrsignal(SIGTERM, sigfunc);
    xrsignal(SIGHUP, sigfunc);

    children = NEW(pid_t, ovdb_conf.numrsprocs);

    for(i = 0; i < ovdb_conf.numrsprocs; i++) {
	if((children[i] = serverproc()) == -1) {
	    for(i--; i >= 0; i--)
		kill(children[i], SIGTERM);
	}
	sleep(1);
    }

    while(!signalled) {
	/* TODO:
	 *  + Do waits here to check for child exits.
	 *  + If all the children are full, we should answer
	 *    new requests with a 'not available' message
	 *    of some kind.  This will require some kind
	 *    of IPC from the children.
	 */

#ifdef SELECT_TIMEOUT
	/* see comment above about SELECT_TIMEOUT */
	sleep(20);
#else
	pause();
#endif
    }

    for(i = 0; i < ovdb_conf.numrsprocs; i++)
	kill(children[i], SIGTERM);

    unlink(cpcatpath(innconf->pathrun, OVDB_SERVER_PIDFILE));

    exit(0);
}



#endif /* USE_BERKELEY_DB */

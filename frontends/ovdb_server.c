/*
 * ovdb_server.c
 * ovdb read server
 */

#include "config.h"
#include "clibrary.h"
#include "portable/mmap.h"
#include "portable/setproctitle.h"
#include "portable/socket.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif
#include <syslog.h>

#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <time.h>

#ifdef HAVE_UNIX_DOMAIN_SOCKETS
# include "portable/socket-unix.h"
#endif
#include <sys/wait.h>

#include "inn/fdflag.h"
#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/libinn.h"
#include "inn/paths.h"
#include "inn/storage.h"
#include "inn/ov.h"

#include "../storage/ovdb/ovdb.h"
#include "../storage/ovdb/ovdb-private.h"

#ifndef HAVE_BDB

int
main(int argc UNUSED, char **argv UNUSED)
{
    die("Berkeley DB support not compiled");
}

#else /* HAVE_BDB */


#define SELECT_TIMEOUT 15


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
    void *currentsearch;
};

static struct reader *readertab;
static int readertablen;
static int numreaders;
static time_t now;
static pid_t parent;

struct child {
    pid_t pid;
    int num;
    time_t started;
};
static struct child *children;
#define wholistens (children[ovdb_conf.numrsprocs].num)

static int signalled = 0;
static void
sigfunc(int sig UNUSED)
{
    signalled = 1;
}

static int updated = 0;
static void
childsig(int sig UNUSED)
{
    updated = 1;
}

static void
parentsig(int sig UNUSED)
{
    int i, which, smallest;
    if(wholistens < 0) {
	which = smallest = -1;
	for(i = 0; i < ovdb_conf.numrsprocs; i++) {
	    if(children[i].pid == -1)
		continue;
	    if(!ovdb_conf.maxrsconn || children[i].num <= ovdb_conf.maxrsconn) {
		if(smallest == -1 || children[i].num < smallest) {
		    smallest = children[i].num;
		    which = i;
		}
	    }
	}
	if(which != -1) {
	    wholistens = which;
	    kill(children[which].pid, SIGUSR1);
	} else {
	    wholistens = -2;
	}
	updated = 1;
    }
}

static int putpid(const char *path)
{
    char buf[30];
    int fd = open(path, O_WRONLY|O_TRUNC|O_CREAT, 0664);
    if(fd == -1) {
        syswarn("cannot open %s", path);
        return -1;
    }
    snprintf(buf, sizeof(buf), "%d\n", getpid());
    if(write(fd, buf, strlen(buf)) < 0) {
        syswarn("cannot write to %s", path);
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

static void
do_groupstats(struct reader *r)
{
    struct rs_groupstats *reply;
    char *group = (char *)(r->buf) + sizeof(struct rs_cmd);
    reply = xmalloc(sizeof(struct rs_groupstats));

    debug("OVDB: rs: do_groupstats '%s'", group);
    if(ovdb_groupstats(group, &reply->lo, &reply->hi, &reply->count, &reply->flag)) {
	reply->status = CMD_GROUPSTATS;
    	reply->aliaslen = 0;
    } else {
	reply->status = CMD_GROUPSTATS | RPLY_ERROR;
    }
    free(r->buf);
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
    reply = xmalloc(sizeof(struct rs_opensrch));

    debug("OVDB: rs: do_opensrch '%s' %d %d", group, cmd->artlo, cmd->arthi);

    if(r->currentsearch != NULL) {
	/* can only open one search at a time */
	reply->status = CMD_OPENSRCH | RPLY_ERROR;
    } else {
	reply->handle = ovdb_opensearch(group, cmd->artlo, cmd->arthi);
	if(reply->handle == NULL) {
	    reply->status = CMD_OPENSRCH | RPLY_ERROR;
	} else {
	    reply->status = CMD_OPENSRCH;
	}
	r->currentsearch = reply->handle;
    }
    free(r->buf);
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
    	reply = xmalloc(sizeof(struct rs_srch) + len);
	reply->status = CMD_SRCH;
	reply->artnum = artnum;
	reply->token = token;
	reply->arrived = arrived;
	reply->len = len;
	memcpy((char *)reply + sizeof(struct rs_srch), data, len);
	r->buflen = sizeof(struct rs_srch) + len;
    } else {
    	reply = xmalloc(sizeof(struct rs_srch));
	reply->status = CMD_SRCH | RPLY_ERROR;
	r->buflen = sizeof(struct rs_srch);
    }
    free(r->buf);
    r->buf = reply;
    r->bufpos = 0;
    r->mode = MODE_WRITE;
}

static void
do_closesrch(struct reader *r)
{
    struct rs_cmd *cmd = r->buf;

    ovdb_closesearch(cmd->handle);
    free(r->buf);
    r->buf = NULL;
    r->bufpos = r->buflen = 0;
    r->mode = MODE_READ;
    r->currentsearch = NULL;
}

static void
do_artinfo(struct reader *r)
{
    struct rs_cmd *cmd = r->buf;
    struct rs_artinfo *reply;
    char *group = (char *)(r->buf) + sizeof(struct rs_cmd);
    TOKEN token;

    debug("OVDB: rs: do_artinfo: '%s' %d", group, cmd->artlo);
    if(ovdb_getartinfo(group, cmd->artlo, &token)) {
    	reply = xmalloc(sizeof(struct rs_artinfo));
	reply->status = CMD_ARTINFO;
	reply->token = token;
	r->buflen = sizeof(struct rs_artinfo);
    } else {
    	reply = xmalloc(sizeof(struct rs_artinfo));
	reply->status = CMD_ARTINFO | RPLY_ERROR;
	r->buflen = sizeof(struct rs_artinfo);
    }
    free(r->buf);
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
	    if(cmd->grouplen == 0) {
		/* shoudn't happen... */
		r->mode = MODE_CLOSED;
		close(r->fd);
		free(r->buf);
		r->buf = NULL;
		return 0;
	    }
	    r->buflen += cmd->grouplen;
            r->buf = xrealloc(r->buf, r->buflen);
	    return 1;
	}
    }

    switch(cmd->what) {
    case CMD_GROUPSTATS:
	((char *)r->buf)[r->buflen - 1] = 0;	/* make sure group is null-terminated */
	do_groupstats(r);
	break;
    case CMD_OPENSRCH:
	((char *)r->buf)[r->buflen - 1] = 0;
	do_opensrch(r);
	break;
    case CMD_SRCH:
	do_srch(r);
	break;
    case CMD_CLOSESRCH:
	do_closesrch(r);
	break;
    case CMD_ARTINFO:
	((char *)r->buf)[r->buflen - 1] = 0;
	do_artinfo(r);
	break;
    default:
	r->mode = MODE_CLOSED;
	close(r->fd);
	free(r->buf);
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

    if(r->buf == NULL) {
    	r->state = STATE_READCMD;
    	r->buf = xmalloc(sizeof(struct rs_cmd));
	r->buflen = sizeof(struct rs_cmd);
	r->bufpos = 0;
    }
again:
    n = read(r->fd, (char *)(r->buf) + r->bufpos, r->buflen - r->bufpos);
    if(n <= 0) {
	if(n < 0 && (errno == EAGAIN || errno == EINTR || errno == EWOULDBLOCK))
	    return;
    	r->mode = MODE_CLOSED;
	close(r->fd);
	free(r->buf);
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
	if(n < 0 && (errno == EAGAIN || errno == EINTR || errno == EWOULDBLOCK))
	    return;
    	r->mode = MODE_CLOSED;
	close(r->fd);
	free(r->buf);
	r->buf = NULL;
    }
    r->bufpos += n;

    if(r->bufpos >= r->buflen) {
	free(r->buf);
    	r->buf = NULL;
	r->bufpos = r->buflen = 0;
	r->mode = MODE_READ;
    }
}

static void
newclient(int fd)
{
    struct reader *r;
    int i;

    fdflag_nonblocking(fd, 1);

    if(numreaders >= readertablen) {
    	readertablen += 50;
        readertab = xrealloc(readertab, readertablen * sizeof(struct reader));
        for(i = numreaders; i < readertablen; i++) {
	    readertab[i].mode = MODE_CLOSED;
	    readertab[i].buf = NULL;
	}
    }

    r = &(readertab[numreaders]);
    numreaders++;

    r->fd = fd;
    r->mode = MODE_WRITE;
    r->buflen = sizeof(OVDB_SERVER_BANNER);
    r->bufpos = 0;
    r->buf = xstrdup(OVDB_SERVER_BANNER);
    r->lastactive = now;
    r->currentsearch = NULL;

    handle_write(r);
}

static void
delclient(int which)
{
    int i;
    struct reader *r = &(readertab[which]);

    if(r->mode != MODE_CLOSED)
    	close(r->fd);

    if(r->buf != NULL) {
    	free(r->buf);
    }
    if(r->currentsearch != NULL) {
	ovdb_closesearch(r->currentsearch);
	r->currentsearch = NULL;
    }

    /* numreaders will get decremented by the calling function */
    for(i = which; i < numreaders-1; i++)
    	readertab[i] = readertab[i+1];

    readertab[i].mode = MODE_CLOSED;
    readertab[i].buf = NULL;
}

static pid_t
serverproc(int me)
{
    fd_set rdset, wrset;
    int i, ret, count, lastfd, lastnumreaders;
    socklen_t salen;
    struct sockaddr_in sa;
    struct timeval tv;
    pid_t pid;

    pid = fork();
    if (pid != 0)
	return pid;

    if (!ovdb_open(OV_READ|OVDB_SERVER))
        die("cannot open overview");
    xsignal_norestart(SIGINT, sigfunc);
    xsignal_norestart(SIGTERM, sigfunc);
    xsignal_norestart(SIGHUP, sigfunc);
    xsignal_norestart(SIGUSR1, childsig);
    xsignal(SIGPIPE, SIG_IGN);

    numreaders = lastnumreaders = 0;
    if(ovdb_conf.maxrsconn) {
	readertablen = ovdb_conf.maxrsconn;
    } else {
    	readertablen = 50;
    }
    readertab = xmalloc(readertablen * sizeof(struct reader));
    for(i = 0; i < readertablen; i++) {
	readertab[i].mode = MODE_CLOSED;
	readertab[i].buf = NULL;
    }

    setproctitle("0 clients");

    /* main loop */
    while(!signalled) {
	FD_ZERO(&rdset);
	FD_ZERO(&wrset);
	lastfd = 0;
	if(wholistens == me) {
	    if(!ovdb_conf.maxrsconn || numreaders < ovdb_conf.maxrsconn) {
		FD_SET(listensock, &rdset);
		lastfd = listensock;
                setproctitle("%d client%s *", numreaders,
                             numreaders == 1 ? "" : "s");
	    } else {
		wholistens = -1;
		kill(parent, SIGUSR1);
	    }
        }

	for(i = 0; i < numreaders; i++) {
	    switch(readertab[i].mode) {
	    case MODE_READ:
	    	FD_SET(readertab[i].fd, &rdset);
		break;
	    case MODE_WRITE:
	    	FD_SET(readertab[i].fd, &wrset);
		break;
	    default:
	    	continue;
	    }
	    if(readertab[i].fd > lastfd)
	    	lastfd = readertab[i].fd;
	}
	tv.tv_usec = 0;
	tv.tv_sec = SELECT_TIMEOUT;
	count = select(lastfd + 1, &rdset, &wrset, NULL, &tv);

	if(signalled)
	    break;
	if(count <= 0)
	    continue;

	now = time(NULL);

	if(FD_ISSET(listensock, &rdset)) {
	    if(!ovdb_conf.maxrsconn || numreaders < ovdb_conf.maxrsconn) {
		salen = sizeof(sa);
	    	ret = accept(listensock, (struct sockaddr *)&sa, &salen);
		if(ret >= 0) {
		    newclient(ret);
		    wholistens = -1;
		    children[me].num = numreaders;
		    kill(parent, SIGUSR1);
		}
	    }
	}

	for(i = 0; i < numreaders; i++) {
	    switch(readertab[i].mode) {
	    case MODE_READ:
	    	if(FD_ISSET(readertab[i].fd, &rdset))
		    handle_read(&(readertab[i]));
	        break;
	    case MODE_WRITE:
	    	if(FD_ISSET(readertab[i].fd, &wrset))
		    handle_write(&(readertab[i]));
		break;
	    }
	}

	for(i = 0; i < numreaders; i++) {
	    if(readertab[i].mode == MODE_CLOSED
		  || (time_t) (readertab[i].lastactive + CLIENT_TIMEOUT) < now) {
	    	delclient(i);
		numreaders--;
		i--;
	    }
	}
	if(children[me].num != numreaders) {
	    children[me].num = numreaders;
	    kill(parent, SIGUSR1);
        }
	if(numreaders != lastnumreaders) {
	    lastnumreaders = numreaders;
            setproctitle("%d client%s", numreaders,
                         numreaders == 1 ? "" : "s");
	}
    }

    ovdb_close();
    exit(0);
}

static int
reap(void)
{
    int i, cs;
    pid_t c;

    while((c = waitpid(-1, &cs, WNOHANG)) > 0) {
	for(i = 0; i < ovdb_conf.numrsprocs; i++) {
	    if(c == children[i].pid) {
		if(children[i].started + 30 > time(NULL))
		    return 1;

		children[i].num = 0;

		if(wholistens == i)
		    wholistens = -1;

		if((children[i].pid = serverproc(i)) == -1)
		    return 1;

		children[i].started = time(NULL);
		break;
	    }
	}
    }
    if(wholistens == -1)
	parentsig(SIGUSR1);
    return 0;
}

#ifndef MAP_ANON
#ifdef MAP_ANONYMOUS
#define MAP_ANON MAP_ANONYMOUS
#endif
#endif

static void *
sharemem(size_t len)
{
#ifdef MAP_ANON
    return mmap(0, len, PROT_READ|PROT_WRITE, MAP_ANON|MAP_SHARED, -1, 0);
#else
    int fd = open("/dev/zero", O_RDWR, 0);
    char *ptr = mmap(0, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    return ptr;
#endif
}

int
main(int argc, char *argv[])
{
    int i, ret;
    socklen_t salen;
    char *path, *pidfile;
#ifdef HAVE_UNIX_DOMAIN_SOCKETS
    struct sockaddr_un sa;
#else
    struct sockaddr_in sa;
#endif
    struct timeval tv;
    fd_set rdset;

    setproctitle_init(argc, argv);

    openlog("ovdb_server", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);
    message_program_name = "ovdb_server";

    if(argc != 2 || strcmp(argv[1], SPACES))
        die("should be started by ovdb_init");
    message_handlers_warn(1, message_log_syslog_err);
    message_handlers_die(1, message_log_syslog_err);

    if (!innconf_read(NULL))
	exit(1);

    if(strcmp(innconf->ovmethod, "ovdb"))
        die("ovmethod not set to ovdb in inn.conf");

    read_ovdb_conf();

#ifdef HAVE_UNIX_DOMAIN_SOCKETS
    listensock = socket(AF_UNIX, SOCK_STREAM, 0);
#else
    listensock = socket(AF_INET, SOCK_STREAM, 0);
#endif
    if(listensock < 0)
        sysdie("cannot create socket");

    fdflag_nonblocking(listensock, 1);

    memset(&sa, 0, sizeof sa);
#ifdef HAVE_UNIX_DOMAIN_SOCKETS
    sa.sun_family = AF_UNIX;
    path = concatpath(innconf->pathrun, OVDB_SERVER_SOCKET);
    strlcpy(sa.sun_path, path, sizeof(sa.sun_path));
    unlink(sa.sun_path);
    free(path);
    ret = bind(listensock, (struct sockaddr *)&sa, SUN_LEN(&sa));
#else
    sa.sin_family = AF_INET;
    sa.sin_port = htons(OVDB_SERVER_PORT);
    sa.sin_addr.s_addr = htonl(0x7f000001UL);
    
    ret = bind(listensock, (struct sockaddr *)&sa, sizeof sa);

    if(ret != 0 && errno == EADDRNOTAVAIL) {
	sa.sin_family = AF_INET;
	sa.sin_port = htons(OVDB_SERVER_PORT);
	sa.sin_addr.s_addr = INADDR_ANY;
	ret = bind(listensock, (struct sockaddr *)&sa, sizeof sa);
    }
#endif

    if(ret != 0)
        sysdie("cannot bind socket");
    if(listen(listensock, MAXLISTEN) < 0)
        sysdie("cannot listen on socket");

    pidfile = concatpath(innconf->pathrun, OVDB_SERVER_PIDFILE);
    if(putpid(pidfile))
        exit(1);

    xsignal_norestart(SIGINT, sigfunc);
    xsignal_norestart(SIGTERM, sigfunc);
    xsignal_norestart(SIGHUP, sigfunc);

    xsignal_norestart(SIGUSR1, parentsig);
    xsignal_norestart(SIGCHLD, childsig);
    parent = getpid();

    children = sharemem(sizeof(struct child) * (ovdb_conf.numrsprocs+1));

    if(children == NULL)
        sysdie("cannot mmap shared memory");
    for(i = 0; i < ovdb_conf.numrsprocs+1; i++) {
	children[i].pid = -1;
	children[i].num = 0;
    }

    for(i = 0; i < ovdb_conf.numrsprocs; i++) {
	if((children[i].pid = serverproc(i)) == -1) {
	    for(i--; i >= 0; i--)
		kill(children[i].pid, SIGTERM);
	    exit(1);
	}
	children[i].started = time(NULL);
	sleep(1);
    }

    while(!signalled) {
	if(reap())
	    break;

	if(wholistens == -2) {
	    FD_ZERO(&rdset);
	    FD_SET(listensock, &rdset);
	    tv.tv_usec = 0;
	    tv.tv_sec = SELECT_TIMEOUT;
	    ret = select(listensock+1, &rdset, NULL, NULL, &tv);

	    if(ret == 1 && wholistens == -2) {
		salen = sizeof(sa);
		ret = accept(listensock, (struct sockaddr *)&sa, &salen);
		if(ret >= 0)
		   close(ret);
	    }
	} else {
	    pause();
	}
    }

    for(i = 0; i < ovdb_conf.numrsprocs; i++)
	if(children[i].pid != -1)
	    kill(children[i].pid, SIGTERM);

    while(wait(&ret) > 0)
	;

    unlink(pidfile);

    exit(0);
}


#endif /* HAVE_BDB */

/*
 * ovdb_monitor
 *   Performs database maintenance tasks
 *   + Transaction checkpoints
 *   + Deadlock detection
 *   + Transaction log removal
 */

#include "config.h"
#include "clibrary.h"
#include "portable/setproctitle.h"
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>
#include <sys/wait.h>

#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/libinn.h"
#include "inn/ov.h"

#include "../storage/ovdb/ovdb.h"
#include "../storage/ovdb/ovdb-private.h"

#ifndef HAVE_BDB

int main(int argc UNUSED, char **argv UNUSED)
{
    exit(0);
}

#else /* HAVE_BDB */

static int signalled = 0;
static void sigfunc(int sig UNUSED)
{
    signalled = 1;
}


static pid_t deadlockpid = 0;
static pid_t checkpointpid = 0;
static pid_t logremoverpid = 0;

static int putpid(const char *path)
{
    char buf[30];
    int fd = open(path, O_WRONLY|O_TRUNC|O_CREAT, 0664);
    if(!fd) {
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

static void deadlock(void)
{
    int ret, status = 0;
    u_int32_t atype = DB_LOCK_YOUNGEST;

    if(ovdb_open_berkeleydb(OV_WRITE, 0))
	_exit(1);

    setproctitle("deadlock");

    while(!signalled) {
	ret = OVDBenv->lock_detect(OVDBenv, 0, atype, NULL);
	if(ret != 0) {
            warn("OVDB: lock_detect: %s", db_strerror(ret));
	    status = 1;
	    break;
	}
	sleep(30);
    }

    ovdb_close_berkeleydb();
    _exit(status);
}

static void checkpoint(void)
{
    int ret, status = 0;
    DB *db;

    if(ovdb_open_berkeleydb(OV_WRITE, 0))
	_exit(1);

    setproctitle("checkpoint");

    /* Open a database and close it.  This is so a necessary initialization
       gets performed (by the db->open function).  */

    ret = db_create(&db, OVDBenv, 0);
    if (ret != 0) {
        warn("OVDB: checkpoint: db_create: %s", db_strerror(ret));
        _exit(1);
    }
    ret = db->open(db, NULL, "version", NULL, DB_BTREE, DB_CREATE, 0666);
    if (ret != 0) {
        db->close(db, 0);
        warn("OVDB: checkpoint: version open: %s", db_strerror(ret));
        _exit(1);
    }
    db->close(db, 0);


    while(!signalled) {
	OVDBenv->txn_checkpoint(OVDBenv, 2048, 1, 0);
	sleep(30);
    }

    ovdb_close_berkeleydb();
    _exit(status);
}

static void logremover(void)
{
    int ret, status = 0;
    char **listp, **p;

    if(ovdb_open_berkeleydb(OV_WRITE, 0))
	_exit(1);

    setproctitle("logremover");

    while(!signalled) {
	ret = OVDBenv->log_archive(OVDBenv, &listp, DB_ARCH_ABS);
	if(ret != 0) {
            warn("OVDB: log_archive: %s", db_strerror(ret));
	    status = 1;
	    break;
	}
	if(listp != NULL) {
	    for(p = listp; *p; p++)
		unlink(*p);
	    free(listp);
	}
	sleep(45);
    }

    ovdb_close_berkeleydb();
    _exit(status);
}

static int start_process(pid_t *pid, void (*func)(void))
{
    pid_t child;

    switch(child = fork()) {
    case 0:
	(*func)();
	_exit(0);
    case -1:
        syswarn("cannot fork");
	return -1;
    default:
	*pid = child;
	return 0;
    }
    /*NOTREACHED*/
}

static void cleanup(int status)
{
    int cs;

    if(deadlockpid)
	kill(deadlockpid, SIGTERM);
    if(checkpointpid)
	kill(checkpointpid, SIGTERM);
    if(logremoverpid)
	kill(logremoverpid, SIGTERM);

    xsignal(SIGINT, SIG_DFL);
    xsignal(SIGTERM, SIG_DFL);
    xsignal(SIGHUP, SIG_DFL);

    if(deadlockpid)
	waitpid(deadlockpid, &cs, 0);
    if(checkpointpid)
	waitpid(checkpointpid, &cs, 0);
    if(logremoverpid)
	waitpid(logremoverpid, &cs, 0);

    unlink(concatpath(innconf->pathrun, OVDB_MONITOR_PIDFILE));
    exit(status);
}

static void monitorloop(void)
{
    int cs, restartit;
    pid_t child;

    while(!signalled) {
	child = waitpid(-1, &cs, WNOHANG);
	if(child > 0) {
	    if(WIFSIGNALED(cs)) {
		restartit = 0;
	    } else {
		if(WEXITSTATUS(cs) == 0)
		    restartit = 1;
		else
		    restartit = 0;
	    }
	    if(child == deadlockpid) {
		deadlockpid = 0;
		if(restartit && start_process(&deadlockpid, deadlock))
		    cleanup(1);
	    } else if(child == checkpointpid) {
		checkpointpid = 0;
		if(restartit && start_process(&checkpointpid, checkpoint))
		    cleanup(1);
	    } else if(child == logremoverpid) {
		logremoverpid = 0;
		if(restartit && start_process(&logremoverpid, logremover))
		    cleanup(1);
	    }
	    if(!restartit)
		cleanup(1);
	}
	sleep(20);
    }
    cleanup(0);
}


int main(int argc, char **argv)
{
    char *pidfile;

    setproctitle_init(argc, argv);

    openlog("ovdb_monitor", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);
    message_program_name = "ovdb_monitor";

    if(argc != 2 || strcmp(argv[1], SPACES))
        die("should be started by ovdb_init");
    message_handlers_warn(1, message_log_syslog_err);
    message_handlers_die(1, message_log_syslog_err);

    if (!innconf_read(NULL))
	exit(1);

    if(strcmp(innconf->ovmethod, "ovdb"))
        die("ovmethod not set to ovdb in inn.conf");
    if(!ovdb_check_user())
        die("command must be run as runasuser user");
    if(!ovdb_getlock(OVDB_LOCK_ADMIN))
        die("cannot lock database");

    xsignal(SIGINT, sigfunc);
    xsignal(SIGTERM, sigfunc);
    xsignal(SIGHUP, sigfunc);

    pidfile = concatpath(innconf->pathrun, OVDB_MONITOR_PIDFILE);
    if(putpid(pidfile))
	exit(1);
    if(start_process(&deadlockpid, deadlock))
	cleanup(1);
    if(start_process(&checkpointpid, checkpoint))
	cleanup(1);
    if(start_process(&logremoverpid, logremover))
	cleanup(1);

    monitorloop();

    /* Never reached. */
    return 1;
}

#endif /* HAVE_BDB */


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
#include "portable/wait.h"
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>

#include "inn/innconf.h"
#include "inn/messages.h"
#include "libinn.h"
#include "ov.h"

#include "../storage/ovdb/ovdb.h"
#include "../storage/ovdb/ovdb-private.h"

#ifndef USE_BERKELEY_DB

int main(int argc UNUSED, char **argv UNUSED)
{
    exit(0);
}

#else /* USE_BERKELEY_DB */

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
#if DB_VERSION_MAJOR == 2
	ret = lock_detect(OVDBenv->lk_info, 0, atype);
#elif DB_VERSION_MAJOR == 3
	ret = lock_detect(OVDBenv, 0, atype, NULL);
#else
	ret = OVDBenv->lock_detect(OVDBenv, 0, atype, NULL);
#endif
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
#if DB_VERSION_MAJOR == 2
    DB_INFO dbinfo;
#endif

    if(ovdb_open_berkeleydb(OV_WRITE, 0))
	_exit(1);

    setproctitle("checkpoint");

    /* Open a database and close it.  This is so a necessary initialization
       gets performed (by the db->open function).  */

#if DB_VERSION_MAJOR == 2
    memset(&dbinfo, 0, sizeof dbinfo);
    ret = db_open("version", DB_BTREE, DB_CREATE, 0666, OVDBenv, &dbinfo, &db);
    if (ret != 0) {
        warn("OVDB: checkpoint: db_open failed: %s", db_strerror(ret));
        _exit(1);
    }
#else
    ret = db_create(&db, OVDBenv, 0);
    if (ret != 0) {
        warn("OVDB: checkpoint: db_create: %s", db_strerror(ret));
        _exit(1);
    }
    ret = db->open(db, "version", NULL, DB_BTREE, DB_CREATE, 0666);
    if (ret != 0) {
        db->close(db, 0);
        warn("OVDB: checkpoint: version open: %s", db_strerror(ret));
        _exit(1);
    }
#endif
    db->close(db, 0);


    while(!signalled) {
#if DB_VERSION_MAJOR == 2
	ret = txn_checkpoint(OVDBenv->tx_info, 2048, 1);
#elif DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR == 0
	ret = txn_checkpoint(OVDBenv, 2048, 1);
#elif DB_VERSION_MAJOR == 3
	ret = txn_checkpoint(OVDBenv, 2048, 1, 0);
#else
	ret = OVDBenv->txn_checkpoint(OVDBenv, 2048, 1, 0);
#endif
	if(ret != 0 && ret != DB_INCOMPLETE) {
            warn("OVDB: txn_checkpoint: %s", db_strerror(ret));
	    status = 1;
	    break;
	}
	if(ret == DB_INCOMPLETE)
	    sleep(2);
	else
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
#if DB_VERSION_MAJOR == 2
	ret = log_archive(OVDBenv->lg_info, &listp, DB_ARCH_ABS, malloc);
#elif DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR <= 2
	ret = log_archive(OVDBenv, &listp, DB_ARCH_ABS, malloc);
#elif DB_VERSION_MAJOR == 3
	ret = log_archive(OVDBenv, &listp, DB_ARCH_ABS);
#else
	ret = OVDBenv->log_archive(OVDBenv, &listp, DB_ARCH_ABS);
#endif
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
        die("command must be run as user " NEWSUSER);
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

#endif /* USE_BERKELEY_DB */


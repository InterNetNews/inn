/*
 * ovdb.c
 * ovdb 2.00 beta4
 * Overview storage using BerkeleyDB 2.x/3.x
 *
 * 2000-12-12 : Add support for BerkeleyDB DB_SYSTEM_MEM option, controlled
 *            : by ovdb.conf 'useshm' and 'shmkey'
 * 2000-11-27 : Update for DB 3.2.x compatibility
 * 2000-11-13 : New 'readserver' feature
 * 2000-10-10 : ovdb_search now closes the cursor right after the last
 *              record is read.
 * 2000-10-05 : artnum member of struct datakey changed from ARTNUM to u_int32_t.
 *              OS's where sizeof(long)==8 will have to rebuild their databases
 *              after this update.
 * 2000-10-05 : from Dan Riley: struct datakey needs to be zero'd, for
 *              64-bit OSs where the struct has internal padding bytes.
 * 2000-09-29 : ovdb_expiregroup can now fix incorrect counts; use new
 *              inn/version.h so can have same ovdb.c for 2.3.0, 2.3.1, and 2.4
 * 2000-09-28 : low mark in ovdb_expiregroup still wasn't right
 * 2000-09-27 : Further improvements to ovdb_expiregroup: restructured the
 *              loop; now updates groupinfo as it goes along rather than
 *              counting records at the end, which prevents a possible
 *              deadlock.
 * 2000-09-19 : *lo wasn't being set in ovdb_expiregroup
 * 2000-09-15 : added ovdb_check_user(); tweaked some error msgs; fixed an
 *              improper use of RENEW
 * 2000-08-28:  New major release: version 2.00 (beta)
 *    + "groupsbyname" and "groupstats" databases replaced with "groupinfo".
 *    + ovdb_recover, ovdb_upgrade, and dbprocs are now deprecated; their
 *         functionality is now in ovdb_init and ovdb_monitor.
 *    + ovdb_init can upgrade a database from the old version of ovdb to
 *         work with this version.
 *    + Rewrote ovdb_expiregroup(); it can now re-write OV data rather
 *         than simply deleting old keys (which can leave 'holes' that result
 *         in inefficient disk-space use).
 *    + Add "nocompact" to ovdb.conf, which controls whether ovdb_expiregroup()
 *         rewrites OV data.
 *    + No longer needs the BerkeleyDB tools db_archive, db_checkpoint, and
 *         db_deadlock.  That functionality is now in ovdb_monitor.
 *    + ovdb_open() won't succeed if ovdb_monitor is not running.  This will
 *         prevent the problems that happen if the database is not regularly
 *         checkpointed and deadlock-tested.
 *    + Internal group IDs (32-bit ints) are now reused.
 *    + Add "maxlocks" to ovdb.conf, which will set the DB lk_max parameter.
 *    + Pull "test" code out into ovdb_stat.  ovdb_stat will also provide
 *         functionality similar to the BerkeleyDB "db_stat" command.
 *    + Update docs: write man pages for the new ovdb_* commands; update
 *         ovdb.pod
 *         
 * 2000-07-11 : fix possible alignment problem; add test code
 * 2000-07-07 : bugfix: timestamp handling
 * 2000-06-10 : Modified groupnum() interface; fix ovdb_add() to return FALSE
 *              for certain groupnum() errors
 * 2000-06-08 : Added BerkeleyDB 3.1.x compatibility
 * 2000-04-09 : Tweak some default parameters; store aliased group info
 * 2000-03-29 : Add DB_RMW flag to the 'get' of get-modify-put sequences
 * 2000-02-17 : Update expire behavior to be consistent with current
 *              ov3 and buffindexed
 * 2000-01-13 : Fix to make compatible with unmodified nnrpd/article.c
 * 2000-01-04 : Added data versioning
 * 1999-12-20 : Added BerkeleyDB 3.x compatibility
 * 1999-12-06 : First Release -- H. Kehoe <hakehoe@avalon.net>
 */

#include "config.h"
#include "clibrary.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_LIMITS_H
# include <limits.h>
#endif
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif
#include <pwd.h>
#include "macros.h"
#include "conffile.h"
#include "libinn.h"
#include "paths.h"
#include "storage.h"
#include "ov.h"
#include "ovinterface.h"
#include "ovdb.h"

#ifdef HAVE_UNIX_DOMAIN_SOCKETS
# include <sys/un.h>
#endif

#ifdef HAVE_INN_VERSION_H
#include "inn/version.h"
#else
/* 2.3.0 doesn't have version.h */
#define INN_VERSION_MAJOR 2
#define INN_VERSION_MINOR 3
#define INN_VERSION_PATCH 0
#endif

#if INN_VERSION_MINOR >= 4
#include "portable/time.h"
#else
#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#endif

#if INN_VERSION_MINOR == 3
#define close_on_exec(fd, flag) CloseOnExec((fd), (flag))
#endif

#ifndef USE_BERKELEY_DB

/* Provide stub functions if we don't have db */

BOOL ovdb_open(int mode)
{
    syslog(L_FATAL, "OVDB: ovdb support not enabled");
    return FALSE;
}

BOOL ovdb_groupstats(char *group, int *lo, int *hi, int *count, int *flag)
{ return FALSE; }

BOOL ovdb_groupadd(char *group, ARTNUM lo, ARTNUM hi, char *flag)
{ return FALSE; }

BOOL ovdb_groupdel(char *group)
{ return FALSE; }

BOOL ovdb_add(char *group, ARTNUM artnum, TOKEN token, char *data, int len, time_t arrived, time_t expires)
{ return FALSE; }

BOOL ovdb_cancel(TOKEN token)
{ return FALSE; }

void *ovdb_opensearch(char *group, int low, int high)
{ return NULL; }

BOOL ovdb_search(void *handle, ARTNUM *artnum, char **data, int *len, TOKEN *token, time_t *arrived)
{ return FALSE; }

void ovdb_closesearch(void *handle) { }

BOOL ovdb_getartinfo(char *group, ARTNUM artnum, char **data, int *len, TOKEN *token)
{ return FALSE; }

BOOL ovdb_expiregroup(char *group, int *lo, struct history *h)
{ return FALSE; }

BOOL ovdb_ctl(OVCTLTYPE type, void *val)
{ return FALSE; }

void ovdb_close(void) { }

#else /* USE_BERKELEY_DB */

#include <db.h>

#include "ovdb-private.h"

#define EXPIREGROUP_TXN_SIZE 100

struct ovdb_conf ovdb_conf;
DB_ENV *OVDBenv = NULL;
int ovdb_errmode = OVDB_ERR_SYSLOG;

static int OVDBmode;
static BOOL Cutofflow;
static DB **dbs = NULL;
static int oneatatime = 0;
static int current_db = -1;
static time_t eo_start = 0;
static int clientmode = 0;

static DB *groupinfo = NULL;
static DB *groupaliases = NULL;

#define OVDBtxn_nosync	1
#define OVDBnumdbfiles	2
#define OVDBpagesize	3
#define OVDBcachesize	4
#define OVDBminkey	5
#define OVDBmaxlocks	6
#define OVDBnocompact	7
#define OVDBreadserver	8
#define OVDBnumrsprocs	9
#define OVDBmaxrsconn	10
#define OVDBuseshm	11
#define OVDBshmkey	12

static CONFTOKEN toks[] = {
  { OVDBtxn_nosync, "txn_nosync" },
  { OVDBnumdbfiles, "numdbfiles" },
  { OVDBpagesize, "pagesize" },
  { OVDBcachesize, "cachesize" },
  { OVDBminkey, "minkey" },
  { OVDBmaxlocks, "maxlocks" },
  { OVDBnocompact, "nocompact" },
  { OVDBreadserver, "readserver" },
  { OVDBnumrsprocs, "numrsprocs" },
  { OVDBmaxrsconn, "maxrsconn" },
  { OVDBuseshm, "useshm" },
  { OVDBshmkey, "shmkey" },
  { 0, NULL },
};

#define _PATH_OVDBCONF "ovdb.conf"

/*********** readserver functions ***********/

static int clientfd = -1;

/* read client send and recieve functions.  If there is
   connection trouble, we just bail out. */

static int csend(void *data, int n)
{
    int r, p = 0;

    if(n == 0)
	return 0;

    while(p < n) {
	r = write(clientfd, (char *)data + p, n - p);
	if(r <= 0) {
	    if(r < 0 && errno == EINTR)
		continue;
	    syslog(LOG_ERR, "OVDB: rc: cant write: %m");
	    exit(1);
	}
	p+= r;
    }
    return p;
}

static int crecv(void *data, int n)
{
    int r, p = 0;

    if(n == 0)
	return 0;

    while(p < n) {
	r = read(clientfd, (char *)data + p, n - p);
	if(r <= 0) {
	    if(r < 0 && errno == EINTR)
		continue;
	    syslog(LOG_ERR, "OVDB: rc: cant read: %m");
	    exit(1);
	}
	p+= r;
    }
    return p;
}

/* Attempt to connect to the readserver.  If anything fails, we
   return -1 so that ovdb_open can open the database directly. */

static int client_connect()
{
    int r, p = 0;
    char *path;
#ifdef HAVE_UNIX_DOMAIN_SOCKETS
    struct sockaddr_un sa;
#else
    struct sockaddr_in sa;
#endif
    char banner[sizeof(OVDB_SERVER_BANNER)];
    fd_set fds;
    struct timeval timeout;

#ifdef HAVE_UNIX_DOMAIN_SOCKETS
    clientfd = socket(AF_UNIX, SOCK_STREAM, 0);
#else
    clientfd = socket(AF_INET, SOCK_STREAM, 0);
#endif
    if(clientfd < 0) {
	syslog(LOG_ERR, "OVDB: rc: socket: %m");
	return -1;
    }

#ifdef HAVE_UNIX_DOMAIN_SOCKETS
    sa.sun_family = AF_UNIX;
    path = concatpath(innconf->pathrun, OVDB_SERVER_SOCKET);
    strcpy(sa.sun_path, path);
    free(path);
#else
    sa.sin_family = AF_INET;
    sa.sin_port = 0;
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bind(clientfd, (struct sockaddr *) &sa, sizeof sa);
    sa.sin_port = htons(OVDB_SERVER_PORT);
#endif
    if((r = connect(clientfd, (struct sockaddr *) &sa, sizeof sa)) != 0) {
	syslog(LOG_ERR, "OVDB: rc: cant connect to server: %m");
	close(clientfd);
	clientfd = -1;
	return -1;
    }

    while(p < sizeof(OVDB_SERVER_BANNER)) {
	FD_ZERO(&fds);
	FD_SET(clientfd, &fds);
	timeout.tv_sec = 30;
	timeout.tv_usec = 0;

	r = select(clientfd+1, &fds, NULL, NULL, &timeout);

	if(r < 0) {
	    if(errno == EINTR)
	    	continue;
	    syslog(LOG_ERR, "OVDB: rc: select: %m");
	    close(clientfd);
	    clientfd = -1;
	    return -1;
	}
	if(r == 0) {
	    syslog(LOG_ERR, "OVDB: rc: timeout waiting for server");
	    close(clientfd);
	    clientfd = -1;
	    return -1;
    	}

	r = read(clientfd, banner + p, sizeof(OVDB_SERVER_BANNER) - p);
	if(r <= 0) {
	    if(r < 0 && errno == EINTR)
		continue;
	    syslog(LOG_ERR, "OVDB: rc: cant read: %m");
	    close(clientfd);
	    clientfd = -1;
	    return -1;
	}
	p+= r;
    }

    if(memcmp(banner, OVDB_SERVER_BANNER, sizeof(OVDB_SERVER_BANNER))) {
	syslog(LOG_ERR, "OVDB: rc: unknown reply from server");
	close(clientfd);
	clientfd = -1;
	return -1;
    }
    return 0;
}

static void client_disconnect(void)
{
    struct rs_cmd rs;
    if(clientfd != -1) {
	rs.what = CMD_QUIT;

	csend(&rs, sizeof rs);
    }
}


/*********** internal functions ***********/

#if DB_VERSION_MAJOR == 2
char *db_strerror(int err)
{
    switch(err) {
    case DB_RUNRECOVERY:
	return "Recovery Needed";
    default:
	return strerror(err);
    }
}
#endif /* DB_VERSION_MAJOR == 2 */


static BOOL conf_bool_val(char *str, BOOL *value)
{
    if(caseEQ(str, "on") || caseEQ(str, "true") || caseEQ(str, "yes")) {
	*value = TRUE;
	return TRUE;
    }
    if(caseEQ(str, "off") || caseEQ(str, "false") || caseEQ(str, "no")) {
	*value = FALSE;
	return TRUE;
    }
    return FALSE;
}

static BOOL conf_long_val(char *str, long *value)
{
    long v;

    errno = 0;
    v = strtol(str, NULL, 10);
    if(v == 0 && errno != 0) {
	return FALSE;
    }
    *value = v;
    return TRUE;
}

void read_ovdb_conf(void)
{
    static int confread = 0;
    int done = 0;
    char *path;
    CONFFILE *f;
    CONFTOKEN *tok;
    BOOL b;
    long l;

    if(confread)
	return;

    /* defaults */
    ovdb_conf.home = innconf->pathoverview;
    ovdb_conf.txn_nosync = 1;
    ovdb_conf.numdbfiles = 32;
    ovdb_conf.pagesize = 8192;
    ovdb_conf.cachesize = 8000 * 1024;
    ovdb_conf.minkey = 0;
    ovdb_conf.maxlocks = 4000;
    ovdb_conf.nocompact = 1000;
    ovdb_conf.readserver = 0;
    ovdb_conf.numrsprocs = 5;
    ovdb_conf.maxrsconn = 0;
    ovdb_conf.useshm = 0;
    ovdb_conf.shmkey = 6400;

    path = concatpath(innconf->pathetc, _PATH_OVDBCONF);
    f = CONFfopen(path);
    free(path);

    if(f) {
	while(!done && (tok = CONFgettoken(toks, f))) {
	    switch(tok->type) {
	    case OVDBtxn_nosync:
		tok = CONFgettoken(0, f);
		if(!tok) {
		    done = 1;
		    continue;
		}
		if(conf_bool_val(tok->name, &b)) {
		    ovdb_conf.txn_nosync = b;
		}
		break;
	    case OVDBnumdbfiles:
		tok = CONFgettoken(0, f);
		if(!tok) {
		    done = 1;
		    continue;
		}
		if(conf_long_val(tok->name, &l) && l > 0) {
		    ovdb_conf.numdbfiles = l;
		}
		break;
	    case OVDBpagesize:
		tok = CONFgettoken(0, f);
		if(!tok) {
		    done = 1;
		    continue;
		}
		if(conf_long_val(tok->name, &l) && l > 0) {
		    ovdb_conf.pagesize = l;
		}
		break;
	    case OVDBcachesize:
		tok = CONFgettoken(0, f);
		if(!tok) {
		    done = 1;
		    continue;
		}
		if(conf_long_val(tok->name, &l) && l > 0) {
		    ovdb_conf.cachesize = l * 1024;
		}
		break;
	    case OVDBminkey:
		tok = CONFgettoken(0, f);
		if(!tok) {
		    done = 1;
		    continue;
		}
		if(conf_long_val(tok->name, &l) && l > 1) {
		    ovdb_conf.minkey = l;
		}
		break;
	    case OVDBmaxlocks:
		tok = CONFgettoken(0, f);
		if(!tok) {
		    done = 1;
		    continue;
		}
		if(conf_long_val(tok->name, &l) && l > 0) {
		    ovdb_conf.maxlocks = l;
		}
		break;
	    case OVDBnocompact:
		tok = CONFgettoken(0, f);
		if(!tok) {
		    done = 1;
		    continue;
		}
		if(conf_long_val(tok->name, &l) && l >= 0) {
		    ovdb_conf.nocompact = l;
		}
		break;
	    case OVDBreadserver:
		tok = CONFgettoken(0, f);
		if(!tok) {
		    done = 1;
		    continue;
		}
		if(conf_bool_val(tok->name, &b)) {
		    ovdb_conf.readserver = b;
		}
		break;
	    case OVDBnumrsprocs:
		tok = CONFgettoken(0, f);
		if(!tok) {
		    done = 1;
		    continue;
		}
		if(conf_long_val(tok->name, &l) && l > 0) {
		    ovdb_conf.numrsprocs = l;
		}
		break;
	    case OVDBmaxrsconn:
		tok = CONFgettoken(0, f);
		if(!tok) {
		    done = 1;
		    continue;
		}
		if(conf_long_val(tok->name, &l) && l >= 0) {
		    ovdb_conf.maxrsconn = l;
		}
		break;
	    case OVDBuseshm:
		tok = CONFgettoken(0, f);
		if(!tok) {
		    done = 1;
		    continue;
		}
		if(conf_bool_val(tok->name, &b)) {
		    ovdb_conf.useshm = b;
		}
		break;
	    case OVDBshmkey:
		tok = CONFgettoken(0, f);
		if(!tok) {
		    done = 1;
		    continue;
		}
		if(conf_long_val(tok->name, &l) && l >= 0) {
		    ovdb_conf.shmkey = l;
		}
		break;
	    }
	}
	CONFfclose(f);
    }

    /* If user did not specify minkey, choose one based on pagesize */
    if(ovdb_conf.minkey == 0) {
	ovdb_conf.minkey = ovdb_conf.pagesize / 2048 - 1;
	if(ovdb_conf.minkey < 2)
	    ovdb_conf.minkey = 2;
    }

    confread = 1;
}


/* Function that db will use to report errors */
static void OVDBerror(char *db_errpfx, char *buffer)
{
    switch(ovdb_errmode) {
    case OVDB_ERR_SYSLOG:
	syslog(L_ERROR, "OVDB: %s", buffer);
	break;
    case OVDB_ERR_STDERR:
	fprintf(stderr, "OVDB: %s\n", buffer);
	break;
    }
}

static u_int32_t _db_flags = 0;
#if DB_VERSION_MAJOR == 2
static DB_INFO   _dbinfo;
#endif

static int open_db_file(int which)
{
    int ret;
    char name[10];

    if(dbs[which] != NULL)
	return 0;

    sprintf(name, "ov%05d", which);

#if DB_VERSION_MAJOR == 2
    if(ret = db_open(name, DB_BTREE, _db_flags, 0666, OVDBenv,
		    &_dbinfo, &(dbs[which]))) {
	dbs[which] = NULL;
	return ret;
    }
#else
    if(ret = db_create(&(dbs[which]), OVDBenv, 0))
	return ret;

    if(ovdb_conf.minkey > 0)
	(dbs[which])->set_bt_minkey(dbs[which], ovdb_conf.minkey);
    if(ovdb_conf.pagesize > 0)
	(dbs[which])->set_pagesize(dbs[which], ovdb_conf.pagesize);

    if(ret = (dbs[which])->open(dbs[which], name, NULL, DB_BTREE,
		_db_flags, 0666)) {
	(dbs[which])->close(dbs[which], 0);
	dbs[which] = NULL;
	return ret;
    }
#endif
    return 0;
}

static void close_db_file(int which)
{
    if(which == -1 || dbs[which] == NULL)
	return;
    
    dbs[which]->close(dbs[which], 0);
    dbs[which] = NULL;
}

static int which_db(char *group)
{
    HASH grouphash;
    unsigned int i;

    grouphash = Hash(group, strlen(group));
    memcpy(&i, &grouphash, sizeof(i));
    return i % ovdb_conf.numdbfiles;
}

static DB *get_db_bynum(int which)
{
    int ret;
    if(which >= ovdb_conf.numdbfiles)
	return NULL;
    if(oneatatime) {
	if(which != current_db && current_db != -1)
	    close_db_file(current_db);

	if(ret = open_db_file(which))
	    syslog(L_ERROR, "OVDB: open_db_file failed: %s", db_strerror(ret));

	current_db = which;
    }
    return(dbs[which]);
}


int ovdb_getgroupinfo(char *group, struct groupinfo *gi, int ignoredeleted, DB_TXN *tid, int getflags)
{
    int ret;
    DBT key, val;

    if(group == NULL)	/* just in case */
	return DB_NOTFOUND;

    memset(&key, 0, sizeof key);
    memset(&val, 0, sizeof val);

    key.data = group;
    key.size = strlen(group);
    val.data = gi;
    val.ulen = sizeof(struct groupinfo);
    val.flags = DB_DBT_USERMEM;

    if(ret = groupinfo->get(groupinfo, tid, &key, &val, getflags))
	return ret;

    if(val.size != sizeof(struct groupinfo)) {
	syslog(L_ERROR, "OVDB: wrong size for %s groupinfo (%u)",
	    group, val.size);
	return DB_NOTFOUND;
    }

    if(ignoredeleted && (gi->status & GROUPINFO_DELETED))
	return DB_NOTFOUND;

    return 0;
}

#define GROUPID_MAX_FREELIST 10240
#define GROUPID_MIN_FREELIST 100

/* allocate a new group ID and return in gno */
/* must be used in a transaction */
static int groupid_new(group_id_t *gno, DB_TXN *tid)
{
    DBT key, val;
    int ret, n;
    group_id_t newgno, *freelist, one;

    memset(&key, 0, sizeof key);
    memset(&val, 0, sizeof val);

    key.data = "!groupid_freelist";
    key.size = sizeof("!groupid_freelist");

    if(ret = groupinfo->get(groupinfo, tid, &key, &val, DB_RMW)) {
	if(ret == DB_NOTFOUND) {
	    val.size = sizeof(group_id_t);
	    val.data = &one;
	    one = 1;
	} else {
	    return ret;
	}
    }

    if(val.size % sizeof(group_id_t)) {
	syslog(L_ERROR, "OVDB: invalid size (%d) for !groupid_freelist",
		val.size);
	return EINVAL;
    }

    n = val.size / sizeof(group_id_t);
    freelist = NEW(group_id_t, n);
    memcpy(freelist, val.data, val.size);
    if(n <= GROUPID_MIN_FREELIST ) {
	newgno = freelist[n-1];
	(freelist[n-1])++;
	val.data = freelist;
    } else {
	newgno = freelist[0];
	val.data = &(freelist[1]);
	val.size -= sizeof(group_id_t);
    }

    if(ret = groupinfo->put(groupinfo, tid, &key, &val, 0)) {
	DISPOSE(freelist);
	return ret;
    }

    DISPOSE(freelist);
    *gno = newgno;
    return 0;
}

/* mark given group ID as "unused" */
/* must be used in a transaction */
static int groupid_free(group_id_t gno, DB_TXN *tid)
{
    DBT key, val;
    int ret, n, i;
    group_id_t *freelist;

    memset(&key, 0, sizeof key);
    memset(&val, 0, sizeof val);

    key.data = "!groupid_freelist";
    key.size = sizeof("!groupid_freelist");

    if(ret = groupinfo->get(groupinfo, tid, &key, &val, DB_RMW)) {
	return ret;
    }

    if(val.size % sizeof(group_id_t)) {
	syslog(L_ERROR, "OVDB: invalid size (%d) for !groupid_freelist",
		val.size);
	return EINVAL;
    }

    n = val.size / sizeof(group_id_t);
    if(n > GROUPID_MAX_FREELIST)
	return 0;
    freelist = NEW(group_id_t, n+1);
    memcpy(freelist, val.data, val.size);

    if(gno >= freelist[n-1]) {	/* shouldn't happen */
	DISPOSE(freelist);
	return 0;
    }
    for(i = 0; i < n-1; i++) {
	if(gno == freelist[i]) {	/* already on freelist */
	    DISPOSE(freelist);
	    return 0;
	}
    }
	
    freelist[n] = freelist[n-1];
    freelist[n-1] = gno;
    val.data = freelist;
    val.size += sizeof(group_id_t);

    ret = groupinfo->put(groupinfo, tid, &key, &val, 0);

    DISPOSE(freelist);
    return ret;
}


static int delete_all_records(int whichdb, group_id_t gno)
{
    DB *db;
    DBC *dbcursor;
    DBT key, val;
    struct datakey dk;
    int ret;

    memset(&key, 0, sizeof key);
    memset(&val, 0, sizeof val);
    memset(&dk, 0, sizeof dk);
    dk.groupnum = gno;
    dk.artnum = 0;

    db = get_db_bynum(whichdb);
    if(db == NULL)
	return DB_NOTFOUND;

    /* get a cursor to traverse the ov records and delete them */
    if(ret = db->cursor(db, NULL, &dbcursor, 0)) {
	if(ret != TRYAGAIN)
	    syslog(L_ERROR, "OVDB: delete_all_records: db->cursor: %s", db_strerror(ret));
	return ret;
    }

    key.data = &dk;
    key.size = sizeof dk;
    val.flags = DB_DBT_PARTIAL;

    switch(ret = dbcursor->c_get(dbcursor, &key, &val, DB_SET_RANGE)) {
    case 0:
    case DB_NOTFOUND:
	break;
    default:
	syslog(L_ERROR, "OVDB: delete_all_records: dbcursor->c_get: %s", db_strerror(ret));
    }

    while(ret == 0
	&& key.size == sizeof dk
	&& !memcmp(key.data, &(dk.groupnum), sizeof(dk.groupnum))) {

	if(ret = dbcursor->c_del(dbcursor, 0))
	    break;

	ret = dbcursor->c_get(dbcursor, &key, &val, DB_NEXT);
    }
    dbcursor->c_close(dbcursor);
    if(ret == DB_NOTFOUND)
	return 0;
    return ret;
}


/* This function deletes overview records for deleted or forgotton groups */
/* argument: 0 = process deleted groups   1 = process forgotton groups */
static BOOL delete_old_stuff(int forgotton)
{
    DBT key, val;
    DBC *cursor;
    DB_TXN *tid;
    struct groupinfo gi;
    char **dellist;
    int listlen = 20, listcount = 0;
    int i, ret;

    dellist = NEW(char *, listlen);

    memset(&key, 0, sizeof key);
    memset(&val, 0, sizeof val);

    val.data = &gi;
    val.ulen = val.dlen = sizeof gi;
    val.flags = DB_DBT_USERMEM | DB_DBT_PARTIAL;

    /* get a cursor to traverse all of the groupinfo records */
    if(ret = groupinfo->cursor(groupinfo, NULL, &cursor, 0)) {
	syslog(L_ERROR, "OVDB: delete_old_stuff: groupinfo->cursor: %s", db_strerror(ret));
	DISPOSE(dellist);
	return FALSE;
    }

    while((ret = cursor->c_get(cursor, &key, &val, DB_NEXT)) == 0) {
	if(key.size == sizeof("!groupid_freelist") &&
		!strcmp("!groupid_freelist", key.data))
	    continue;
	if(val.size != sizeof(struct groupinfo)) {
	    syslog(L_ERROR, "OVDB: delete_old_stuff: wrong size for groupinfo record");
	    continue;
	}
	if((!forgotton && (gi.status & GROUPINFO_DELETED))
		|| (forgotton && (gi.expired < eo_start))) {
	    dellist[listcount] = NEW(char, key.size + 1);
	    memcpy(dellist[listcount], key.data, key.size);
	    dellist[listcount][key.size] = 0;
	    listcount++;
	    if(listcount >= listlen) {
		listlen += 20;
		RENEW(dellist, char *, listlen);
	    }
	}
    }
    cursor->c_close(cursor);
    if(ret != DB_NOTFOUND)
	syslog(L_ERROR, "OVDB: delete_old_stuff: cursor->c_get: %s", db_strerror(ret));

    for(i = 0; i < listcount; i++) {
	TXN_START(t_dos, tid);

	if(tid==NULL)
	    goto out;

	switch(ret = ovdb_getgroupinfo(dellist[i], &gi, FALSE, tid, DB_RMW)) {
	case 0:
	    break;
	case TRYAGAIN:
	    TXN_RETRY(t_dos, tid);
	case DB_NOTFOUND:
	    TXN_ABORT(t_dos, tid);
	    continue;
	default:
	    syslog(L_ERROR, "OVDB: delete_old_stuff: getgroupinfo: %s\n", db_strerror(ret));
	    TXN_ABORT(t_dos, tid);
	    continue;
	}

	if(delete_all_records(gi.current_db, gi.current_gid)) {
	    TXN_ABORT(t_dos, tid);
	    continue;
	}

	switch(groupid_free(gi.current_gid, tid)) {
	case 0:
	    break;
	case TRYAGAIN:
	    TXN_RETRY(t_dos, tid);
	default:
	    TXN_ABORT(t_dos, tid);
	    continue;
	}

	if(gi.status & GROUPINFO_MOVING) {
	    if(delete_all_records(gi.new_db, gi.new_gid)) {
		TXN_ABORT(t_dos, tid);
		continue;
	    }
	    switch(groupid_free(gi.new_gid, tid)) {
	    case 0:
		break;
	    case TRYAGAIN:
		TXN_RETRY(t_dos, tid);
	    default:
		TXN_ABORT(t_dos, tid);
		continue;
	    }
	}
	key.data = dellist[i];
	key.size = strlen(dellist[i]);

	/* delete the corresponding groupaliases record (if exists) */
	switch(groupaliases->del(groupaliases, tid, &key, 0)) {
	case 0:
	case DB_NOTFOUND:
	    break;
	case TRYAGAIN:
	    TXN_RETRY(t_dos, tid);
	default:
	    TXN_ABORT(t_dos, tid);
	    continue;
	}

	switch(groupinfo->del(groupinfo, tid, &key, 0)) {
	case 0:
	case DB_NOTFOUND:
	    break;
	case TRYAGAIN:
	    TXN_RETRY(t_dos, tid);
	default:
	    TXN_ABORT(t_dos, tid);
	    continue;
	}

	TXN_COMMIT(t_dos, tid);
    }
out:
    for(i = 0; i < listcount; i++)
	DISPOSE(dellist[i]);
    DISPOSE(dellist);
    return TRUE;
}

static int count_records(struct groupinfo *gi)
{
    int ret;
    DB *db;
    DBC *cursor;
    DBT key, val;
    struct datakey dk;
    u_int32_t artnum, newlow = 0;

    memset(&key, 0, sizeof key);
    memset(&val, 0, sizeof val);
    memset(&dk, 0, sizeof dk);

    db = get_db_bynum(gi->current_db);
    if(db == NULL)
	return DB_NOTFOUND;

    dk.groupnum = gi->current_gid;
    dk.artnum = 0;
    key.data = &dk;
    key.size = key.ulen = sizeof dk;
    key.flags = DB_DBT_USERMEM;
    val.flags = DB_DBT_PARTIAL;

    gi->count = 0;

    if(ret = db->cursor(db, NULL, &cursor, 0))
	return ret;

    switch(ret = cursor->c_get(cursor, &key, &val, DB_SET_RANGE)) {
    case 0:
    case DB_NOTFOUND:
	break;
    default:
	cursor->c_close(cursor);
	return ret;
    }
    while(ret == 0 && key.size == sizeof(dk) && dk.groupnum == gi->current_gid) {
	artnum = ntohl(dk.artnum);
	if(newlow == 0 || newlow > artnum)
	    newlow = artnum;
	if(artnum > gi->high)
	    gi->high = artnum;
	gi->count++;

	ret = cursor->c_get(cursor, &key, &val, DB_NEXT);
    }
    cursor->c_close(cursor);
    if(gi->count == 0)
	gi->low = gi->high + 1;
    else
	gi->low = newlow;

    if(ret == DB_NOTFOUND)
	return 0;
    return ret;
}


/*
 * Locking:  OVopen() calls ovdb_getlock(OVDB_LOCK_NORMAL).  This
 * aquires a read (shared) lock on the lockfile.  Multiple processes
 * can have this file locked at the same time.  That way, if there
 * are any processes that are using the database, the lock file will
 * have one or more shared locks on it.
 *
 * ovdb_init, when starting, calls ovdb_getlock(OVDB_LOCK_EXCLUSIVE).
 * This tries to get a write (exclusive) lock, which will fail if
 * anyone has a shared lock.  This way, ovdb_init can tell if there
 * are any processes using the database.  If not, and the excl. lock
 * succeeds, ovdb_init is free to do DB_RUNRECOVER.
 *
 * ovdb_getlock() in the "normal" lock mode calls ovdb_check_monitor,
 * which looks for the OVDB_MONITOR_PIDFILE.  If said file does not
 * exist, or the PID in it does not exist, it will fail.  This will
 * prevent OVopen() from opening the database if ovdb_monitor is not running.
 *
 * The OVDB_LOCK_ADMIN mode is used by ovdb_monitor to get a shared lock
 * without testing the pidfile.
 */
static int lockfd = -1;
BOOL ovdb_getlock(int mode)
{
    if(lockfd == -1) {
	char *lockfn = concatpath(innconf->pathrun, OVDB_LOCKFN);
	lockfd = open(lockfn,
		mode == OVDB_LOCK_NORMAL ? O_RDWR : O_CREAT|O_RDWR, 0660);
	if(lockfd == -1) {
	    DISPOSE(lockfn);
	    if(errno == ENOENT)
		syslog(L_FATAL, "OVDB: can not open database unless ovdb_monitor is running.");
	    return FALSE;
	}
	close_on_exec(lockfd, TRUE);
	DISPOSE(lockfn);
    } else
	return TRUE;

    if(mode == OVDB_LOCK_NORMAL) {
	if(!ovdb_check_pidfile(OVDB_MONITOR_PIDFILE)) {
	    syslog(L_FATAL, "OVDB: can not open database unless ovdb_monitor is running.");
	    return FALSE;
	}
    }
    if(mode == OVDB_LOCK_EXCLUSIVE) {
	if(!inn_lock_file(lockfd, INN_LOCK_WRITE, FALSE)) {
	    close(lockfd);
	    lockfd = -1;
	    return FALSE;
	}
	return TRUE;
    } else {
	if(!inn_lock_file(lockfd, INN_LOCK_READ, FALSE)) {
	    close(lockfd);
	    lockfd = -1;
	    return FALSE;
	}
	return TRUE;
    }
}

BOOL ovdb_releaselock(void)
{
    BOOL r;
    if(lockfd == -1)
	return TRUE;
    r = inn_lock_file(lockfd, INN_LOCK_UNLOCK, FALSE);
    close(lockfd);
    lockfd = -1;
    return r;
}

BOOL ovdb_check_pidfile(char *file)
{
    int f, pid;
    char buf[SMBUF];
    char *pidfn = concatpath(innconf->pathrun, file);

    f = open(pidfn, O_RDONLY);
    if(f == -1) {
	if(errno != ENOENT)
	    syslog(L_FATAL, "OVDB: can't open %s: %m", pidfn);
	DISPOSE(pidfn);
	return FALSE;
    }
    memset(buf, 0, SMBUF);
    if(read(f, buf, SMBUF-1) < 0) {
	syslog(L_FATAL, "OVDB: can't read from %s: %m", pidfn);
	DISPOSE(pidfn);
	close(f);
	return FALSE;
    }
    close(f);
    DISPOSE(pidfn);
    pid = atoi(buf);
    if(pid <= 1) {
	return FALSE;
    }
    if(kill(pid, 0) < 0 && errno == ESRCH) {
	return FALSE;
    }
    return TRUE;
}

/* make sure the effective uid is that of NEWSUSER */
BOOL ovdb_check_user(void)
{
    struct passwd *p;
    static int result = -1;

    if(result == -1) {
	p = getpwnam(NEWSUSER);
	if(!p) {
	    syslog(L_FATAL, "OVDB: getpwnam(" NEWSUSER ") failed: %m");
	    return FALSE;
	}
	result = (p->pw_uid == geteuid());
    }
    return result;
}

static int check_version()
{
    int ret;
    DB *vdb;
    DBT key, val;
    u_int32_t dv;

#if DB_VERSION_MAJOR == 2
    DB_INFO dbinfo;
    memset(&dbinfo, 0, sizeof dbinfo);

    if(ret = db_open("version", DB_BTREE, _db_flags, 0666, OVDBenv,
		    &dbinfo, &vdb)) {
	syslog(L_FATAL, "OVDB: db_open failed: %s", db_strerror(ret));
	return ret;
    }
#else
    /* open version db */
    if(ret = db_create(&vdb, OVDBenv, 0)) {
	syslog(L_FATAL, "OVDB: open: db_create: %s", db_strerror(ret));
	return ret;
    }
    if(ret = vdb->open(vdb, "version", NULL, DB_BTREE,
		_db_flags, 0666)) {
	vdb->close(vdb, 0);
	syslog(L_FATAL, "OVDB: open: version->open: %s", db_strerror(ret));
	return ret;
    }
#endif
    memset(&key, 0, sizeof key);
    memset(&val, 0, sizeof val);
    key.data = "dataversion";
    key.size = sizeof("dataversion");
    if(ret = vdb->get(vdb, NULL, &key, &val, 0)) {
	if(ret != DB_NOTFOUND) {
	    syslog(L_FATAL, "OVDB: open: can't retrieve version: %s", db_strerror(ret));
	    vdb->close(vdb, 0);
	    return ret;
	}
    }
    if(ret == DB_NOTFOUND || val.size != sizeof dv) {
	dv = DATA_VERSION;
	if(!(OVDBmode & OV_WRITE)) {
	    vdb->close(vdb, 0);
	    return EACCES;
	}
	val.data = &dv;
	val.size = sizeof dv;
	if(ret = vdb->put(vdb, NULL, &key, &val, 0)) {
	    syslog(L_FATAL, "OVDB: open: can't store version: %s", db_strerror(ret));
	    vdb->close(vdb, 0);
	    return ret;
	}
    } else
	memcpy(&dv, val.data, sizeof dv);

    if(dv > DATA_VERSION) {
	syslog(L_FATAL, "OVDB: can't open database: unknown version %d", dv);
	vdb->close(vdb, 0);
	return EINVAL;
    }
    if(dv < DATA_VERSION) {
	syslog(L_FATAL, "OVDB: database is an old version, please run ovdb_init");
	vdb->close(vdb, 0);
	return EINVAL;
    }

    /* The version db also stores some config values, which will override the
       corresponding ovdb.conf setting. */

    key.data = "numdbfiles";
    key.size = sizeof("numdbfiles");
    if(ret = vdb->get(vdb, NULL, &key, &val, 0)) {
	if(ret != DB_NOTFOUND) {
	    syslog(L_FATAL, "OVDB: open: can't retrieve numdbfiles: %s", db_strerror(ret));
	    vdb->close(vdb, 0);
	    return ret;
	}
    }
    if(ret == DB_NOTFOUND || val.size != sizeof(ovdb_conf.numdbfiles)) {
	if(!(OVDBmode & OV_WRITE)) {
	    vdb->close(vdb, 0);
	    return EACCES;
	}
	val.data = &(ovdb_conf.numdbfiles);
	val.size = sizeof(ovdb_conf.numdbfiles);
	if(ret = vdb->put(vdb, NULL, &key, &val, 0)) {
	    syslog(L_FATAL, "OVDB: open: can't store numdbfiles: %s", db_strerror(ret));
	    vdb->close(vdb, 0);
	    return ret;
	}
    } else {
	memcpy(&(ovdb_conf.numdbfiles), val.data, sizeof(ovdb_conf.numdbfiles));
    }

    vdb->close(vdb, 0);
    return 0;
}


int ovdb_open_berkeleydb(int mode, int flags)
{
    int ret;
    u_int32_t ai_flags = DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN;

    OVDBmode = mode;
    read_ovdb_conf();

    if(OVDBenv != NULL)
	return 0;	/* already opened */

    if(OVDBmode & OV_WRITE) {
	_db_flags |= DB_CREATE;
	ai_flags |= DB_CREATE;
    } else {
	_db_flags |= DB_RDONLY;
    }
    if(flags & OVDB_RECOVER)
	ai_flags |= DB_RECOVER;

#if DB_VERSION_MAJOR == 2 || (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR < 2)
    if(ovdb_conf.txn_nosync)
	ai_flags |= DB_TXN_NOSYNC;
#endif

#if DB_VERSION_MAJOR == 2

    OVDBenv = NEW(DB_ENV,1);
    memset(OVDBenv, 0, sizeof(DB_ENV));

    OVDBenv->db_errcall = OVDBerror;
    OVDBenv->mp_size = ovdb_conf.cachesize;
    OVDBenv->lk_max = ovdb_conf.maxlocks;

    /* initialize environment */
    if(ret = db_appinit(ovdb_conf.home, NULL, OVDBenv, ai_flags)) {
	DISPOSE(OVDBenv);
	OVDBenv = NULL;
	syslog(L_FATAL, "OVDB: db_appinit failed: %s", db_strerror(ret));
	return ret;
    }
#else
    if(ret = db_env_create(&OVDBenv, 0)) {
	syslog(L_FATAL, "OVDB: db_env_create: %s", db_strerror(ret));
	return ret;
    }

    if(ovdb_conf.useshm)
	ai_flags |= DB_SYSTEM_MEM;
#if DB_VERSION_MAJOR >= 4 || (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR > 0)
    OVDBenv->set_shm_key(OVDBenv, ovdb_conf.shmkey);
#endif

    OVDBenv->set_errcall(OVDBenv, OVDBerror);
    OVDBenv->set_cachesize(OVDBenv, 0, ovdb_conf.cachesize, 1);
    OVDBenv->set_lk_max(OVDBenv, ovdb_conf.maxlocks);

#if DB_VERSION_MAJOR >= 4 || (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR >= 2)
    if(ovdb_conf.txn_nosync)
	OVDBenv->set_flags(OVDBenv, DB_TXN_NOSYNC, 1);
#endif

#if DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR == 0
    if(ret = OVDBenv->open(OVDBenv, ovdb_conf.home, NULL, ai_flags, 0666)) {
#else
    if(ret = OVDBenv->open(OVDBenv, ovdb_conf.home, ai_flags, 0666)) {
#endif

	OVDBenv->close(OVDBenv, 0);
	OVDBenv = NULL;
	syslog(L_FATAL, "OVDB: OVDBenv->open: %s", db_strerror(ret));
	return ret;
    }
#endif /* DB_VERSION_MAJOR == 2 */

    return 0;
}

BOOL ovdb_open(int mode)
{
    int i, ret;
    char name[50];
    DBT key, val;

#if DB_VERSION_MAJOR == 2
    DB_INFO dbinfo;
#endif

    if(OVDBenv != NULL || clientmode) {
	syslog(L_ERROR, "OVDB: ovdb_open called more than once");
	return FALSE;
    }

    read_ovdb_conf();
    if(ovdb_conf.readserver && mode == OV_READ)
	clientmode = 1;

    if(mode & OVDB_SERVER)
	clientmode = 0;

    if(clientmode) {
	if(client_connect() == 0)
	    return TRUE;
	clientmode = 0;
    }
    if(!ovdb_check_user()) {
	syslog(L_FATAL, "OVDB: must be running as " NEWSUSER " to access overview.");
	return FALSE;
    }
    if(!ovdb_getlock(OVDB_LOCK_NORMAL)) {
	syslog(L_FATAL, "OVDB: ovdb_getlock failed: %m");
	return FALSE;
    }

    if(ovdb_open_berkeleydb(mode, 0) != 0)
	return FALSE;

    if(check_version() != 0)
	return FALSE;

    if(mode & OV_WRITE || mode & OVDB_SERVER) {
	oneatatime = 0;
    } else {
	oneatatime = 1;
    }

#if DB_VERSION_MAJOR == 2
    memset(&_dbinfo, 0, sizeof _dbinfo);
    _dbinfo.db_pagesize = ovdb_conf.pagesize;
    _dbinfo.bt_minkey = ovdb_conf.minkey;
#endif

    dbs = NEW(DB *, ovdb_conf.numdbfiles);
    memset(dbs, 0, sizeof(DB *) * ovdb_conf.numdbfiles);
    
    if(!oneatatime) {
	for(i = 0; i < ovdb_conf.numdbfiles; i++) {
	    if(ret = open_db_file(i)) {
		syslog(L_FATAL, "OVDB: open_db_file failed: %s", db_strerror(ret));
		return FALSE;
	    }
	}
    }

#if DB_VERSION_MAJOR == 2
    memset(&dbinfo, 0, sizeof dbinfo);

    if(ret = db_open("groupinfo", DB_BTREE, _db_flags, 0666, OVDBenv,
		    &dbinfo, &groupinfo)) {
	syslog(L_FATAL, "OVDB: db_open failed: %s", db_strerror(ret));
	return FALSE;
    }

    if(ret = db_open("groupaliases", DB_HASH, _db_flags, 0666, OVDBenv,
		    &dbinfo, &groupaliases)) {
	syslog(L_FATAL, "OVDB: db_open failed: %s", db_strerror(ret));
	return FALSE;
    }
#else
    if(ret = db_create(&groupinfo, OVDBenv, 0)) {
	syslog(L_FATAL, "OVDB: open: db_create: %s", db_strerror(ret));
	return FALSE;
    }
    if(ret = groupinfo->open(groupinfo, "groupinfo", NULL, DB_BTREE,
		_db_flags, 0666)) {
	groupinfo->close(groupinfo, 0);
	syslog(L_FATAL, "OVDB: open: groupinfo->open: %s", db_strerror(ret));
	return FALSE;
    }
    if(ret = db_create(&groupaliases, OVDBenv, 0)) {
	syslog(L_FATAL, "OVDB: open: db_create: %s", db_strerror(ret));
	return FALSE;
    }
    if(ret = groupaliases->open(groupaliases, "groupaliases", NULL, DB_HASH,
		_db_flags, 0666)) {
	groupaliases->close(groupaliases, 0);
	syslog(L_FATAL, "OVDB: open: groupaliases->open: %s", db_strerror(ret));
	return FALSE;
    }
#endif

    Cutofflow = FALSE;
    return TRUE;
}


BOOL ovdb_groupstats(char *group, int *lo, int *hi, int *count, int *flag)
{
    int ret;
    struct groupinfo gi;

    if(clientmode) {
	struct rs_cmd rs;
	struct rs_groupstats repl;

	rs.what = CMD_GROUPSTATS;
	rs.grouplen = strlen(group)+1;

	csend(&rs, sizeof(rs));
	csend(group, rs.grouplen);
	crecv(&repl, sizeof(repl));

	if(repl.status != CMD_GROUPSTATS)
	    return FALSE;

	/* we don't use the alias yet, but the OV API will be extended
	   at some point so that the alias is returned also */
	if(repl.aliaslen != 0) {
	    char *buf = NEW(char, repl.aliaslen);
	    crecv(buf, repl.aliaslen);
	    DISPOSE(buf);
	}

	if(lo)
	    *lo = repl.lo;
	if(hi)
	    *hi = repl.hi;
	if(count)
	    *count = repl.count;
	if(flag)
	    *flag = repl.flag;
	return TRUE;
    }

    switch(ret = ovdb_getgroupinfo(group, &gi, TRUE, NULL, 0)) {
    case 0:
	break;
    case DB_NOTFOUND:
	return FALSE;
    default:
	syslog(L_ERROR, "OVDB: ovdb_getgroupinfo failed: %s", db_strerror(ret));
	return FALSE;
    }

    if(lo != NULL)
	*lo = gi.low;
    if(hi != NULL)
	*hi = gi.high;
    if(count != NULL)
	*count = gi.count;
    if(flag != NULL)
	*flag = gi.flag;
    return TRUE;
}

BOOL ovdb_groupadd(char *group, ARTNUM lo, ARTNUM hi, char *flag)
{
    DBT key, val;
    struct groupinfo gi;
    DB_TXN *tid;
    int ret, new;

    memset(&key, 0, sizeof key);
    memset(&val, 0, sizeof val);

    TXN_START(t_groupadd, tid);

    if(tid==NULL)
	return FALSE;

    new = 0;
    switch(ret = ovdb_getgroupinfo(group, &gi, FALSE, tid, DB_RMW)) {
    case DB_NOTFOUND:
	new = 1;
    case 0:
	break;
    case TRYAGAIN:
	TXN_RETRY(t_groupadd, tid);
    default:
	TXN_ABORT(t_groupadd, tid);
	syslog(L_ERROR, "OVDB: ovdb_getgroupinfo failed: %s", db_strerror(ret));
	return FALSE;
    }

    if(!new && (gi.status & GROUPINFO_DELETED)
		&& !(gi.status & GROUPINFO_EXPIRING)
		&& !(gi.status & GROUPINFO_MOVING)) {
	int s, c = 0;
	char g[MAXHEADERSIZE];

	strcpy(g, group);
	s = strlen(g) + 1;
	key.data = g;
	key.size = s + sizeof(int);
	do {
	    c++;
	    memcpy(g+s, &c, sizeof(int));
	    ret = groupinfo->get(groupinfo, tid, &key, &val, 0);
	} while(ret == 0);
	if(ret == TRYAGAIN) {
	    TXN_RETRY(t_groupadd, tid);
	}
	val.data = &gi;
	val.size = sizeof(gi);
	switch(ret = groupinfo->put(groupinfo, tid, &key, &val, 0)) {
	case 0:
	    break;
	case TRYAGAIN:
	    TXN_RETRY(t_groupadd, tid);
	default:
	    TXN_ABORT(t_groupadd, tid);
	    syslog(L_ERROR, "OVDB: groupinfo->put: %s", db_strerror(ret));
	    return FALSE;
	}
	key.data = group;
	key.size = strlen(group);
	switch(ret = groupinfo->del(groupinfo, tid, &key, 0)) {
	case 0:
	    break;
	case TRYAGAIN:
	    TXN_RETRY(t_groupadd, tid);
	default:
	    TXN_ABORT(t_groupadd, tid);
	    syslog(L_ERROR, "OVDB: groupinfo->del: %s", db_strerror(ret));
	    return FALSE;
	}
	new = 1;
    }

    if(new) {
	switch(ret = groupid_new(&gi.current_gid, tid)) {
	case 0:
	    break;
	case TRYAGAIN:
	    TXN_RETRY(t_groupadd, tid);
	default:
	    TXN_ABORT(t_groupadd, tid);
	    syslog(L_ERROR, "OVDB: groupid_new: %s", db_strerror(ret));
	    return FALSE;
	}
	gi.low = lo ? lo : 1;
	gi.high = hi;
	gi.count = 0;
	gi.flag = *flag;
	gi.expired = time(NULL);
	gi.expiregrouppid = 0;
	gi.current_db = gi.new_db = which_db(group);
	gi.new_gid = gi.current_gid;
	gi.status = 0;
    } else {
	gi.flag = *flag;
    }

    key.data = group;
    key.size = strlen(group);
    val.data = &gi;
    val.size = sizeof gi;

    switch(ret = groupinfo->put(groupinfo, tid, &key, &val, 0)) {
    case 0:
	break;
    case TRYAGAIN:
	TXN_RETRY(t_groupadd, tid);
    default:
	TXN_ABORT(t_groupadd, tid);
	syslog(L_ERROR, "OVDB: groupadd: groupinfo->put: %s", db_strerror(ret));
	return FALSE;
    }

    if(*flag == '=') {
	key.data = group;
	key.size = strlen(group);
	val.data = flag + 1;
	val.size = strcspn(flag, "\n") - 1;

	switch(ret = groupaliases->put(groupaliases, tid, &key, &val, 0)) {
	case 0:
	    break;
	case TRYAGAIN:
	    TXN_RETRY(t_groupadd, tid);
	default:
	    TXN_ABORT(t_groupadd, tid);
	    syslog(L_ERROR, "OVDB: groupadd: groupaliases->put: %s", db_strerror(ret));
	    return FALSE;
	}
    }

    TXN_COMMIT(t_groupadd, tid);
    return TRUE;
}

BOOL ovdb_groupdel(char *group)
{
    DBT key, val;
    struct groupinfo gi;
    DB_TXN *tid;
    int ret;

    memset(&key, 0, sizeof key);
    memset(&val, 0, sizeof val);

    /* We only need to set the deleted flag in groupinfo to prevent readers
       from seeing this group.  The actual overview records aren't deleted
       now, since that could take a significant amount of time (and innd
       is who normally calls this function).  The expireover run will
       clean up the deleted groups. */

    TXN_START(t_groupdel, tid);

    if(tid==NULL)
	return FALSE;

    switch(ret = ovdb_getgroupinfo(group, &gi, TRUE, tid, DB_RMW)) {
    case DB_NOTFOUND:
	return TRUE;
    case 0:
	break;
    case TRYAGAIN:
	TXN_RETRY(t_groupdel, tid);
    default:
	syslog(L_ERROR, "OVDB: ovdb_getgroupinfo failed: %s", db_strerror(ret));
	TXN_ABORT(t_groupdel, tid);
	return FALSE;
    }

    gi.status |= GROUPINFO_DELETED;

    key.data = group;
    key.size = strlen(group);
    val.data = &gi;
    val.size = sizeof gi;

    switch(ret = groupinfo->put(groupinfo, tid, &key, &val, 0)) {
    case 0:
	break;
    case TRYAGAIN:
	TXN_RETRY(t_groupdel, tid);
    default:
	TXN_ABORT(t_groupdel, tid);
	syslog(L_ERROR, "OVDB: groupadd: groupinfo->put: %s", db_strerror(ret));
	return FALSE;
    }

    switch(ret = groupaliases->del(groupaliases, tid, &key, 0)) {
    case 0:
    case DB_NOTFOUND:
	break;
    case TRYAGAIN:
	TXN_RETRY(t_groupdel, tid);
    default:
	syslog(L_ERROR, "OVDB: groupdel: groupaliases->del: %s", db_strerror(ret));
	TXN_ABORT(t_groupdel, tid);
	return FALSE;
    }

    TXN_COMMIT(t_groupdel, tid);
    return TRUE;
}

BOOL ovdb_add(char *group, ARTNUM artnum, TOKEN token, char *data, int len, time_t arrived, time_t expires)
{
    static int  databuflen = 0;
    static char *databuf;
    ARTNUM      bartnum;
    DB		*db;
    DBT		key, val;
    DB_TXN	*tid;
    struct groupinfo gi;
    struct datakey dk;
    int ret;

    memset(&dk, 0, sizeof dk);

    if(databuflen == 0) {
	databuflen = BIG_BUFFER;
	databuf = NEW(char, databuflen);
    }
    if(databuflen < len + sizeof(struct ovdata)) {
	databuflen = len + sizeof(struct ovdata);
	RENEW(databuf, char, databuflen);
    }

    /* hmm... BerkeleyDB needs something like a 'struct iovec' so that we don't
       have to make a new buffer and copy everything in to it */

    ((struct ovdata *)databuf)->token = token;
    ((struct ovdata *)databuf)->arrived = arrived;
    ((struct ovdata *)databuf)->expires = expires;
    memcpy(databuf + sizeof(struct ovdata), data, len);
    len += sizeof(struct ovdata);

    memset(&key, 0, sizeof key);
    memset(&val, 0, sizeof val);

    /* start the transaction */
    TXN_START(t_add, tid);

    if(tid==NULL)
	return FALSE;

    /* first, retrieve groupinfo */
    switch(ret = ovdb_getgroupinfo(group, &gi, TRUE, tid, DB_RMW)) {
    case 0:
	break;
    case DB_NOTFOUND:
	TXN_ABORT(t_add, tid);
	return TRUE;
    case TRYAGAIN:
	TXN_RETRY(t_add, tid);
    default:
	TXN_ABORT(t_add, tid);
	syslog(L_ERROR, "OVDB: add: ovdb_getgroupinfo: %s", db_strerror(ret));
	return FALSE;
    }

    /* adjust groupinfo */
    if(Cutofflow && gi.low > artnum) {
	TXN_ABORT(t_add, tid);
	return TRUE;
    }
    if(gi.low == 0 || gi.low > artnum)
	gi.low = artnum;
    if(gi.high < artnum)
	gi.high = artnum;
    gi.count++;

    /* store groupinfo */
    key.data = group;
    key.size = strlen(group);
    val.data = &gi;
    val.size = sizeof gi;

    switch(ret = groupinfo->put(groupinfo, tid, &key, &val, 0)) {
    case 0:
	break;
    case TRYAGAIN:
	TXN_RETRY(t_add, tid);
    default:
	TXN_ABORT(t_add, tid);
	syslog(L_ERROR, "OVDB: add: groupinfo->put: %s", db_strerror(ret));
	return FALSE;
    }

    /* store overview */
    db = get_db_bynum(gi.current_db);
    if(db == NULL) {
	TXN_ABORT(t_add, tid);
	return FALSE;
    }
    dk.groupnum = gi.current_gid;
    dk.artnum = htonl((u_int32_t)artnum);

    key.data = &dk;
    key.size = sizeof dk;
    val.data = databuf;
    val.size = len;

    switch(ret = db->put(db, tid, &key, &val, 0)) {
    case 0:
	break;
    case TRYAGAIN:
	TXN_RETRY(t_add, tid);
    default:
	TXN_ABORT(t_add, tid);
	syslog(L_ERROR, "OVDB: add: db->put: %s", db_strerror(ret));
	return FALSE;
    }

    if(artnum < gi.high && gi.status & GROUPINFO_MOVING) {
	/* If the GROUPINFO_MOVING flag is set, then expireover
	   is writing overview records under a new groupid.
	   If this overview record is not at the highmark,
	   we need to also store it under the new groupid */
	db = get_db_bynum(gi.new_db);
	if(db == NULL) {
	    TXN_ABORT(t_add, tid);
	    return FALSE;
	}
	dk.groupnum = gi.new_gid;

	switch(ret = db->put(db, tid, &key, &val, 0)) {
	case 0:
	    break;
	case TRYAGAIN:
	    TXN_RETRY(t_add, tid);
	default:
	    TXN_ABORT(t_add, tid);
	    syslog(L_ERROR, "OVDB: add: db->put: %s", db_strerror(ret));
	    return FALSE;
	}
    }

    TXN_COMMIT(t_add, tid);
    return TRUE;
}

BOOL ovdb_cancel(TOKEN token)
{
    return TRUE;
}

struct ovdbsearch {
    DBC *cursor;
    group_id_t gid;
    u_int32_t firstart;
    u_int32_t lastart;
    int state;
};

void *ovdb_opensearch(char *group, int low, int high)
{
    DB *db;
    struct ovdbsearch *s;
    struct groupinfo gi;
    int ret;

    if(clientmode) {
	struct rs_cmd rs;
	struct rs_opensrch repl;

	rs.what = CMD_OPENSRCH;
	rs.grouplen = strlen(group)+1;
	rs.artlo = low;
	rs.arthi = high;

	csend(&rs, sizeof(rs));
	csend(group, rs.grouplen);
	crecv(&repl, sizeof(repl));

	if(repl.status != CMD_OPENSRCH)
	    return NULL;

	return repl.handle;
    }

    switch(ret = ovdb_getgroupinfo(group, &gi, TRUE, NULL, 0)) {
    case 0:
	break;
    case DB_NOTFOUND:
	return NULL;
    default:
	syslog(L_ERROR, "OVDB: ovdb_getgroupinfo failed: %s", db_strerror(ret));
	return NULL;
    }

    s = NEW(struct ovdbsearch, 1);
    db = get_db_bynum(gi.current_db);
    if(db == NULL) {
	DISPOSE(s);
	return NULL;
    }

    if(ret = db->cursor(db, NULL, &(s->cursor), 0)) {
	syslog(L_ERROR, "OVDB: opensearch: s->db->cursor: %s", db_strerror(ret));
	DISPOSE(s);
	return NULL;
    }

    s->gid = gi.current_gid;
    s->firstart = low;
    s->lastart = high;
    s->state = 0;

    return (void *)s;
}

BOOL ovdb_search(void *handle, ARTNUM *artnum, char **data, int *len, TOKEN *token, time_t *arrived)
{
    struct ovdbsearch *s = (struct ovdbsearch *)handle;
    DBT key, val;
    u_int32_t flags;
    struct ovdata ovd;
    struct datakey dk;
    int ret;

    if(clientmode) {
	struct rs_cmd rs;
	struct rs_srch repl;
	static char *databuf;
	static int buflen = 0;

	rs.what = CMD_SRCH;
	rs.handle = handle;

	csend(&rs, sizeof(rs));
	crecv(&repl, sizeof(repl));

	if(repl.status != CMD_SRCH)
	    return FALSE;
	if(repl.len > buflen) {
	    if(buflen == 0) {
		buflen = repl.len + 512;
		databuf = NEW(char, buflen);
	    } else {
		buflen = repl.len + 512;
		RENEW(databuf, char, buflen);
	    }
	}
	crecv(databuf, repl.len);

	if(artnum)
	    *artnum = repl.artnum;
	if(token)
	    *token = repl.token;
	if(arrived)
	    *arrived = repl.arrived;
	if(len)
	    *len = repl.len;
	if(data)
	    *data = databuf;
	return TRUE;
    }

    switch(s->state) {
    case 0:
	flags = DB_SET_RANGE;
	memset(&dk, 0, sizeof dk);
	dk.groupnum = s->gid;
	dk.artnum = htonl(s->firstart);
	s->state = 1;
	break;
    case 1:
	flags = DB_NEXT;
	break;
    case 2:
	s->state = 3;
	return FALSE;
    default:
	syslog(L_ERROR, "OVDB: OVsearch called again after FALSE return");
	return FALSE;
    }

    memset(&key, 0, sizeof key);
    memset(&val, 0, sizeof val);
    key.data = &dk;
    key.size = key.ulen = sizeof(struct datakey);
    key.flags = DB_DBT_USERMEM;

    if(!data && !len) {
	/* caller doesn't need data, so we don't have to retrieve it all */
	val.flags |= DB_DBT_PARTIAL;

	if(token || arrived)
	    val.dlen = sizeof(struct ovdata);
    }

    switch(ret = s->cursor->c_get(s->cursor, &key, &val, flags)) {
    case 0:
	break;
    case DB_NOTFOUND:
	s->state = 3;
	s->cursor->c_close(s->cursor);
	s->cursor = NULL;
	return FALSE;
    default:
	syslog(L_ERROR, "OVDB: search: c_get: %s", db_strerror(ret));
	s->state = 3;
	s->cursor->c_close(s->cursor);
	s->cursor = NULL;
	return FALSE;
    }

    if(key.size != sizeof(struct datakey)) {
	s->state = 3;
	s->cursor->c_close(s->cursor);
	s->cursor = NULL;
	return FALSE;
    }

    if(dk.groupnum != s->gid || ntohl(dk.artnum) > s->lastart) {
	s->state = 3;
	s->cursor->c_close(s->cursor);
	s->cursor = NULL;
	return FALSE;
    }

    if( ((len || data) && val.size <= sizeof(struct ovdata))
	|| ((token || arrived) && val.size < sizeof(struct ovdata)) ) {
	syslog(L_ERROR, "OVDB: search: bad value length");
	s->state = 3;
	s->cursor->c_close(s->cursor);
	s->cursor = NULL;
	return FALSE;
    }

    if(ntohl(dk.artnum) == s->lastart) {
	s->state = 2;
	s->cursor->c_close(s->cursor);
	s->cursor = NULL;
    }

    if(artnum)
	*artnum = ntohl(dk.artnum);

    if(token || arrived)
	memcpy(&ovd, val.data, sizeof(struct ovdata));
    if(token)
	*token = ovd.token;
    if(arrived)
	*arrived = ovd.arrived;

    if(len)
	*len = val.size - sizeof(struct ovdata);
    if(data)
	*data = (char *)val.data + sizeof(struct ovdata);

    return TRUE;
}

void ovdb_closesearch(void *handle)
{
    if(clientmode) {
	struct rs_cmd rs;

	rs.what = CMD_CLOSESRCH;
	rs.handle = handle;
	csend(&rs, sizeof(rs));
	/* no reply is sent for a CMD_CLOSESRCH */
    } else {
	struct ovdbsearch *s = (struct ovdbsearch *)handle;

	if(s->cursor)
	    s->cursor->c_close(s->cursor);

	DISPOSE(handle);
    }
}

BOOL ovdb_getartinfo(char *group, ARTNUM artnum, char **data, int *len, TOKEN *token)
{
    int ret, cdb;
    group_id_t cgid;
    DB *db;
    DBT key, val;
    struct ovdata ovd;
    struct datakey dk;
    struct groupinfo gi;
    int pass = 0;

    if(clientmode) {
	struct rs_cmd rs;
	struct rs_artinfo repl;
	static char *databuf;
	static int buflen = 0;

	rs.what = CMD_ARTINFO;
	rs.grouplen = strlen(group)+1;
	rs.artlo = artnum;

	csend(&rs, sizeof(rs));
	csend(group, rs.grouplen);
	crecv(&repl, sizeof(repl));

	if(repl.status != CMD_ARTINFO)
	    return FALSE;
	if(repl.len > buflen) {
	    if(buflen == 0) {
		buflen = repl.len + 512;
		databuf = NEW(char, buflen);
	    } else {
		buflen = repl.len + 512;
		RENEW(databuf, char, buflen);
	    }
	}
	crecv(databuf, repl.len);

	if(token)
	    *token = repl.token;
	if(len)
	    *len = repl.len;
	if(data)
	    *data = databuf;
	return TRUE;
    }

    while(1) {
	switch(ret = ovdb_getgroupinfo(group, &gi, TRUE, NULL, 0)) {
	case 0:
	    break;
	case DB_NOTFOUND:
	    return FALSE;
	default:
	    syslog(L_ERROR, "OVDB: ovdb_getgroupinfo failed: %s", db_strerror(ret));
	    return FALSE;
	}

	if(pass) {
	    /* This was our second groupinfo retrieval; because the article
	       retrieval came up empty.  If the group ID hasn't changed
	       since the first groupinfo retrieval, we can assume the article
	       is definitely not there.  Otherwise, we'll try to retrieve
	       it the article again. */
	    if(cdb == gi.current_db && cgid == gi.current_gid)
		return FALSE;
	}

	db = get_db_bynum(gi.current_db);
	if(db == NULL)
	    return FALSE;

	memset(&dk, 0, sizeof dk);
	dk.groupnum = gi.current_gid;
	dk.artnum = htonl((u_int32_t)artnum);

	memset(&key, 0, sizeof key);
	memset(&val, 0, sizeof val);

	key.data = &dk;
	key.size = sizeof dk;

	if(!data && !len) {
	    /* caller doesn't need data, so we don't have to retrieve it all */
	    val.flags = DB_DBT_PARTIAL;

	    if(token)
		val.dlen = sizeof(struct ovdata);
	}

	switch(ret = db->get(db, NULL, &key, &val, 0)) {
	case 0:
	case DB_NOTFOUND:
	    break;
	default:
	    syslog(L_ERROR, "OVDB: getartinfo: db->get: %s", db_strerror(ret));
	    return FALSE;
	}

	if(ret == DB_NOTFOUND) {
	    /* If the group is being moved (i.e., its group ID is going
	       to change), there's a chance the article is now under the
	       new ID.  So we'll grab the groupinfo again to check for
	       that. */
	    if(!pass && (gi.status & GROUPINFO_MOVING)) {
		cdb = gi.current_db;
		cgid = gi.current_gid;
		pass++;
		continue;
	    }
	    return FALSE;
	}
	break;
    }

    if( ( (len || data) && val.size <= sizeof(struct ovdata) )
	|| (token && val.size < sizeof(struct ovdata) ) ) {
	syslog(L_ERROR, "OVDB: getartinfo: data too short");
	return FALSE;
    }

    if(len)
	*len = val.size - sizeof(struct ovdata);
    if(data)
	*data = (char *)val.data + sizeof(struct ovdata);

    if(token) {
	memcpy(&ovd, val.data, sizeof(struct ovdata));
	*token = ovd.token;
    }
    return TRUE;
}

BOOL ovdb_expiregroup(char *group, int *lo, struct history *h)
{
    DB *db, *ndb;
    DBT key, val, nkey, gkey, gval;
    DB_TXN *tid;
    DBC *cursor = NULL;
    int ret, delete, old_db;
    struct groupinfo gi;
    struct ovdata ovd;
    struct datakey dk, ndk;
    group_id_t old_gid;
    ARTHANDLE *ah;
    u_int32_t artnum, currentart, lowest;
    int i, compact, done, currentcount, newcount;

    if(eo_start == 0) {
	eo_start = time(NULL);
	delete_old_stuff(0);	/* remove deleted groups first */
    }

    /* Special case:  when called with NULL group, we're to clean out
     * records for forgotton groups (groups removed from the active file
     * but not from overview).
     * This happens at the end of the expireover run, and only if all
     * of the groups in the active file have been processed.
     * delete_old_stuff(1) will remove groups that are in ovdb but
     * have not been processed during this run.
     */

    if(group == NULL)
	return delete_old_stuff(1);

    memset(&key, 0, sizeof key);
    memset(&nkey, 0, sizeof nkey);
    memset(&val, 0, sizeof val);
    memset(&dk, 0, sizeof dk);
    memset(&ndk, 0, sizeof ndk);

    TXN_START(t_expgroup_1, tid);

    if(tid==NULL)
	return FALSE;

    switch(ret = ovdb_getgroupinfo(group, &gi, TRUE, tid, DB_RMW)) {
    case 0:
	break;
    case TRYAGAIN:
	TXN_RETRY(t_expgroup_1, tid);
    default:
	syslog(L_ERROR, "OVDB: expiregroup: ovdb_getgroupinfo failed: %s", db_strerror(ret));
    case DB_NOTFOUND:
	TXN_ABORT(t_expgroup_1, tid);
	return FALSE;
    }

    if(gi.status & GROUPINFO_EXPIRING) {
	/* is there another expireover working on this group? */
	switch(ret = kill(gi.expiregrouppid, 0)) {
	case 0:
	case EPERM:
	    TXN_ABORT(t_expgroup_1, tid);
	    return FALSE;
	}

	/* a previous expireover run must've died.  We'll clean
	   up after it */
	if(gi.status & GROUPINFO_MOVING) {
	    if(delete_all_records(gi.new_db, gi.new_gid)) {
		TXN_ABORT(t_expgroup_1, tid);
		return FALSE;
	    }
	    if(groupid_free(gi.new_gid, tid) == TRYAGAIN) {
		TXN_RETRY(t_expgroup_1, tid);
	    }
	    gi.status &= ~GROUPINFO_MOVING;
	}
    }

    if(gi.count < ovdb_conf.nocompact || ovdb_conf.nocompact == 0)
	compact = 1;
    else
	compact = 0;

    if(gi.count == 0)
	compact = 0;

    db = get_db_bynum(gi.current_db);
    if(db == NULL) {
	TXN_ABORT(t_expgroup_1, tid);
	return FALSE;
    }

    gi.status |= GROUPINFO_EXPIRING;
    gi.expiregrouppid = getpid();
    if(compact) {
	gi.status |= GROUPINFO_MOVING;
	gi.new_db = gi.current_db;
	ndb = db;
	switch(ret = groupid_new(&gi.new_gid, tid)) {
	case 0:
	    break;
	case TRYAGAIN:
	    TXN_RETRY(t_expgroup_1, tid);
	default:
	    TXN_ABORT(t_expgroup_1, tid);
	    syslog(L_ERROR, "OVDB: expiregroup: groupid_new: %s", db_strerror(ret));
	    return FALSE;
	}
    }

    key.data = group;
    key.size = strlen(group);
    val.data = &gi;
    val.size = sizeof gi;

    switch(ret = groupinfo->put(groupinfo, tid, &key, &val, 0)) {
    case 0:
	break;
    case TRYAGAIN:
	TXN_RETRY(t_expgroup_1, tid);
    default:
	TXN_ABORT(t_expgroup_1, tid);
	syslog(L_ERROR, "OVDB: expiregroup: groupinfo->put: %s", db_strerror(ret));
	return FALSE;
    }
    TXN_COMMIT(t_expgroup_1, tid);

    /*
     * The following loop iterates over the OV records for the group in
     * "batches", to limit transaction sizes.
     *
     * loop {
     *    start transaction
     *    get groupinfo
     *    process EXPIREGROUP_TXN_SIZE records
     *    write updated groupinfo
     *    commit transaction
     * }
     */
    currentart = 0;
    lowest = currentcount = 0;

    memset(&gkey, 0, sizeof gkey);
    memset(&gval, 0, sizeof gval);
    gkey.data = group;
    gkey.size = strlen(group);
    gval.data = &gi;
    gval.size = sizeof gi;

    while(1) {
	TXN_START(t_expgroup_loop, tid);
	if(tid==NULL)
	    return FALSE;
        done = 0;
	newcount = 0;

	switch(ret = ovdb_getgroupinfo(group, &gi, FALSE, tid, DB_RMW)) {
	case 0:
	    break;
	case TRYAGAIN:
	    TXN_RETRY(t_expgroup_loop, tid);
	default:
	    TXN_ABORT(t_expgroup_loop, tid);
	    syslog(L_ERROR, "OVDB: expiregroup: ovdb_getgroupinfo: %s", db_strerror(ret));
	    return FALSE;
	}

	switch(ret = db->cursor(db, tid, &cursor, 0)) {
	case 0:
	    break;
	case TRYAGAIN:
	    TXN_RETRY(t_expgroup_loop, tid);
	default:
	    TXN_ABORT(t_expgroup_loop, tid);
	    syslog(L_ERROR, "OVDB: expiregroup: db->cursor: %s", db_strerror(ret));
	    return FALSE;
	}

	dk.groupnum = gi.current_gid;
	dk.artnum = htonl(currentart);
	key.data = &dk;
	key.size = key.ulen = sizeof dk;
	key.flags = DB_DBT_USERMEM;

	for(i=0; i < EXPIREGROUP_TXN_SIZE; i++) {
	    switch(ret = cursor->c_get(cursor, &key, &val, i == 0 ? DB_SET_RANGE : DB_NEXT)) {
	    case 0:
	    case DB_NOTFOUND:
		break;
	    case TRYAGAIN:
		cursor->c_close(cursor);
		TXN_RETRY(t_expgroup_loop, tid);
	    default:
		cursor->c_close(cursor);
		TXN_ABORT(t_expgroup_loop, tid);
		syslog(L_ERROR, "OVDB: expiregroup: c_get: %s", db_strerror(ret));
		return FALSE;
	    }

	    /* stop if: there are no more keys, an unknown key is reached,
	       or reach a different group */

	    if(ret == DB_NOTFOUND
		    || key.size != sizeof dk
		    || dk.groupnum != gi.current_gid) {
		done++;
		break;
	    }

	    artnum = ntohl(dk.artnum);

	    delete = 0;
	    if(val.size < sizeof ovd) {
		delete = 1;	/* must be corrupt, just delete it */
	    } else {
		memcpy(&ovd, val.data, sizeof ovd);

		ah = NULL;
#if INN_VERSION_MAJOR == 2 && INN_VERSION_MINOR == 3 && INN_VERSION_PATCH == 0
		if (SMprobe(SELFEXPIRE, &ovd.token, NULL)) {
		    if((ah = SMretrieve(ovd.token, RETR_STAT)) == NULL) { 
			delete = 1;
		    }
		} else {
		    if (!innconf->groupbaseexpiry
			    && !OVhisthasmsgid(h, (char *)val.data + sizeof(ovd))) {
			delete = 1;
		    }
		}
		if(ah)
		    SMfreearticle(ah);
#else
		if (!SMprobe(EXPENSIVESTAT, &ovd.token, NULL) || OVstatall) {
		    if((ah = SMretrieve(ovd.token, RETR_STAT)) == NULL) {
			delete = 1;
		    } else
			SMfreearticle(ah);
		} else {
		    if (!OVhisthasmsgid(h, (char *)val.data + sizeof(ovd))) {
			delete = 1;
		    }
		}
#endif
		if (!delete && innconf->groupbaseexpiry &&
			    OVgroupbasedexpire(ovd.token, group,
				    (char *)val.data + sizeof(ovd),
				    val.size - sizeof(ovd),
				    ovd.arrived, ovd.expires)) {
		    delete = 1;
		}
	    }

	    if(delete) {
		if(!compact) {
		    switch(ret = cursor->c_del(cursor, 0)) {
		    case 0:
		    case DB_NOTFOUND:
		    case DB_KEYEMPTY:
			break;
		    case TRYAGAIN:
			cursor->c_close(cursor);
			TXN_RETRY(t_expgroup_loop, tid);
		    default:
			cursor->c_close(cursor);
			TXN_ABORT(t_expgroup_loop, tid);
			syslog(L_ERROR, "OVDB: expiregroup: c_del: %s", db_strerror(ret));
			return FALSE;
		    }
		}
		if(gi.count > 0)
		    gi.count--;
	    } else {
		if(compact) {
		    ndk.groupnum = gi.new_gid;
		    ndk.artnum = dk.artnum;
		    nkey.data = &ndk;
		    nkey.size = sizeof ndk;

		    switch(ret = ndb->put(ndb, tid, &nkey, &val, 0)) {
		    case 0:
			break;
		    case TRYAGAIN:
			cursor->c_close(cursor);
			TXN_RETRY(t_expgroup_loop, tid);
		    default:
			cursor->c_close(cursor);
			TXN_ABORT(t_expgroup_loop, tid);
			syslog(L_ERROR, "OVDB: expiregroup: ndb->put: %s", db_strerror(ret));
			return FALSE;
		    }
		}
		newcount++;
		if(lowest != -1 && (lowest == 0 || artnum < lowest))
		    lowest = artnum;
	    }
	}
	/* end of for loop */

	if(cursor->c_close(cursor) == TRYAGAIN) {
	    TXN_RETRY(t_expgroup_loop, tid);
	}

	if(lowest != 0 && lowest != -1)
	    gi.low = lowest;

	if(done) {
	    if(compact) {
		old_db = gi.current_db;
		gi.current_db = gi.new_db;
		old_gid = gi.current_gid;
		gi.current_gid = gi.new_gid;
		gi.status &= ~GROUPINFO_MOVING;
	    }

	    gi.status &= ~GROUPINFO_EXPIRING;
	    gi.expired = time(NULL);
	    if(gi.count == 0 && lowest == 0)
		gi.low = gi.high+1;
	}

	switch(ret = groupinfo->put(groupinfo, tid, &gkey, &gval, 0)) {
	case 0:
	    break;
	case TRYAGAIN:
	    TXN_RETRY(t_expgroup_loop, tid);
	default:
	    TXN_ABORT(t_expgroup_loop, tid);
	    syslog(L_ERROR, "OVDB: expiregroup: groupinfo->put: %s", db_strerror(ret));
	    return FALSE;
	}
        TXN_COMMIT(t_expgroup_loop, tid);

	currentcount += newcount;
	if(lowest != 0)
	    lowest = -1;

	if(done)
	    break;

	currentart = artnum+1;
    }

    if(compact) {
	if(ret = delete_all_records(old_db, old_gid)) {
	    syslog(L_ERROR, "OVDB: expiregroup: delete_all_records: %s", db_strerror(ret));
	    return FALSE;
	}

	TXN_START(t_expgroup_cleanup, tid);
	if(tid == NULL)
	    return FALSE;

	switch(ret = groupid_free(old_gid, tid)) {
	case 0:
	    break;
	case TRYAGAIN:
	    TXN_RETRY(t_expgroup_cleanup, tid);
	default:
	    TXN_ABORT(t_expgroup_cleanup, tid);
	    syslog(L_ERROR, "OVDB: expiregroup: groupid_free: %s", db_strerror(ret));
	    return FALSE;
	}

	TXN_COMMIT(t_expgroup_cleanup, tid);
    }

    if(currentcount != gi.count) {
	syslog(L_NOTICE, "OVDB: expiregroup: recounting %s", group);

	TXN_START(t_expgroup_recount, tid);
	if(tid == NULL)
	    return FALSE;

	switch(ret = ovdb_getgroupinfo(group, &gi, FALSE, tid, DB_RMW)) {
	case 0:
	    break;
	case TRYAGAIN:
	    TXN_RETRY(t_expgroup_recount, tid);
	default:
	    TXN_ABORT(t_expgroup_recount, tid);
	    syslog(L_ERROR, "OVDB: expiregroup: ovdb_getgroupinfo: %s", db_strerror(ret));
	    return FALSE;
	}

	if(count_records(&gi) != 0) {
	    TXN_ABORT(t_expgroup_recount, tid);
	    return FALSE;
	}

	switch(ret = groupinfo->put(groupinfo, tid, &gkey, &gval, 0)) {
	case 0:
	    break;
	case TRYAGAIN:
	    TXN_RETRY(t_expgroup_recount, tid);
	default:
	    TXN_ABORT(t_expgroup_recount, tid);
	    syslog(L_ERROR, "OVDB: expiregroup: groupinfo->put: %s", db_strerror(ret));
	    return FALSE;
	}
        TXN_COMMIT(t_expgroup_recount, tid);
    }

    if(lo)
	*lo = gi.low;
    return TRUE;
}

BOOL ovdb_ctl(OVCTLTYPE type, void *val)
{
    int *i;
    OVSORTTYPE *sorttype;
#if INN_VERSION_MINOR >= 4
    bool *boolval;
#endif

    switch (type) {
    case OVSPACE:
        i = (int *)val;
        *i = -1;
        return TRUE;
    case OVSORT:
        sorttype = (OVSORTTYPE *)val;
        *sorttype = OVNEWSGROUP;
        return TRUE;
    case OVCUTOFFLOW:
        Cutofflow = *(BOOL *)val;
        return TRUE;
    case OVSTATICSEARCH:
	i = (int *)val;
	*i = TRUE;
	return TRUE;
#if INN_VERSION_MINOR >= 4
    case OVCACHEKEEP:
    case OVCACHEFREE:
        boolval = (bool *)val;
        *boolval = FALSE;
        return TRUE;
#endif
    default:
        return FALSE;
    }
}

void ovdb_close_berkeleydb(void)
{
    if(OVDBenv) {
	/* close db environment */
#if DB_VERSION_MAJOR == 2
	db_appexit(OVDBenv);
	DISPOSE(OVDBenv);
#else
	OVDBenv->close(OVDBenv, 0);
#endif
	OVDBenv = NULL;
    }
}

void ovdb_close(void)
{
    int i;

    if(clientmode) {
	client_disconnect();
	return;
    }

    if(dbs) {
	/* close databases */
	for(i = 0; i < ovdb_conf.numdbfiles; i++)
	    close_db_file(i);

	DISPOSE(dbs);
	dbs = NULL;
    }
    if(groupinfo) {
	groupinfo->close(groupinfo, 0);
	groupinfo = NULL;
    }
    if(groupaliases) {
	groupaliases->close(groupaliases, 0);
	groupaliases = NULL;
    }

    ovdb_close_berkeleydb();
    ovdb_releaselock();
}


#endif /* USE_BERKELEY_DB */


/*
 * ovdb.c
 * ovdb 2.00
 * Overview storage using Berkeley DB 4.4 or higher
 *
 * 2004-02-17 : Need to track search cursors, since it's possible that
 *              ovdb_closesearch does not get called.  We now close
 *              any cursors still open in ovdb_close, or they'd be in
 *              the database indefinitely causing deadlocks.
 * 2002-08-13 : Change BOOL to bool, remove portability to < 2.4.
 * 2002-08-11 : Cleaned up use of sprintf and fixed a bunch of warnings.
 * 2002-02-28 : Update getartinfo for the overview API change in 2.4.  This
 *              breaks compatibility with INN 2.3.x....
 * 2000-12-12 : Add support for Berkeley DB DB_SYSTEM_MEM option, controlled
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
 * 2000-08-28 : New major release: version 2.00 (beta)
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
 *    + No longer needs the Berkeley DB tools db_archive, db_checkpoint, and
 *         db_deadlock.  That functionality is now in ovdb_monitor.
 *    + ovdb_open() won't succeed if ovdb_monitor is not running.  This will
 *         prevent the problems that happen if the database is not regularly
 *         checkpointed and deadlock-tested.
 *    + Internal group IDs (32-bit ints) are now reused.
 *    + Add "maxlocks" to ovdb.conf, which will set the DB lk_max parameter.
 *    + Pull "test" code out into ovdb_stat.  ovdb_stat will also provide
 *         functionality similar to the Berkeley DB "db_stat" command.
 *    + Update docs: write man pages for the new ovdb_* commands; update
 *         ovdb.pod
 *         
 * 2000-07-11 : fix possible alignment problem; add test code
 * 2000-07-07 : bugfix: timestamp handling
 * 2000-06-10 : Modified groupnum() interface; fix ovdb_add() to return false
 *              for certain groupnum() errors
 * 2000-06-08 : Added Berkeley DB 3.1.x compatibility
 * 2000-04-09 : Tweak some default parameters; store aliased group info
 * 2000-03-29 : Add DB_RMW flag to the 'get' of get-modify-put sequences
 * 2000-02-17 : Update expire behavior to be consistent with current
 *              ov3 and buffindexed
 * 2000-01-13 : Fix to make compatible with unmodified nnrpd/article.c
 * 2000-01-04 : Added data versioning
 * 1999-12-20 : Added Berkeley DB 3.x compatibility
 * 1999-12-06 : First Release -- H. Kehoe <hakehoe@avalon.net>
 */

#include "config.h"
#include "clibrary.h"
#include "portable/socket.h"
#include <errno.h>
#include <fcntl.h>
#ifdef HAVE_LIMITS_H
# include <limits.h>
#endif
#include <signal.h>
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif
#ifndef HAVE_ZLIB_H
# undef HAVE_ZLIB
#endif
#ifdef HAVE_ZLIB
# include <zlib.h>
# define MAX_UNZIP_SZ 100000
# define COMPRESS_MIN 600
#endif

#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <time.h>

#include "conffile.h"
#include "inn/fdflag.h"
#include "inn/innconf.h"
#include "inn/libinn.h"
#include "inn/messages.h"
#include "inn/newsuser.h"
#include "inn/paths.h"
#include "inn/storage.h"

#ifdef HAVE_UNIX_DOMAIN_SOCKETS
# include "portable/socket-unix.h"
#endif

#include "inn/ov.h"
#include "ovinterface.h"
#include "ovdb.h"
#include "ovdb-private.h"
#ifndef HAVE_BDB

/* Provide stub functions if we don't have db */

bool ovdb_open(int mode UNUSED)
{
    warn("OVDB: ovdb support not enabled");
    return false;
}

bool ovdb_groupstats(const char *group UNUSED, int *lo UNUSED, int *hi UNUSED, int *count UNUSED, int *flag UNUSED)
{ return false; }

bool ovdb_groupadd(const char *group UNUSED, ARTNUM lo UNUSED, ARTNUM hi UNUSED, char *flag UNUSED)
{ return false; }

bool ovdb_groupdel(const char *group UNUSED)
{ return false; }

bool ovdb_add(const char *group UNUSED, ARTNUM artnum UNUSED, TOKEN token UNUSED, char *data UNUSED, int len UNUSED, time_t arrived UNUSED, time_t expires UNUSED)
{ return false; }

bool ovdb_cancel(const char *group UNUSED, ARTNUM artnum UNUSED)
{ return false; }

void *ovdb_opensearch(const char *group UNUSED, int low UNUSED, int high UNUSED)
{ return NULL; }

bool ovdb_search(void *handle UNUSED, ARTNUM *artnum UNUSED, char **data UNUSED, int *len UNUSED, TOKEN *token UNUSED, time_t *arrived UNUSED)
{ return false; }

void ovdb_closesearch(void *handle UNUSED) { }

bool ovdb_getartinfo(const char *group UNUSED, ARTNUM artnum UNUSED, TOKEN *token UNUSED)
{ return false; }

bool ovdb_expiregroup(const char *group UNUSED, int *lo UNUSED, struct history *h UNUSED)
{ return false; }

bool ovdb_ctl(OVCTLTYPE type UNUSED, void *val UNUSED)
{ return false; }

void ovdb_close(void) { }

#else /* HAVE_BDB */

#define EXPIREGROUP_TXN_SIZE 100
#define DELETE_TXN_SIZE 500

int ovdb_data_ver = 0;
struct ovdb_conf ovdb_conf;
DB_ENV *OVDBenv = NULL;

static int OVDBmode;
static bool Cutofflow;
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
#define OVDBcompress	13
#define OVDBncache	14

static CONFTOKEN toks[] = {
    { OVDBtxn_nosync,   (char *) "txn_nosync"   },
    { OVDBnumdbfiles,   (char *) "numdbfiles"   },
    { OVDBpagesize,     (char *) "pagesize"     },
    { OVDBcachesize,    (char *) "cachesize"    },
    { OVDBminkey,       (char *) "minkey"       },
    { OVDBmaxlocks,     (char *) "maxlocks"     },
    { OVDBnocompact,    (char *) "nocompact"    },
    { OVDBreadserver,   (char *) "readserver"   },
    { OVDBnumrsprocs,   (char *) "numrsprocs"   },
    { OVDBmaxrsconn,    (char *) "maxrsconn"    },
    { OVDBuseshm,       (char *) "useshm"       },
    { OVDBshmkey,       (char *) "shmkey"       },
    { OVDBcompress,     (char *) "compress"     },
    { OVDBncache,       (char *) "ncache"       },
    { 0,                NULL                    }
};

#define _PATH_OVDBCONF "ovdb.conf"

/*********** readserver functions ***********/

static int clientfd = -1;

/* read client send and recieve functions. */

static int
csend(const void *data, int n)
{
    ssize_t status;

    if (n == 0)
	return 0;
    status = xwrite(clientfd, data, n);
    if (status < 0)
        syswarn("OVDB: rc: cant write");
    return status;
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
            syswarn("OVDB: rc: cant read");
	    clientfd = -1;
	    exit(1);
	}
	p+= r;
    }
    return p;
}

/* Attempt to connect to the readserver.  If anything fails, we
   return -1 so that ovdb_open can open the database directly. */

static int client_connect(void)
{
    ssize_t r;
    size_t p = 0;
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
        syswarn("OVDB: socket");
	return -1;
    }

    memset(&sa, 0, sizeof sa);
#ifdef HAVE_UNIX_DOMAIN_SOCKETS
    sa.sun_family = AF_UNIX;
    path = concatpath(innconf->pathrun, OVDB_SERVER_SOCKET);
    strlcpy(sa.sun_path, path, sizeof(sa.sun_path));
    free(path);
    r = connect(clientfd, (struct sockaddr *) &sa, SUN_LEN(&sa));
#else
    sa.sin_family = AF_INET;
    sa.sin_port = 0;
    sa.sin_addr.s_addr = htonl(0x7f000001UL);
    bind(clientfd, (struct sockaddr *) &sa, sizeof sa);
    sa.sin_port = htons(OVDB_SERVER_PORT);
    r = connect(clientfd, (struct sockaddr *) &sa, sizeof sa);
#endif
    if(r != 0) {
        syswarn("OVDB: rc: cant connect to server");
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
            syswarn("OVDB: select");
	    close(clientfd);
	    clientfd = -1;
	    return -1;
	}
	if(r == 0) {
            warn("OVDB: rc: timeout waiting for server");
	    close(clientfd);
	    clientfd = -1;
	    return -1;
    	}

	r = read(clientfd, banner + p, sizeof(OVDB_SERVER_BANNER) - p);
	if(r <= 0) {
	    if(r < 0 && errno == EINTR)
		continue;
            syswarn("OVDB: rc: cant read");
	    close(clientfd);
	    clientfd = -1;
	    return -1;
	}
	p+= r;
    }

    if(memcmp(banner, OVDB_SERVER_BANNER, sizeof(OVDB_SERVER_BANNER))) {
        warn("OVDB: rc: unknown reply from server");
	close(clientfd);
	clientfd = -1;
	return -1;
    }
    return 0;
}

static void
client_disconnect(void)
{
    struct rs_cmd rs;

    if (clientfd != -1) {
	rs.what = CMD_QUIT;
	csend(&rs, sizeof(rs));
    }
    clientfd = -1;
}


/*********** internal functions ***********/

static bool conf_bool_val(char *str, bool *value)
{
    if(strcasecmp(str, "on") == 0
       || strcasecmp(str, "true") == 0
       || strcasecmp(str, "yes") == 0) {
	*value = true;
	return true;
    }
    if(strcasecmp(str, "off") == 0
       || strcasecmp(str, "false") == 0
       || strcasecmp(str, "no") == 0) {
	*value = false;
	return true;
    }
    return false;
}

static bool conf_long_val(char *str, long *value)
{
    long v;

    errno = 0;
    v = strtol(str, NULL, 10);
    if(v == 0 && errno != 0) {
	return false;
    }
    *value = v;
    return true;
}

void read_ovdb_conf(void)
{
    static int confread = 0;
    int done = 0;
    char *path;
    CONFFILE *f;
    CONFTOKEN *tok;
    bool b;
    long l;

    if(confread)
	return;

    /* defaults */
    ovdb_conf.home = innconf->pathoverview;
    ovdb_conf.txn_nosync = 1;
    ovdb_conf.numdbfiles = 32;
    ovdb_conf.pagesize = 8192;
    ovdb_conf.cachesize = 8000 * 1024;
    ovdb_conf.ncache = 1;
    ovdb_conf.minkey = 0;
    ovdb_conf.maxlocks = 4000;
    ovdb_conf.nocompact = 1;
    ovdb_conf.readserver = 0;
    ovdb_conf.numrsprocs = 5;
    ovdb_conf.maxrsconn = 0;
    ovdb_conf.useshm = 0;
    ovdb_conf.shmkey = 6400;
    ovdb_conf.compress = 0;

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
	    case OVDBncache:
		tok = CONFgettoken(0, f);
		if(!tok) {
		    done = 1;
		    continue;
		}
		if(conf_long_val(tok->name, &l) && l > 0) {
		    ovdb_conf.ncache = l;
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
	    case OVDBcompress:
#ifdef HAVE_ZLIB
		tok = CONFgettoken(0, f);
		if(!tok) {
		    done = 1;
		    continue;
		}
		if(conf_bool_val(tok->name, &b)) {
		    ovdb_conf.compress = b;
		}
#endif
		break;
	    }
	}
	CONFfclose(f);
    }

    /* If user did not specify minkey, choose one based on pagesize */
    if(ovdb_conf.minkey == 0) {
	if(ovdb_conf.compress) {
	    ovdb_conf.minkey = ovdb_conf.pagesize / 1500;
	} else {
	    ovdb_conf.minkey = ovdb_conf.pagesize / 2600;
	}
	if(ovdb_conf.minkey < 2)
	    ovdb_conf.minkey = 2;
    }

    confread = 1;
}

/* Function that db will use to report errors */
static void OVDBerror(const DB_ENV *dbenv UNUSED, const char *db_errpfx UNUSED, const char *buffer)
{
    warn("OVDB: %s", buffer);
}

static u_int32_t _db_flags = 0;

static int open_db_file(int which)
{
    int ret;
    char name[10];
    DB_TXN *tid;

    if(dbs[which] != NULL)
	return 0;

    snprintf(name, sizeof(name), "ov%05d", which);

    ret = db_create(&(dbs[which]), OVDBenv, 0);
    if (ret != 0)
	return ret;

    if(ovdb_conf.minkey > 0)
	(dbs[which])->set_bt_minkey(dbs[which], ovdb_conf.minkey);
    if(ovdb_conf.pagesize > 0)
	(dbs[which])->set_pagesize(dbs[which], ovdb_conf.pagesize);

    TXN_START_NORETRY(t_open_db_file, tid);
    ret = (dbs[which])->open(dbs[which], tid, name, NULL, DB_BTREE, _db_flags,
                             0666);
    if (ret == 0)
	TXN_COMMIT(t_open_db_file, tid);
    if (ret != 0) {
	(dbs[which])->close(dbs[which], 0);
	dbs[which] = NULL;
	return ret;
    }
    return 0;
}

static void close_db_file(int which)
{
    if(which == -1 || dbs[which] == NULL)
	return;
    
    dbs[which]->close(dbs[which], 0);
    dbs[which] = NULL;
}

static int which_db(const char *group)
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

        ret = open_db_file(which);
	if (ret != 0)
            warn("OVDB: open_db_file failed: %s", db_strerror(ret));

	current_db = which;
    }
    return(dbs[which]);
}


int
ovdb_getgroupinfo(const char *group, struct groupinfo *gi, int ignoredeleted,
                  DB_TXN *tid, int getflags)
{
    int ret;
    DBT key, val;

    if(group == NULL)	/* just in case */
	return DB_NOTFOUND;

    memset(&key, 0, sizeof key);
    memset(&val, 0, sizeof val);

    key.data = (char *) group;
    key.size = strlen(group);
    val.data = gi;
    val.ulen = sizeof(struct groupinfo);
    val.flags = DB_DBT_USERMEM;

    ret = groupinfo->get(groupinfo, tid, &key, &val, getflags);
    if (ret != 0)
	return ret;

    if(val.size != sizeof(struct groupinfo)) {
        warn("OVDB: wrong size for %s groupinfo (%u)", group, val.size);
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

    key.data = (char *) "!groupid_freelist";
    key.size = sizeof("!groupid_freelist");

    ret = groupinfo->get(groupinfo, tid, &key, &val, DB_RMW);
    if (ret != 0) {
	if(ret == DB_NOTFOUND) {
	    val.size = sizeof(group_id_t);
	    val.data = &one;
	    one = 1;
	} else {
	    return ret;
	}
    }

    if(val.size % sizeof(group_id_t)) {
        warn("OVDB: invalid size (%d) for !groupid_freelist", val.size);
	return EINVAL;
    }

    n = val.size / sizeof(group_id_t);
    freelist = xmalloc(n * sizeof(group_id_t));
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

    ret = groupinfo->put(groupinfo, tid, &key, &val, 0);
    if (ret != 0) {
	free(freelist);
	return ret;
    }

    free(freelist);
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

    key.data = (char *) "!groupid_freelist";
    key.size = sizeof("!groupid_freelist");

    ret = groupinfo->get(groupinfo, tid, &key, &val, DB_RMW);
    if (ret != 0) {
	return ret;
    }

    if(val.size % sizeof(group_id_t)) {
        warn("OVDB: invalid size (%d) for !groupid_freelist", val.size);
	return EINVAL;
    }

    n = val.size / sizeof(group_id_t);
    if(n > GROUPID_MAX_FREELIST)
	return 0;
    freelist = xmalloc((n + 1) * sizeof(group_id_t));
    memcpy(freelist, val.data, val.size);

    if(gno >= freelist[n-1]) {	/* shouldn't happen */
	free(freelist);
	return 0;
    }
    for(i = 0; i < n-1; i++) {
	if(gno == freelist[i]) {	/* already on freelist */
	    free(freelist);
	    return 0;
	}
    }
	
    freelist[n] = freelist[n-1];
    freelist[n-1] = gno;
    val.data = freelist;
    val.size += sizeof(group_id_t);

    ret = groupinfo->put(groupinfo, tid, &key, &val, 0);

    free(freelist);
    return ret;
}

/* Must be called outside of a transaction because it makes its own
   transactions */
static int delete_all_records(int whichdb, group_id_t gno)
{
    DB *db;
    DBC *dbcursor;
    DBT key, val;
    struct datakey dk;
    int count;
    int ret = 0;
    DB_TXN *tid;

    memset(&key, 0, sizeof key);
    memset(&val, 0, sizeof val);
    memset(&dk, 0, sizeof dk);

    db = get_db_bynum(whichdb);
    if(db == NULL)
        return DB_NOTFOUND;

    dk.groupnum = gno;
    dk.artnum = 0;

    while(1) {
        TXN_START(t_del, tid);

        /* get a cursor to traverse the ov records and delete them */
        ret = db->cursor(db, tid, &dbcursor, 0);
        switch(ret)
        {
        case 0:
            break;
        case TRYAGAIN:
            TXN_RETRY(t_del, tid);
        default:
            TXN_ABORT(t_del, tid);
            warn("OVDB: delete_all_records: db->cursor: %s", db_strerror(ret));
            return ret;
        }

        key.data = &dk;
        key.size = sizeof dk;
        val.flags = DB_DBT_PARTIAL;

        for(count = 0; count < DELETE_TXN_SIZE; count++) {
            ret = dbcursor->c_get(dbcursor, &key, &val,
                            count ? DB_NEXT : DB_SET_RANGE);
            switch (ret)
            {
            case 0:
                break;
            case DB_NOTFOUND:
                dbcursor->c_close(dbcursor);
                TXN_COMMIT(t_del, tid);
                return 0;
            case TRYAGAIN:
                dbcursor->c_close(dbcursor);
                TXN_RETRY(t_del, tid);
            default:
                warn("OVDB: delete_all_records: DBcursor->c_get: %s",
                     db_strerror(ret));
                dbcursor->c_close(dbcursor);
                TXN_ABORT(t_del, tid);
                return ret;
            }

            if(key.size == sizeof dk
                    && memcmp(key.data, &gno, sizeof gno)) {
                break;
            }

            ret = dbcursor->c_del(dbcursor, 0);
            switch (ret) {
            case 0:
            case DB_KEYEMPTY:
                break;
            case TRYAGAIN:
                dbcursor->c_close(dbcursor);
                TXN_RETRY(t_del, tid);
            default:
                warn("OVDB: delete_all_records: DBcursor->c_del: %s",
                     db_strerror(ret));
                dbcursor->c_close(dbcursor);
                TXN_ABORT(t_del, tid);
                return ret;
            }
        }
        dbcursor->c_close(dbcursor);
        TXN_COMMIT(t_del, tid);
        if(count < DELETE_TXN_SIZE) {
            break;
        }
    }
    return 0;
}

/* Make a temporary groupinfo key using the given db number and group ID.
   Must be called in a transaction */
static int
mk_temp_groupinfo(int whichdb, group_id_t gno, DB_TXN *tid)
{
    char keystr[1 + sizeof gno];
    DBT key, val;
    struct groupinfo gi;
    int ret;

    memset(&key, 0, sizeof key);
    memset(&val, 0, sizeof val);
    memset(&gi, 0, sizeof gi);

    keystr[0] = 0;
    memcpy(keystr + 1, &gno, sizeof gno);

    gi.current_db = whichdb;
    gi.current_gid = gno;
    gi.status = GROUPINFO_DELETED;

    key.data = keystr;
    key.size = sizeof keystr;
    val.data = &gi;
    val.size = sizeof gi;

    ret = groupinfo->put(groupinfo, tid, &key, &val, 0);
    switch (ret) {
    case 0:
        break;
    default:
        warn("OVDB: mk_temp_groupinfo: groupinfo->put: %s", db_strerror(ret));
    case TRYAGAIN:
        return ret;
    }

    return 0;
}

/* Delete a temporary groupinfo key created by mk_temp_groupid, then
   frees the group id.
   Must NOT be called within a transaction. */
static int
rm_temp_groupinfo(group_id_t gno)
{
    char keystr[1 + sizeof gno];
    DB_TXN *tid;
    DBT key;
    int ret = 0;

    memset(&key, 0, sizeof key);

    keystr[0] = 0;
    memcpy(keystr + 1, &gno, sizeof gno);

    key.data = keystr;
    key.size = sizeof keystr;

    TXN_START(t_tmp, tid);

    ret = groupinfo->del(groupinfo, tid, &key, 0);
    switch(ret) {
    case 0:
    case DB_NOTFOUND:
        break;
    case TRYAGAIN:
        TXN_RETRY(t_tmp, tid);
    default:
        TXN_ABORT(t_tmp, tid);
        warn("OVDB: rm_temp_groupinfo: groupinfo->del: %s", db_strerror(ret));
        return ret;
    }

    switch(groupid_free(gno, tid)) {
    case 0:
        break;
    case TRYAGAIN:
        TXN_RETRY(t_tmp, tid);
    default:
        TXN_ABORT(t_tmp, tid);
        warn("OVDB: rm_temp_groupinfo: groupid_free: %s", db_strerror(ret));
        return ret;
    }

    TXN_COMMIT(t_tmp, tid);
    return 0;
}

/* This function deletes overview records for deleted or forgotton groups */
/* argument: 0 = process deleted groups   1 = process forgotton groups */
static bool delete_old_stuff(int forgotton)
{
    DBT key, val;
    DBC *cursor;
    DB_TXN *tid;
    struct groupinfo gi;
    char **dellist = NULL;
    size_t *dellistsz = NULL;
    int listlen, listcount = 0;
    int i, ret = 0;

    TXN_START(t_dellist, tid);
    if (dellist != NULL) {
        for (i = 0; i < listcount; ++i)
            free(dellist[i]);
        free(dellist);
    }
    if (dellistsz != NULL)
	free(dellistsz);
    listlen = 20;
    listcount = 0;
    dellist = xmalloc(listlen * sizeof(char *));
    dellistsz = xmalloc(listlen * sizeof(size_t));

    memset(&key, 0, sizeof key);
    memset(&val, 0, sizeof val);

    val.data = &gi;
    val.ulen = val.dlen = sizeof gi;
    val.flags = DB_DBT_USERMEM | DB_DBT_PARTIAL;

    /* get a cursor to traverse all of the groupinfo records */
    ret = groupinfo->cursor(groupinfo, tid, &cursor, 0);
    if (ret != 0) {
        warn("OVDB: delete_old_stuff: groupinfo->cursor: %s",
             db_strerror(ret));
	free(dellist);
	free(dellistsz);
	return false;
    }

    while((ret = cursor->c_get(cursor, &key, &val, DB_NEXT)) == 0) {
	if(key.size == sizeof("!groupid_freelist") &&
		!strcmp("!groupid_freelist", key.data))
	    continue;
	if(val.size != sizeof(struct groupinfo)) {
            warn("OVDB: delete_old_stuff: wrong size for groupinfo record");
	    continue;
	}
	if((!forgotton && (gi.status & GROUPINFO_DELETED))
		|| (forgotton && (gi.expired < eo_start))) {
	    dellist[listcount] = xmalloc(key.size);
	    memcpy(dellist[listcount], key.data, key.size);
	    dellistsz[listcount] = key.size;
	    listcount++;
	    if(listcount >= listlen) {
		listlen += 20;
                dellist = xrealloc(dellist, listlen * sizeof(char *));
                dellistsz = xrealloc(dellistsz, listlen * sizeof(size_t));
	    }
	}
    }
    cursor->c_close(cursor);
    switch (ret) {
    case 0:
    case DB_NOTFOUND:
        TXN_COMMIT(t_dellist, tid);
        break;
    case TRYAGAIN:
        TXN_RETRY(t_dellist, tid);
    default:
        TXN_ABORT(t_dellist, tid);
        warn("OVDB: delete_old_stuff: cursor->c_get: %s", db_strerror(ret));
        for (i = 0; i < listcount; ++i)
            free(dellist[i]);
        free(dellist);
        free(dellistsz);
        return false;
    }

    for(i = 0; i < listcount; i++) {
	TXN_START(t_dos, tid);

	/* Can't use ovdb_getgroupinfo here */
	key.data = dellist[i];
	key.size = dellistsz[i];
	val.data = &gi;
	val.ulen = sizeof(struct groupinfo);
	val.flags = DB_DBT_USERMEM;

	ret = groupinfo->get(groupinfo, tid, &key, &val, 0);

	switch (ret)
        {
	case 0:
	    break;
	case TRYAGAIN:
	    TXN_RETRY(t_dos, tid);
	case DB_NOTFOUND:
	    TXN_ABORT(t_dos, tid);
	    continue;
	default:
            warn("OVDB: delete_old_stuff: groupinfo->get: %s",
                 db_strerror(ret));
	    TXN_ABORT(t_dos, tid);
	    continue;
	}
	if(val.size != sizeof(struct groupinfo)) {
	    TXN_ABORT(t_dos, tid);
	    continue;
	}

	/* This if() is true if this ISN'T a key created by mk_temp_groupinfo */
	if(*((char *)key.data) != 0 || key.size != 1 + sizeof(group_id_t)) {

	    switch(mk_temp_groupinfo(gi.current_db, gi.current_gid, tid)) {
	    case 0:
		break;
	    case TRYAGAIN:
		TXN_RETRY(t_dos, tid);
	    default:
		TXN_ABORT(t_dos, tid);
		continue;
	    }
	    if(gi.status & GROUPINFO_MOVING) {
		switch(mk_temp_groupinfo(gi.new_db, gi.new_gid, tid)) {
		case 0:
		    break;
		case TRYAGAIN:
		    TXN_RETRY(t_dos, tid);
		default:
		    TXN_ABORT(t_dos, tid);
		    continue;
		}
	    }

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
	}

	TXN_COMMIT(t_dos, tid);

	if(delete_all_records(gi.current_db, gi.current_gid) == 0) {
	    rm_temp_groupinfo(gi.current_gid);
	}
	if(gi.status & GROUPINFO_MOVING) {
	    if(delete_all_records(gi.new_db, gi.new_gid) == 0) {
		rm_temp_groupinfo(gi.new_gid);
	    }
	}
    }

    for(i = 0; i < listcount; i++)
	free(dellist[i]);
    free(dellist);
    free(dellistsz);
    return true;
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

    ret = db->cursor(db, NULL, &cursor, 0);
    if (ret)
	return ret;

    ret = cursor->c_get(cursor, &key, &val, DB_SET_RANGE);
    switch (ret)
    {
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
bool ovdb_getlock(int mode)
{
    if(lockfd == -1) {
	char *lockfn = concatpath(innconf->pathrun, OVDB_LOCKFN);
	lockfd = open(lockfn,
		mode == OVDB_LOCK_NORMAL ? O_RDWR : O_CREAT|O_RDWR, 0660);
	if(lockfd == -1) {
	    free(lockfn);
	    if(errno == ENOENT)
                warn("OVDB: can not open database unless ovdb_monitor is"
                     " running");
	    return false;
	}
	fdflag_close_exec(lockfd, true);
	free(lockfn);
    } else
	return true;

    if(mode == OVDB_LOCK_NORMAL) {
	if(!ovdb_check_pidfile(OVDB_MONITOR_PIDFILE)) {
            warn("OVDB: can not open database unless ovdb_monitor is"
                 " running");
	    return false;
	}
    }
    if(mode == OVDB_LOCK_EXCLUSIVE) {
	if(!inn_lock_file(lockfd, INN_LOCK_WRITE, false)) {
	    close(lockfd);
	    lockfd = -1;
	    return false;
	}
	return true;
    } else {
	if(!inn_lock_file(lockfd, INN_LOCK_READ, false)) {
	    close(lockfd);
	    lockfd = -1;
	    return false;
	}
	return true;
    }
}

bool ovdb_releaselock(void)
{
    bool r;
    if(lockfd == -1)
	return true;
    r = inn_lock_file(lockfd, INN_LOCK_UNLOCK, false);
    close(lockfd);
    lockfd = -1;
    return r;
}

bool ovdb_check_pidfile(const char *file)
{
    int f, pid;
    char buf[SMBUF];
    char *pidfn = concatpath(innconf->pathrun, file);

    f = open(pidfn, O_RDONLY);
    if(f == -1) {
	if(errno != ENOENT)
            syswarn("OVDB: can't open %s", pidfn);
	free(pidfn);
	return false;
    }
    memset(buf, 0, SMBUF);
    if(read(f, buf, SMBUF-1) < 0) {
        syswarn("OVDB: can't read from %s", pidfn);
	free(pidfn);
	close(f);
	return false;
    }
    close(f);
    free(pidfn);
    pid = atoi(buf);
    if(pid <= 1) {
	return false;
    }
    if(kill(pid, 0) < 0 && errno == ESRCH) {
	return false;
    }
    return true;
}

/* Make sure the effective uid is that of the runasuser user. */
bool ovdb_check_user(void)
{
    static int result = -1;

    if(result == -1) {
        int rv;
        uid_t uid;
        
        rv = get_news_uid_gid(&uid, false, false);

        if (rv != 0) {
            syswarn("OVDB: can't resolve runasuser user to a UID");
            return false;
        }
        result = (uid == geteuid());
    }
    return result;
}

static int check_version(void)
{
    int ret;
    DB *vdb;
    DBT key, val;
    u_int32_t dv;

    /* open version db */
    ret = db_create(&vdb, OVDBenv, 0);
    if (ret != 0) {
        warn("OVDB: open: db_create: %s", db_strerror(ret));
	return ret;
    }
    ret = vdb->open(vdb, NULL, "version", NULL, DB_BTREE, _db_flags, 0666);
    if (ret != 0) {
	vdb->close(vdb, 0);
        warn("OVDB: open: version->open: %s", db_strerror(ret));
	return ret;
    }
    memset(&key, 0, sizeof key);
    memset(&val, 0, sizeof val);
    key.data = (char *) "dataversion";
    key.size = sizeof("dataversion");
    ret = vdb->get(vdb, NULL, &key, &val, 0);
    if (ret != 0) {
	if(ret != DB_NOTFOUND) {
            warn("OVDB: open: can't retrieve version: %s", db_strerror(ret));
	    vdb->close(vdb, 0);
	    return ret;
	}
    }
    if(ret == DB_NOTFOUND || val.size != sizeof dv) {
	dv = ovdb_conf.compress ? DATA_VERSION_COMPRESS : DATA_VERSION;
	if(!(OVDBmode & OV_WRITE)) {
	    vdb->close(vdb, 0);
	    return EACCES;
	}
	val.data = &dv;
	val.size = sizeof dv;
        ret = vdb->put(vdb, NULL, &key, &val, 0);
	if (ret != 0) {
            warn("OVDB: open: can't store version: %s", db_strerror(ret));
	    vdb->close(vdb, 0);
	    return ret;
	}
    } else
	memcpy(&dv, val.data, sizeof dv);

    if(dv > DATA_VERSION_COMPRESS) {
        warn("OVDB: can't open database: unknown version %d", dv);
	vdb->close(vdb, 0);
	return EINVAL;
    }
    if(dv < DATA_VERSION) {
        warn("OVDB: database is an old version, please run ovdb_init");
	vdb->close(vdb, 0);
	return EINVAL;
    }
#ifndef HAVE_ZLIB
    if(dv == DATA_VERSION_COMPRESS) {
	warn("OVDB: database is compressed but INN was not built with zlib");
	vdb->close(vdb, 0);
	return EINVAL;
    }
#endif
    if(ovdb_conf.compress && dv == DATA_VERSION && (OVDBmode & OV_WRITE)) {
	/* "Upgrade" the database to indicate there may be compressed
	   records */
	dv = DATA_VERSION_COMPRESS;
	val.data = &dv;
	val.size = sizeof dv;

        ret = vdb->put(vdb, NULL, &key, &val, 0);
	if (ret != 0) {
	    warn("OVDB: open: can't store version: %s", db_strerror(ret));
	    vdb->close(vdb, 0);
	    return ret;
	}
    }
    ovdb_data_ver = dv;

    /* The version db also stores some config values, which will override the
       corresponding ovdb.conf setting. */

    key.data = (char *) "numdbfiles";
    key.size = sizeof("numdbfiles");
    ret = vdb->get(vdb, NULL, &key, &val, 0);
    if (ret != 0) {
	if(ret != DB_NOTFOUND) {
            warn("OVDB: open: can't retrieve numdbfiles: %s",
                 db_strerror(ret));
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
        ret = vdb->put(vdb, NULL, &key, &val, 0);
	if (ret != 0) {
            warn("OVDB: open: can't store numdbfiles: %s", db_strerror(ret));
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

    ret = db_env_create(&OVDBenv, 0);
    if (ret != 0) {
        warn("OVDB: db_env_create: %s", db_strerror(ret));
	return ret;
    }

    if((flags & (OVDB_UPGRADE | OVDB_RECOVER)) == (OVDB_UPGRADE | OVDB_RECOVER))
	ai_flags |= DB_PRIVATE;
    if(!(ai_flags & DB_PRIVATE)) {
	if(ovdb_conf.useshm)
	    ai_flags |= DB_SYSTEM_MEM;
	OVDBenv->set_shm_key(OVDBenv, ovdb_conf.shmkey);
    }

    OVDBenv->set_errcall(OVDBenv, OVDBerror);
    OVDBenv->set_cachesize(OVDBenv, 0, ovdb_conf.cachesize, ovdb_conf.ncache);
    OVDBenv->set_lk_max_locks(OVDBenv, ovdb_conf.maxlocks);
    OVDBenv->set_lk_max_lockers(OVDBenv, ovdb_conf.maxlocks);
    OVDBenv->set_lk_max_objects(OVDBenv, ovdb_conf.maxlocks);

    if(ovdb_conf.txn_nosync)
	OVDBenv->set_flags(OVDBenv, DB_TXN_NOSYNC, 1);

    if((flags & (OVDB_UPGRADE | OVDB_RECOVER)) != OVDB_UPGRADE) {
	ret = OVDBenv->open(OVDBenv, ovdb_conf.home, ai_flags, 0666);
	if (ret != 0) {
	    OVDBenv->close(OVDBenv, 0);
	    OVDBenv = NULL;
            warn("OVDB: OVDBenv->open: %s", db_strerror(ret));
	    return ret;
	}
    }

    return 0;
}

bool ovdb_open(int mode)
{
    int i, ret;
    DB_TXN *tid;

    if(OVDBenv != NULL || clientmode) {
        warn("OVDB: ovdb_open called more than once");
	return false;
    }

    read_ovdb_conf();
    if(ovdb_conf.readserver && mode == OV_READ)
	clientmode = 1;

    if(mode & OVDB_SERVER)
	clientmode = 0;

    if(clientmode) {
	if(client_connect() == 0)
	    return true;
	clientmode = 0;
    }
    if(!ovdb_check_user()) {
        warn("OVDB: must be running as runasuser user to access overview");
	return false;
    }
    if(!ovdb_getlock(OVDB_LOCK_NORMAL)) {
        syswarn("OVDB: ovdb_getlock failed");
	return false;
    }

    if(ovdb_open_berkeleydb(mode, 0) != 0)
	return false;

    if(check_version() != 0)
	return false;

    if(mode & OV_WRITE || mode & OVDB_SERVER) {
	oneatatime = 0;
    } else {
	oneatatime = 1;
    }

    dbs = xcalloc(ovdb_conf.numdbfiles, sizeof(DB *));
    
    if(!oneatatime) {
	for(i = 0; i < ovdb_conf.numdbfiles; i++) {
            ret = open_db_file(i);
	    if (ret != 0) {
                warn("OVDB: open_db_file failed: %s", db_strerror(ret));
		return false;
	    }
	}
    }

    ret = db_create(&groupinfo, OVDBenv, 0);
    if (ret != 0) {
        warn("OVDB: open: db_create: %s", db_strerror(ret));
	return false;
    }
    TXN_START_NORETRY(t_open_groupinfo, tid);
    ret = groupinfo->open(groupinfo, tid, "groupinfo", NULL, DB_BTREE,
                          _db_flags, 0666);
    if (ret == 0)
	TXN_COMMIT(t_open_groupinfo, tid);
    if (ret != 0) {
	groupinfo->close(groupinfo, 0);
        warn("OVDB: open: groupinfo->open: %s", db_strerror(ret));
	return false;
    }
    ret = db_create(&groupaliases, OVDBenv, 0);
    if (ret != 0) {
        warn("OVDB: open: db_create: %s", db_strerror(ret));
	return false;
    }
    TXN_START_NORETRY(t_open_groupaliases, tid);
    ret = groupaliases->open(groupaliases, tid, "groupaliases", NULL, DB_HASH,
                             _db_flags, 0666);
    if (ret == 0)
	TXN_COMMIT(t_open_groupaliases, tid);
    if (ret != 0) {
	groupaliases->close(groupaliases, 0);
        warn("OVDB: open: groupaliases->open: %s", db_strerror(ret));
	return false;
    }

    Cutofflow = false;
    return true;
}


bool
ovdb_groupstats(const char *group, int *lo, int *hi, int *count, int *flag)
{
    int ret;
    struct groupinfo gi;

    if (clientmode) {
	struct rs_cmd rs;
	struct rs_groupstats repl;

	rs.what = CMD_GROUPSTATS;
	rs.grouplen = strlen(group)+1;

	if (csend(&rs, sizeof(rs)) < 0)
            return false;
	if (csend(group, rs.grouplen) < 0)
            return false;
	crecv(&repl, sizeof(repl));

	if(repl.status != CMD_GROUPSTATS)
	    return false;

	/* we don't use the alias yet, but the OV API will be extended
	   at some point so that the alias is returned also */
	if(repl.aliaslen != 0) {
	    char *buf = xmalloc(repl.aliaslen);
	    crecv(buf, repl.aliaslen);
	    free(buf);
	}

	if(lo)
	    *lo = repl.lo;
	if(hi)
	    *hi = repl.hi;
	if(count)
	    *count = repl.count;
	if(flag)
	    *flag = repl.flag;
	return true;
    }

    ret = ovdb_getgroupinfo(group, &gi, true, NULL, 0);
    switch (ret)
    {
    case 0:
	break;
    case DB_NOTFOUND:
	return false;
    default:
        warn("OVDB: ovdb_getgroupinfo failed: %s", db_strerror(ret));
	return false;
    }

    if(lo != NULL)
	*lo = gi.low;
    if(hi != NULL)
	*hi = gi.high;
    if(count != NULL)
	*count = gi.count;
    if(flag != NULL)
	*flag = gi.flag;
    return true;
}

bool
ovdb_groupadd(const char *group, ARTNUM lo, ARTNUM hi, char *flag)
{
    DBT key, val;
    struct groupinfo gi;
    DB_TXN *tid;
    int ret = 0;
    int new;

    memset(&key, 0, sizeof key);
    memset(&val, 0, sizeof val);

    TXN_START(t_groupadd, tid);

    if(tid==NULL)
	return false;

    new = 0;
    ret = ovdb_getgroupinfo(group, &gi, false, tid, DB_RMW);
    switch (ret)
    {
    case DB_NOTFOUND:
	new = 1;
    case 0:
	break;
    case TRYAGAIN:
	TXN_RETRY(t_groupadd, tid);
    default:
	TXN_ABORT(t_groupadd, tid);
        warn("OVDB: ovdb_getgroupinfo failed: %s", db_strerror(ret));
	return false;
    }

    if(!new && (gi.status & GROUPINFO_DELETED)
		&& !(gi.status & GROUPINFO_EXPIRING)
		&& !(gi.status & GROUPINFO_MOVING)) {
	int s, c = 0;
	char g[MED_BUFFER];

	strlcpy(g, group, sizeof(g));
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
        ret = groupinfo->put(groupinfo, tid, &key, &val, 0);
	switch (ret)
        {
	case 0:
	    break;
	case TRYAGAIN:
	    TXN_RETRY(t_groupadd, tid);
	default:
	    TXN_ABORT(t_groupadd, tid);
            warn("OVDB: groupinfo->put: %s", db_strerror(ret));
	    return false;
	}
	key.data = (char *) group;
	key.size = strlen(group);
        ret = groupinfo->del(groupinfo, tid, &key, 0);
	switch (ret)
        {
	case 0:
	    break;
	case TRYAGAIN:
	    TXN_RETRY(t_groupadd, tid);
	default:
	    TXN_ABORT(t_groupadd, tid);
            warn("OVDB: groupinfo->del: %s", db_strerror(ret));
	    return false;
	}
	new = 1;
    }

    if(new) {
        ret = groupid_new(&gi.current_gid, tid);
	switch (ret)
        {
	case 0:
	    break;
	case TRYAGAIN:
	    TXN_RETRY(t_groupadd, tid);
	default:
	    TXN_ABORT(t_groupadd, tid);
            warn("OVDB: groupid_new: %s", db_strerror(ret));
	    return false;
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

    key.data = (char *) group;
    key.size = strlen(group);
    val.data = &gi;
    val.size = sizeof gi;

    ret = groupinfo->put(groupinfo, tid, &key, &val, 0);
    switch (ret) {
    case 0:
	break;
    case TRYAGAIN:
	TXN_RETRY(t_groupadd, tid);
    default:
	TXN_ABORT(t_groupadd, tid);
        warn("OVDB: groupadd: groupinfo->put: %s", db_strerror(ret));
	return false;
    }

    if(*flag == NF_FLAG_ALIAS) {
	key.data = (char *) group;
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
            warn("OVDB: groupadd: groupaliases->put: %s", db_strerror(ret));
	    return false;
	}
    }

    TXN_COMMIT(t_groupadd, tid);
    return true;
}

bool
ovdb_groupdel(const char *group)
{
    DBT key, val;
    struct groupinfo gi;
    DB_TXN *tid;
    int ret = 0;

    memset(&key, 0, sizeof key);
    memset(&val, 0, sizeof val);

    /* We only need to set the deleted flag in groupinfo to prevent readers
       from seeing this group.  The actual overview records aren't deleted
       now, since that could take a significant amount of time (and innd
       is who normally calls this function).  The expireover run will
       clean up the deleted groups. */

    TXN_START(t_groupdel, tid);

    if(tid==NULL)
	return false;

    ret = ovdb_getgroupinfo(group, &gi, true, tid, DB_RMW);
    switch (ret)
    {
    case DB_NOTFOUND:
	return true;
    case 0:
	break;
    case TRYAGAIN:
	TXN_RETRY(t_groupdel, tid);
    default:
        warn("OVDB: ovdb_getgroupinfo failed: %s", db_strerror(ret));
	TXN_ABORT(t_groupdel, tid);
	return false;
    }

    gi.status |= GROUPINFO_DELETED;

    key.data = (char *) group;
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
        warn("OVDB: groupadd: groupinfo->put: %s", db_strerror(ret));
	return false;
    }

    switch(ret = groupaliases->del(groupaliases, tid, &key, 0)) {
    case 0:
    case DB_NOTFOUND:
	break;
    case TRYAGAIN:
	TXN_RETRY(t_groupdel, tid);
    default:
        warn("OVDB: groupdel: groupaliases->del: %s", db_strerror(ret));
	TXN_ABORT(t_groupdel, tid);
	return false;
    }

    TXN_COMMIT(t_groupdel, tid);
    return true;
}

bool
ovdb_add(const char *group, ARTNUM artnum, TOKEN token, char *data, int len,
         time_t arrived, time_t expires)
{
    static size_t databuflen = 0;
    static char *databuf;
#ifdef HAVE_ZLIB
    uLong	c_sz = 0;
#else
    #define	c_sz 0
#endif
    DB		*db;
    DBT		key, val;
    DB_TXN	*tid;
    struct groupinfo gi;
    struct datakey dk;
    int ret = 0;

    memset(&dk, 0, sizeof dk);

    if(databuflen == 0) {
	databuflen = BIG_BUFFER;
	databuf = xmalloc(databuflen);
    }
#ifdef HAVE_ZLIB
    if(ovdb_conf.compress) {
	/* Allow for worst-case compression */
	c_sz = len + (len / 1000) + 20;
    }
#endif
    
    if(databuflen < len + sizeof(struct ovdata) + c_sz) {
	databuflen = len + sizeof(struct ovdata) + c_sz;
        databuf = xrealloc(databuf, databuflen);
    }

    /* Hmm...  Berkeley DB needs something like a 'struct iovec' so that we don't
       have to make a new buffer and copy everything in to it. */

    ((struct ovdata *)databuf)->token = token;
    ((struct ovdata *)databuf)->arrived = arrived;
    ((struct ovdata *)databuf)->expires = expires;

#ifdef HAVE_ZLIB
    if(ovdb_conf.compress && len > COMPRESS_MIN) {
	uint32_t sz;
	c_sz -= sizeof(uint32_t);
	ret = compress(
		(Bytef *)(databuf + sizeof(struct ovdata) + sizeof(uint32_t)),
		&c_sz, (Bytef *)data, (uLong)len);
	if(ret != Z_OK) {
	    memcpy(databuf + sizeof(struct ovdata), data, len);
	    len += sizeof(struct ovdata);
	} else {
	    sz = htonl((uint32_t)len);
	    memcpy(databuf + sizeof(struct ovdata), &sz, sizeof(uint32_t));

	    /* The following line is mostly paranoia. Just want to make sure
	       that the first byte is 0 (it should be 0 anyway), so ovdb_search
	       recognizes this as compressed data.  */
	    *(databuf + sizeof(struct ovdata)) = 0;

	    len = c_sz + sizeof(struct ovdata) + sizeof(uint32_t);
	}
    } else {
#endif
      memcpy(databuf + sizeof(struct ovdata), data, len);
      len += sizeof(struct ovdata);
#ifdef HAVE_ZLIB
    }
#endif

    memset(&key, 0, sizeof key);
    memset(&val, 0, sizeof val);

    /* start the transaction */
    TXN_START(t_add, tid);

    if(tid==NULL)
	return false;

    /* first, retrieve groupinfo */
    ret = ovdb_getgroupinfo(group, &gi, true, tid, DB_RMW);
    switch (ret)
    {
    case 0:
	break;
    case DB_NOTFOUND:
	TXN_ABORT(t_add, tid);
	return true;
    case TRYAGAIN:
	TXN_RETRY(t_add, tid);
    default:
	TXN_ABORT(t_add, tid);
        warn("OVDB: add: ovdb_getgroupinfo: %s", db_strerror(ret));
	return false;
    }

    /* adjust groupinfo */
    if(Cutofflow && gi.low > artnum) {
	TXN_ABORT(t_add, tid);
	return true;
    }
    if(gi.low == 0 || gi.low > artnum)
	gi.low = artnum;
    if(gi.high < artnum)
	gi.high = artnum;
    gi.count++;

    /* store groupinfo */
    key.data = (char *) group;
    key.size = strlen(group);
    val.data = &gi;
    val.size = sizeof gi;

    ret = groupinfo->put(groupinfo, tid, &key, &val, 0);
    switch (ret)
    {
    case 0:
	break;
    case TRYAGAIN:
	TXN_RETRY(t_add, tid);
    default:
	TXN_ABORT(t_add, tid);
        warn("OVDB: add: groupinfo->put: %s", db_strerror(ret));
	return false;
    }

    /* store overview */
    db = get_db_bynum(gi.current_db);
    if(db == NULL) {
	TXN_ABORT(t_add, tid);
	return false;
    }
    dk.groupnum = gi.current_gid;
    dk.artnum = htonl((u_int32_t)artnum);

    key.data = &dk;
    key.size = sizeof dk;
    val.data = databuf;
    val.size = len;

    ret = db->put(db, tid, &key, &val, 0);
    switch (ret)
    {
    case 0:
	break;
    case TRYAGAIN:
	TXN_RETRY(t_add, tid);
    default:
	TXN_ABORT(t_add, tid);
        warn("OVDB: add: db->put: %s", db_strerror(ret));
	return false;
    }

    if(artnum < gi.high && gi.status & GROUPINFO_MOVING) {
	/* If the GROUPINFO_MOVING flag is set, then expireover
	   is writing overview records under a new groupid.
	   If this overview record is not at the highmark,
	   we need to also store it under the new groupid */
	db = get_db_bynum(gi.new_db);
	if(db == NULL) {
	    TXN_ABORT(t_add, tid);
	    return false;
	}
	dk.groupnum = gi.new_gid;

        ret = db->put(db, tid, &key, &val, 0);
	switch (ret)
        {
	case 0:
	    break;
	case TRYAGAIN:
	    TXN_RETRY(t_add, tid);
	default:
	    TXN_ABORT(t_add, tid);
            warn("OVDB: add: db->put: %s", db_strerror(ret));
	    return false;
	}
    }

    TXN_COMMIT(t_add, tid);
    return true;
}

bool ovdb_cancel(const char *group UNUSED, ARTNUM artnum UNUSED)
{
    return true;
}

#ifdef HAVE_ZLIB
static char *
myuncompress(char *buf, size_t buflen, size_t *newlen)
{
    static char *dbuf = NULL;
    static uLongf dbuflen = 0, ulen;
    uint32_t sz;
    int ret;

    memcpy(&sz, buf, sizeof(sz));
    sz = ntohl(sz);

    if(sz >= dbuflen) {
	if(dbuflen == 0) {
	    dbuflen = sz + 512;
	    dbuf = xmalloc(dbuflen);
	} else {
	    dbuflen = sz + 512;
	    dbuf = xrealloc(dbuf, dbuflen);
	}
    }
    ulen = dbuflen - 1;

    ret = uncompress((Bytef *)dbuf, &ulen, (Bytef *)(buf + sizeof(uint32_t)), buflen - sizeof(uint32_t));
    if(ret != Z_OK) {
	warn("OVDB: uncompress failed");
	return NULL;
    }
    dbuf[ulen] = 0;	/* paranoia */
    if(newlen)
	*newlen = ulen;
    return dbuf;
}
#endif

struct ovdbsearch {
    DBC *cursor;
    group_id_t gid;
    u_int32_t firstart;
    u_int32_t lastart;
    int state;
};

/* Even though nnrpd only does one search at a time, a read server process
   could do many concurrent searches; hence we must keep track of an arbitrary
   number of open searches */
static struct ovdbsearch **searches = NULL;
static int nsearches = 0;
static int maxsearches = 0;

void *
ovdb_opensearch(const char *group, int low, int high)
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

	if (csend(&rs, sizeof(rs)) < 0)
            return NULL;
	if (csend(group, rs.grouplen) < 0)
            return NULL;
	crecv(&repl, sizeof(repl));

	if(repl.status != CMD_OPENSRCH)
	    return NULL;

	return repl.handle;
    }

    ret = ovdb_getgroupinfo(group, &gi, true, NULL, 0);
    switch (ret)
    {
    case 0:
	break;
    case DB_NOTFOUND:
	return NULL;
    default:
        warn("OVDB: ovdb_getgroupinfo failed: %s", db_strerror(ret));
	return NULL;
    }

    s = xmalloc(sizeof(struct ovdbsearch));
    db = get_db_bynum(gi.current_db);
    if(db == NULL) {
	free(s);
	return NULL;
    }

    ret = db->cursor(db, NULL, &(s->cursor), 0);
    if (ret != 0) {
        warn("OVDB: opensearch: s->db->cursor: %s", db_strerror(ret));
	free(s);
	return NULL;
    }

    s->gid = gi.current_gid;
    s->firstart = low;
    s->lastart = high;
    s->state = 0;

    if(searches == NULL) {
	nsearches = 0;
	maxsearches = 50;
	searches = xmalloc(sizeof(struct ovdbsearch *) * maxsearches);
    }
    if(nsearches == maxsearches) {
	maxsearches += 50;
	searches = xrealloc(searches, sizeof(struct ovdbsearch *) * maxsearches);
    }
    searches[nsearches] = s;
    nsearches++;

    return (void *)s;
}

bool
ovdb_search(void *handle, ARTNUM *artnum, char **data, int *len,
            TOKEN *token, time_t *arrived)
{
    struct ovdbsearch *s = (struct ovdbsearch *)handle;
    DBT key, val;
    u_int32_t flags;
    struct ovdata ovd;
    uint32_t sz = 0;
    struct datakey dk;
    int ret;
    char *dp;

    if (clientmode) {
	struct rs_cmd rs;
	struct rs_srch repl;
	static char *databuf;
	static int buflen = 0;

	rs.what = CMD_SRCH;
	rs.handle = handle;

	if (csend(&rs, sizeof(rs)) < 0)
            return false;
	if (crecv(&repl, sizeof(repl)) < 0)
            return false;

	if(repl.status != CMD_SRCH)
	    return false;
	if(repl.len > buflen) {
	    if(buflen == 0) {
		buflen = repl.len + 512;
		databuf = xmalloc(buflen);
	    } else {
		buflen = repl.len + 512;
                databuf = xrealloc(databuf, buflen);
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
	return true;
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
	return false;
    default:
        warn("OVDB: OVsearch called again after false return");
	return false;
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
	return false;
    default:
        warn("OVDB: search: c_get: %s", db_strerror(ret));
	s->state = 3;
	s->cursor->c_close(s->cursor);
	s->cursor = NULL;
	return false;
    }

    if(key.size != sizeof(struct datakey)) {
	s->state = 3;
	s->cursor->c_close(s->cursor);
	s->cursor = NULL;
	return false;
    }

    if(dk.groupnum != s->gid || ntohl(dk.artnum) > s->lastart) {
	s->state = 3;
	s->cursor->c_close(s->cursor);
	s->cursor = NULL;
	return false;
    }

    if( ((len || data) && val.size <= sizeof(struct ovdata))
	|| ((token || arrived) && val.size < sizeof(struct ovdata)) ) {
        warn("OVDB: search: bad value length");
	s->state = 3;
	s->cursor->c_close(s->cursor);
	s->cursor = NULL;
	return false;
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

    dp = (char *)val.data + sizeof(struct ovdata);
#ifdef HAVE_ZLIB
    if(val.size >= (sizeof(struct ovdata) + sizeof(uint32_t)) && *dp == 0) {
	/* data is compressed */
	memcpy(&sz, dp, sizeof(uint32_t));
	sz = ntohl(sz);
	if(sz > MAX_UNZIP_SZ) {  /* sanity check */
	    warn("OVDB: search: bogus sz: %d", sz);
	    sz = 0;
	}
    }
#endif

    if(len) {
	if(sz) {
	    *len = sz;
	} else {
	    *len = val.size - sizeof(struct ovdata);
	}
    }

    if(data) {
#ifdef HAVE_ZLIB
	if(sz && val.size > sizeof(struct ovdata) + sizeof(uint32_t)) {
	    *data = myuncompress(dp, val.size - sizeof(struct ovdata), NULL);
	    if(*data == NULL) {
		s->state = 3;
		s->cursor->c_close(s->cursor);
		s->cursor = NULL;
		return false;
	    }
	} else
#endif
	  *data = dp;
    }

    return true;
}

void ovdb_closesearch(void *handle)
{
    int i;
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

	for(i = 0; i < nsearches; i++) {
	    if(s == searches[i]) {
		break;
	    }
	}
	nsearches--;
	for( ; i < nsearches; i++) {
	    searches[i] = searches[i+1];
	}

	free(handle);
    }
}

bool
ovdb_getartinfo(const char *group, ARTNUM artnum, TOKEN *token)
{
    int ret, cdb = 0;
    group_id_t cgid = 0;
    DB *db;
    DBT key, val;
    struct ovdata ovd;
    struct datakey dk;
    struct groupinfo gi;
    int pass = 0;

    if(clientmode) {
	struct rs_cmd rs;
	struct rs_artinfo repl;

	rs.what = CMD_ARTINFO;
	rs.grouplen = strlen(group)+1;
	rs.artlo = artnum;

	if (csend(&rs, sizeof(rs)) < 0)
            return false;
	if (csend(group, rs.grouplen) < 0)
            return false;
	crecv(&repl, sizeof(repl));

	if(repl.status != CMD_ARTINFO)
	    return false;

	if(token)
	    *token = repl.token;

	return true;
    }

    while(1) {
        ret = ovdb_getgroupinfo(group, &gi, true, NULL, 0);
	switch (ret)
        {
	case 0:
	    break;
	case DB_NOTFOUND:
	    return false;
	default:
            warn("OVDB: ovdb_getgroupinfo failed: %s", db_strerror(ret));
	    return false;
	}

	if(pass) {
	    /* This was our second groupinfo retrieval; because the article
	       retrieval came up empty.  If the group ID hasn't changed
	       since the first groupinfo retrieval, we can assume the article
	       is definitely not there.  Otherwise, we'll try to retrieve
	       it the article again. */
	    if(cdb == gi.current_db && cgid == gi.current_gid)
		return false;
	}

	db = get_db_bynum(gi.current_db);
	if(db == NULL)
	    return false;

	memset(&dk, 0, sizeof dk);
	dk.groupnum = gi.current_gid;
	dk.artnum = htonl((u_int32_t)artnum);

	memset(&key, 0, sizeof key);
	memset(&val, 0, sizeof val);

	key.data = &dk;
	key.size = sizeof dk;

        /* caller doesn't need data, so we don't have to retrieve it all */
        val.flags = DB_DBT_PARTIAL;

        if(token)
            val.dlen = sizeof(struct ovdata);

	switch(ret = db->get(db, NULL, &key, &val, 0)) {
	case 0:
	case DB_NOTFOUND:
	    break;
	default:
            warn("OVDB: getartinfo: db->get: %s", db_strerror(ret));
	    return false;
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
	    return false;
	}
	break;
    }

    if(token && val.size < sizeof(struct ovdata)) {
        warn("OVDB: getartinfo: data too short");
	return false;
    }

    if(token) {
	memcpy(&ovd, val.data, sizeof(struct ovdata));
	*token = ovd.token;
    }
    return true;
}

bool
ovdb_expiregroup(const char *group, int *lo, struct history *h)
{
    DB *db, *ndb = NULL;
    DBT key, val, nkey, gkey, gval;
    DB_TXN *tid;
    DBC *cursor = NULL;
    int ret = 0, delete, old_db = 0, cleanup;
    struct groupinfo gi;
    struct ovdata ovd;
    struct datakey dk, ndk;
    group_id_t old_gid = 0;
    ARTHANDLE *ah;
    u_int32_t artnum = 0, currentart, lowest;
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
	return false;

    cleanup = 0;

    ret = ovdb_getgroupinfo(group, &gi, true, tid, DB_RMW);
    switch (ret)
    {
    case 0:
	break;
    case TRYAGAIN:
	TXN_RETRY(t_expgroup_1, tid);
    default:
        warn("OVDB: expiregroup: ovdb_getgroupinfo failed: %s",
             db_strerror(ret));
    case DB_NOTFOUND:
	TXN_ABORT(t_expgroup_1, tid);
	return false;
    }

    if(gi.status & GROUPINFO_EXPIRING) {
	/* is there another expireover working on this group? */
        ret = kill(gi.expiregrouppid, 0);
	switch(ret)
        {
	case 0:
	case EPERM:
	    TXN_ABORT(t_expgroup_1, tid);
	    return false;
	}

	/* a previous expireover run must've died.  We'll clean
	   up after it */
	if(gi.status & GROUPINFO_MOVING) {
	    cleanup = 1;
	    old_db = gi.new_db;
	    old_gid = gi.new_gid;

	    ret = mk_temp_groupinfo(old_db, old_gid, tid);
	    switch(ret) {
	    case 0:
	    	break;
	    case TRYAGAIN:
	    	TXN_RETRY(t_expgroup_1, tid);
	    default:
	    	TXN_ABORT(t_expgroup_1, tid);
		return false;
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
	return false;
    }

    gi.status |= GROUPINFO_EXPIRING;
    gi.expiregrouppid = getpid();
    if(compact) {
	gi.status |= GROUPINFO_MOVING;
	gi.new_db = gi.current_db;
	ndb = db;
        ret = groupid_new(&gi.new_gid, tid);
	switch (ret)
        {
	case 0:
	    break;
	case TRYAGAIN:
	    TXN_RETRY(t_expgroup_1, tid);
	default:
	    TXN_ABORT(t_expgroup_1, tid);
            warn("OVDB: expiregroup: groupid_new: %s", db_strerror(ret));
	    return false;
	}
    }

    key.data = (char *) group;
    key.size = strlen(group);
    val.data = &gi;
    val.size = sizeof gi;

    ret = groupinfo->put(groupinfo, tid, &key, &val, 0);
    switch (ret)
    {
    case 0:
	break;
    case TRYAGAIN:
	TXN_RETRY(t_expgroup_1, tid);
    default:
	TXN_ABORT(t_expgroup_1, tid);
        warn("OVDB: expiregroup: groupinfo->put: %s", db_strerror(ret));
	return false;
    }
    TXN_COMMIT(t_expgroup_1, tid);

    if(cleanup) {
	if(delete_all_records(old_db, old_gid) == 0) {
	    rm_temp_groupinfo(old_gid);
	}
    }

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
    gkey.data = (char *) group;
    gkey.size = strlen(group);
    gval.data = &gi;
    gval.size = sizeof gi;

    while(1) {
	TXN_START(t_expgroup_loop, tid);
	if(tid==NULL)
	    return false;
        done = 0;
	newcount = 0;

        ret = ovdb_getgroupinfo(group, &gi, false, tid, DB_RMW);
	switch (ret)
        {
	case 0:
	    break;
	case TRYAGAIN:
	    TXN_RETRY(t_expgroup_loop, tid);
	default:
	    TXN_ABORT(t_expgroup_loop, tid);
            warn("OVDB: expiregroup: ovdb_getgroupinfo: %s",
                 db_strerror(ret));
	    return false;
	}

        ret = db->cursor(db, tid, &cursor, 0);
	switch (ret)
        {
	case 0:
	    break;
	case TRYAGAIN:
	    TXN_RETRY(t_expgroup_loop, tid);
	default:
	    TXN_ABORT(t_expgroup_loop, tid);
            warn("OVDB: expiregroup: db->cursor: %s", db_strerror(ret));
	    return false;
	}

	dk.groupnum = gi.current_gid;
	dk.artnum = htonl(currentart);
	key.data = &dk;
	key.size = key.ulen = sizeof dk;
	key.flags = DB_DBT_USERMEM;

	for(i=0; i < EXPIREGROUP_TXN_SIZE; i++) {
            ret = cursor->c_get(cursor, &key, &val,
                                i == 0 ? DB_SET_RANGE : DB_NEXT);
	    switch (ret)
            {
	    case 0:
	    case DB_NOTFOUND:
		break;
	    case TRYAGAIN:
		cursor->c_close(cursor);
		TXN_RETRY(t_expgroup_loop, tid);
	    default:
		cursor->c_close(cursor);
		TXN_ABORT(t_expgroup_loop, tid);
                warn("OVDB: expiregroup: c_get: %s", db_strerror(ret));
		return false;
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
		char *p = (char *)val.data + sizeof(ovd);
		size_t sz = val.size - sizeof(ovd);
#ifdef HAVE_ZLIB
		if(*p == 0)
		    p = myuncompress(p, sz, &sz);
		if(p == NULL) {
		    p = (char *) "";
		    delete = 1;
		}
#endif
		memcpy(&ovd, val.data, sizeof ovd);

		ah = NULL;
		if (!SMprobe(EXPENSIVESTAT, &ovd.token, NULL) || OVstatall) {
		    if((ah = SMretrieve(ovd.token, RETR_STAT)) == NULL) {
			delete = 1;
		    } else
			SMfreearticle(ah);
		} else {
		    if (!delete && !OVhisthasmsgid(h, p)) {
			delete = 1;
		    }
		}
		if (!delete && innconf->groupbaseexpiry
			    && OVgroupbasedexpire(ovd.token, group,
				    p, sz, ovd.arrived, ovd.expires)) {
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
                        warn("OVDB: expiregroup: c_del: %s",
                             db_strerror(ret));
			return false;
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
                        warn("OVDB: expiregroup: ndb->put: %s",
                             db_strerror(ret));
			return false;
		    }
		}
		newcount++;
                if (lowest != (u_int32_t) -1)
                    if (lowest == 0 || artnum < lowest)
                        lowest = artnum;
	    }
	}
	/* end of for loop */

	if(cursor->c_close(cursor) == TRYAGAIN) {
	    TXN_RETRY(t_expgroup_loop, tid);
	}

	if(lowest != 0 && lowest != (u_int32_t) -1)
	    gi.low = lowest;

	if(done) {
	    if(compact) {
		old_db = gi.current_db;
		gi.current_db = gi.new_db;
		old_gid = gi.current_gid;
		gi.current_gid = gi.new_gid;
		gi.status &= ~GROUPINFO_MOVING;

		ret = mk_temp_groupinfo(old_db, old_gid, tid);
		switch(ret) {
		case 0:
		    break;
		case TRYAGAIN:
		    TXN_RETRY(t_expgroup_loop, tid);
		default:
		    TXN_ABORT(t_expgroup_loop, tid);
		    return false;
		}
	    }

	    gi.status &= ~GROUPINFO_EXPIRING;
	    gi.expired = time(NULL);
	    if(gi.count == 0 && lowest == 0)
		gi.low = gi.high+1;
	}

        ret = groupinfo->put(groupinfo, tid, &gkey, &gval, 0);
	switch (ret)
        {
	case 0:
	    break;
	case TRYAGAIN:
	    TXN_RETRY(t_expgroup_loop, tid);
	default:
	    TXN_ABORT(t_expgroup_loop, tid);
            warn("OVDB: expiregroup: groupinfo->put: %s", db_strerror(ret));
	    return false;
	}
        TXN_COMMIT(t_expgroup_loop, tid);

	currentcount += newcount;
	if(lowest != 0)
	    lowest = (u_int32_t) -1;

	if(done)
	    break;

	currentart = artnum+1;
    }

    if(compact) {
	if(delete_all_records(old_db, old_gid) == 0) {
	    rm_temp_groupinfo(old_gid);
	}
    }

    if(currentcount != gi.count) {
        notice("OVDB: expiregroup: recounting %s", group);

	TXN_START(t_expgroup_recount, tid);
	if(tid == NULL)
	    return false;

	switch(ret = ovdb_getgroupinfo(group, &gi, false, tid, DB_RMW)) {
	case 0:
	    break;
	case TRYAGAIN:
	    TXN_RETRY(t_expgroup_recount, tid);
	default:
	    TXN_ABORT(t_expgroup_recount, tid);
            warn("OVDB: expiregroup: ovdb_getgroupinfo: %s",
                 db_strerror(ret));
	    return false;
	}

	if(count_records(&gi) != 0) {
	    TXN_ABORT(t_expgroup_recount, tid);
	    return false;
	}

        ret = groupinfo->put(groupinfo, tid, &gkey, &gval, 0);
	switch (ret)
        {
	case 0:
	    break;
	case TRYAGAIN:
	    TXN_RETRY(t_expgroup_recount, tid);
	default:
	    TXN_ABORT(t_expgroup_recount, tid);
            warn("OVDB: expiregroup: groupinfo->put: %s", db_strerror(ret));
	    return false;
	}
        TXN_COMMIT(t_expgroup_recount, tid);
    }

    if(lo)
	*lo = gi.low;
    return true;
}

bool ovdb_ctl(OVCTLTYPE type, void *val)
{
    int *i;
    OVSORTTYPE *sorttype;
    bool *boolval;

    switch (type) {
    case OVSPACE:
        i = (int *)val;
        *i = -1;
        return true;
    case OVSORT:
        sorttype = (OVSORTTYPE *)val;
        *sorttype = OVNEWSGROUP;
        return true;
    case OVCUTOFFLOW:
        Cutofflow = *(bool *)val;
        return true;
    case OVSTATICSEARCH:
	i = (int *)val;
	*i = true;
	return true;
    case OVCACHEKEEP:
    case OVCACHEFREE:
        boolval = (bool *)val;
        *boolval = false;
        return true;
    default:
        return false;
    }
}

void ovdb_close_berkeleydb(void)
{
    if(OVDBenv) {
	/* close db environment */
	OVDBenv->close(OVDBenv, 0);
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

    while(searches != NULL && nsearches) {
	ovdb_closesearch(searches[0]);
    }
    if(searches != NULL) {
	free(searches);
	searches = NULL;
    }

    if(dbs) {
	/* close databases */
	for(i = 0; i < ovdb_conf.numdbfiles; i++)
	    close_db_file(i);

	free(dbs);
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

#endif /* HAVE_BDB */

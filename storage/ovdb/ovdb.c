/*
 * ovdb.c
 * Overview storage using BerkeleyDB 2.x/3.x
 *
 * 2000-10-05 : From Dan Riley: struct datakey needs to be zero'd, for
 *              64-bit OSs where the struct has internal padding bytes.
 *              Artnum member of struct datakey changed from ARTNUM to u_int32_t.
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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pwd.h>
#include "macros.h"
#include "conffile.h"
#include "libinn.h"
#include "paths.h"
#include "storage.h"
#include "ov.h"
#include "ovinterface.h"
#include "ovdb.h"


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

BOOL ovdb_expiregroup(char *group, int *lo)
{ return FALSE; }

BOOL ovdb_ctl(OVCTLTYPE type, void *val)
{ return FALSE; }

void ovdb_close(void) { }

#else /* USE_BERKELEY_DB */

#include <db.h>

#if DB_VERSION_MAJOR == 2
#if DB_VERSION_MINOR < 6
#error Need BerkeleyDB 2.6.x, 2.7.x, or 3.x
#endif
#else
#if DB_VERSION_MAJOR != 3 && DB_VERSION_MAJOR != 4
#error Need BerkeleyDB 2.6.x, 2.7.x, or 3.x
#endif
#endif

/*
 * How data is stored:
 *
 * Each group is assigned an integer ID.  The mapping between a group name
 * and its ID is stored in the groupsbyname hash DB.  Overview data itself
 * is stored in one or more btree DBs.  The specific DB file that is used
 * to store data for a certain group is chosen by taking the hash of the
 * group name, copying the first bytes of the hash into an int, and then
 * modding the int value to the number of DBs.
 *
 * Each group has one groupstats structure in the groupstats DB, whose key
 * is the four-byte group ID.  The overview records for the group have a
 * 'struct datakey' as their keys, which consists of the group ID (in
 * native byteorder) followed by the article number in network byteorder.
 * The reason for storing the article number in net byte order (big-endian)
 * is that the keys will sort correctly using BerkeleyDB's default sort
 * function (basically, a memcmp).
 *
 * The overview records consist of a 'struct ovdata' followed by the actual
 * overview data.  The struct ovdata contains the token and arrival time.
 */ 

struct ovdb_conf {
    char *home;		/* path to directory where db files are stored */
    int  txn_nosync;	/* whether to pass DB_TXN_NOSYNC to db_appinit */
    int  numdbfiles;
    size_t cachesize;
    size_t pagesize;
    int minkey;
};

struct groupstats {
    ARTNUM low;
    ARTNUM high;
    int count;
    int flag;
    time_t expired;	/* when this group was last touched by expiregroup */
};

typedef u_int32_t group_id_t;

struct datakey {
    group_id_t groupnum;	/* must be the first member of this struct */
    u_int32_t artnum;
};

struct ovdata {
    TOKEN token;
    time_t arrived;
    time_t expires;
};

struct ovdbsearch {
    DB *db;
    DBC *cursor;
    struct datakey lokey;
    struct datakey hikey;
    int state;
};


#define DATA_VERSION 1

static int OVDBmode;
static BOOL Cutofflow;
static struct ovdb_conf ovdb_conf;
static DB_ENV *OVDBenv = NULL;
static DB **dbs = NULL;
static int oneatatime = 0;
static int current_db = -1;
static time_t eo_start = 0;

static DB *groupstats = NULL;
static DB *groupsbyname = NULL;
static DB *groupaliases = NULL;

#define OVDBtxn_nosync	1
#define OVDBnumdbfiles	2
#define OVDBpagesize	3
#define OVDBcachesize	4
#define OVDBminkey	5

static CONFTOKEN toks[] = {
  { OVDBtxn_nosync, "txn_nosync" },
  { OVDBnumdbfiles, "numdbfiles" },
  { OVDBpagesize, "pagesize" },
  { OVDBcachesize, "cachesize" },
  { OVDBminkey, "minkey" },
  { 0, NULL },
};

#define _PATH_OVDBCONF "ovdb.conf"

/*********** internal functions ***********/

#if DB_VERSION_MAJOR == 2
static char *db_strerror(int err)
{
    switch(err) {
    case DB_RUNRECOVERY:
	return "Recovery Needed";
    default:
	return strerror(err);
    }
}

static int my_txn_begin(DB_ENV *env, DB_TXN *parent, DB_TXN **tid)
{
    return txn_begin(env->tx_info, parent, tid);
}

#define my_txn_commit txn_commit
#define TRYAGAIN EAGAIN

#else
/* version 3 */

static int my_txn_begin(DB_ENV *env, DB_TXN *parent, DB_TXN **tid)
{
    return txn_begin(env, parent, tid, 0);
}

static int my_txn_commit(DB_TXN *tid)
{
    return txn_commit(tid, 0);
}

#define TRYAGAIN DB_LOCK_DEADLOCK

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

static void read_ovdb_conf(void)
{
    int done = 0;
    CONFFILE *f;
    CONFTOKEN *tok;
    BOOL b;
    long l;

    /* defaults */
    ovdb_conf.home = innconf->pathoverview;
    ovdb_conf.txn_nosync = 1;
    ovdb_conf.numdbfiles = 32;
    ovdb_conf.pagesize = 8192;
    ovdb_conf.cachesize = 8000 * 1024;
    ovdb_conf.minkey = 0;

    f = CONFfopen(cpcatpath(innconf->pathetc, _PATH_OVDBCONF));

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
	    }
	}
	CONFfclose(f);
    }

    /* If user did not specify minkey, choose one based on pagesize */
    if(ovdb_conf.minkey == 0) {
	ovdb_conf.minkey = ovdb_conf.pagesize / 2048;
	if(ovdb_conf.minkey < 2)
	    ovdb_conf.minkey = 2;
    }
}

/* Function that db will use to report errors */
static void OVDBerror(char *db_errpfx, char *buffer)
{
#ifdef TEST_BDB
    fprintf(stderr, "OVDB: %s\n", buffer);
#else
    syslog(L_ERROR, "OVDB: %s", buffer);
#endif
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

static u_int32_t _db_flags = 0;
static char   ** _dbnames;
#if DB_VERSION_MAJOR == 2
static DB_INFO   _dbinfo;
#endif

static int open_db_file(int which)
{
    int ret;

    if(dbs[which] != NULL)
	return 0;

#if DB_VERSION_MAJOR == 2
    if(ret = db_open(_dbnames[which], DB_BTREE, _db_flags, 0666, OVDBenv,
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

#if (DB_VERSION_MAJOR >= 4 && DB_VERSION_MAJOR >= 1)
/* starting sometime early db 4.X, db->open gets a new parameter */
    if(ret = (dbs[which])->open(dbs[which], 0, _dbnames[which], NULL,
        DB_BTREE, _db_flags, 0666)) {
	(dbs[which])->close(dbs[which], 0);
	dbs[which] = NULL;
	return ret;
    }
#else
    if(ret = (dbs[which])->open(dbs[which], _dbnames[which], NULL, DB_BTREE,
		_db_flags, 0666)) {
	(dbs[which])->close(dbs[which], 0);
	dbs[which] = NULL;
	return ret;
    }
#endif /* #if DB_VERSION_MAJOR >= 4 */
#endif /* #if DB_VERSION_MAJOR == 2 */
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
    if(oneatatime) {
	if(which != current_db && current_db != -1)
	    close_db_file(current_db);

	if(ret = open_db_file(which))
	    syslog(L_ERROR, "OVDB: open_db_file failed: %s", db_strerror(ret));

	current_db = which;
    }
    return(dbs[which]);
}

/* returns the db that the given newsgroup is stored in,
   opening it if necessary */
static DB *get_db(char *group)
{
    return get_db_bynum(which_db(group));
}

/* returns group ID for given group in gno */
static int groupnum(char *group, group_id_t *gno)
{
    int ret;
    DBT key, val;

    if(group == NULL)	/* just in case */
	return DB_NOTFOUND;

    memset(&key, 0, sizeof key);
    memset(&val, 0, sizeof val);

    key.data = group;
    key.size = strlen(group);
    val.data = gno;
    val.ulen = sizeof(group_id_t);
    val.flags = DB_DBT_USERMEM;

    if(ret = groupsbyname->get(groupsbyname, NULL, &key, &val, 0)) {
	if(ret != DB_NOTFOUND)
	    syslog(L_ERROR, "OVDB: groupnum: get: %s", db_strerror(ret));
	return ret;
    }

    if(val.size != sizeof(group_id_t)) {
	syslog(L_ERROR, "OVDB: groupnum: wrong size for groupnum val (%d, %s)", val.size, group);
	return DB_NOTFOUND;
    }

    return 0;
}

static void delete_all_records(group_id_t gno)
{
    DB *db;
    DBC *dbcursor;
    DBT key, val;
    struct datakey dk;
    int i, ret;

    memset(&key, 0, sizeof key);
    memset(&val, 0, sizeof val);
    memset(&dk, 0, sizeof dk);
    dk.groupnum = gno;
    dk.artnum = 0;

    /* Have to check every db file, since we don't know which one has
       the data. */

    for(i = 0; i < ovdb_conf.numdbfiles; i++) {
	db = get_db_bynum(i);
	if(db == NULL)
	    continue;

	/* get a cursor to traverse the ov records and delete them */
	if(ret = db->cursor(db, NULL, &dbcursor, 0)) {
	    syslog(L_ERROR, "OVDB: delete_all_records: db->cursor: %s", db_strerror(ret));
	    continue;
	}

	key.data = &dk;
	key.size = sizeof dk;
	val.flags = DB_DBT_PARTIAL;

	switch(ret = dbcursor->c_get(dbcursor, &key, &val, DB_SET_RANGE)) {
	case 0:
	case DB_NOTFOUND:
	    break;	/* from the switch */
	default:
	    syslog(L_ERROR, "OVDB: delete_all_records: dbcursor->c_get: %s", db_strerror(ret));
	}

	while(1) {
	    if(ret != 0
		    || key.size != sizeof dk
		    || memcmp(key.data, &(dk.groupnum), sizeof(dk.groupnum))) {
		/* this key is for a different group; so we're done */
		dbcursor->c_close(dbcursor);
		break;
	    }

	    dbcursor->c_del(dbcursor, 0);

	    ret = dbcursor->c_get(dbcursor, &key, &val, DB_NEXT);
	}
    }
}

/* This function deletes overview records for deleted or forgotton groups */
static BOOL delete_old_stuff()
{
    DBT key, val;
    DBC *cursor;
    group_id_t gno;
    struct groupstats gs;
    int ret;

    memset(&key, 0, sizeof key);
    memset(&val, 0, sizeof val);

    key.data = &gno;
    key.ulen = key.dlen = sizeof gno;
    key.flags = DB_DBT_USERMEM | DB_DBT_PARTIAL;
    val.data = &gs;
    val.ulen = key.dlen = sizeof gs;
    val.flags = DB_DBT_USERMEM | DB_DBT_PARTIAL;

    /* get a cursor to traverse all of the groupstats records */
    if(ret = groupstats->cursor(groupstats, NULL, &cursor, 0)) {
	syslog(L_ERROR, "OVDB: delete_old_stuff: groupstats->cursor: %s", db_strerror(ret));
	return FALSE;
    }

    while((ret = cursor->c_get(cursor, &key, &val, DB_NEXT)) == 0) {
	if(key.size != sizeof(group_id_t))
	    continue;
	if(val.size != sizeof(struct groupstats))
	    continue;

	if(gs.expired >= eo_start)
	    continue;

	delete_all_records(gno);

	/* delete the groupstats key */
	cursor->c_del(cursor, 0);
    }
    if(ret != DB_NOTFOUND)
	syslog(L_ERROR, "OVDB: delete_old_stuff: cursor->c_get: %s", db_strerror(ret));

    cursor->c_close(cursor);

    /* TO DO:  iterate over groupsbyname keys and delete those that
       don't have a groupstats entry */

    return TRUE;
}

#if DB_VERSION_MAJOR >= 3
static int upgrade_database(char *name)
{
    int ret;
    DB *db;

    if(ret = db_create(&db, OVDBenv, 0))
	return ret;
    ret = db->upgrade(db, name, 0);
    db->close(db, 0);
    return ret;
}

static int upgrade_databases()
{
    int ret, i;
    char name[50];

    if(ret = upgrade_database("groupsbyname"))
	return ret;
    if(ret = upgrade_database("groupstats"))
	return ret;
    if(ret = upgrade_database("groupaliases"))
	return ret;
    if(ret = upgrade_database("version"))
	return ret;
    for(i = 0; i < ovdb_conf.numdbfiles; i++) {
	sprintf(name, "ov%05d", i);
        if(ret = upgrade_database(name))
	    return ret;
    }
    return 0;
}
#endif


/*********** external functions ***********/

int ovdb_open_berkeleydb(int mode, int flags)
{
    int ret;
    u_int32_t ai_flags = DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN;
    u_int32_t dv;
    DB *vdb;
#if DB_VERSION_MAJOR == 2
    DB_INFO dbinfo;
#endif
    DBT key, val;

    if(!ovdb_check_user()) {
	syslog(L_FATAL, "OVDB: must be running as " NEWSUSER " to access overview.");
	return -1;
    }

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

    /* initialize environment */
    if(ret = db_appinit(ovdb_conf.home, NULL, OVDBenv, ai_flags)) {
	DISPOSE(OVDBenv);
	OVDBenv = NULL;
	syslog(L_FATAL, "OVDB: db_appinit failed: %s", db_strerror(ret));
	return ret;
    }

    /* open version db */
    memset(&dbinfo, 0, sizeof dbinfo);

    if(ret = db_open("version", DB_BTREE, _db_flags, 0666, OVDBenv,
		    &dbinfo, &vdb)) {
	syslog(L_FATAL, "OVDB: db_open failed: %s", db_strerror(ret));
	return ret;
    }
#else
    if(ret = db_env_create(&OVDBenv, 0)) {
	syslog(L_FATAL, "OVDB: db_env_create: %s", db_strerror(ret));
	return ret;
    }

    OVDBenv->set_errcall(OVDBenv, OVDBerror);
    OVDBenv->set_cachesize(OVDBenv, 0, ovdb_conf.cachesize, 1);

#if (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR >= 2) || DB_VERSION_MAJOR >= 4
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

    if(flags & OVDB_DBUPGRADE) {
	if(ret = upgrade_databases()) {
	   fprintf(stderr, "OVDB: Unable to upgrade databases: %s\n", db_strerror(ret));
	   return ret;
	}
    }

    /* open version db */
    if(ret = db_create(&vdb, OVDBenv, 0)) {
	syslog(L_FATAL, "OVDB: open: db_create: %s", db_strerror(ret));
	return ret;
    }

#if (DB_VERSION_MAJOR >= 4 && DB_VERSION_MAJOR >= 1)
/* starting sometime early db 4.X, db->open gets a new parameter */
    if(ret = vdb->open(vdb, 0, "version", NULL, DB_BTREE,
		_db_flags, 0666)) {
	vdb->close(vdb, 0);
	syslog(L_FATAL, "OVDB: open: version->open: %s", db_strerror(ret));
	return ret;
    }
#else
    if(ret = vdb->open(vdb, "version", NULL, DB_BTREE, _db_flags, 0666)) {
        vdb->close(vdb, 0);
        syslog(L_FATAL, "OVDB: open: version->open: %s", db_strerror(ret));
        return ret;
    }
#endif /* DB_VERSION_MAJOR >= 4 */
#endif /* DB_VERSION_MAJOR == 2 */

    memset(&key, 0, sizeof key);
    memset(&val, 0, sizeof val);
    key.data = "dataversion";
    key.size = sizeof("dataversion");
    if(ret = vdb->get(vdb, NULL, &key, &val, 0)) {
	if(ret != DB_NOTFOUND) {
	    syslog(L_FATAL, "OVDB: open: can't retrieve version: %s", db_strerror(ret));
	    return FALSE;
	}
    }
    if(ret == DB_NOTFOUND || val.size != sizeof dv) {
	dv = DATA_VERSION;
	if(!(OVDBmode & OV_WRITE))
	    return EACCES;

	val.data = &dv;
	val.size = sizeof dv;
	if(ret = vdb->put(vdb, NULL, &key, &val, 0)) {
	    syslog(L_FATAL, "OVDB: open: can't store version: %s", db_strerror(ret));
	    return ret;
	}
    } else
	memcpy(&dv, val.data, sizeof dv);

    vdb->close(vdb, 0);

    if(dv > DATA_VERSION) {
	syslog(L_FATAL, "OVDB: can't open database: unknown version %d", dv);
	return EINVAL;
    }
    if(dv < DATA_VERSION && !(flags & OVDB_DBUPGRADE)) {
	syslog(L_FATAL, "OVDB: database is an old version, please run ovdb_upgrade");
	return EINVAL;
    }
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

    if(OVDBenv != NULL) {
	syslog(L_ERROR, "ovdb_open called more than once");
	return FALSE;
    }

    if(ovdb_open_berkeleydb(mode, 0) != 0)
	return FALSE;

    if(OVDBmode & OV_WRITE) {
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
    _dbnames = NEW(char *, ovdb_conf.numdbfiles);
    
    for(i = 0; i < ovdb_conf.numdbfiles; i++) {
	sprintf(name, "ov%05d", i);
	_dbnames[i] = COPY(name);
	if(!oneatatime) {
	    if(ret = open_db_file(i)) {
		syslog(L_FATAL, "OVDB: open_db_file failed: %s", db_strerror(ret));
		return FALSE;
	    }
	}
    }

#if DB_VERSION_MAJOR == 2
    memset(&dbinfo, 0, sizeof dbinfo);

    if(ret = db_open("groupstats", DB_BTREE, _db_flags, 0666, OVDBenv,
		    &dbinfo, &groupstats)) {
	syslog(L_FATAL, "OVDB: db_open failed: %s", db_strerror(ret));
	return FALSE;
    }

    if(ret = db_open("groupsbyname", DB_HASH, _db_flags, 0666, OVDBenv,
		    &dbinfo, &groupsbyname)) {
	syslog(L_FATAL, "OVDB: db_open failed: %s", db_strerror(ret));
	return FALSE;
    }

    if(ret = db_open("groupaliases", DB_HASH, _db_flags, 0666, OVDBenv,
		    &dbinfo, &groupaliases)) {
	syslog(L_FATAL, "OVDB: db_open failed: %s", db_strerror(ret));
	return FALSE;
    }
#else
    if(ret = db_create(&groupstats, OVDBenv, 0)) {
	syslog(L_FATAL, "OVDB: open: db_create: %s", db_strerror(ret));
	return FALSE;
    }

#if (DB_VERSION_MAJOR >= 4 && DB_VERSION_MAJOR >= 1)
/* starting sometime early db 4.X, db->open gets a new parameter */
    if(ret = groupstats->open(groupstats, 0, "groupstats", NULL,
        DB_BTREE, _db_flags, 0666)) {
	groupstats->close(groupstats, 0);
	syslog(L_FATAL, "OVDB: open: groupstats->open: %s", db_strerror(ret));
	return FALSE;
    }
#else
    if(ret = groupstats->open(groupstats, "groupstats", NULL, DB_BTREE,
		_db_flags, 0666)) {
	groupstats->close(groupstats, 0);
	syslog(L_FATAL, "OVDB: open: groupstats->open: %s", db_strerror(ret));
	return FALSE;
    }
#endif /* #if DB_VERSION_MAJOR >= 4 */
    if(ret = db_create(&groupsbyname, OVDBenv, 0)) {
	syslog(L_FATAL, "OVDB: open: db_create: %s", db_strerror(ret));
	return FALSE;
    }
#if (DB_VERSION_MAJOR >= 4 && DB_VERSION_MAJOR >= 1)
    if(ret = groupsbyname->open(groupsbyname, 0, "groupsbyname", NULL, DB_HASH,
		_db_flags, 0666)) {
	groupsbyname->close(groupsbyname, 0);
	syslog(L_FATAL, "OVDB: open: groupsbyname->open: %s", db_strerror(ret));
	return FALSE;
    }
#else
    if(ret = groupsbyname->open(groupsbyname, "groupsbyname", NULL, DB_HASH,
		_db_flags, 0666)) {
	groupsbyname->close(groupsbyname, 0);
	syslog(L_FATAL, "OVDB: open: groupsbyname->open: %s", db_strerror(ret));
	return FALSE;
    }
#endif /* #if DB_VERSION_MAJOR >= 4 */
    if(ret = db_create(&groupaliases, OVDBenv, 0)) {
	syslog(L_FATAL, "OVDB: open: db_create: %s", db_strerror(ret));
	return FALSE;
    }
#if (DB_VERSION_MAJOR >= 4 && DB_VERSION_MAJOR >= 1)
    if(ret = groupaliases->open(groupaliases, 0, "groupaliases", NULL, DB_HASH,
		_db_flags, 0666)) {
	groupaliases->close(groupaliases, 0);
	syslog(L_FATAL, "OVDB: open: groupaliases->open: %s", db_strerror(ret));
	return FALSE;
    }
#else
    if(ret = groupaliases->open(groupaliases, "groupaliases", NULL, DB_HASH,
		_db_flags, 0666)) {
	groupaliases->close(groupaliases, 0);
	syslog(L_FATAL, "OVDB: open: groupaliases->open: %s", db_strerror(ret));
	return FALSE;
    }
#endif /* #if DB_VERSION_MAJOR >= 4 */
#endif /* #if DB_VERSION_MAJOR == 2 */

    Cutofflow = FALSE;
    return TRUE;
}

BOOL ovdb_groupstats(char *group, int *lo, int *hi, int *count, int *flag)
{
    int ret;
    group_id_t gno;
    DBT key, val;
    struct groupstats gs;

    if(groupnum(group, &gno) != 0)
	return FALSE;

    memset(&key, 0, sizeof key);
    memset(&val, 0, sizeof val);

    key.data = &gno;
    key.size = sizeof gno;
    val.data = &gs;
    val.ulen = sizeof gs;
    val.flags = DB_DBT_USERMEM;

    switch(ret = groupstats->get(groupstats, NULL, &key, &val, 0)) {
    case 0:
	break;
    case DB_NOTFOUND:
	return FALSE;
    default:
	syslog(L_ERROR, "OVDB: groupstats->get failed: %s", db_strerror(ret));
	return FALSE;
    }

    if(val.size != sizeof gs) {
	syslog(L_ERROR, "OVDB: wrong size for %s groupstats (%u)\n",
	    group, val.size);
	return FALSE;
    }

    if(lo != NULL)
	*lo = gs.low;
    if(hi != NULL)
	*hi = gs.high;
    if(count != NULL)
	*count = gs.count;
    if(flag != NULL)
	*flag = gs.flag;
    return TRUE;
}

BOOL ovdb_groupadd(char *group, ARTNUM lo, ARTNUM hi, char *flag)
{
    group_id_t gno;
    DBT key, val;
    struct groupstats gs;
    DB_TXN *tid;
    int ret, new = 0;

retry:
    switch(groupnum(group, &gno)) {
    case DB_NOTFOUND:
	new = 1;
    case 0:
	break;
    default:
	return FALSE;
    }
    memset(&key, 0, sizeof key);
    memset(&val, 0, sizeof val);

    if(ret = my_txn_begin(OVDBenv, NULL, &tid)) {
	syslog(L_ERROR, "OVDB: groupadd: txn_begin: %s", db_strerror(ret));
	return FALSE;
    }

    if(!new) {
	key.data = &gno;
	key.size = sizeof gno;
	val.data = &gs;
	val.ulen = sizeof gs;
	val.flags = DB_DBT_USERMEM;

	switch(ret = groupstats->get(groupstats, tid, &key, &val, DB_RMW)) {
	case 0:
	    if(val.size != sizeof gs)	/* invalid size; rewrite groupstats */
		new = 1;
	    break;
	case DB_NOTFOUND:
	    new = 1;
	    break;
	case TRYAGAIN:
	    txn_abort(tid);
	    goto retry;
	default:
	    txn_abort(tid);
	    syslog(L_ERROR, "OVDB: groupadd: groupstats->get: %s", db_strerror(ret));
	    return FALSE;
	}
    } else {
	/* allocate a new group number */
	key.data = "!";
	key.size = 1;
	val.data = &gno;
	val.ulen = sizeof gno;
	val.flags = DB_DBT_USERMEM;

	switch(ret = groupsbyname->get(groupsbyname, tid, &key, &val, 0)) {
	case 0:
	    if(val.size != sizeof gno)
		gno = 1;
	    break;
	case DB_NOTFOUND:
	    gno = 1;
	    break;
	case TRYAGAIN:
	    txn_abort(tid);
	    goto retry;
	default:
	    txn_abort(tid);
	    syslog(L_ERROR, "OVDB: groupadd: groupsbyname->get: %s", db_strerror(ret));
	    return FALSE;
	}
	val.size = sizeof gno;
	/* set new himark */
	gno++;
	switch(ret = groupsbyname->put(groupsbyname, tid, &key, &val, 0)) {
	case 0:
	    break;
	case TRYAGAIN:
	    txn_abort(tid);
	    goto retry;
	default:
	    txn_abort(tid);
	    syslog(L_ERROR, "OVDB: groupadd: groupsbyname->put: %s", db_strerror(ret));
	    return FALSE;
	}
	gno--;
	/* record group name -> number */
	key.data = group;
	key.size = strlen(group);
	switch(ret = groupsbyname->put(groupsbyname, tid, &key, &val, 0)) {
	case 0:
	    break;
	case TRYAGAIN:
	    txn_abort(tid);
	    goto retry;
	default:
	    txn_abort(tid);
	    syslog(L_ERROR, "OVDB: groupadd: groupsbyname->put: %s", db_strerror(ret));
	    return FALSE;
	}
    }

    if(new) {
	gs.low = lo ? lo : 1;
	gs.high = hi;
	gs.count = 0;
	gs.flag = *flag;
	gs.expired = time(NULL);
    } else {
	gs.flag = *flag;
    }

    key.data = &gno;
    key.size = sizeof gno;
    val.data = &gs;
    val.size = sizeof gs;
    val.flags = val.ulen = 0;

    switch(ret = groupstats->put(groupstats, tid, &key, &val, 0)) {
    case 0:
	break;
    case TRYAGAIN:
	txn_abort(tid);
	goto retry;
    default:
	txn_abort(tid);
	syslog(L_ERROR, "OVDB: groupadd: groupstats->put: %s", db_strerror(ret));
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
	    txn_abort(tid);
	    goto retry;
	default:
	    txn_abort(tid);
	    syslog(L_ERROR, "OVDB: groupadd: groupaliases->put: %s", db_strerror(ret));
	    return FALSE;
	}
    }

    my_txn_commit(tid);
    return TRUE;
}

BOOL ovdb_groupdel(char *group)
{
    DBT key;
    int ret;

    memset(&key, 0, sizeof key);

    /* We only need to delete the groupsbyname key to prevent readers
       from seeing this group.  The actual overview records aren't deleted
       now, since that could take a significant amount of time (and innd
       is who normally calls this function).  The expireover run will
       clean up the deleted groups. */

    key.data = group;
    key.size = strlen(group);

    switch(ret = groupsbyname->del(groupsbyname, NULL, &key, 0)) {
    case 0:
    case DB_NOTFOUND:
	break;
    default:
	syslog(L_ERROR, "OVDB: groupdel: groupsbyname->del: %s", db_strerror(ret));
	return FALSE;
    }

    switch(ret = groupaliases->del(groupaliases, NULL, &key, 0)) {
    case 0:
    case DB_NOTFOUND:
	break;
    default:
	syslog(L_ERROR, "OVDB: groupdel: groupaliases->del: %s", db_strerror(ret));
	return FALSE;
    }

    return TRUE;
}

BOOL ovdb_add(char *group, ARTNUM artnum, TOKEN token, char *data, int len, time_t arrived, time_t expires)
{
    static int  databuflen = 0;
    static char *databuf;
    DB		*db;
    DBT		key, val;
    DB_TXN	*tid;
    struct groupstats gs;
    struct datakey dk;
    group_id_t	gno;
    int ret;

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

    db = get_db(group);
    if(!db)
	return FALSE;
    ret = groupnum(group, &gno);
    if(ret == DB_NOTFOUND)
	return TRUE;
    if(ret != 0)
	return FALSE;

    /* start the transaction */
retry:
    if(ret = my_txn_begin(OVDBenv, NULL, &tid)) {
	syslog(L_ERROR, "OVDB: add: txn_begin: %s", db_strerror(ret));
	return FALSE;
    }

    /* first, retrieve groupstats */
    memset(&key, 0, sizeof key);
    memset(&val, 0, sizeof val);
    key.data = &gno;
    key.size = sizeof gno;
    val.data = &gs;		/* have BerkeleyDB write directly into gs */
    val.ulen = sizeof gs;
    val.flags = DB_DBT_USERMEM;

    switch(ret = groupstats->get(groupstats, tid, &key, &val, DB_RMW)) {
    case 0:
	break;
    case DB_NOTFOUND:
	txn_abort(tid);
	return TRUE;
    case TRYAGAIN:
	txn_abort(tid);
	goto retry;
    default:
	txn_abort(tid);
	syslog(L_ERROR, "OVDB: add: groupstats->get: %s", db_strerror(ret));
	return FALSE;
    }
    if(val.size < sizeof gs) {
	txn_abort(tid);
	return TRUE;
    }

    /* adjust groupstats */
    if(Cutofflow && gs.low > artnum) {
	txn_abort(tid);
	return TRUE;
    }
    if(gs.low == 0 || gs.low > artnum)
	gs.low = artnum;
    if(gs.high < artnum)
	gs.high = artnum;
    gs.count++;

    /* store groupstats */
    val.data = &gs;
    val.size = sizeof gs;
    val.ulen = val.flags = 0;

    switch(ret = groupstats->put(groupstats, tid, &key, &val, 0)) {
    case 0:
	break;
    case TRYAGAIN:
	txn_abort(tid);
	goto retry;
    default:
	txn_abort(tid);
	syslog(L_ERROR, "OVDB: add: groupstats->put: %s", db_strerror(ret));
	return FALSE;
    }

    memset(&dk, 0, sizeof dk);
    /* store overview */
    dk.groupnum = gno;
    dk.artnum = htonl((u_int32_t)artnum);

    key.data = &dk;
    key.size = sizeof dk;
    val.data = databuf;
    val.size = len;

    switch(ret = db->put(db, tid, &key, &val, 0)) {
    case 0:
	break;
    case TRYAGAIN:
	txn_abort(tid);
	goto retry;
    default:
	txn_abort(tid);
	syslog(L_ERROR, "OVDB: add: db->put: %s", db_strerror(ret));
	return FALSE;
    }

    my_txn_commit(tid);
    return TRUE;
}

BOOL ovdb_cancel(TOKEN token)
{
    return TRUE;
}

void *ovdb_opensearch(char *group, int low, int high)
{
    struct ovdbsearch *s;
    group_id_t gno;
    int ret;

    s = NEW(struct ovdbsearch, 1);

    if(groupnum(group, &gno) != 0) {
	DISPOSE(s);
	return NULL;
    }
    s->db = get_db(group);
    if(!s->db) {
	DISPOSE(s);
	return NULL;
    }

    if(ret = s->db->cursor(s->db, NULL, &(s->cursor), 0)) {
	syslog(L_ERROR, "OVDB: opensearch: s->db->cursor: %s", db_strerror(ret));
	DISPOSE(s);
	return NULL;
    }

    s->lokey.groupnum = gno;
    s->lokey.artnum = htonl(low);
    s->hikey.groupnum = gno;
    s->hikey.artnum = htonl(high);
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

    memset(&key, 0, sizeof key);
    memset(&val, 0, sizeof val);

    switch(s->state) {
    case 0:
	flags = DB_SET_RANGE;
	memset(&dk, 0, sizeof dk);
	key.data = &(s->lokey);
	key.size = sizeof(struct datakey);
	s->state = 1;
	break;
    case 1:
	flags = DB_NEXT;
	break;
    default:
	syslog(L_ERROR, "OVDB: OVsearch called again after FALSE return");
	return FALSE;
    }

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
	s->state = 2;
	return FALSE;
    default:
	syslog(L_ERROR, "OVDB: search: c_get: %s", db_strerror(ret));
	s->state = 2;
	return FALSE;
    }

    if(key.size != sizeof(struct datakey)) {
	s->state = 2;
	return FALSE;
    }

    if(memcmp(key.data, &(s->hikey), sizeof(struct datakey)) > 0) {
	s->state = 2;
	return FALSE;
    }

    if( ((len || data) && val.size <= sizeof(struct ovdata))
	|| ((token || arrived) && val.size < sizeof(struct ovdata)) ) {
	syslog(L_ERROR, "OVDB: search: bad value length");
	s->state = 2;
	return FALSE;
    }

    if(artnum) {
	memcpy(&dk, key.data, sizeof(struct datakey));
	*artnum = ntohl(dk.artnum);
    }

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
    struct ovdbsearch *s = (struct ovdbsearch *)handle;

    s->cursor->c_close(s->cursor);
   
    DISPOSE(s);
}

BOOL ovdb_getartinfo(char *group, ARTNUM artnum, char **data, int *len, TOKEN *token)
{
    int ret;
    DB *db = get_db(group);
    group_id_t gno;
    DBT key, val;
    struct ovdata ovd;
    struct datakey dk;

    if(!db)
	return FALSE;
    if(groupnum(group, &gno) != 0)
	return FALSE;

    memset(&dk, 0, sizeof dk);
    dk.groupnum = gno;
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
	break;
    case DB_NOTFOUND:
	return FALSE;
    default:
	syslog(L_ERROR, "OVDB: getartinfo: db->get: %s", db_strerror(ret));
	return FALSE;
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

BOOL ovdb_expiregroup(char *group, int *lo)
{
    DB *db;
    group_id_t gno;
    DBT key, val;
    DB_TXN *tid;
    DBC *cursor = NULL;
    int ret, delete;
    struct groupstats gs;
    struct ovdata ovd;
    struct datakey dk;
    ARTHANDLE *ah;
    u_int32_t newlo = 0, artnum, oldhi;
    int newcount = 0;

    if(eo_start == 0)
	eo_start = time(NULL);

    /* Special case:  when called with NULL group, we're to clean out
       records for deleted groups.  This will also clean out forgotton
       groups (groups removed from the active file but not from overview) */

    if(group == NULL)
	return delete_old_stuff();

    db = get_db(group);
    if(!db)
	return FALSE;
    if(groupnum(group, &gno) != 0)
	return FALSE;

    memset(&key, 0, sizeof key);
    memset(&val, 0, sizeof val);

    /* We don't transactionify the traversal, since we can assume we're the
       only process who will be deleting records */

    /* retrieve the current himark */
    key.data = &gno;
    key.size = sizeof gno;
    val.data = &gs;
    val.ulen = sizeof gs;
    val.flags = DB_DBT_USERMEM;
    if(ret = groupstats->get(groupstats, NULL, &key, &val, 0)) {
	syslog(L_ERROR, "OVDB: expiregroup: groupstats->get: %s", db_strerror(ret));
	return FALSE;
    }
    oldhi = gs.high;
    memset(&val, 0, sizeof val);

    /* get a cursor to traverse the ov records */
    if(ret = db->cursor(db, NULL, &cursor, 0)) {
	syslog(L_ERROR, "OVDB: expiregroup: db->cursor: %s", db_strerror(ret));
	return FALSE;
    }
    memset(&dk, 0, sizeof dk);
    dk.groupnum = gno;
    dk.artnum = 0;    
    key.data = &dk;
    key.size = key.ulen = sizeof dk;
    key.flags = DB_DBT_USERMEM;

    switch(ret = cursor->c_get(cursor, &key, &val, DB_SET_RANGE)) {
    case 0:
    case DB_NOTFOUND:
	break;
    default:
	cursor->c_close(cursor);
	syslog(L_ERROR, "OVDB: expiregroup: c_get: %s", db_strerror(ret));
	return FALSE;
    }

    while(1) {
	artnum = ntohl(dk.artnum);

	/* stop if: there are no more keys, an unknown key is reached,
	   reach a different group, or past the old himark */

	if(ret == DB_NOTFOUND
		|| key.size != sizeof dk
		|| dk.groupnum != gno
		|| artnum > oldhi) {

	    cursor->c_close(cursor);
retry:
	    if(ret = my_txn_begin(OVDBenv, NULL, &tid)) {
		syslog(L_ERROR, "OVDB: expiregroup: txn_begin: %s", db_strerror(ret));
		return FALSE;
	    }

	    /* retrieve groupstats */
	    memset(&key, 0, sizeof key);
	    memset(&val, 0, sizeof val);
	    key.data = &gno;
	    key.size = sizeof gno;
	    val.data = &gs;
	    val.ulen = sizeof gs;
	    val.flags = DB_DBT_USERMEM;

	    switch(ret = groupstats->get(groupstats, tid, &key, &val, DB_RMW)) {
	    case 0:
		break;
	    case DB_NOTFOUND:
		txn_abort(tid);
		return FALSE;
	    case TRYAGAIN:
		txn_abort(tid);
		goto retry;
	    default:
		txn_abort(tid);
		syslog(L_ERROR, "OVDB: expiregroup: groupstats->get: %s", db_strerror(ret));
		return FALSE;
	    }
	    if(val.size != sizeof gs) {
		txn_abort(tid);
		syslog(L_ERROR, "OVDB: expiregroup: bad groupstats entry for %s", group);
		return FALSE;
	    }

	    gs.count = newcount + (gs.high - oldhi);
	    if(newcount == 0)
		gs.low = oldhi + 1;
	    else
		gs.low = newlo;

	    gs.expired = time(NULL);

	    /* write out revised groupstats */
	    switch(ret = groupstats->put(groupstats, tid, &key, &val, 0)) {
	    case 0:
		break;
	    case TRYAGAIN:
		txn_abort(tid);
		goto retry;
	    default:
		txn_abort(tid);
		syslog(L_ERROR, "OVDB: expiregroup: groupstats->put: %s", db_strerror(ret));
		return FALSE;
	    }
	    if(lo)
		*lo = gs.low;

	    my_txn_commit(tid);
	    return TRUE;
	}

	delete = 0;
	if(val.size < sizeof ovd) {
	    delete = 1;	/* must be corrupt, just delete it */
	} else {
	    memcpy(&ovd, val.data, sizeof ovd);

	    ah = NULL;
	    if (!SMprobe(EXPENSIVESTAT, &ovd.token, NULL) || OVstatall) {
		if((ah = SMretrieve(ovd.token, RETR_STAT)) == NULL) { 
		    delete = 1;
		} else
		    SMfreearticle(ah);
	    } else {
		if (!OVhisthasmsgid((char *)val.data + sizeof(ovd))) {
		    delete = 1;
		}
	    }
	    if (!delete && innconf->groupbaseexpiry &&
			OVgroupbasedexpire(ovd.token, group,
				(char *)val.data + sizeof(ovd),
				val.size - sizeof(ovd),
				ovd.arrived, ovd.expires)) {
		delete = 1;
	    }
	}

	if(delete) {
	    switch(ret = cursor->c_del(cursor, 0)) {
	    case 0:
	    case DB_NOTFOUND:
	    case DB_KEYEMPTY:
		break;
	    default:
		cursor->c_close(cursor);
		syslog(L_ERROR, "OVDB: expiregroup: c_del: %s", db_strerror(ret));
		return FALSE;
	    }
	} else {
	    newcount++;
	    if(newlo == 0 || artnum < newlo)
		newlo = artnum;
	}
	
	/* go to the next record */
	switch(ret = cursor->c_get(cursor, &key, &val, DB_NEXT)) {
	case 0:
	case DB_NOTFOUND:
	    break;
	default:
	    cursor->c_close(cursor);
	    syslog(L_ERROR, "OVDB: expiregroup: c_get: %s", db_strerror(ret));
	    return FALSE;
	}
    }
    /*NOTREACHED*/
}

BOOL ovdb_ctl(OVCTLTYPE type, void *val)
{
    int *i;
    OVSORTTYPE *sorttype;
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

    if(dbs) {
	/* close databases */
	for(i = 0; i < ovdb_conf.numdbfiles; i++)
	    close_db_file(i);

	DISPOSE(dbs);
	dbs = NULL;
    }
    if(_dbnames) {
	for(i = 0; i < ovdb_conf.numdbfiles; i++)
	    DISPOSE(_dbnames[i]);
	DISPOSE(_dbnames);
    }
    if(groupstats) {
	groupstats->close(groupstats, 0);
	groupstats = NULL;
    }
    if(groupsbyname) {
	groupsbyname->close(groupsbyname, 0);
	groupsbyname = NULL;
    }
    if(groupaliases) {
	groupaliases->close(groupaliases, 0);
	groupaliases = NULL;
    }

    ovdb_close_berkeleydb();
}

#ifdef TEST_BDB

static int signalled = 0;
int sigfunc()
{
    signalled = 1;
}

int main(int argc, char *argv[])
{
    void *s;
    ARTNUM a, start=0, stop=0;
    char *data;
    int len, c, low, high, count, flag, from=0, to=0, single=0;
    int getgs=0, getcount=0, getwhich=0, err=0, gotone=0;

    ReadInnConf();
    if(!ovdb_open(OV_READ))
	exit(1);

    xsignal(SIGINT, sigfunc);
    xsignal(SIGTERM, sigfunc);
    xsignal(SIGHUP, sigfunc);

    while((c = getopt(argc, argv, ":gcwr:f:t:")) != -1) {
	switch(c) {
	case 'g':
	    getgs = 1;
	    gotone++;
	    break;
	case 'c':
	    getcount = 1;
	    gotone++;
	    break;
	case 'w':
	    getwhich = 1;
	    gotone++;
	    break;
	case 'r':
	    single = atoi(optarg);
	    gotone++;
	    break;
	case 'f':
	    from = atoi(optarg);
	    gotone++;
	    break;
	case 't':
	    to = atoi(optarg);
	    gotone++;
	    break;
	case ':':
	    fprintf(stderr, "Option -%c requires an argument\n", optopt);
	    err = 1;
	    break;
	case '?':
	    fprintf(stderr, "Unrecognized option: -%c\n", optopt);
	    err = 1;
	    break;
	}
    }
    if(!gotone)
	getgs++;
    if(optind == argc) {
	fprintf(stderr, "Missing newsgroup argument(s)\n");
	err = 1;
    }
    if(err) {
	fprintf(stderr, "Usage: ovdb [-g|-c|-w] [-r artnum] newsgroup [newsgroup ...]\n");
	fprintf(stderr, "      -g        : show groupstats info\n");
	fprintf(stderr, "      -c        : show groupstats info by counting actual records\n");
	fprintf(stderr, "      -w        : display DB file group is stored in\n");
	fprintf(stderr, "      -r artnum : retrieve single OV record for article number\n");
	fprintf(stderr, "      -f artnum : retrieve OV records starting at article number\n");
	fprintf(stderr, "      -t artnum : retrieve OV records ending at article number\n");
	goto out;
    }
    if(single) {
	start = single;
	stop = single;
    }
    if(from || to) {
	if(from)
	    start = from;
	else
	    start = 0;
	if(to)
	    stop = to;
	else
	    stop = 0xffffffff;
    }
    for( ; optind < argc; optind++) {
	if(getgs) {
	    if(ovdb_groupstats(argv[optind], &low, &high, &count, &flag)) {
		printf("%s: groupstats: low: %d, high: %d, count: %d, flag: %c\n",
			argv[optind], low, high, count, flag);
	    }
	}
	if(getcount) {
	    low = high = count = 0;
	    if(s = ovdb_opensearch(argv[optind], 1, 0xffffffff)) {
		while(ovdb_search(s, &a, NULL, NULL, NULL, NULL)) {
		    if(low == 0 || a < low)
			low = a;
		    if(a > high)
			high = a;
		    count++;
		    if(signalled)
			break;
		}
		ovdb_closesearch(s);
		if(signalled)
		    goto out;
		printf("%s:    counted: low: %d, high: %d, count: %d\n",
			argv[optind], low, high, count);
	    }
	}
	if(getwhich) {
	    c = which_db(argv[optind]);
	    printf("%s: stored in ov%05d\n", argv[optind], c);
	}
	if(start || stop) {
	    if(s = ovdb_opensearch(argv[optind], start, stop)) {
		while(ovdb_search(s, &a, &data, &len, NULL, NULL)) {
		    fwrite(data, len, 1, stdout);
		    if(signalled)
			break;
		}
		ovdb_closesearch(s);
		if(signalled)
		    goto out;
	    }
	}
	if(signalled)
	    goto out;
    }
out:
    ovdb_close();
}
#endif /* TEST_BDB */

#endif /* USE_BERKELEY_DB */


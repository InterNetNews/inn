#ifdef USE_BERKELEY_DB

#include <db.h>

#if DB_VERSION_MAJOR == 2
#if DB_VERSION_MINOR < 6
#error "Need BerkeleyDB 2.6.x, 2.7.x, 3.x or 4.x"
#endif
#else
#if DB_VERSION_MAJOR < 3 || DB_VERSION_MAJOR > 4
#error "Need BerkeleyDB 2.6.x, 2.7.x, 3.x or 4.x"
#endif
#endif

/*
 * How data is stored:
 *
 * Each group is assigned an integer ID.  The mapping between a group name
 * and its ID is stored in the groupinfo DB.  Overview data itself
 * is stored in one or more btree DBs.  The specific DB file that is used
 * to store data for a certain group is chosen by taking the hash of the
 * group name, copying the first bytes of the hash into an int, and then
 * modding the int value to the number of DBs.
 *
 * Each group has one groupinfo structure in the groupinfo DB, whose key
 * is the newsgroup name.  The overview records for the group have a
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
    int maxlocks;
    int nocompact;
    int readserver;
    int numrsprocs;
    int maxrsconn;
    int useshm;
    int shmkey;
};

typedef u_int32_t group_id_t;

struct groupinfo {
    ARTNUM     low;
    ARTNUM     high;
    int        count;
    int        flag;
    time_t     expired;		/* when this group was last touched by expiregroup */
    group_id_t current_gid;	/* group ID */
    group_id_t new_gid;		/* pending ID (expireover) */
    int        current_db;	/* which DB file the records are in */
    int        new_db;		/* pending DB file */
    pid_t      expiregrouppid;	/* PID of expireover process */
    int        status;
};
#define GROUPINFO_DELETED    1
#define GROUPINFO_EXPIRING   (1<<1)
#define GROUPINFO_MOVING     (1<<2)
#define GROUPINFO_MOVE_REQUESTED (1<<3) /*NYI*/

struct datakey {
    group_id_t groupnum;	/* must be the first member of this struct */
    u_int32_t artnum;
};

struct ovdata {
    TOKEN token;
    time_t arrived;
    time_t expires;
};


#define DATA_VERSION 2

extern struct ovdb_conf ovdb_conf;
extern DB_ENV *OVDBenv;

#define OVDB_ERR_NONE   0
#define OVDB_ERR_SYSLOG 1	/* default */
#define OVDB_ERR_STDERR 2
extern int ovdb_errmode;

void read_ovdb_conf(void);
int ovdb_open_berkeleydb(int mode, int flags);
void ovdb_close_berkeleydb(void);
int ovdb_getgroupinfo(char *group, struct groupinfo *gi, int ignoredeleted, DB_TXN *tid, int getflags);

#define OVDB_RECOVER    1

#define OVDB_LOCK_NORMAL 0
#define OVDB_LOCK_ADMIN 1
#define OVDB_LOCK_EXCLUSIVE 2

BOOL ovdb_getlock(int mode);
BOOL ovdb_releaselock(void);
BOOL ovdb_check_pidfile(char *file);
BOOL ovdb_check_user(void);

#define OVDB_LOCKFN "ovdb.sem"
#define OVDB_MONITOR_PIDFILE "ovdb_monitor.pid"
#define OVDB_SERVER_PIDFILE "ovdb_server.pid"
#define SPACES "                "

/* read server stuff */
#define CMD_QUIT	0x01
#define CMD_GROUPSTATS	0x02
#define CMD_OPENSRCH	0x03
#define CMD_SRCH	0x04
#define CMD_CLOSESRCH	0x05
#define CMD_ARTINFO	0x06
#define CMD_MASK	0x0F
#define RPLY_OK		0x00
#define RPLY_ERROR	0x10
#define OVDB_SERVER	(1<<4)
#define OVDB_SERVER_BANNER "ovdb read protocol 1"
#define OVDB_SERVER_PORT 32323	/* only used if don't have unix domain sockets */
#define OVDB_SERVER_SOCKET "ovdb.server"

struct rs_cmd {
    uint32_t	what;
    uint32_t	grouplen;
    uint32_t	artlo;
    uint32_t	arthi;
    void *	handle;
};

struct rs_groupstats {
    uint32_t	status;
    int		lo;
    int		hi;
    int		count;
    int		flag;
    uint32_t	aliaslen;
    /* char alias */
};

struct rs_opensrch {
    uint32_t	status;
    void *	handle;
};

struct rs_srch {
    uint32_t	status;
    ARTNUM	artnum;
    TOKEN	token;
    time_t	arrived;
    int		len;
    /* char data */
};

struct rs_artinfo {
    uint32_t	status;
    TOKEN	token;
};


#if DB_VERSION_MAJOR == 2
char *db_strerror(int err);

#define TXN_START(label, tid) \
label: { \
  int ret; \
  if(ret = txn_begin(OVDBenv->tx_info, NULL, &tid)) { \
    syslog(L_ERROR, "OVDB: " #label " txn_begin: %s", db_strerror(ret)); \
    tid = NULL; \
  } \
}

#define TXN_RETRY(label, tid) \
{ txn_abort(tid); goto label; }

#define TXN_ABORT(label, tid) txn_abort(tid)
#define TXN_COMMIT(label, tid) txn_commit(tid)

#define TRYAGAIN EAGAIN

#else
/* version 3 */

#define TXN_START(label, tid) \
label: { \
  int ret; \
  if(ret = txn_begin(OVDBenv, NULL, &tid, 0)) { \
    syslog(L_ERROR, "OVDB: " #label " txn_begin: %s", db_strerror(ret)); \
    tid = NULL; \
  } \
}

#define TXN_RETRY(label, tid) \
{ txn_abort(tid); goto label; }

#define TXN_ABORT(label, tid) txn_abort(tid)
#define TXN_COMMIT(label, tid) txn_commit(tid, 0)

#define TRYAGAIN DB_LOCK_DEADLOCK

#endif /* DB_VERSION_MAJOR == 2 */

#endif /* USE_BERKELEY_DB */

#ifndef HAVE_DB_H
# undef HAVE_BDB
#endif
#ifdef HAVE_BDB

#include <db.h>

#if DB_VERSION_MAJOR < 4 || (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR < 3)
#error "Need Berkeley DB 4.4 or higher"
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
 * is that the keys will sort correctly using Berkeley DB's default sort
 * function (basically, a memcmp).
 *
 * The overview records consist of a 'struct ovdata' followed by the actual
 * overview data.  The struct ovdata contains the token and arrival time.
 * If compression is enabled, overview records larger than COMPRESS_MIN
 * get compressed using zlib.  A compressed record has the same 'struct
 * ovdata', but is followed by a uint32_t in network byteorder (which is
 * the length of the uncompressed data), and then followed by the compressed
 * data.  Since overview data never starts with a null byte, a compressed
 * record is identified by the presense of a null byte immediately after
 * the struct ovdata (which is part of the uint32_t).
 */ 

struct ovdb_conf {
    char *home;		/* path to directory where db files are stored */
    int  txn_nosync;	/* whether to pass DB_TXN_NOSYNC to db_appinit */
    int  numdbfiles;
    size_t cachesize;
    int ncache;
    size_t pagesize;
    int minkey;
    int maxlocks;
    int nocompact;
    int readserver;
    int numrsprocs;
    int maxrsconn;
    int useshm;
    int shmkey;
    int compress;
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
#define DATA_VERSION_COMPRESS 3

extern struct ovdb_conf ovdb_conf;
extern int ovdb_data_ver;
extern DB_ENV *OVDBenv;

void read_ovdb_conf(void);
int ovdb_open_berkeleydb(int mode, int flags);
void ovdb_close_berkeleydb(void);
int ovdb_getgroupinfo(const char *group, struct groupinfo *gi,
                      int ignoredeleted, DB_TXN *tid, int getflags);

#define OVDB_RECOVER    1
#define OVDB_UPGRADE    2

#define OVDB_LOCK_NORMAL 0
#define OVDB_LOCK_ADMIN 1
#define OVDB_LOCK_EXCLUSIVE 2

bool ovdb_getlock(int mode);
bool ovdb_releaselock(void);
bool ovdb_check_pidfile(const char *file);
bool ovdb_check_user(void);

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


/* Used when TXN_RETRY will never be called, to avoid a warning about an
   unused label. */
#define TXN_START_NORETRY(label, tid) \
{ \
  int txn_ret; \
  txn_ret = OVDBenv->txn_begin(OVDBenv, NULL, &tid, 0); \
  if (txn_ret != 0) { \
    warn("OVDB: " #label " txn_begin: %s", db_strerror(ret)); \
    tid = NULL; \
  } \
}

#define TXN_START(label, tid) label: TXN_START_NORETRY(label, tid)

#define TXN_RETRY(label, tid) \
{ (tid)->abort(tid); goto label; }

#define TXN_ABORT(label, tid) (tid)->abort(tid)
#define TXN_COMMIT(label, tid) (tid)->commit(tid, 0)

#define TRYAGAIN DB_LOCK_DEADLOCK

#endif /* HAVE_BDB */

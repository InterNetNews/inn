/*
 * ovdb_init
 *  Performs recovery on OV database, if needed
 *  Performs upgrade of OV database, if needed and if '-u' used
 *  Starts ovdb_monitor, if needed
 */

#include "config.h"
#include "clibrary.h"
#include "libinn.h"
#include <errno.h>
#include <syslog.h>
#include <string.h>

#include "ov.h"
#include "../storage/ovdb/ovdb.h"
#include "../storage/ovdb/ovdb-private.h"

#ifndef USE_BERKELEY_DB

int main(int argc, char **argv)
{
    fprintf(stderr, "Error: BerkeleyDB not compiled in.\n");
    exit(1);
}

#else /* USE_BERKELEY_DB */

static int open_db(DB **db, char *name, int type)
{
    int ret;
#if DB_VERSION_MAJOR == 2
    DB_INFO dbinfo;
    memset(&dbinfo, 0, sizeof dbinfo);

    if(ret = db_open(name, type, DB_CREATE, 0666, OVDBenv,
		    &dbinfo, db)) {
	fprintf(stderr, "ovdb_init: db_open failed: %s\n", db_strerror(ret));
	return ret;
    }
#else
    if(ret = db_create(db, OVDBenv, 0)) {
	fprintf(stderr, "ovdb_init: db_create: %s\n", db_strerror(ret));
	return ret;
    }
    if(ret = (*db)->open(*db, name, NULL, type, DB_CREATE, 0666)) {
	(*db)->close(*db, 0);
	fprintf(stderr, "ovdb_init: %s->open: %s\n", name, db_strerror(ret));
	return ret;
    }
#endif
    return 0;
}

/* Upgrade BerkeleyDB version */
static int upgrade_database(char *name)
{
#if DB_VERSION_MAJOR == 2
    return 0;
#else
    int ret;
    DB *db;

    if(ret = db_create(&db, OVDBenv, 0))
	return ret;

    printf("ovdb_init: upgrading %s...\n", name);
    if(ret = db->upgrade(db, name, 0))
	fprintf(stderr, "ovdb_init: db->upgrade(%s): %s\n", name, db_strerror(ret));

    db->close(db, 0);
    return ret;
#endif
}


struct groupstats {
    ARTNUM low;
    ARTNUM high;
    int count;
    int flag;
    time_t expired;
};

static int v1_which_db(char *group)
{
    HASH grouphash;
    unsigned int i;

    grouphash = Hash(group, strlen(group));
    memcpy(&i, &grouphash, sizeof(i));
    return i % ovdb_conf.numdbfiles;
}

/* Upgrade ovdb data format version 1 to 2 */
/* groupstats and groupsbyname are replaced by groupinfo */
static int upgrade_v1_to_v2()
{
    DB *groupstats, *groupsbyname, *groupinfo, *vdb;
    DBT key, val, ikey, ival;
    DBC *cursor;
    group_id_t gid, higid = 0, higidbang = 0;
    struct groupinfo gi;
    struct groupstats gs;
    char group[MAXHEADERSIZE];
    u_int32_t v2 = 2;
    int ret;
    char *p;

    printf("ovdb_init: Upgrading data to version 2\n");
    if(ret = open_db(&groupstats, "groupstats", DB_BTREE))
	return ret;
    if(ret = open_db(&groupsbyname, "groupsbyname", DB_HASH))
	return ret;
    if(ret = open_db(&groupinfo, "groupinfo", DB_BTREE))
	return ret;

    memset(&key, 0, sizeof key);
    memset(&val, 0, sizeof val);
    memset(&ikey, 0, sizeof ikey);
    memset(&ival, 0, sizeof ival);

    if(ret = groupsbyname->cursor(groupsbyname, NULL, &cursor, 0))
	return ret;

    while((ret = cursor->c_get(cursor, &key, &val, DB_NEXT)) == 0) {
	if(key.size == 1 && *((char *)(key.data)) == '!') {
	    if(val.size == sizeof(group_id_t))
		memcpy(&higidbang, val.data, sizeof(group_id_t));
	    continue;
	}
	if(key.size >= MAXHEADERSIZE)
	    continue;
	memcpy(group, key.data, key.size);
	group[key.size] = 0;

	if(val.size != sizeof(group_id_t))
	    continue;
	memcpy(&gid, val.data, sizeof(group_id_t));
	if(gid > higid)
	    higid = gid;
	ikey.data = &gid;
	ikey.size = sizeof(group_id_t);

	if(ret = groupstats->get(groupstats, NULL, &ikey, &ival, 0))
	    continue;
	if(ival.size != sizeof(struct groupstats))
	    continue;
	memcpy(&gs, ival.data, sizeof(struct groupstats));

	gi.low = gs.low;
	gi.high = gs.high;
	gi.count = gs.count;
	gi.flag = gs.flag;
	gi.expired = gs.expired;
	gi.current_gid = gi.new_gid = gid;
	gi.current_db = gi.new_db = v1_which_db(group);
	gi.expiregrouppid = gi.status = 0;

	val.data = &gi;
	val.size = sizeof(gi);
	if(ret = groupinfo->put(groupinfo, NULL, &key, &val, 0)) {
	    fprintf(stderr, "ovdb_init: groupinfo->put failed: %s\n", db_strerror(ret));
	    cursor->c_close(cursor);
	    return ret;
	}
    }
    cursor->c_close(cursor);
    if(ret != DB_NOTFOUND) {
	fprintf(stderr, "ovdb_init: cursor->get failed: %s\n", db_strerror(ret));
	return ret;
    }

    higid++;
    if(higidbang > higid)
	higid = higidbang;

    key.data = "!groupid_freelist";
    key.size = sizeof("!groupid_freelist");
    val.data = &higid;
    val.size = sizeof(group_id_t);

    if(ret = groupinfo->put(groupinfo, NULL, &key, &val, 0)) {
	fprintf(stderr, "ovdb_init: groupinfo->put failed: %s\n", db_strerror(ret));
	return ret;
    }

    if(ret = open_db(&vdb, "version", DB_BTREE))
	return ret;

    key.data = "dataversion";
    key.size = sizeof("dataversion");
    val.data = &v2;
    val.size = sizeof v2;

    if(ret = vdb->put(vdb, NULL, &key, &val, 0)) {
	fprintf(stderr, "ovdb_init: version->put failed: %s\n", db_strerror(ret));
	return ret;
    }

    groupstats->close(groupstats, 0);
    groupsbyname->close(groupsbyname, 0);
    groupinfo->close(groupinfo, 0);
    vdb->close(vdb, 0);
    
#if DB_VERSION_MAJOR == 3
    if(ret = db_create(&groupstats, OVDBenv, 0))
	return ret;
    groupstats->remove(groupstats, "groupstats", NULL, 0);
    if(ret = db_create(&groupsbyname, OVDBenv, 0))
	return ret;
    groupsbyname->remove(groupsbyname, "groupsbyname", NULL, 0);
#else
    /* This won't work if someone changed DB_DATA_DIR in DB_CONFIG */
    p = concatpath(ovdb_conf.home, "groupstats");
    unlink(p);
    free(p);
    p = concatpath(ovdb_conf.home, "groupsbyname");
    unlink(p);
    free(p);
#endif

    return 0;
}

static int check_upgrade(int do_upgrade)
{
    int ret, i;
    DB *db;
    DBT key, val;
    u_int32_t dv;
    char name[50];

    if(do_upgrade && (ret = upgrade_database("version")))
	return ret;

    if(ret = open_db(&db, "version", DB_BTREE))
	return ret;

    memset(&key, 0, sizeof key);
    memset(&val, 0, sizeof val);
    key.data = "dataversion";
    key.size = sizeof("dataversion");
    if(ret = db->get(db, NULL, &key, &val, 0)) {
	if(ret != DB_NOTFOUND) {
	    fprintf(stderr, "ovdb_init: can't retrieve version: %s\n", db_strerror(ret));
	    db->close(db, 0);
	    return ret;
	}
    }
    if(ret == DB_NOTFOUND || val.size != sizeof dv) {
	dv = DATA_VERSION;

	val.data = &dv;
	val.size = sizeof dv;
	if(ret = db->put(db, NULL, &key, &val, 0)) {
	    fprintf(stderr, "ovdb_init: can't store version: %s\n", db_strerror(ret));
	    db->close(db, 0);
	    return ret;
	}
    } else
	memcpy(&dv, val.data, sizeof dv);

    key.data = "numdbfiles";
    key.size = sizeof("numdbfiles");
    if(ret = db->get(db, NULL, &key, &val, 0) == 0)
	if(val.size == sizeof(ovdb_conf.numdbfiles))
	    memcpy(&(ovdb_conf.numdbfiles), val.data, sizeof(ovdb_conf.numdbfiles));
    db->close(db, 0);

    if(do_upgrade) {
	if(dv == 1) {
	    if(ret = upgrade_database("groupstats"))
		return ret;
	    if(ret = upgrade_database("groupsbyname"))
		return ret;
	} else {
	    if(ret = upgrade_database("groupinfo"))
		return ret;
	}
	if(ret = upgrade_database("groupaliases"))
	    return ret;
	for(i = 0; i < ovdb_conf.numdbfiles; i++) {
	    sprintf(name, "ov%05d", i);
	    if(ret = upgrade_database(name))
		return ret;
	}
    }

    if(dv > DATA_VERSION) {
	fprintf(stderr, "ovdb_init: can't open database: unknown version %d\n", dv);
	return EINVAL;
    }
    if(dv < DATA_VERSION) {
	if(do_upgrade)
	    return upgrade_v1_to_v2();

	fprintf(stderr, "ovdb_init: Database needs to be upgraded.\n");
	return EINVAL;
    }
    return 0;
}

int main(int argc, char **argv)
{
    int ret, c, do_upgrade = 0, recover_only = 0, err = 0;
    BOOL locked;

    openlog("ovdb_init", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);

    if (ReadInnConf() < 0) {
	fprintf(stderr, "Error: Can't read inn.conf\n");
	exit(1);
    }

    if(strcmp(innconf->ovmethod, "ovdb")) {
	fprintf(stderr, "Error: ovmethod not set to ovdb\n");
	exit(1);
    }

    if(!ovdb_check_user()) {
	fprintf(stderr, "Error: Only run this command as user " NEWSUSER "\n");
	exit(1);
    }

    chdir(innconf->pathtmp);
    ovdb_errmode = OVDB_ERR_STDERR;

    while((c = getopt(argc, argv, "ru")) != -1) {
	switch(c) {
	case 'r':
	    recover_only = 1;
	    break;
	case 'u':
	    do_upgrade = 1;
	    break;
	case '?':
	    fprintf(stderr, "Unrecognized option: -%c\n", optopt);
	    err++;
	    break;
	}
    }
    if(recover_only && do_upgrade) {
	fprintf(stderr, "Can't have both -r and -u at once.\n");
	err++;
    }
    if(err) {
	fprintf(stderr, "Usage: ovdb_init [-r|-u]\n");
	exit(1);
    }

    locked = ovdb_getlock(OVDB_LOCK_EXCLUSIVE);
    if(locked) {
	if(do_upgrade)
	    fprintf(stderr, "ovdb_init: Database is quiescent.  Upgrading...\n");
	else
	    fprintf(stderr, "ovdb_init: Database is quiescent.  Running normal recovery.\n");
    } else {
	fprintf(stderr, "ovdb_init: Database is active.");
	if(do_upgrade) {
	    fprintf(stderr, "  Upgrade will not be attempted.");
	    do_upgrade = 0;
	}
	if(recover_only) {
	    fprintf(stderr, "  Recovery will not be attempted.\n");
	    exit(1);
	}
	fprintf(stderr, "\n");
	ovdb_getlock(OVDB_LOCK_ADMIN);
    }

    ret = ovdb_open_berkeleydb(OV_WRITE, (locked && !do_upgrade) ? OVDB_RECOVER : 0);
    if(ret == DB_RUNRECOVERY) {
	if(locked)
	    fprintf(stderr, "ovdb_init: Database could not be recovered.\n");
	else
	    fprintf(stderr, "ovdb_init: Database needs recovery, but recovery can not be performed until the\nother processes accessing the database are killed.\n");
	exit(1);
    }
    if(ret != 0) {
	fprintf(stderr, "ovdb_init: Could not open BerkeleyDB: %s\n", db_strerror(ret));
	exit(1);
    }

    if(recover_only)
	exit(0);

    if(check_upgrade(do_upgrade)) {
	ovdb_close_berkeleydb();
	exit(1);
    }

    ovdb_close_berkeleydb();
    ovdb_releaselock();

    if(ovdb_check_pidfile(OVDB_MONITOR_PIDFILE) == FALSE) {
	fprintf(stderr, "ovdb_init: Starting ovdb monitor\n");
	switch(fork()) {
	case -1:
	    fprintf(stderr, "can't fork: %s\n", strerror(errno));
	    exit(1);
	case 0:
	    setsid();
	    execl(concatpath(innconf->pathbin, "ovdb_monitor"),
		"ovdb_monitor", SPACES, NULL);
	    fprintf(stderr, "Can't exec ovdb_monitor: %s\n", strerror(errno));
	    _exit(1);
	}
	sleep(2);	/* give the monitor a chance to start */
    } else
	fprintf(stderr, "ovdb_init: ovdb monitor is running\n");

    if(ovdb_conf.readserver) {
	if(ovdb_check_pidfile(OVDB_SERVER_PIDFILE) == FALSE) {
	    fprintf(stderr, "ovdb_init: Starting ovdb server\n");
	    switch(fork()) {
	    case -1:
		fprintf(stderr, "can't fork: %s\n", strerror(errno));
		exit(1);
	    case 0:
		setsid();
		execl(concatpath(innconf->pathbin, "ovdb_server"),
		    "ovdb_server", SPACES, NULL);
		fprintf(stderr, "Can't exec ovdb_server: %s\n", strerror(errno));
		_exit(1);
	    }
	} else
	    fprintf(stderr, "ovdb_init: ovdb server is running\n");
    }

    exit(0);
}
#endif /* USE_BERKELEY_DB */


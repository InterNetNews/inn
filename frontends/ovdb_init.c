/*
 * ovdb_init
 *  Performs recovery on OV database, if needed
 *  Performs upgrade of OV database, if needed and if '-u' used
 *  Starts ovdb_monitor, if needed
 */

#include "config.h"
#include "clibrary.h"
#include "inn/libinn.h"
#include <errno.h>
#include <syslog.h>

#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/ov.h"
#include "../storage/ovdb/ovdb.h"
#include "../storage/ovdb/ovdb-private.h"

#ifndef HAVE_BDB

int main(int argc UNUSED, char **argv UNUSED)
{
    die("Berkeley DB support not compiled");
}

#else /* HAVE_BDB */

static int open_db(DB **db, const char *name, int type)
{
    int ret;
    ret = db_create(db, OVDBenv, 0);
    if (ret != 0) {
	warn("db_create failed: %s\n", db_strerror(ret));
	return ret;
    }
    ret = (*db)->open(*db, NULL, name, NULL, type, DB_CREATE, 0666);
    if (ret != 0) {
	(*db)->close(*db, 0);
        warn("%s->open failed: %s", name, db_strerror(ret));
	return ret;
    }
    return 0;
}

/* Upgrade Berkeley DB version */
static int upgrade_database(const char *name UNUSED)
{
    int ret;
    DB *db;

    ret = db_create(&db, OVDBenv, 0);
    if (ret != 0)
	return ret;

    notice("upgrading %s...", name);
    ret = db->upgrade(db, name, 0);
    if (ret != 0)
        warn("db->upgrade(%s) failed: %s", name, db_strerror(ret));

    db->close(db, 0);
    return ret;
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
static int upgrade_v1_to_v2(void)
{
    DB *groupstats, *groupsbyname, *groupinfo, *vdb;
    DBT key, val, ikey, ival;
    DBC *cursor;
    group_id_t gid, higid = 0, higidbang = 0;
    struct groupinfo gi;
    struct groupstats gs;
    char group[MED_BUFFER];
    u_int32_t v2 = 2;
    int ret;

    notice("upgrading data to version 2");
    ret = open_db(&groupstats, "groupstats", DB_BTREE);
    if (ret != 0)
	return ret;
    ret = open_db(&groupsbyname, "groupsbyname", DB_HASH);
    if (ret != 0)
	return ret;
    ret = open_db(&groupinfo, "groupinfo", DB_BTREE);
    if (ret != 0)
	return ret;

    memset(&key, 0, sizeof key);
    memset(&val, 0, sizeof val);
    memset(&ikey, 0, sizeof ikey);
    memset(&ival, 0, sizeof ival);

    ret = groupsbyname->cursor(groupsbyname, NULL, &cursor, 0);
    if (ret != 0)
	return ret;

    while((ret = cursor->c_get(cursor, &key, &val, DB_NEXT)) == 0) {
	if(key.size == 1 && *((char *)(key.data)) == '!') {
	    if(val.size == sizeof(group_id_t))
		memcpy(&higidbang, val.data, sizeof(group_id_t));
	    continue;
	}
	if(key.size >= MED_BUFFER)
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

        ret = groupstats->get(groupstats, NULL, &ikey, &ival, 0);
	if (ret != 0)
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
        ret = groupinfo->put(groupinfo, NULL, &key, &val, 0);
	if (ret != 0) {
            warn("groupinfo->put failed: %s", db_strerror(ret));
	    cursor->c_close(cursor);
	    return ret;
	}
    }
    cursor->c_close(cursor);
    if(ret != DB_NOTFOUND) {
        warn("cursor->get failed: %s", db_strerror(ret));
	return ret;
    }

    higid++;
    if(higidbang > higid)
	higid = higidbang;

    key.data = (char *) "!groupid_freelist";
    key.size = sizeof("!groupid_freelist");
    val.data = &higid;
    val.size = sizeof(group_id_t);

    ret = groupinfo->put(groupinfo, NULL, &key, &val, 0);
    if (ret != 0) {
        warn("groupinfo->put failed: %s", db_strerror(ret));
	return ret;
    }

    ret = open_db(&vdb, "version", DB_BTREE);
    if (ret != 0)
	return ret;

    key.data = (char *) "dataversion";
    key.size = sizeof("dataversion");
    val.data = &v2;
    val.size = sizeof v2;

    ret = vdb->put(vdb, NULL, &key, &val, 0);
    if (ret != 0) {
        warn("version->put failed: %s", db_strerror(ret));
	return ret;
    }

    groupstats->close(groupstats, 0);
    groupsbyname->close(groupsbyname, 0);
    groupinfo->close(groupinfo, 0);
    vdb->close(vdb, 0);
    
    ret = db_create(&groupstats, OVDBenv, 0);
    if (ret != 0)
	return ret;
    groupstats->remove(groupstats, "groupstats", NULL, 0);
    ret = db_create(&groupsbyname, OVDBenv, 0);
    if (ret != 0)
	return ret;
    groupsbyname->remove(groupsbyname, "groupsbyname", NULL, 0);

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

    ret = open_db(&db, "version", DB_BTREE);
    if (ret != 0)
	return ret;

    memset(&key, 0, sizeof key);
    memset(&val, 0, sizeof val);
    key.data = (char *) "dataversion";
    key.size = sizeof("dataversion");
    ret = db->get(db, NULL, &key, &val, 0);
    if (ret != 0) {
	if(ret != DB_NOTFOUND) {
            warn("cannot retrieve version: %s", db_strerror(ret));
	    db->close(db, 0);
	    return ret;
	}
    }
    if(ret == DB_NOTFOUND || val.size != sizeof dv) {
	dv = DATA_VERSION;

	val.data = &dv;
	val.size = sizeof dv;
        ret = db->put(db, NULL, &key, &val, 0);
	if (ret != 0) {
            warn("cannot store version: %s", db_strerror(ret));
	    db->close(db, 0);
	    return ret;
	}
    } else
	memcpy(&dv, val.data, sizeof dv);

    key.data = (char *) "numdbfiles";
    key.size = sizeof("numdbfiles");
    if ((ret = db->get(db, NULL, &key, &val, 0)) == 0)
	if(val.size == sizeof(ovdb_conf.numdbfiles))
	    memcpy(&(ovdb_conf.numdbfiles), val.data, sizeof(ovdb_conf.numdbfiles));
    db->close(db, 0);

    if(do_upgrade) {
	if(dv == 1) {
            ret = upgrade_database("groupstats");
	    if (ret != 0)
		return ret;
            ret = upgrade_database("groupsbyname");
	    if (ret != 0)
		return ret;
	} else {
            ret = upgrade_database("groupinfo");
	    if (ret != 0)
		return ret;
	}
        ret = upgrade_database("groupaliases");
	if (ret != 0)
	    return ret;
	for(i = 0; i < ovdb_conf.numdbfiles; i++) {
	    snprintf(name, sizeof(name), "ov%05d", i);
            ret = upgrade_database(name);
	    if (ret != 0)
		return ret;
	}
    }

    if(dv > DATA_VERSION_COMPRESS) {
        warn("cannot open database: unknown version %d", dv);
	return EINVAL;
    }
    if(dv < DATA_VERSION) {
	if(do_upgrade)
	    return upgrade_v1_to_v2();

        warn("database needs to be upgraded");
	return EINVAL;
    }
    return 0;
}

static int
upgrade_environment(void)
{
    int ret;

    ovdb_close_berkeleydb();
    ret = ovdb_open_berkeleydb(OV_WRITE, OVDB_UPGRADE);
    if (ret != 0)
	return ret;
    ret = OVDBenv->remove(OVDBenv, ovdb_conf.home, 0);
    if (ret != 0)
	return ret;
    OVDBenv = NULL;
    ret = ovdb_open_berkeleydb(OV_WRITE, 0);
    return ret;
}

int main(int argc, char **argv)
{
    int ret, c, do_upgrade = 0, recover_only = 0, err = 0;
    bool locked;
    int flags;

    openlog("ovdb_init", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);
    message_program_name = "ovdb_init";

    if (!innconf_read(NULL))
        exit(1);

    if(strcmp(innconf->ovmethod, "ovdb"))
        die("ovmethod not set to ovdb in inn.conf");

    if(!ovdb_check_user())
        die("command must be run as runasuser user");

    chdir(innconf->pathtmp);

    while((c = getopt(argc, argv, "ru")) != -1) {
	switch(c) {
	case 'r':
	    recover_only = 1;
	    break;
	case 'u':
	    do_upgrade = 1;
	    break;
	case '?':
            warn("unrecognized option -%c", optopt);
	    err++;
	    break;
	}
    }
    if(recover_only && do_upgrade) {
        warn("cannot use both -r and -u at the same time");
	err++;
    }
    if(err) {
	fprintf(stderr, "Usage: ovdb_init [-r|-u]\n");
	exit(1);
    }

    locked = ovdb_getlock(OVDB_LOCK_EXCLUSIVE);
    if(locked) {
	if(do_upgrade) {
            notice("database is quiescent, upgrading");
	    flags = OVDB_RECOVER | OVDB_UPGRADE;
	}
	else {
            notice("database is quiescent, running normal recovery");
	    flags = OVDB_RECOVER;
	}
    } else {
        warn("database is active");
	if(do_upgrade) {
            warn("upgrade will not be attempted");
	    do_upgrade = 0;
	}
	if(recover_only)
            die("recovery will not be attempted");
	ovdb_getlock(OVDB_LOCK_ADMIN);
	flags = 0;
    }

    ret = ovdb_open_berkeleydb(OV_WRITE, flags);
    if(ret == DB_RUNRECOVERY) {
	if(locked)
            die("database could not be recovered");
	else {
            warn("database needs recovery but cannot be locked");
            die("other processes accessing the database must exit to start"
                " recovery");
        }
    }
    if(ret != 0)
        die("cannot open Berkeley DB: %s", db_strerror(ret));

    if(recover_only)
	exit(0);

    if(do_upgrade) {
	ret = upgrade_environment();
	if(ret != 0)
	    die("cannot upgrade Berkeley DB environment: %s", db_strerror(ret));
    }

    if(check_upgrade(do_upgrade)) {
	ovdb_close_berkeleydb();
	exit(1);
    }

    ovdb_close_berkeleydb();
    ovdb_releaselock();

    if(ovdb_check_pidfile(OVDB_MONITOR_PIDFILE) == false) {
        notice("starting ovdb monitor");
	switch(fork()) {
	case -1:
            sysdie("cannot fork");
	case 0:
	    setsid();
	    execl(concatpath(innconf->pathbin, "ovdb_monitor"),
		"ovdb_monitor", SPACES, NULL);
            syswarn("cannot exec ovdb_monitor");
	    _exit(1);
	}
	sleep(2);	/* give the monitor a chance to start */
    } else
        warn("ovdb_monitor already running");

    if(ovdb_conf.readserver) {
	if(ovdb_check_pidfile(OVDB_SERVER_PIDFILE) == false) {
            notice("starting ovdb server");
            daemonize(innconf->pathtmp);
            execl(concatpath(innconf->pathbin, "ovdb_server"), "ovdb_server",
                SPACES, NULL);
            syswarn("cannot exec ovdb_server");
            _exit(1);
	} else
            warn("ovdb_server already running");
    }

    exit(0);
}
#endif /* HAVE_BDB */


/*
 * ovdb_recover
 *  Performs recovery on OV database, if needed
 * 
 * ovdb_upgrade
 *  Performs upgrade of OV database, if needed
 */

#include "config.h"
#include "clibrary.h"
#include "libinn.h"
#include <errno.h>
#include <syslog.h>
#include <string.h>

#ifndef USE_BERKELEY_DB

int main(int argc, char **argv)
{
    exit(0);
}

#else /* USE_BERKELEY_DB */

#include <db.h>
#include "ov.h"
#include "../storage/ovdb/ovdb.h"

char *basename(char *path)
{
    char *c = strrchr(path, '/');
    return c ? c+1 : path;
}

int main(int argc, char **argv)
{
    int ret;

    openlog("ovdb_recover", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);

    if (ReadInnConf() < 0) { exit(1); }

    if(strcmp(innconf->ovmethod, "ovdb"))
	exit(0);

    if(!strcmp(basename(argv[0]), "ovdb_upgrade")) {
	fprintf(stderr, "Upgrading BerkeleyDB databases ... \n");
	ret = ovdb_open_berkeleydb(OV_WRITE, OVDB_DBUPGRADE);
	if(ret)
	    exit(1);
    } else {
	if(!strcmp(argv[1], "-f"))
	    ret = DB_RUNRECOVERY;
	else
	    ret = ovdb_open_berkeleydb(OV_WRITE, 0);

	if(ret != 0) {
	    if(ret == DB_RUNRECOVERY) {
		fprintf(stderr, "Recovering BerkeleyDB environment ...\n");
		ret = ovdb_open_berkeleydb(OV_WRITE, OVDB_RECOVER);
	    }
	}
	if(ret != 0) {
	    fprintf(stderr, "Unable to open BerkeleyDB: %s", strerror(ret));
	    exit(1);
	}
    }
    ovdb_close_berkeleydb();
    exit(0);
}

#endif /* USE_BERKELEY_DB */


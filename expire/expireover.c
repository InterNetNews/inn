/*  $Revision$
**
**  Expire overview database.
*/
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include "configdata.h"
#include "clibrary.h"
#include "qio.h"
#include "libinn.h"
#include "macros.h"
#include "paths.h"
#include "storage.h"
#include "ov.h"
#include <syslog.h>

void usage(void) {
    fprintf(stderr, "Usage: expireover [flags]\n");
	exit(1);
}

int main(int argc, char *argv[]) {
    int		i;
    char        activefn[BIG_BUFFER] = "";
    QIOSTATE	*qp;
    char	*line;
    char	*p;
    int		lo;
    FILE	*F;
    BOOL	Nonull, LowmarkFile = FALSE;
    char	*lofile;
    OVGE	ovge;

    /* First thing, set up logging and our identity. */
    openlog("expireover", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);     

    ovge.earliest = FALSE;
    ovge.keep = FALSE;
    ovge.ignoreselfexpire = FALSE;
    ovge.usepost = FALSE;
    ovge.quiet = FALSE;
    ovge.timewarp = 0;
    ovge.filename = NULL;
    ovge.delayrm = FALSE;
    while ((i = getopt(argc, argv, "ef:kNpqw:z:Z:")) != EOF) {
	switch (i) {
	case 'e':
	    ovge.earliest = TRUE;
	    break;
	case 'f':
	    strcpy(activefn, optarg);
		    break;
	case 'k':
	    ovge.keep = TRUE;
	    break;
	case 'N':
	    ovge.ignoreselfexpire = TRUE;
	    break;
	case 'p':
	    ovge.usepost = TRUE;
	    break;
	case 'q':
	    ovge.quiet = TRUE;
	    break;
	case 'w':
	    ovge.timewarp = (time_t)(atof(optarg) * 86400.);
            break;
	case 'z':
	    ovge.filename = optarg;
	    ovge.delayrm = TRUE;
	    break;
	case 'Z':
	    LowmarkFile = TRUE;
	    lofile = COPY(optarg);
		break;
	default:
	    usage();
	    }
    }
    if (ovge.earliest && ovge.keep) {
	fprintf(stderr, "expireover: -e and -k cannot be specified at the same time\n");
	exit(1);
    }
    if (!ovge.earliest && !ovge.keep)
	ovge.earliest = TRUE;

    if (ReadInnConf() < 0) exit(1);

    if (LowmarkFile) {
	if (unlink(lofile) < 0 && errno != ENOENT)
	    (void)fprintf(stderr, "Warning: expireover can't remove %s, %s\n",
		    lofile, strerror(errno));
	if ((F = fopen(lofile, "a")) == NULL) {
	    (void)fprintf(stderr, "expireover: can't open %s, %s\n",
		    lofile, strerror(errno));
	    exit(1);
	}
    }

    i = 1;
    if (SMsetup(SM_PREOPEN, (void *)&i) && !SMinit()) {
	fprintf(stderr, "expireover: cant initialize storage method, %s",SMerrorstr);
	exit(1);
	}

    if (!OVopen(OV_READ | OV_WRITE)) {
	fprintf(stderr, "expireover: could not open OV database\n");
	exit(1);
    }
    if (innconf->groupbaseexpiry) {
	(void)time(&ovge.now);
	if (!OVctl(OVGROUPBASEDEXPIRE, (void *)&ovge)) {
	    fprintf(stderr, "expireover: OVctl failed\n");
	    exit(1);
	}
    }

    if (activefn[0] == '\0') {
	strcpy(activefn, cpcatpath(innconf->pathdb, _PATH_ACTIVE));
	Nonull = FALSE;
    } else {
	Nonull = TRUE;
    }
    if (strcmp(activefn, "-") == 0) {
	qp = QIOfdopen(fileno(stdin));
    } else {
	if ((qp = QIOopen(activefn)) == NULL) {
	    fprintf(stderr, "expireover: could not open active file (%s)\n", activefn);
		OVclose();
		exit(1);
	}
    }
    while ((line = QIOread(qp)) != NULL) {
	if ((p = strchr(line, ' ')) != NULL)
	    *p = '\0';
	if ((p = strchr(line, '\t')) != NULL)
	    *p = '\0';

	if (OVexpiregroup(line, &lo)) {
	    if (LowmarkFile && lo != 0) {
		(void)fprintf(F, "%s %u\n", line, lo);
	    }
	} else {
	    fprintf(stderr, "expireover: could not expire %s\n", line);
	}
    }
    /* purge deleted newsgroups */
    if (!Nonull && !OVexpiregroup(NULL, NULL)) {
	fprintf(stderr, "expireover: could not expire purged newsgroups\n");
    }
    QIOclose(qp);

    OVclose();
    if (LowmarkFile && (fclose(F) == EOF)) {
	(void)fprintf(stderr, "expireover: can't close %s, %s\n", lofile, strerror(errno));
    }
    return 0;
}

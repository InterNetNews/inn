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
#include "ov3.h"

void usage(void) {
    fprintf(stderr, "Usage: expireover [flags]\n");
	exit(1);
}

int main(int argc, char *argv[]) {
    int		                i;
    char                activefn[BIG_BUFFER] = "";
    int                 linenum = 0;
    QIOSTATE			*qp;
    char			*line;
    char			*p;
    BOOL                RebuildData = FALSE;
    int				lo;
    FILE			*F;
    BOOL			LowmarkFile = FALSE;
    char			*lofile;

    while ((i = getopt(argc, argv, "df:Z:")) != EOF) {
	switch (i) {
	case 'f':	    
	    strcpy(activefn, optarg);
		    break;
	case 'd':
	    RebuildData = TRUE;
		break;
	case 'Z':
	    LowmarkFile = TRUE;
	    lofile = COPY(optarg);
		break;
	default:
	    usage();
	    }
    }

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

    if (!OV3open(1, OV3_READ | OV3_WRITE)) {
	fprintf(stderr, "expireover: could not open OV3 database\n");
	exit(1);
    }

    if (activefn[0] == '\0')
	strcpy(activefn, cpcatpath(innconf->pathdb, _PATH_ACTIVE));
    if (strcmp(activefn, "-") == 0) {
	qp = QIOfdopen(fileno(stdin));
    } else {
	if ((qp = QIOopen(activefn)) == NULL) {
	    fprintf(stderr, "expireover: could not open active file (%s)\n", activefn);
		OV3close();
		exit(1);
	    }
	}
    while ((line = QIOread(qp)) != NULL) {
	linenum++;
	if ((p = strchr(line, ' ')) != NULL)
	    *p = '\0';
	if ((p = strchr(line, '\t')) != NULL)
	    *p = '\0';

	if (RebuildData) {
	    if (!OV3rebuilddatafromindex(line)) {
		fprintf(stderr, "expireover: could not rebuld data for %s\n", line);
	    }
	    continue;
	}
	if (OV3expiregroup(line, &lo)) {
	    if (LowmarkFile) {
		(void)fprintf(F, "%s %u\n", line, lo);
	    }
	} else {
	    fprintf(stderr, "expireover: could not expire %s\n", line);
	}
    }

    OV3close();
    if (LowmarkFile && (fclose(F) == EOF)) {
	(void)fprintf(stderr, "expireover: can't close %s, %s\n", lofile, strerror(errno));
    }
    return 0;
}

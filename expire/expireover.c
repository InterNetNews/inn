/*  $Revision$
**
**  Expire overview database.
*/
#include <stdio.h>
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

    while ((i = getopt(argc, argv, "df:")) != EOF) {
	switch (i) {
	case 'f':	    
	    strcpy(activefn, optarg);
		    break;
	case 'd':
	    RebuildData = TRUE;
		break;
	default:
	    usage();
	    }
    }

    if (ReadInnConf() < 0) exit(1);
    i = 1;
    if (SMsetup(SM_PREOPEN, (void *)&i) && !SMinit()) {
	fprintf(stderr, "cant initialize storage method, %s",SMerrorstr);
	exit(1);
	}

    if (!OV3open(1, OV3_READ | OV3_WRITE)) {
	fprintf(stderr, "Could not open OV3 database\n");
	exit(1);
    }

    if (activefn[0] == '\0')
	strcpy(activefn, cpcatpath(innconf->pathdb, _PATH_ACTIVE));
    if (strcmp(activefn, "-") == 0) {
	qp = QIOfdopen(fileno(stdin));
    } else {
	if ((qp = QIOopen(activefn)) == NULL) {
	    fprintf(stderr, "Could not open active file (%s)\n", activefn);
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
		fprintf(stderr, "Could not rebuld data for %s\n", line);
		}
	            continue;
	}
	if (!OV3expiregroup(line)) {
	    fprintf(stderr, "Could not expire %s\n", line);
    }
    }

    OV3close();
    return 0;
}

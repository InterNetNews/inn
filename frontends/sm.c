/*  $Revision$
**
** Provide a command line interface to the storage manager
*/

#include <stdio.h>
#include <errno.h>
#include "configdata.h"
#include "clibrary.h"
#include "libinn.h"
#include "macros.h"
#include "paths.h"
#include "qio.h"

#define MAXOVERLINE	4096

void Usage(void) {
    fprintf(stderr, "Usage sm [-r] [-d] [-o] token [token] [token] ...\n");
    exit(1);
}

int main(int argc, char **argv) {
    int                 c;
    BOOL                Delete = FALSE;
    BOOL                Overview = FALSE;
    BOOL		OVERmmap;
    BOOL		val;
    int                 i;
    char                *p;
    QIOSTATE            *qp;
    TOKEN		token;
    int			linelen;
    
    while ((c = getopt(argc, argv, "rdo")) != EOF) {
	switch (c) {
	case 'r':
	case 'd':
	    Delete = TRUE;
	    break;
	case 'o':
	    Overview = TRUE;
	    break;
	default:
	    Usage();
	}
    }
    if (Delete && Overview) {
	fprintf(stderr, "-o cannot be used with -r or -d\n");
	exit(1);
    }

    if (Overview) {
	OVERmmap = GetBooleanConfigValue(_CONF_OVERMMAP, TRUE);
	if (OVERmmap)
	    val = TRUE;
	else
	    val = FALSE;
	if (!OVERsetup(OVER_MMAP, (void *)&val)) {
	    fprintf(stderr, "Can't setup unified overview mmap: %s\n", strerror(errno));
	    exit(1);
	}
	if (!OVERsetup(OVER_MODE, "r")) {
	    fprintf(stderr, "Can't setup unified overview mode: %s\n", strerror(errno));
	    exit(1);
	}
	if (!OVERinit()) {
	    fprintf(stderr, "Can't initialize unified overview mode: %s\n", strerror(errno));
	    exit(1);
	}
    } else if (!SMinit()) {
	fprintf(stderr, "Could not initialize the storage manager: %s", SMerrorstr);
	exit(1);
    }
    
    for (i = optind; i < argc; i++) {
	if (Delete) {
	    if (!IsToken(argv[i])) {
		fprintf(stderr, "%s is not a storage token\n", argv[i]);
		continue;
	    }
	    if (!SMcancel(TextToToken(argv[i])))
		fprintf(stderr, "Could not remove %s: %s\n", argv[i], SMerrorstr);
	} else if (Overview) {
	    if (!IsToken(argv[i])) {
		fprintf(stderr, "%s is not a storage token\n", argv[i]);
		continue;
	    }
	    token = TextToToken(argv[i]);
	    if ((p = OVERretrieve(&token, &linelen)) == (char *)NULL)
		fprintf(stderr, "Could not retrieve %s\n", argv[i]);
	    if (fwrite(p, linelen, 1, stdout) != 1) {
		fprintf(stderr, "Output failed: %s\n", strerror(errno));
		exit(1);
	    }
	    printf("\n");
	} else {
	    if ((qp = QIOopen(argv[i])) == NULL) {
		fprintf(stderr, "Coult not open %s\n", argv[i]);
		continue;
	    } else {
		while ((p = QIOread(qp)) != NULL) {
		    if (QIOlength(qp) != 0)
			if (fwrite(p, QIOlength(qp), 1, stdout) != 1) {
			    fprintf(stderr, "Output failed: %s\n", strerror(errno));
			    exit(1);
			}
		    printf("\n");
		}
		QIOclose(qp);
	    }
	}
	  
    }
    SMshutdown();
    return 0;
}

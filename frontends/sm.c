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
#include "qio.h"

void Usage(void) {
    fprintf(stderr, "Usage sm [-r] [-d] token [token] [token] ...\n");
    exit(1);
}

int main(int argc, char **argv) {
    int                 c;
    BOOL                Delete = FALSE;
    int                 i;
    char                *p;
    QIOSTATE            *qp;
    
    while ((c = getopt(argc, argv, "rd")) != EOF) {
	switch (c) {
	case 'r':
	case 'd':
	    Delete = TRUE;
	    break;
	default:
	    Usage();
	}
    }

    if (!SMinit()) {
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

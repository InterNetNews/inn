/*  $Revision$
**
** Provide a command line interface to the storage manager
*/

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include "configdata.h"
#include "clibrary.h"
#include "libinn.h"
#include "macros.h"
#include "paths.h"
#include "qio.h"
#include <syslog.h>  

void Usage(void) {
    fprintf(stderr, "Usage sm [-q] [-r] [-d] [-o] [-R] token [token] [token] ...\n");
    exit(1);
}

int main(int argc, char **argv) {
    int                 c;
    BOOL                Quiet = FALSE;
    BOOL                Delete = FALSE;
    BOOL                Overview = FALSE;
    BOOL                Rawformat = FALSE;
    BOOL		val;
    BOOL		notinitialized = TRUE;
    int                 i;
    char                *p;
    QIOSTATE            *qp;
    TOKEN		token;
    int			linelen;
    ARTHANDLE		*art;
    
    /* First thing, set up logging and our identity. */
    openlog("sm", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);     

    if (ReadInnConf() < 0) { exit(1); }

    while ((c = getopt(argc, argv, "qrdoR")) != EOF) {
	switch (c) {
	case 'q':
	    Quiet = TRUE;
	    break;
	case 'r':
	case 'd':
	    Delete = TRUE;
	    break;
	case 'o':
	    Overview = TRUE;
	    break;
	case 'R':
	    Rawformat = TRUE;
	    break;
	default:
	    Usage();
	}
    }
    if (Delete && Overview) {
	if (!Quiet)
	    fprintf(stderr, "-o cannot be used with -r or -d\n");
	exit(1);
    }

    if (Overview) {
	if (innconf->overviewmmap)
	    val = TRUE;
	else
	    val = FALSE;
	if (!OVERsetup(OVER_MMAP, (void *)&val)) {
	    if (!Quiet)
		fprintf(stderr, "Can't setup unified overview mmap\n");
	    exit(1);
	}
	if (!OVERsetup(OVER_MODE, "r")) {
	    if (!Quiet)
		fprintf(stderr, "Can't setup unified overview mode\n");
	    exit(1);
	}
	if (!OVERinit()) {
	    if (!Quiet)
		fprintf(stderr, "Can't initialize unified overview mode\n");
	    exit(1);
	}
    } else {
	if (Delete) {
	    val = TRUE;
	    if (!SMsetup(SM_RDWR, (void *)&val)) {
		fprintf(stderr, "Can't setup storage manager\n");
		exit(1);
	    }
	}
    }
    
    for (i = optind; i < argc; i++) {
	if (Delete) {
	    if (!IsToken(argv[i])) {
		if (!Quiet)
		    fprintf(stderr, "%s is not a storage token\n", argv[i]);
		continue;
	    }
	    if (notinitialized) {
		if (!SMinit()) {
		    if (!Quiet)
			fprintf(stderr, "Could not initialize the storage manager: %s", SMerrorstr);
		    exit(1);
		}
		notinitialized = FALSE;
	    }
	    if (!SMcancel(TextToToken(argv[i]))) {
		if (!Quiet)
		    fprintf(stderr, "Could not remove %s: %s\n", argv[i], SMerrorstr);
	    }
	} else if (Overview) {
	    if (!IsToken(argv[i])) {
		if (!Quiet)
		    fprintf(stderr, "%s is not a storage token\n", argv[i]);
		continue;
	    }
	    if (notinitialized) {
		if (!SMinit()) {
		    if (!Quiet)
			fprintf(stderr, "Could not initialize the storage manager: %s", SMerrorstr);
		    exit(1);
		}
		notinitialized = FALSE;
	    }
	    token = TextToToken(argv[i]);
	    if ((p = OVERretrieve(&token, &linelen)) == (char *)NULL) {
		if (!Quiet)
		    fprintf(stderr, "Could not retrieve %s\n", argv[i]);
		continue;
	    }
	    if (fwrite(p, linelen, 1, stdout) != 1) {
		if (!Quiet)
		    fprintf(stderr, "Output failed\n");
		exit(1);
	    }
	    printf("\n");
	} else {
	    if (Rawformat) {
		if (!IsToken(argv[i])) {
		    if (!Quiet)
			fprintf(stderr, "%s is not a storage token\n", argv[i]);
		    continue;
		}
		token = TextToToken(argv[i]);
		if (notinitialized) {
		    if (!SMinit()) {
			if (!Quiet)
			     fprintf(stderr, "Could not initialize the storage manager: %s", SMerrorstr);
			exit(1);
		    }
		    notinitialized = FALSE;
		}
		if ((art = SMretrieve(token, RETR_ALL)) == NULL) {
		    if (!Quiet)
			fprintf(stderr, "Could not retrieve %s\n", argv[i]);
		    continue;
		}
		if (fwrite(art->data, art->len, 1, stdout) != 1) {
		    if (!Quiet)
			fprintf(stderr, "Output failed\n");
		    exit(1);
		}
		SMfreearticle(art);
	    } else {
		if (IsToken(argv[i])) {
		    if (notinitialized) {
			if (!SMinit()) {
			    if (!Quiet)
				fprintf(stderr, "Could not initialize the storage manager: %s", SMerrorstr);
			    exit(1);
			}
			notinitialized = FALSE;
		    }
		}
		if ((qp = QIOopen(argv[i])) == NULL) {
		    if (!Quiet)
			fprintf(stderr, "Could not open %s\n", argv[i]);
		    continue;
		} else {
		    while ((p = QIOread(qp)) != NULL) {
			if (QIOlength(qp) != 0)
			    if (fwrite(p, QIOlength(qp), 1, stdout) != 1) {
				if (!Quiet)
				    fprintf(stderr, "Output failed: %s\n", strerror(errno));
				exit(1);
			    }
			printf("\n");
		    }
		    QIOclose(qp);
		}
	    }
	}
	  
    }
    SMshutdown();
    return 0;
}

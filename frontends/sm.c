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
    fprintf(stderr, "Usage sm [-q] [-r] [-d] [-R] [-i] token [token] [token] ...\n");
    exit(1);
}

int main(int argc, char **argv) {
    int                 c;
    BOOL                Quiet = FALSE;
    BOOL                Delete = FALSE;
    BOOL                Rawformat = FALSE;
    BOOL                Artinfo = FALSE;
    BOOL		val;
    int                 i;
    char                *p;
    QIOSTATE            *qp;
    TOKEN		token;
    ARTHANDLE		*art;
    struct artngnum	ann;

    /* First thing, set up logging and our identity. */
    openlog("sm", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);     

    if (ReadInnConf() < 0) { exit(1); }

    while ((c = getopt(argc, argv, "iqrdR")) != EOF) {
	switch (c) {
	case 'q':
	    Quiet = TRUE;
	    break;
	case 'r':
	case 'd':
	    Delete = TRUE;
	    break;
	case 'R':
	    Rawformat = TRUE;
	    break;
	case 'i':
	    Artinfo = TRUE;
	    break;
	default:
	    Usage();
	}
    }

	if (Delete) {
	    val = TRUE;
	    if (!SMsetup(SM_RDWR, (void *)&val)) {
		fprintf(stderr, "Can't setup storage manager\n");
		exit(1);
	    }
	}
	if (!SMinit()) {
	    if (!Quiet)
		fprintf(stderr, "Could not initialize the storage manager: %s", SMerrorstr);
	    exit(1);
	}
    
    for (i = optind; i < argc; i++) {
	if (Artinfo) {
	    if (!IsToken(argv[i])) {
		if (!Quiet)
		    fprintf(stderr, "%s is not a storage token\n", argv[i]);
		continue;
	    }
	    token = TextToToken(argv[i]);
	    if (!SMprobe(SMARTNGNUM, &token, (void *)&ann)) {
		if (!Quiet)
		    fprintf(stderr, "Could not get art info %s\n", argv[i]);
	    } else {
		fprintf(stdout, "%s: %d\n", ann.groupname, ann.artnum);
		DISPOSE(ann.groupname);
	    }
	} else if (Delete) {
	    if (!IsToken(argv[i])) {
		if (!Quiet)
		    fprintf(stderr, "%s is not a storage token\n", argv[i]);
		continue;
	    }
	    if (!SMcancel(TextToToken(argv[i]))) {
		if (!Quiet)
		    fprintf(stderr, "Could not remove %s: %s\n", argv[i], SMerrorstr);
	    }
	} else  if (Rawformat) {
		if (!IsToken(argv[i])) {
		    if (!Quiet)
			fprintf(stderr, "%s is not a storage token\n", argv[i]);
		    continue;
		}
		token = TextToToken(argv[i]);
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
	  
    SMshutdown();
    return 0;
}

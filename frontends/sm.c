/*  $Id$
**
**  Provide a command line interface to the storage manager
*/

#include "config.h"
#include "clibrary.h"
#include <errno.h>
#include <syslog.h>  

#include "libinn.h"
#include "macros.h"
#include "paths.h"
#include "storage.h"

bool	Quiet = FALSE;
bool	Delete = FALSE;
bool	Rawformat = FALSE;
bool	Artinfo = FALSE;

static void Usage(void) {
    fprintf(stderr, "Usage sm [-q] [-r] [-d] [-R] [-i] [token] [token] ...\n");
    exit(1);
}

static void getinfo(const char *p) {
    TOKEN		token;
    struct artngnum	ann;
    ARTHANDLE		*art;
    int                 len;
    char		*q;

    if (!IsToken(p)) {
	if (!Quiet)
	    fprintf(stderr, "%s is not a storage token\n", p);
	return;
    }
    token = TextToToken(p);
    if (Artinfo) {
	if (!SMprobe(SMARTNGNUM, &token, (void *)&ann)) {
	    if (!Quiet)
		fprintf(stderr, "Could not get art info %s\n", p);
	} else {
	    fprintf(stdout, "%s: %lu\n", ann.groupname, ann.artnum);
	    DISPOSE(ann.groupname);
	}
    } else if (Delete) {
	if (!SMcancel(token)) {
	    if (!Quiet)
		fprintf(stderr, "Could not remove %s: %s\n", p, SMerrorstr);
	}
    } else {
	if ((art = SMretrieve(token, RETR_ALL)) == NULL) {
	    if (!Quiet)
		fprintf(stderr, "Could not retrieve %s\n", p);
	    return;
	}
	if (Rawformat) {
	    if (fwrite(art->data, art->len, 1, stdout) != 1) {
		if (!Quiet)
		    fprintf(stderr, "Output failed\n");
		exit(1);
	    }
	} else {
	    q = FromWireFmt(art->data, art->len, &len);
	    if (fwrite(q, len, 1, stdout) != 1) {
		if (!Quiet)
		    fprintf(stderr, "Output failed\n");
		exit(1);
	    }
	    DISPOSE(q);
	}
	SMfreearticle(art);
    }
}

int main(int argc, char **argv) {
    int		c;
    bool	val;
    int		i;
    char	*p, buff[BUFSIZ];

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

    if (optind == argc) {
	while (fgets(buff, sizeof buff, stdin) != NULL) {
	    if ((p = strchr(buff, '\n')) == NULL)
		continue;
	    *p = '\0';
	    getinfo(buff);
	}
    } else {
	for (i = optind; i < argc; i++) {
	    getinfo(argv[i]);
	}
    }

    SMshutdown();
    exit(0);
}

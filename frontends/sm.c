/*  $Id$
**
**  Provide a command line interface to the storage manager
*/

#include "config.h"
#include "clibrary.h"
#include <errno.h>
#include <syslog.h>  

#include "inn/innconf.h"
#include "inn/messages.h"
#include "libinn.h"
#include "macros.h"
#include "paths.h"
#include "storage.h"

bool	Delete = FALSE;
bool	Rawformat = FALSE;
bool	Artinfo = FALSE;

static void Usage(void) {
    fprintf(stderr, "Usage sm [-qrdRi] [token] [token] ...\n");
    exit(1);
}

static void getinfo(const char *p) {
    TOKEN		token;
    struct artngnum	ann;
    ARTHANDLE		*art;
    int                 len;
    char		*q;

    if (!IsToken(p)) {
        warn("%s is not a storage token", p);
	return;
    }
    token = TextToToken(p);
    if (Artinfo) {
	if (!SMprobe(SMARTNGNUM, &token, (void *)&ann)) {
            warn("could not get article information for %s", p);
	} else {
	    printf("%s: %lu\n", ann.groupname, ann.artnum);
	    DISPOSE(ann.groupname);
	}
    } else if (Delete) {
	if (!SMcancel(token))
            warn("could not remove %s: %s", p, SMerrorstr);
    } else {
	if ((art = SMretrieve(token, RETR_ALL)) == NULL) {
            warn("could not retrieve %s", p);
	    return;
	}
	if (Rawformat) {
	    if (fwrite(art->data, art->len, 1, stdout) != 1)
                die("output failed");
	} else {
	    q = FromWireFmt(art->data, art->len, &len);
	    if (fwrite(q, len, 1, stdout) != 1)
                die("output failed");
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
    message_program_name = "sm";

    if (!innconf_read(NULL))
        exit(1);

    while ((c = getopt(argc, argv, "iqrdR")) != EOF) {
	switch (c) {
	case 'q':
            message_handlers_warn(0);
            message_handlers_die(0);
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
	if (!SMsetup(SM_RDWR, (void *)&val))
            die("cannot set up storage manager");
    }
    if (!SMinit())
        die("cannot initialize storage manager: %s", SMerrorstr);

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

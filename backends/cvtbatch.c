/*  $Revision$
**
**  Read file list on standard input and spew out batchfiles.
*/
#include <stdio.h>
#include <sys/types.h>
#include "configdata.h"
#include "clibrary.h"
#include <errno.h>
#include <sys/stat.h>
#if	defined(DO_NEED_TIME)
#include <time.h>
#endif	/* defined(DO_NEED_TIME) */
#include <sys/time.h>
#include "paths.h"
#include "qio.h"
#include "libinn.h"
#include "macros.h"
#include <syslog.h>  


/*
**  Print a usage message and exit.
*/
STATIC NORETURN
Usage()
{
    (void)fprintf(stderr, "convertbatch usage_error.\n");
    exit(1);
}


int
main(int ac, char *av[]) {
    static char	HDR[] = "Message-ID:";
    int		i;
    QIOSTATE	*qp;
    char	*line;
    char	*text;
    char	*format;
    char	*p, *q;
    BOOL	Dirty;
    TOKEN	token;
    ARTHANDLE	*art;
    int		len;

    /* First thing, set up logging and our identity. */
    openlog("cvtbatch", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);           

    if (ReadInnConf() < 0) exit(1);
    /* Parse JCL. */
    format = "nm";
    while ((i = getopt(ac, av, "w:")) != EOF)
	switch (i) {
	default:
	    Usage();
	    /* NOTREACHED */
	case 'w':
	    for (p = format = optarg; *p; p++) {
		switch (*p) {
		case FEED_BYTESIZE:
		case FEED_FULLNAME:
		case FEED_MESSAGEID:
		case FEED_NAME:
		    continue;
		}
		(void)fprintf(stderr, "Ignoring \"%c\" in -w flag.\n", *p);
	    }
	}
    ac -= optind;
    av += optind;
    if (ac)
	Usage();

    if (chdir(innconf->patharticles) < 0) {
	(void)fprintf(stderr, "batchconvert cant chdir %s, %s\n",
		innconf->patharticles, strerror(errno));
	exit(1);
    }
    if (!SMinit()) {
	(void)fprintf(stderr, "cvtbatch: Could not initialize the storage manager: %s", SMerrorstr);
	exit(1);
    }

    /* Loop over all input. */
    qp = QIOfdopen((int)fileno(stdin));
    while ((line = QIOread(qp)) != NULL) {
	for (p = line; *p; p++)
	    if (ISWHITE(*p)) {
		*p = '\0';
		break;
	    }

	if (!IsToken(line))
	    continue;
	token = TextToToken(line);
	if ((art = SMretrieve(token, RETR_HEAD)) == NULL)
	    continue;
	if ((text = (char *)HeaderFindMem(art->data, art->len, "Message-ID", 10)) == NULL) {
	    SMfreearticle(art);
	    continue;
	}
	len = art->len;
	for (p = text; p < art->data + art->len; p++) {
	    if (*p == '\r' || *p == '\n')
		break;
	}
	if (p == art->data + art->len) {
	    SMfreearticle(art);
	    continue;
	}
	q = NEW(char, p - text + 1);
	memcpy(q, text, p - text);
	SMfreearticle(art);
	q[p - text] = '\0';

	/* Write the desired info. */
	for (Dirty = FALSE, p = format; *p; p++) {
	    switch (*p) {
	    default:
		continue;
	    case FEED_BYTESIZE:
		if (Dirty)
		    (void)putchar(' ');
		(void)printf("%ld", len);
		break;
	    case FEED_FULLNAME:
	    case FEED_NAME:
		if (Dirty)
		    (void)putchar(' ');
		(void)printf("%s", line);
		break;
	    case FEED_MESSAGEID:
		if (Dirty)
		    (void)putchar(' ');
		(void)printf("%s", q);
		break;
	    }
	    Dirty = TRUE;
	}
	DISPOSE(q);
	if (Dirty)
	    (void)putchar('\n');
    }

    exit(0);
    /* NOTREACHED */
}

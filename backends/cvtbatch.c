/*  $Id$
**
**  Read file list on standard input and spew out batchfiles.
*/

#include "config.h"
#include "clibrary.h"

#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/qio.h"
#include "libinn.h"
#include "macros.h"
#include "paths.h"
#include "storage.h"


int
main(int ac, char *av[]) {
    int		i;
    QIOSTATE	*qp;
    char	*line;
    const char	*text;
    char	*format;
    char	*p, *q;
    bool	Dirty;
    TOKEN	token;
    ARTHANDLE	*art;
    int		len;

    /* First thing, set up our identity. */
    message_program_name = "cvtbatch";
    if (!innconf_read(NULL))
        exit(1);

    /* Parse JCL. */
    format = COPY("nm");
    while ((i = getopt(ac, av, "w:")) != EOF)
	switch (i) {
	default:
            die("usage error");
            break;
	case 'w':
	    for (p = format = optarg; *p; p++) {
		switch (*p) {
		case FEED_BYTESIZE:
		case FEED_FULLNAME:
		case FEED_MESSAGEID:
		case FEED_NAME:
		    continue;
		}
                warn("ignoring %c in -w flag", *p);
	    }
	}
    ac -= optind;
    av += optind;
    if (ac)
	die("usage error");

    if (!SMinit())
        die("cannot initialize storage manager: %s", SMerrorstr);

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
	if ((text = HeaderFindMem(art->data, art->len, "Message-ID", 10)) == NULL) {
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
		(void)printf("%d", len);
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


/*
 * delete a list of filenames from stdin
 *
 * exit(0) if all is OK (files that can't be unlinked because they
 *	didn't exist is "OK")
 *
 * exit(1) in other cases - problems with stdin, no permission, ...
 * written by <kre@munnari.oz.au>
 */

#include "config.h"
#include "clibrary.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <syslog.h>  

#include "inn/qio.h"
#include "libinn.h"
#include "macros.h"
#include "storage.h"

STATIC char	*MyName;


STATIC void err_exit(char *s)
{
    (void)fprintf(stderr, "%s: %s\n", MyName, s);
    SMshutdown();
    exit(1);
}


static void
err_alloc(const char *what, size_t size, const char *file, int line)
{
    fprintf(stderr, "%s: Can't %s %lu bytes at line %d of %s: %s", MyName,
            what, (unsigned long) size, line, file, strerror(errno));
    SMshutdown();
    exit(1);
}


int main(int ac, char *av[])
{
    int		i;
    char	*p, *line;
    BOOL	empty_error, val;
    QIOSTATE	*qp;
    TOKEN	token;

    /* First thing, set up logging and our identity. */
    openlog("fastrm", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);

    MyName = av[0];
    if ((p = strrchr(MyName, '/')) != NULL)
	MyName = p + 1;
    ONALLOCFAIL(err_alloc);
    empty_error = FALSE;

    if (ReadInnConf() < 0) { exit(1); }

    while ((i = getopt(ac, av, "e")) != EOF) {
	switch (i) {
	case 'e':
	    empty_error = TRUE;
	    break;
	default:
	    err_exit("Usage error, unknown flag");
	    break;
	}
    }

    val = TRUE;
    if (!SMsetup(SM_RDWR, (void *)&val) || !SMsetup(SM_PREOPEN, (void *)&val)) {
	fprintf(stderr, "Can't setup storage manager\n");
	exit(1);
    }
    if (!SMinit()) {
	fprintf(stderr, "Can't initialize storage manager\n");
	exit(1);
    }

    qp = QIOfdopen(fileno(stdin));
    while ((line = QIOread(qp)) != NULL) {
	if (!IsToken(line))
	    continue;
	token = TextToToken(line);
	if (!SMcancel(token) && SMerrno != SMERR_NOENT && SMerrorstr != NULL)
	    fprintf(stderr, "Could not remove %s: %s\n", line, SMerrorstr);
	empty_error = FALSE;
    }
    QIOclose(qp);

    SMshutdown();
    
    if (empty_error)
	err_exit("No files to remove");

    exit(0);
    /* NOTREACHED */
}

/*
** Refile articles into the storage manager under the current storage.conf
** rules, deleting articles from their old place in the spool.
** Written 10-09-99 by rmtodd@servalan.servalan.com
**
** Note that history and overview will have to be rebuilt for the moved
** articles to be visible after they're moved.
*/

/* include foo needed by libinn/storage manager */
#include "config.h"
#include "clibrary.h"
#include <errno.h>

#include "inn/innconf.h"
#include "inn/qio.h"
#include "inn/libinn.h"
#include "inn/paths.h"
#include "inn/storage.h"

char *ME;

static void
ProcessLine(char *line)
{
    char *tokenptr;
    int len;
    ARTHANDLE *art;
    ARTHANDLE newart = ARTHANDLE_INITIALIZER;
    TOKEN token, newtoken;
    char *arttmp;

    tokenptr = line;
    
    /* zap newline at end of tokenptr, if present. */
    len = strlen(tokenptr);
    if (tokenptr[len-1] == '\n') {
	tokenptr[len-1] = '\0';
    }

    if (!IsToken(tokenptr)) {
        fprintf(stderr, "%s: bad token format %s\n", ME, tokenptr);
        return;
    }
    token = TextToToken(tokenptr);
    if ((art = SMretrieve(token, RETR_ALL)) == NULL) return;

    len = art->len;
    arttmp = xmalloc(len);
    memcpy(arttmp, art->data, len);
    SMfreearticle(art);
    if (!SMcancel(token)) {
	fprintf(stderr, "%s: cant cancel %s:%s\n", ME, tokenptr, SMerrorstr);
	return;
    }

    newart.data = arttmp;
    newart.len = len;
    newart.arrived = (time_t) 0; /* set current time */
    newart.token = (TOKEN *)NULL;

    newtoken = SMstore(newart);
    if (newtoken.type == TOKEN_EMPTY) {
	fprintf(stderr, "%s: cant store article:%s\n", ME, SMerrorstr);
	return;
    }
    free(arttmp);
    printf("refiled %s ",TokenToText(token));
    printf("to %s\n", TokenToText(newtoken));
    return;
}

int
main(int argc UNUSED, char *argv[])
{
    bool 	one = true;
    char	buff[SMBUF];

    ME = argv[0];

    if (!innconf_read(NULL))
        exit(1);

    if (!SMsetup(SM_PREOPEN, &one) || !SMsetup(SM_RDWR, (void *)&one)) {
	fprintf(stderr, "can't init storage manager");
	exit(1);
    }
    if (!SMinit()) {
	fprintf(stderr, "Can't init storage manager: %s", SMerrorstr);
    }
    while (fgets(buff, SMBUF, stdin)) {
	ProcessLine(buff);
    }
    printf("\nYou will now need to rebuild history and overview for the moved"
           "\narticles to be visible again.\n");
    exit(0);
}

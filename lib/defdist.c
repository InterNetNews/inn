/*  $Id$
**
*/

#include "config.h"
#include "clibrary.h"
#include <ctype.h>
#include <errno.h>

#include "libinn.h"
#include "macros.h"
#include "paths.h"


typedef struct _DDENTRY {
    char	*Pattern;
    char	*Value;
    int		Weight;
} DDENTRY;

struct _DDHANDLE {
    int		Count;
    DDENTRY	*Entries;
    DDENTRY	*Current;
};
typedef struct _DDHANDLE	DDHANDLE;

struct _DDHANDLE *
DDstart(FILE *FromServer, FILE *ToServer)
{
    DDHANDLE	*h;
    DDENTRY	*ep;
    FILE	*F;
    char	buff[BUFSIZ];
    char	*p;
    char	*q;
    char        *path;
    int		i;
    int         fd;
    char	*name = NULL;

    /* Open the file. */
    path = concatpath(innconf->pathetc, _PATH_DISTPATS);
    F = fopen(path, "r");
    free(path);
    if (F == NULL) {
	/* Not available locally; try remotely. */
	if (FromServer == NULL || ToServer == NULL)
	    /* We're probably nnrpd running on the server and the
	     * file isn't installed.  Oh well. */
	    return NULL;
        name = concatpath(innconf->pathtmp, _PATH_TEMPACTIVE);
        fd = mkstemp(name);
        if (fd < 0)
            return NULL;
        close(fd);
	if ((F = CA_listopen(name, FromServer, ToServer,
		    "distrib.pats")) == NULL)
	    return NULL;
    }

    /* Count lines. */
    for (i = 0; fgets(buff, sizeof buff, F) != NULL; i++)
	continue;

    /* Allocate space for the handle. */
    if ((h = NEW(DDHANDLE, 1)) == NULL) {
	i = errno;
	(void)fclose(F);
	if (name != NULL)
	    unlink(name);
	errno = i;
	return NULL;
    }
    h->Count = 0;
    h->Current = NULL;
    if (i == 0) {
        return NULL ;
    } else if ((h->Entries = NEW(DDENTRY, i)) == NULL) {
	i = errno;
	DISPOSE(h);
	(void)fclose(F);
	if (name != NULL)
	    unlink(name);
	errno = i;
	return NULL;
    }

    fseeko(F, 0, SEEK_SET);
    for (ep = h->Entries; fgets(buff, sizeof buff, F) != NULL; ) {
	if ((p = strchr(buff, '\n')) != NULL)
	    *p = '\0';
	if (buff[0] == '\0' || buff[0] == '#')
	    continue;
	if ((p = strchr(buff, ':')) == NULL
	 || (q = strchr(p + 1, ':')) == NULL)
	    continue;
	*p++ = '\0';
	ep->Weight = atoi(buff);
	ep->Pattern = COPY(p);
	q = strchr(ep->Pattern, ':');
	*q++ = '\0';
	ep->Value = q;
	ep++;
    }
    h->Count = ep - h->Entries;

    (void)fclose(F);
    if (name != NULL)
	unlink(name);
    return h;
}


void
DDcheck(DDHANDLE *h, char *group)
{
    DDENTRY	*ep;
    int		i;
    int		w;

    if (h == NULL || group == NULL)
	return;

    w = h->Current ? h->Current->Weight : -1;
    for (ep = h->Entries, i = h->Count; --i >= 0; ep++)
	if (ep->Weight > w && uwildmat(group, ep->Pattern)) {
	    h->Current = ep;
	    w = ep->Weight;
	}
}


char *
DDend(DDHANDLE *h)
{
    static char	NIL[] = "";
    char	*p;
    int		i;
    DDENTRY	*ep;

    if (h == NULL) {
	p = NIL;
	return COPY(p);
    }

    if (h->Current == NULL)
	p = NIL;
    else
	p = h->Current->Value;
    p = COPY(p);

    for (ep = h->Entries, i = h->Count; --i >= 0; ep++)
	DISPOSE(ep->Pattern);
    DISPOSE(h->Entries);
    DISPOSE(h);
    return p;
}

#if	defined(TEST)
int
main(int ac, char *av[])
{
    struct _DDHANDLE	*h;
    char		*p;
    FILE		*FromServer;
    FILE		*ToServer;
    char		buff[SMBUF];

    if (NNTPremoteopen(NNTP_PORT, &FromServer, &ToServer, buff) < 0) {
	if ((p = strchr(buff, '\n')) != NULL)
	    *p = '\0';
	if ((p = strchr(buff, '\r')) != NULL)
	    *p = '\0';
	if (buff[0])
	    (void)fprintf(stderr, "%s\n", buff);
	else
	    perror("Can't connect");
	exit(1);
    }

    if ((h = DDstart(FromServer, ToServer)) == NULL)
	perror("Init failed, proceeding anyway");
    while ((p = *++av) != NULL)
	DDcheck(h, p);
    p = DDend(h);
    printf(">%s<\n", p);
    exit(0);
    /* NOTREACHED */
}
#endif	/* defined(TEST) */

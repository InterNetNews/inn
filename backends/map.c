/*  $Id$
**
*/

#include "config.h"
#include "clibrary.h"
#include <errno.h>

#include "libinn.h"
#include "macros.h"
#include "paths.h"

#include "map.h"


typedef struct _PAIR {
    char	First;
    char	*Key;
    char	*Value;
} PAIR;

static PAIR	*MAPdata;
static PAIR	*MAPend;


/*
**  Free the map.
*/
void
MAPfree(void)
{
    PAIR	*mp;

    for (mp = MAPdata; mp < MAPend; mp++) {
	free(mp->Key);
	free(mp->Value);
    }
    free(MAPdata);
    MAPdata = NULL;
}


/*
**  Read the map file.
*/
void
MAPread(const char *name)
{
    FILE	*F;
    int	i;
    PAIR	*mp;
    char	*p;
    char		buff[BUFSIZ];

    if (MAPdata != NULL)
	MAPfree();

    /* Open file, count lines. */
    if ((F = fopen(name, "r")) == NULL) {
	fprintf(stderr, "Can't open %s, %s\n", name, strerror(errno));
	exit(1);
    }
    for (i = 0; fgets(buff, sizeof buff, F) != NULL; i++)
	continue;
    mp = MAPdata = xmalloc((i + 1) * sizeof(PAIR));

    /* Read each line; ignore blank and comment lines. */
    fseeko(F, 0, SEEK_SET);
    while (fgets(buff, sizeof buff, F) != NULL) {
	if ((p = strchr(buff, '\n')) != NULL)
	    *p = '\0';
	if (buff[0] == '\0'
         || buff[0] == '#'
	 || (p = strchr(buff, ':')) == NULL)
	    continue;
	*p++ = '\0';
	mp->First = buff[0];
	mp->Key = xstrdup(buff);
	mp->Value = xstrdup(p);
	mp++;
    }
    fclose(F);
    MAPend = mp;
}


/*
**  Look up a name in the map, return original value if not found.
*/
char *
MAPname(char *p)
{
    PAIR	*mp;
    char	c;

    for (c = *p, mp = MAPdata; mp < MAPend; mp++)
	if (c == mp->First && strcmp(p, mp->Key) == 0)
	    return mp->Value;
    return p;
}

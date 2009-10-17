/*  $Id$
**
**  Routines for parsing arguments.
*/

#include "config.h"
#include "clibrary.h"
#include <ctype.h>
#include <time.h>

#include "inn/innconf.h"
#include "inn/libinn.h"

/*
**  Parse a string into a NULL-terminated array of words; return number
**  of words.  If argvp isn't NULL, it and what it points to will be freed.
*/
int
Argify(char *line, char ***argvp)
{
    char        **argv;
    char        *p;

    if (*argvp != NULL) {
        free(*argvp[0]);
        free(*argvp);
    }

    /* Copy the line, which we will split up. */
    while (ISWHITE(*line))
        line++;
    p = xstrdup(line);

    /* Allocate worst-case amount of space. */
    for (*argvp = argv = xmalloc((strlen(p) + 2) * sizeof(char *)); *p; ) {
        /* Mark start of this word, find its end. */
        for (*argv++ = p; *p && !ISWHITE(*p); )
            p++;
        if (*p == '\0')
            break;

        /* Nip off word, skip whitespace. */
        for (*p++ = '\0'; ISWHITE(*p); )
            p++;
    }
    *argv = NULL;
    return argv - *argvp;
}


/*
**  Take a vector which Argify made and glue it back together with
**  spaces between each element.  Returns a pointer to dynamic space.
*/
char *
Glom(char **av)
{
    char        **v;
    int i;
    char                *save;

    /* Get space. */
    for (i = 0, v = av; *v; v++)
        i += strlen(*v) + 1;
    i++;

    save = xmalloc(i);
    save[0] = '\0';
    for (v = av; *v; v++) {
        if (v > av)
            strlcat(save, " ", i);
        strlcat(save, *v, i);
    }

    return save;
}

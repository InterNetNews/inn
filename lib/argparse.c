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
    return nArgify(line, argvp, -1);
}


/*
**  Parse a string into a NULL-terminated array of at most n words;
**  return number of words.  If there are more than n words, stop
**  processing at the beginning of the (n+1)th word, store everything
**  from the beginning of word (n+1) in argv[n] and return (n+1).
**  If n is negative, parses all words.  If argvp isn't NULL, it and
**  what it points to will be freed.
*/
int
nArgify(char *line, char ***argvp, int n)
{
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
    *argvp = xmalloc((strlen(p) + 2) * sizeof(char *));

    return reArgify(p, *argvp, n, true);
}


/*
**  Destructively parse a string into a NULL-terminated array of at most
**  n words; return number of words.  Behaviour on negative n and strings
**  of more than n words matches that of nArgify (see above).  Caller
**  must supply an array of sufficient size (such as created by nArgify).
**
**  Note that the sequence
**     ac = nArgify(line, &argv, n1);
**     ac--;
**     ac += reArgify(argv[ac], &argv[ac], n2, true);
**  is equivalent to
**     ac = nArgify(line, &argv, n1 + n2);
**
**  It is sometimes useful not to strip spaces.  For instance
**  "AUTHINFO PASS  test" should be understood as giving the
**  password " test", beginning with a space.
*/
int
reArgify(char *p, char **argv, int n, bool stripspaces)
{
    char **save = argv;

    /* Should never happen unless caller modifies argv between calls
     * or stripspaces has previously been set to false. */
    if (stripspaces) {
        while (ISWHITE(*p))
            p++;
    }

    for ( ; *p; ) {
        if (n == 0) {
            *argv++ = p;
            break;
        }

        /* Decrement limit, mark start of this word, find its end. */
        for (n--, *argv++ = p; *p && !ISWHITE(*p); )
            p++;

        if (*p == '\0')
            break;

        /* Nip off word. */
        *p++ = '\0';
        
        /* Skip whitespace. */
        if (stripspaces) {
            while (ISWHITE(*p))
                p++;
        }
    }

    *argv = NULL;

    return argv - save;
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

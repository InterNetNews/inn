#include <stdio.h>
#include <sys/types.h>
#include <ctype.h>
#include "configdata.h"
#include "clibrary.h"
#include "libinn.h"
#include "macros.h"

void freeargify(char ***argvp) {
    if (**argvp != NULL) {
	DISPOSE(*argvp[0]);
	DISPOSE(*argvp);
	*argvp = NULL;
    }
}

/*
**  Parse a string into a NULL-terminated array of words; return number
**  of words.  If argvp isn't NULL, it and what it points to will be
**  DISPOSE'd.
*/
int argify(char *line, char ***argvp)
{
    char	        **argv;
    char	        *p;
    int	                i;

    if (*argvp != NULL)
	freeargify(argvp);

    /*  Copy the line, which we will split up. */
    while (ISWHITE(*line))
	line++;
    i = strlen(line);
    p = NEW(char, i + 1);
    (void)strcpy(p, line);

    /* Allocate worst-case amount of space. */
    for (*argvp = argv = NEW(char*, i + 2); *p; ) {
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
char *glom(char **av)
{
    char	        **v;
    char	        *p;
    int	                i;
    char		*save;

    /* Get space. */
    for (i = 0, v = av; *v; v++)
	i += strlen(*v) + 1;

    for (save = p = NEW(char, i + 1), v = av; *v; v++) {
	if (p > save)
	    *p++ = ' ';
	p += strlen(strcpy(p, *v));
    }

    return save;
}

#include <stdio.h>
#include <sys/types.h>
#include <ctype.h>
#include "configdata.h"
#include "clibrary.h"
#include "libinn.h"
#include "macros.h"

void freeargify(char ***argvp) {
    if (**argvp != NULL) {
	DISPOSE(*argvp[0]);
	DISPOSE(*argvp);
	*argvp = NULL;
    }
}

/*
**  Parse a string into a NULL-terminated array of words; return number
**  of words.  If argvp isn't NULL, it and what it points to will be
**  DISPOSE'd.
*/
int argify(char *line, char ***argvp)
{
    char	        **argv;
    char	        *p;
    int	                i;

    if (*argvp != NULL)
	freeargify(argvp);

    /*  Copy the line, which we will split up. */
    while (ISWHITE(*line))
	line++;
    i = strlen(line);
    p = NEW(char, i + 1);
    (void)strcpy(p, line);

    /* Allocate worst-case amount of space. */
    for (*argvp = argv = NEW(char*, i + 2); *p; ) {
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
char *glom(char **av)
{
    char	        **v;
    char	        *p;
    int	                i;
    char		*save;

    /* Get space. */
    for (i = 0, v = av; *v; v++)
	i += strlen(*v) + 1;

    for (save = p = NEW(char, i + 1), v = av; *v; v++) {
	if (p > save)
	    *p++ = ' ';
	p += strlen(strcpy(p, *v));
    }

    return save;
}

#include <stdio.h>
#include <sys/types.h>
#include <ctype.h>
#include "configdata.h"
#include "clibrary.h"
#include "libinn.h"
#include "macros.h"

void freeargify(char ***argvp) {
    if (**argvp != NULL) {
	DISPOSE(*argvp[0]);
	DISPOSE(*argvp);
	*argvp = NULL;
    }
}

/*
**  Parse a string into a NULL-terminated array of words; return number
**  of words.  If argvp isn't NULL, it and what it points to will be
**  DISPOSE'd.
*/
int argify(char *line, char ***argvp)
{
    char	        **argv;
    char	        *p;
    int	                i;

    if (*argvp != NULL)
	freeargify(argvp);

    /*  Copy the line, which we will split up. */
    while (ISWHITE(*line))
	line++;
    i = strlen(line);
    p = NEW(char, i + 1);
    (void)strcpy(p, line);

    /* Allocate worst-case amount of space. */
    for (*argvp = argv = NEW(char*, i + 2); *p; ) {
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
char *glom(char **av)
{
    char	        **v;
    char	        *p;
    int	                i;
    char		*save;

    /* Get space. */
    for (i = 0, v = av; *v; v++)
	i += strlen(*v) + 1;

    for (save = p = NEW(char, i + 1), v = av; *v; v++) {
	if (p > save)
	    *p++ = ' ';
	p += strlen(strcpy(p, *v));
    }

    return save;
}


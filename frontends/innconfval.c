/*  $Revision$
**
**  Get a config value from INN.
*/
#include <stdio.h>
#include <sys/types.h>
#include "configdata.h"
#include "clibrary.h"
#include "libinn.h"
#include "macros.h"
#include "paths.h"

/* Global and initialized; to work around SunOS -Bstatic bug, sigh. */
STATIC char		ConfigBuff[SMBUF] = "";
int		format = 0;

int isnum(char *v)
{
    for (; *v; v++)
	if (!isdigit(*v)) return(0);
    return(1);
}

char *upit(char *v)
{
    register int i;

    for (i=0; i<strlen(v); i++) v[i] = toupper(v[i]);
    return(v);
}

void
printit(char *v, char *val)
{
    switch (format) {
	case 0: printf("%s\n", val); break;
	case 1:   /* sh */
	    v = upit(v);
	    if (strchr(val, ' ') == NULL)
	    	printf("%s=%s; export %s;\n", v, val, v);
	    else
	    	printf("%s=\"%s\"; export %s;\n", v, val, v);
	    break;
	case 2:   /* csh */
	    if (strchr(val, ' ') == NULL)
	    	printf("set inn_%s = %s\n", v, val);
	    else
	    	printf("set inn_%s = \"%s\"\n", v, val);
	    break;
	case 3:   /* perl */
	    if (isnum(val))
	    	printf("$%s = %s;\n", v, val);
	    else
	    	printf("$%s = \'%s\';\n", v, val);
	    break;
	case 4:   /* tcl */
	    if (isnum(val))
	    	printf("set inn_%s %s\n", v, val);
	    else
	    	printf("set inn_%s \"%s\"\n", v, val);
	    break;
    }
}

void
wholeconfig()
{
    FILE	        *F;
    int	                i;
    char	        *p;
    char	        c;

    /* Read the config file. */
    if ((F = fopen(innconffile, "r")) != NULL) {
	while (fgets(ConfigBuff, sizeof ConfigBuff, F) != NULL) {
	    if ((p = strchr(ConfigBuff, '\n')) != NULL)
		*p = '\0';
	    if (ConfigBuff[0] == '\0' || ConfigBuff[0] == COMMENT_CHAR)
		continue;
	    p = strchr(ConfigBuff, ':');
	    if (*p == ':') {
		*p++ = '\0';
		for (; ISWHITE(*p); p++)
		    continue;
		printit(ConfigBuff, p);
	    }
	}
	(void)fclose(F);
    }
    exit(0);
}

int
main(ac, av)
    int			ac;
    char		*av[];
{
    register char	*p;
    register char	*val;
    register BOOL	File;
    register int	i;

    /* Parse JCL. */
    File = FALSE;
    while ((i = getopt(ac, av, "fcpsti:")) != EOF)
	switch (i) {
	default:
	    (void)fprintf(stderr, "Usage error.\n");
	    exit(1);
	    /* NOTREACHED */
	case 'f':
	    File = TRUE;
	    break;
	case 's':
	    format = 1;
	    break;
	case 'c':
	    format = 2;
	    break;
	case 'p':
	    format = 3;
	    break;
	case 't':
	    format = 4;
	    break;
	case 'i':
	    innconffile = optarg;
	    break;
	}
    ac -= optind;
    av += optind;

    if (!*av) wholeconfig();   /* Doesn't return */

    /* Loop over parameters, each a config value. */
    while ((p = *av++) != NULL) {
	val = File ? GetFileConfigValue(p) : GetConfigValue(p);
	if (val != NULL)
	    printit(p, val);
    }

    exit(0);
    /* NOTREACHED */
}

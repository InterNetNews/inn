/*  $Id$
**
**  Get a config value from INN.
*/

#include "config.h"
#include "clibrary.h"
#include <ctype.h>
#include <syslog.h>

#include "libinn.h"
#include "macros.h"
#include "paths.h"

/* Global and initialized; to work around SunOS -Bstatic bug, sigh. */
static char	ConfigBuff[SMBUF] = "";
int		format = 0;
static bool	version = FALSE;

static int isnum(const char *v)
{
    if (!*v) return(0);
    for (; *v; v++)
	if (!isdigit(*v)) return(0);
    return(1);
}

static void upit(char *v)
{
    for ( ; *v; v++)
        *v = toupper(*v);
}

static void
printit(char *v, const char *val)
{
    switch (format) {
	case 0: printf("%s\n", val); break;
	case 1:   /* sh */
	    upit(v);
	    if ((strchr(val, ' ') == NULL) && *val)
	    	printf("%s=%s; export %s;\n", v, val, v);
	    else
	    	printf("%s=\"%s\"; export %s;\n", v, val, v);
	    break;
	case 2:   /* csh */
	    if ((strchr(val, ' ') == NULL) && *val)
	    	printf("set inn_%s = %s\n", v, val);
	    else
	    	printf("set inn_%s = \"%s\"\n", v, val);
	    break;
	case 3:   /* perl */
	    if (isnum(val))
	    	printf("$%s = %s;\n", v, val);
	    else {
	    	printf("$%s = '", v);
		while (*val) {
			if ((*val == '\'') || (*val == '\\')) printf("\\");
			printf("%c", *val++);
		    }
	    	printf("';\n");
	    }
	    break;
	case 4:   /* tcl */
	    if (isnum(val))
	    	printf("set inn_%s %s\n", v, val);
	    else {
                int i;
                static const char* unsafe_chars = "$[]{}\"\\";

                printf("set inn_%s \"", v);
                for (i = 0; val[i] != '\0'; i++) {
                    if (strchr (unsafe_chars, val[i]) != NULL)
                        putchar('\\');
                    putchar(val[i]);
                }
	    	printf("\"\n");
            }
	    break;
    }
}

static void
wholeconfig(void)
{
    FILE	        *F;
    char	        *p;

    /* Read the config file. */
    if ((F = fopen(innconffile, "r")) != NULL) {
	while (fgets(ConfigBuff, sizeof ConfigBuff, F) != NULL) {
	    if ((p = strchr(ConfigBuff, '\n')) != NULL)
		*p = '\0';
	    if (ConfigBuff[0] == '\0' || ConfigBuff[0] == COMMENT_CHAR)
		continue;
	    p = strchr(ConfigBuff, ':');
	    if (p != NULL && *p == ':') {
		*p++ = '\0';
		for (; ISWHITE(*p); p++)
		    continue;
		printit(ConfigBuff, p);
	    }
	}
	(void)fclose(F);
    }
    printit(COPY("version"), inn_version_string);
    exit(0);
}

int
main(int ac, char **av)
{
    char	*p;
    char	*val;
    bool	File;
    int	i;

    /* First thing, set up logging and our identity. */
    openlog("innconfval", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);     

    /* Parse JCL. */
    File = FALSE;
    while ((i = getopt(ac, av, "fcpsti:v")) != EOF)
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
	case 'v':
	    version = TRUE;
	    break;
	}
    ac -= optind;
    av += optind;

    if (version) {
	printit(COPY("version"), inn_version_string);
	exit(0);
    }
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

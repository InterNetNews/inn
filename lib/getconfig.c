/*  $Revision$
**
*/
#include <stdio.h>
#include <sys/types.h>
#include "configdata.h"
#include "clibrary.h"
#include "paths.h"
#include "libinn.h"
#include "macros.h"


/* Global and initialized; to work around SunOS -Bstatic bug, sigh. */
STATIC char		ConfigBuff[SMBUF] = "";


char *GetFileConfigValue(char *value)
{
    FILE	        *F;
    int	                i;
    char	        *p;
    char	        c;

    /* Read the config file. */
    if ((F = fopen(_PATH_CONFIG, "r")) != NULL) {
	c = *value;
	i = strlen(value);
	while (fgets(ConfigBuff, sizeof ConfigBuff, F) != NULL) {
	    if ((p = strchr(ConfigBuff, '\n')) != NULL)
		*p = '\0';
	    if (ConfigBuff[0] == '\0' || ConfigBuff[0] == COMMENT_CHAR)
		continue;
	    if (ConfigBuff[0] == c
	     && ConfigBuff[i] == ':'
	     && EQn(ConfigBuff, value, i)) {
		(void)fclose(F);
		for (p = &ConfigBuff[i + 1]; ISWHITE(*p); p++)
		    continue;
		return p;
	    }
	}
	(void)fclose(F);
    }
    return NULL;
}


/*
**  Get a configuration parameter, usually from reading the file.
*/
char *GetConfigValue(char *value)
{
    char	        *p;

    /* Some environment variables override the file. */
    if (EQ(value, _CONF_SERVER)
     && (p = getenv(_ENV_NNTPSERVER)) != NULL)
	return p;
    if (EQ(value, _CONF_ORGANIZATION)
     && (p = getenv(_ENV_ORGANIZATION)) != NULL)
	return p;
    if (EQ(value, _CONF_FROMHOST)
     && (p = getenv(_ENV_FROMHOST)) != NULL)
	return p;

    if ((p = GetFileConfigValue(value)) != NULL)
	return p;

    /* Some values have defaults if not in the file. */
    if (EQ(value, _CONF_FROMHOST) || EQ(value, _CONF_PATHHOST))
	return GetFQDN();
    if (EQ(value, _CONF_CONTENTTYPE))
	return "text/plain; charset=US-ASCII";
    if (EQ(value, _CONF_ENCODING))
	return "7bit";
    return NULL;
}

/*
**  Get a boolean config value and return it by value
*/
BOOL GetBooleanConfigValue(char *key, BOOL defaultvalue) {
    char *value;

    if ((value = GetConfigValue(key)) == NULL)
	return defaultvalue;

    if (caseEQ(value, "on") || caseEQ(value, "true") || caseEQ(value, "yes"))
	return TRUE;
    if (caseEQ(value, "off") || caseEQ(value, "false") || caseEQ(value, "no"))
	return FALSE;
    return defaultvalue;
}

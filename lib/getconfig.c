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

/*
  To add a new config value, add it to the following:
	include/paths.h		:	Add #define _CONF_VARNAME "varname"
	include/libinn.h	:	Add varname to conf_vars struct
	lib/getconfig.c		:	SetDefaults() & ReadConfig()
	samples/inn.conf	:	Set the default value
	doc/inn.conf.5		:	Document it!
	wherever you need it	:	Use as innconf->varname
*/

struct	conf_vars *innconf = NULL;

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

void SetDefaults()
{
    innconf->fromhost = NULL;
    innconf->server = NULL;
    innconf->pathhost = NULL;
    innconf->organization = NULL;
    innconf->moderatormailer = NULL;
    innconf->domain = NULL;
    innconf->mimeversion = NULL;
    innconf->mimecontenttype = NULL;
    innconf->mimeencoding = NULL;
    innconf->hiscachesize = 0;
    innconf->wireformat = FALSE;
    innconf->xrefslave = FALSE;
    innconf->timespool = FALSE;
    innconf->complaints = NULL;
    innconf->spoolfirst = FALSE;
    innconf->writelinks = TRUE;
    innconf->timer = 0;
    innconf->status = 0;
    innconf->storageapi = FALSE;
    innconf->articlemmap = FALSE;
    innconf->overviewmmap = TRUE;
    innconf->mta = _PATH_SENDMAIL;
    innconf->checkincludedtext = FALSE;
    innconf->maxforks = MAX_FORKS;
    innconf->maxartsize = 1000000L;
    innconf->nicekids = 4;
    innconf->verifycancels = FALSE;
    innconf->logcancelcomm = FALSE;
    innconf->wanttrash = FALSE;
    innconf->remembertrash = TRUE;
    innconf->linecountfuzz = 0;
    innconf->peertimeout = 1 * 60 * 60;
    innconf->clienttimeout = 10 * 60;
    innconf->allowreaders = TRUE;
    innconf->allownewnews = TRUE;
    innconf->localmaxartsize = 1000000L;
    innconf->logartsize = TRUE;
    innconf->logipaddr = TRUE;
    innconf->chaninacttime = 600;
    innconf->maxconnections = 50;
    innconf->chanretrytime = 300;
    innconf->artcutoff = 14 * 24 * 60 * 60;
    innconf->pauseretrytime = 300;
    innconf->nntplinklog = FALSE;
    innconf->nntpactsync = 200;
    innconf->badiocount = 5;
    innconf->blockbackoff = 120;
    innconf->icdsynccount = 10;
}

void ClearInnConf()
{
    if (innconf->fromhost != NULL) DISPOSE(innconf->fromhost);
    if (innconf->server != NULL) DISPOSE(innconf->server);
    if (innconf->pathhost != NULL) DISPOSE(innconf->pathhost);
    if (innconf->organization != NULL) DISPOSE(innconf->organization);
    if (innconf->moderatormailer != NULL) DISPOSE(innconf->moderatormailer);
    if (innconf->domain != NULL) DISPOSE(innconf->domain);
    if (innconf->mimeversion != NULL) DISPOSE(innconf->mimeversion);
    if (innconf->mimecontenttype != NULL) DISPOSE(innconf->mimecontenttype);
    if (innconf->mimeencoding != NULL) DISPOSE(innconf->mimeencoding);
    if (innconf->complaints != NULL) DISPOSE(innconf->complaints);
    if (innconf->mta != NULL) DISPOSE(innconf->mta);
}

int ReadInnConf(char *configfile)
{
    FILE	        *F;
    int	                i;
    char	        *p;
    char	        c;
    int			boolval;

    if (innconf != NULL) {
	ClearInnConf();
	DISPOSE(innconf);
    }
    innconf = NEW(struct conf_vars, 1);
    if (innconf == NULL)
	return(-2);
    SetDefaults();
    /* Read the config file. */
    if ((F = fopen(configfile, "r")) != NULL) {
	while (fgets(ConfigBuff, sizeof ConfigBuff, F) != NULL) {
	    if ((p = strchr(ConfigBuff, '\n')) != NULL)
		*p = '\0';
	    if (ConfigBuff[0] == '\0' || ConfigBuff[0] == COMMENT_CHAR)
		continue;
	    if ((p = strchr(ConfigBuff, ':')) != NULL)
		*p++ = '\0';
	    else
		continue;
	    for ( ; ISWHITE(*p); p++)
		continue;
	    boolval = -1;
	    if (caseEQ(p, "on") || caseEQ(p, "true") || caseEQ(p, "yes"))
		boolval = TRUE;
	    if (caseEQ(p, "off") || caseEQ(p, "false") || caseEQ(p, "no"))
		boolval = FALSE;
	    if (EQ(ConfigBuff,_CONF_FROMHOST)) {
		innconf->fromhost = COPY(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_SERVER)) {
		innconf->server = COPY(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_PATHHOST)) {
		innconf->pathhost = COPY(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_ORGANIZATION)) {
		innconf->organization = COPY(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_MODMAILER)) {
		innconf->moderatormailer = COPY(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_DOMAIN)) {
		innconf->domain = COPY(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_MIMEVERSION)) {
		innconf->mimeversion = COPY(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_CONTENTTYPE)) {
		innconf->mimecontenttype = COPY(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_ENCODING)) {
		innconf->mimeencoding = COPY(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_HISCACHESIZE)) {
		innconf->hiscachesize = atoi(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_WIREFORMAT)) {
		if (boolval != -1) innconf->wireformat = boolval;
	    } else
	    if (EQ (ConfigBuff,_CONF_XREFSLAVE)) {
		if (boolval != -1) innconf->xrefslave = boolval;
	    } else
	    if (EQ(ConfigBuff,_CONF_TIMESPOOL)) {
		if (boolval != -1) innconf->timespool = boolval;
	    } else
	    if (EQ(ConfigBuff,_CONF_COMPLAINTS)) {
		innconf->complaints = COPY(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_NNRP_SPOOLFIRST)) {
		if (boolval != -1) innconf->spoolfirst = boolval;
	    } else
	    if (EQ(ConfigBuff,_CONF_WRITELINKS)) {
		if (boolval != -1) innconf->writelinks = boolval;
	    } else
	    if (EQ(ConfigBuff,_CONF_TIMER)) {
		innconf->timer = atoi(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_STATUS)) {
		innconf->status = atoi(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_STORAGEAPI)) {
		if (boolval != -1) innconf->storageapi = boolval;
	    } else
	    if (EQ(ConfigBuff,_CONF_ARTMMAP)) {
		if (boolval != -1) innconf->articlemmap = boolval;
	    } else
	    if (EQ(ConfigBuff,_CONF_OVERMMAP)) {
		if (boolval != -1) innconf->overviewmmap = boolval;
	    } else
	    if (EQ(ConfigBuff,_CONF_MTA)) {
		innconf->mta = COPY(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_CHECK_INC_TEXT)) {
		if (boolval != -1) innconf->checkincludedtext = boolval;
	    } else
	    if (EQ(ConfigBuff,_CONF_MAX_FORKS)) {
		innconf->maxforks = atoi(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_MAX_ART_SIZE)) {
		innconf->maxartsize = atol(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_NICE_KIDS)) {
		innconf->nicekids = atoi(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_VERIFY_CANCELS)) {
		if (boolval != -1) innconf->verifycancels = boolval;
	    } else
	    if (EQ(ConfigBuff,_CONF_LOG_CANCEL_COMM)) {
		if (boolval != -1) innconf->logcancelcomm = boolval;
	    } else
	    if (EQ(ConfigBuff,_CONF_WANT_TRASH)) {
		if (boolval != -1) innconf->wanttrash = boolval;
	    } else
	    if (EQ(ConfigBuff,_CONF_REMEMBER_TRASH)) {
		if (boolval != -1) innconf->remembertrash = boolval;
	    } else
	    if (EQ(ConfigBuff,_CONF_LINECOUNT_FUZZ)) {
		innconf->linecountfuzz = atoi(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_PEER_TIMEOUT)) {
		innconf->peertimeout = atoi(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_CLIENT_TIMEOUT)) {
		innconf->clienttimeout = atoi(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_ALLOW_READERS)) {
		if (boolval != -1) innconf->allowreaders = boolval;
	    } else
	    if (EQ(ConfigBuff,_CONF_ALLOW_NEWNEWS)) {
		if (boolval != -1) innconf->allownewnews = boolval;
	    } else
	    if (EQ(ConfigBuff,_CONF_LOCAL_MAX_ARTSIZE)) {
		innconf->localmaxartsize = atoi(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_LOG_ARTSIZE)) {
		if (boolval != -1) innconf->logartsize = boolval;
	    } else
	    if (EQ(ConfigBuff,_CONF_LOG_IPADDR)) {
		if (boolval != -1) innconf->logipaddr = boolval;
	    } else
	    if (EQ(ConfigBuff,_CONF_CHAN_INACT_TIME)) {
		innconf->chaninacttime = atoi(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_MAX_CONNECTIONS)) {
		innconf->maxconnections = atoi(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_CHAN_RETRY_TIME)) {
		innconf->chanretrytime = atoi(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_ART_CUTOFF)) {
		innconf->artcutoff = atoi(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_PAUSE_RETRY_TIME)) {
		innconf->pauseretrytime = atoi(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_NNTPLINK_LOG)) {
		if (boolval != -1) innconf->nntplinklog = boolval;
	    } else
	    if (EQ(ConfigBuff,_CONF_NNTP_ACT_SYNC)) {
		innconf->nntpactsync = atoi(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_BAD_IO_COUNT)) {
		innconf->badiocount = atoi(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_BLOCK_BACKOFF)) {
		innconf->blockbackoff = atoi(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_ICD_SYNC_COUNT)) {
		innconf->icdsynccount = atoi(p);
	    }
	}
	(void)fclose(F);
    } else return(-1);
    return(0);
}

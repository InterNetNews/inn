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
#include "logging.h"

/* Global and initialized; to work around SunOS -Bstatic bug, sigh. */
STATIC char		ConfigBuff[SMBUF] = "";
/*
  To add a new config value, add it to the following:
	Use the comment embedded method in include/libinn.h, then
	run developconfig.sh, which splits items out to the other
	locations.
  OR:
	include/paths.h		:	Add #define _CONF_VARNAME "varname"
	include/libinn.h	:	Add varname to conf_vars struct
	lib/getconfig.c		:	SetDefaults() & ReadConfig(), ClearInnConf()
	samples/inn.conf	:	Set the default value
	doc/inn.conf.5		:	Document it!
	wherever you need it	:	Use as innconf->varname
*/

struct conf_vars	*innconf = NULL;
char			*innconffile = _PATH_CONFIG;
char			pathbuff[SMBUF];

char *cpcatpath(p, f)
    char *p;
    char *f;
{
    if (strchr(f, '/') != NULL) {
	return(f);
    } else {
	strcpy(pathbuff, p);
	strcat(pathbuff, "/");
	strcat(pathbuff, f);
    }
    return(pathbuff);
}

char *GetFileConfigValue(char *value)
{
    FILE	        *F;
    int	                i;
    char	        *p;
    char	        c;

    /* Read the config file. */
    if ((F = fopen(innconffile, "r")) != NULL) {
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
/* BEGIN_AUTO_INSERTED_SECTION from ../include/libinn.h ||GETVALUE */
if (EQ(value,"fromhost")) { return innconf->fromhost; }
/* END_AUTO_INSERTED_SECTION from ../include/libinn.h ||GETVALUE */
    if (EQ(value, _CONF_INNBINDADDR)
     && (p = getenv(_ENV_INNBINDADDR)) != NULL)
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
char *p;	/* Temporary working variable */
/* BEGIN_AUTO_INSERTED_SECTION from ../include/libinn.h ||DEFAULT */
innconf->fromhost = NULL;
if ((p = getenv(_ENV_FROMHOST)) != NULL) { innconf->fromhost = COPY(p); }
/* END_AUTO_INSERTED_SECTION from ../include/libinn.h ||DEFAULT */
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
    innconf->complaints = NULL;
    innconf->spoolfirst = FALSE;
    innconf->writelinks = TRUE;
    innconf->timer = 0;
    innconf->status = 0;
    innconf->storageapi = FALSE;
    innconf->articlemmap = FALSE;
    innconf->overviewmmap = TRUE;
    innconf->mta = NULL;
    innconf->mailcmd = NULL;
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
    innconf->bindaddress = NULL;
    innconf->port = NNTP_PORT;
    innconf->readertrack = FALSE;
    innconf->strippostcc = FALSE;
    innconf->overviewname = NULL;

    innconf->pathnews = NULL;
    innconf->pathbin = NULL;
    innconf->pathfilter = NULL;
    innconf->pathcontrol = NULL;
    innconf->pathdb = NULL;
    innconf->pathetc = NULL;
    innconf->pathrun = NULL;
    innconf->pathlog = NULL;
    innconf->pathspool = NULL;
    innconf->patharticles = NULL;
    innconf->pathoverview = NULL;
    innconf->pathoutgoing = NULL;
    innconf->pathincoming = NULL;
    innconf->patharchive = NULL;
}

void ClearInnConf()
{
/* BEGIN_AUTO_INSERTED_SECTION from ../include/libinn.h ||CLEAR */
if (innconf->fromhost != NULL) DISPOSE(innconf->fromhost);
/* END_AUTO_INSERTED_SECTION from ../include/libinn.h ||CLEAR */
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
    if (innconf->mailcmd != NULL) DISPOSE(innconf->mailcmd);
    if (innconf->bindaddress != NULL) DISPOSE(innconf->bindaddress);
    if (innconf->overviewname != NULL) DISPOSE(innconf->bindaddress);

    if (innconf->pathnews != NULL) DISPOSE(innconf->pathnews);
    if (innconf->pathbin != NULL) DISPOSE(innconf->pathbin);
    if (innconf->pathfilter != NULL) DISPOSE(innconf->pathfilter);
    if (innconf->pathcontrol != NULL) DISPOSE(innconf->pathcontrol);
    if (innconf->pathdb != NULL) DISPOSE(innconf->pathdb);
    if (innconf->pathetc != NULL) DISPOSE(innconf->pathetc);
    if (innconf->pathrun != NULL) DISPOSE(innconf->pathrun);
    if (innconf->pathlog != NULL) DISPOSE(innconf->pathlog);
    if (innconf->pathspool != NULL) DISPOSE(innconf->pathspool);
    if (innconf->patharticles != NULL) DISPOSE(innconf->patharticles);
    if (innconf->pathoverview != NULL) DISPOSE(innconf->pathoverview);
    if (innconf->pathoutgoing != NULL) DISPOSE(innconf->pathoutgoing);
    if (innconf->pathincoming != NULL) DISPOSE(innconf->pathincoming);
    if (innconf->patharchive != NULL) DISPOSE(innconf->patharchive);
}

/*
   Make sure some compulsory inn.conf values are set and set them
   to defaults if possible 
*/
int CheckInnConf()
{
    if (innconf->mta == NULL) {
	syslog(L_FATAL, "Must set 'mta' in inn.conf");
	(void)fprintf(stderr, "Must set 'mta' in inn.conf");
	return(-1);
    }
    if (innconf->mailcmd == NULL)
	innconf->mailcmd = innconf->mta;
    if (innconf->overviewname == NULL) 
	innconf->overviewname = COPY(".overview");

    if (innconf->pathnews == NULL) {
	syslog(L_FATAL, "Must set 'pathnews' in inn.conf");
	(void)fprintf(stderr, "Must set 'pathnews' in inn.conf");
	return(-1);
    }
    if (innconf->pathbin == NULL) {
	innconf->pathbin = COPY(cpcatpath(innconf->pathnews, "bin"));
    }
    if (innconf->pathfilter == NULL) {
	innconf->pathfilter = COPY(cpcatpath(innconf->pathnews, "filter"));
    }
    if (innconf->pathcontrol == NULL) {
	innconf->pathcontrol = COPY(cpcatpath(innconf->pathnews, "control"));
    }
    if (innconf->pathdb == NULL) {
	innconf->pathdb = COPY(cpcatpath(innconf->pathnews, "db"));
    }
    if (innconf->pathetc == NULL) {
	innconf->pathetc = COPY(cpcatpath(innconf->pathnews, "etc"));
    }
    if (innconf->pathrun == NULL) {
	innconf->pathrun = COPY(cpcatpath(innconf->pathnews, "run"));
    }
    if (innconf->pathlog == NULL) {
	innconf->pathlog = COPY(cpcatpath(innconf->pathnews, "log"));
    }
    if (innconf->pathspool == NULL) {
	innconf->pathspool = COPY(cpcatpath(innconf->pathnews, "spool"));
    }
    if (innconf->patharticles == NULL) {
	innconf->patharticles = COPY(cpcatpath(innconf->pathspool, "articles"));
    }
    if (innconf->pathoverview == NULL) {
	innconf->pathoverview = COPY(cpcatpath(innconf->pathspool, "overview"));
    }
    if (innconf->pathoutgoing == NULL) {
	innconf->pathoutgoing = COPY(cpcatpath(innconf->pathspool, "outgoing"));
    }
    if (innconf->pathincoming == NULL) {
	innconf->pathincoming = COPY(cpcatpath(innconf->pathspool, "incoming"));
    }
    if (innconf->patharchive == NULL) {
	innconf->patharchive = COPY(cpcatpath(innconf->pathspool, "archive"));
    }
    return(0);
}

int ReadInnConf()
{
    FILE	        *F;
    char	        *p;
    int			boolval;

    if (innconf != NULL) {
	ClearInnConf();
	DISPOSE(innconf);
    }
    innconf = NEW(struct conf_vars, 1);
    if (innconf == NULL) {
	syslog(L_FATAL, "Cannot malloc for innconf");
        (void)fprintf(stderr, "Cannot malloc for innconf\n");
	return(-1);
    }
    SetDefaults();
    syslog(L_NOTICE, "Reading config from %s", innconffile);
    /* Read the config file. */
    if ((F = fopen(innconffile, "r")) != NULL) {
	while (fgets(ConfigBuff, sizeof ConfigBuff, F) != NULL) {
	    if ((p = strchr(ConfigBuff, '\n')) != NULL)
		*p = '\0';
	    else
		ConfigBuff[sizeof(ConfigBuff)-1] = '\0';
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
/* BEGIN_AUTO_INSERTED_SECTION from ../include/libinn.h ||READ */
/*  Special: For read, must not overwrite the ENV_FROMHOST set by DEFAULT */
if (EQ(ConfigBuff,"fromhost")) {
if (innconf->fromhost == NULL) { innconf->fromhost = COPY(p); }
} else
/* END_AUTO_INSERTED_SECTION from ../include/libinn.h ||READ */
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
		if (innconf->storageapi == TRUE) innconf->wireformat = TRUE;
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
	    if (EQ(ConfigBuff,_CONF_MAILCMD)) {
		innconf->mailcmd = COPY(p);
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
		innconf->artcutoff = atoi(p) * 24 * 60 * 60;
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
	    } else
	    if (EQ(ConfigBuff,_CONF_INNBINDADDR)) {
		if (EQ(p,"all") || EQ(p,"any"))
		    innconf->bindaddress =  NULL;
		else
		    innconf->bindaddress =  COPY(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_INNPORT)) {
		innconf->port = atoi(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_READERTRACK)) {
		if (boolval != -1) innconf->readertrack = boolval;
	    } else
	    if (EQ(ConfigBuff,_CONF_STRIPPOSTCC)) {
		if (boolval != -1) innconf->strippostcc = boolval;
	    } else
	    if (EQ(ConfigBuff,_CONF_OVERVIEWNAME)) {
		    innconf->overviewname =  COPY(p);

	    } else
	    if (EQ(ConfigBuff,_CONF_PATHNEWS)) {
		    innconf->pathnews =  COPY(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_PATHBIN)) {
		    innconf->pathbin =  COPY(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_PATHFILTER)) {
		    innconf->pathfilter =  COPY(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_PATHCONTROL)) {
		    innconf->pathcontrol =  COPY(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_PATHDB)) {
		    innconf->pathdb =  COPY(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_PATHETC)) {
		    innconf->pathetc =  COPY(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_PATHRUN)) {
		    innconf->pathrun =  COPY(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_PATHLOG)) {
		    innconf->pathlog =  COPY(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_PATHSPOOL)) {
		    innconf->pathspool =  COPY(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_PATHARTICLES)) {
		    innconf->patharticles =  COPY(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_PATHOVERVIEW)) {
		    innconf->pathoverview =  COPY(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_PATHOUTGOING)) {
		    innconf->pathoutgoing =  COPY(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_PATHINCOMING)) {
		    innconf->pathincoming =  COPY(p);
	    } else
	    if (EQ(ConfigBuff,_CONF_PATHARCHIVE)) {
		    innconf->patharchive =  COPY(p);
	    }
	}
	(void)fclose(F);
    } else {
	syslog(L_FATAL, "Cannot open %s", _PATH_CONFIG);
	(void)fprintf(stderr, "Cannot open %s\n", _PATH_CONFIG);
	return(-1);
    }
    return(CheckInnConf());
}

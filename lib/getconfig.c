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
#include <syslog.h> 

/* Global and initialized; to work around SunOS -Bstatic bug, sigh. */
STATIC char		ConfigBuff[SMBUF] = "";
STATIC char		*ConfigBit;
STATIC int		ConfigBitsize;
#define	TEST_CONFIG(a, b) \
    { \
	int byte, offset; \
	offset = a % 8; \
	byte = (a - offset) / 8; \
	b = ((ConfigBit[byte] & (1 << offset)) != 0) ? TRUE : FALSE; \
    }
#define	SET_CONFIG(a) \
    { \
	int byte, offset; \
	offset = a % 8; \
	byte = (a - offset) / 8; \
	ConfigBit[byte] |= (1 << offset); \
    }
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
    if ((F = Fopen(innconffile, "r", TEMPORARYOPEN)) != NULL) {
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
		(void)Fclose(F);
		for (p = &ConfigBuff[i + 1]; ISWHITE(*p); p++)
		    continue;
		return p;
	    }
	}
	(void)Fclose(F);
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
    if (ConfigBit == NULL) {
	if (MAX_CONF_VAR % 8 == 0)
	    ConfigBitsize = MAX_CONF_VAR/8;
	else
	    ConfigBitsize = (MAX_CONF_VAR - (MAX_CONF_VAR % 8))/8 + 1;
	ConfigBit = NEW(char, ConfigBitsize);
	memset(ConfigBit, '\0', ConfigBitsize);
    }
    innconf->fromhost = NULL;
    if ((p = getenv(_ENV_FROMHOST)) != NULL) { innconf->fromhost = COPY(p); }
    innconf->server = NULL;
    innconf->pathhost = NULL;
    innconf->pathalias = NULL;
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
    innconf->allowreaders = FALSE;
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
    innconf->keywords = FALSE;		
    innconf->keylimit = 512 ;		
    innconf->keyartlimit = 100000;
    innconf->keymaxwords = 250;
    innconf->nnrpdposthost = NULL;

    innconf->pathnews = NULL;
    innconf->pathbin = NULL;
    innconf->pathfilter = NULL;
    innconf->pathcontrol = NULL;
    innconf->pathdb = NULL;
    innconf->pathetc = NULL;
    innconf->pathrun = NULL;
    innconf->pathlog = NULL;
    innconf->pathhttp = NULL;
    innconf->pathspool = NULL;
    innconf->patharticles = NULL;
    innconf->pathoverview = NULL;
    innconf->pathoutgoing = NULL;
    innconf->pathincoming = NULL;
    innconf->patharchive = NULL;

    innconf->logsitename = TRUE;
    innconf->extendeddbz = FALSE;
    innconf->nnrpdoverstats = FALSE;
    innconf->decnetdomain = NULL;
    innconf->backoff_auth = FALSE;
    innconf->backoff_db = NULL;
    innconf->backoff_k = 1L;
    innconf->backoff_postfast = 0L;
    innconf->backoff_postslow = 1L;
    innconf->backoff_trigger = 10000L;
}

void ClearInnConf()
{
    if (innconf->fromhost != NULL) DISPOSE(innconf->fromhost);
    if (innconf->server != NULL) DISPOSE(innconf->server);
    if (innconf->pathhost != NULL) DISPOSE(innconf->pathhost);
    if (innconf->pathalias != NULL) DISPOSE(innconf->pathalias);
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
    if (innconf->nnrpdposthost != NULL) DISPOSE(innconf->nnrpdposthost);

    if (innconf->pathnews != NULL) DISPOSE(innconf->pathnews);
    if (innconf->pathbin != NULL) DISPOSE(innconf->pathbin);
    if (innconf->pathfilter != NULL) DISPOSE(innconf->pathfilter);
    if (innconf->pathcontrol != NULL) DISPOSE(innconf->pathcontrol);
    if (innconf->pathdb != NULL) DISPOSE(innconf->pathdb);
    if (innconf->pathetc != NULL) DISPOSE(innconf->pathetc);
    if (innconf->pathrun != NULL) DISPOSE(innconf->pathrun);
    if (innconf->pathlog != NULL) DISPOSE(innconf->pathlog);
    if (innconf->pathhttp != NULL) DISPOSE(innconf->pathhttp);
    if (innconf->pathspool != NULL) DISPOSE(innconf->pathspool);
    if (innconf->patharticles != NULL) DISPOSE(innconf->patharticles);
    if (innconf->pathoverview != NULL) DISPOSE(innconf->pathoverview);
    if (innconf->pathoutgoing != NULL) DISPOSE(innconf->pathoutgoing);
    if (innconf->pathincoming != NULL) DISPOSE(innconf->pathincoming);
    if (innconf->patharchive != NULL) DISPOSE(innconf->patharchive);
    if (innconf->decnetdomain != NULL) DISPOSE(innconf->decnetdomain);
    if (innconf->backoff_db != NULL) DISPOSE(innconf->backoff_db);
    memset(ConfigBit, '\0', ConfigBitsize);
}

/*
   Make sure some compulsory inn.conf values are set and set them
   to defaults if possible 
*/
int CheckInnConf()
{
    if (GetFQDN() == NULL) {
	syslog(L_FATAL, "Must set 'domain' in inn.conf");
	(void)fprintf(stderr, "Must set 'domain' in inn.conf");
	return(-1);
    }
    if (innconf->fromhost == NULL) {
	innconf->fromhost = COPY(GetFQDN());
    }
    if (innconf->pathhost == NULL && ((innconf->pathhost = COPY(GetFQDN())) == NULL)) {
	syslog(L_FATAL, "Must set 'pathhost' in inn.conf");
	(void)fprintf(stderr, "Must set 'pathhost' in inn.conf");
	return(-1);
    }
    if (innconf->mta == NULL) {
	syslog(L_FATAL, "Must set 'mta' in inn.conf");
	(void)fprintf(stderr, "Must set 'mta' in inn.conf");
	return(-1);
    }
    if (innconf->mailcmd == NULL)
	innconf->mailcmd = innconf->mta;
    if (innconf->overviewname == NULL) 
	innconf->overviewname = COPY(".overview");

    if (innconf->storageapi != TRUE)
	innconf->extendeddbz = FALSE;

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
    if (innconf->pathhttp == NULL) {
	innconf->pathhttp = COPY(innconf->pathlog);
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
    BOOL		bit;

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
    if ((F = Fopen(innconffile, "r", TEMPORARYOPEN)) != NULL) {
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
	    if (EQ(ConfigBuff,"fromhost")) {
		TEST_CONFIG(CONF_VAR_FROMHOST, bit);
		if (!bit) {
		    if (innconf->fromhost != NULL) DISPOSE(innconf->fromhost);
		    innconf->fromhost = COPY(p);
		}
		SET_CONFIG(CONF_VAR_FROMHOST);
	    } else
	    if (EQ(ConfigBuff,_CONF_SERVER)) {
		TEST_CONFIG(CONF_VAR_SERVER, bit);
		if (!bit) innconf->server = COPY(p);
		SET_CONFIG(CONF_VAR_SERVER);
	    } else
	    if (EQ(ConfigBuff,_CONF_PATHHOST)) {
		TEST_CONFIG(CONF_VAR_PATHHOST, bit);
		if (!bit) innconf->pathhost = COPY(p);
		SET_CONFIG(CONF_VAR_PATHHOST);
	    } else
	    if (EQ(ConfigBuff,_CONF_PATHALIAS)) {
		TEST_CONFIG(CONF_VAR_PATHALIAS, bit);
		if (!bit) innconf->pathalias = COPY(p);
		SET_CONFIG(CONF_VAR_PATHALIAS);
	    } else
	    if (EQ(ConfigBuff,_CONF_ORGANIZATION)) {
		TEST_CONFIG(CONF_VAR_ORGANIZATION, bit);
		if (!bit) innconf->organization = COPY(p);
		SET_CONFIG(CONF_VAR_ORGANIZATION);
	    } else
	    if (EQ(ConfigBuff,_CONF_MODMAILER)) {
		TEST_CONFIG(CONF_VAR_MODERATORMAILER, bit);
		if (!bit) innconf->moderatormailer = COPY(p);
		SET_CONFIG(CONF_VAR_MODERATORMAILER);
	    } else
	    if (EQ(ConfigBuff,_CONF_DOMAIN)) {
		TEST_CONFIG(CONF_VAR_DOMAIN, bit);
		if (!bit) innconf->domain = COPY(p);
		SET_CONFIG(CONF_VAR_DOMAIN);
	    } else
	    if (EQ(ConfigBuff,_CONF_MIMEVERSION)) {
		TEST_CONFIG(CONF_VAR_MIMEVERSION, bit);
		if (!bit) innconf->mimeversion = COPY(p);
		SET_CONFIG(CONF_VAR_MIMEVERSION);
	    } else
	    if (EQ(ConfigBuff,_CONF_CONTENTTYPE)) {
		TEST_CONFIG(CONF_VAR_MIMECONTENTTYPE, bit);
		if (!bit) innconf->mimecontenttype = COPY(p);
		SET_CONFIG(CONF_VAR_MIMECONTENTTYPE);
	    } else
	    if (EQ(ConfigBuff,_CONF_ENCODING)) {
		TEST_CONFIG(CONF_VAR_MIMEENCODING, bit);
		if (!bit) innconf->mimeencoding = COPY(p);
		SET_CONFIG(CONF_VAR_MIMEENCODING);
	    } else
	    if (EQ(ConfigBuff,_CONF_HISCACHESIZE)) {
		TEST_CONFIG(CONF_VAR_HISCACHESIZE, bit);
		if (!bit) innconf->hiscachesize = atoi(p);
		SET_CONFIG(CONF_VAR_HISCACHESIZE);
	    } else
	    if (EQ(ConfigBuff,_CONF_WIREFORMAT)) {
		TEST_CONFIG(CONF_VAR_WIREFORMAT, bit);
		if (!bit) {
		if (innconf->storageapi != TRUE && boolval != -1) innconf->wireformat = boolval;
		}
		SET_CONFIG(CONF_VAR_WIREFORMAT);
	    } else
	    if (EQ (ConfigBuff,_CONF_XREFSLAVE)) {
		TEST_CONFIG(CONF_VAR_XREFSLAVE, bit);
		if (!bit && boolval != -1) innconf->xrefslave = boolval;
		SET_CONFIG(CONF_VAR_XREFSLAVE);
	    } else
	    if (EQ(ConfigBuff,_CONF_COMPLAINTS)) {
		TEST_CONFIG(CONF_VAR_COMPLAINTS, bit);
		if (!bit) innconf->complaints = COPY(p);
		SET_CONFIG(CONF_VAR_COMPLAINTS);
	    } else
	    if (EQ(ConfigBuff,_CONF_NNRP_SPOOLFIRST)) {
		TEST_CONFIG(CONF_VAR_SPOOLFIRST, bit);
		if (!bit && boolval != -1) innconf->spoolfirst = boolval;
		SET_CONFIG(CONF_VAR_SPOOLFIRST);
	    } else
	    if (EQ(ConfigBuff,_CONF_WRITELINKS)) {
		TEST_CONFIG(CONF_VAR_WRITELINKS, bit);
		if (!bit && boolval != -1) innconf->writelinks = boolval;
		SET_CONFIG(CONF_VAR_WRITELINKS);
	    } else
	    if (EQ(ConfigBuff,_CONF_TIMER)) {
		TEST_CONFIG(CONF_VAR_TIMER, bit);
		if (!bit) innconf->timer = atoi(p);
		SET_CONFIG(CONF_VAR_TIMER);
	    } else
	    if (EQ(ConfigBuff,_CONF_STATUS)) {
		TEST_CONFIG(CONF_VAR_STATUS, bit);
		if (!bit) innconf->status = atoi(p);
		SET_CONFIG(CONF_VAR_STATUS);
	    } else
	    if (EQ(ConfigBuff,_CONF_STORAGEAPI)) {
		TEST_CONFIG(CONF_VAR_STORAGEAPI, bit);
		if (!bit) {
		if (boolval != -1) innconf->storageapi = boolval;
		if (innconf->storageapi == TRUE) innconf->wireformat = TRUE;
		}
		SET_CONFIG(CONF_VAR_STORAGEAPI);
	    } else
	    if (EQ(ConfigBuff,_CONF_ARTMMAP)) {
		TEST_CONFIG(CONF_VAR_ARTICLEMMAP, bit);
		if (!bit && boolval != -1) innconf->articlemmap = boolval;
		SET_CONFIG(CONF_VAR_ARTICLEMMAP);
	    } else
	    if (EQ(ConfigBuff,_CONF_OVERMMAP)) {
		TEST_CONFIG(CONF_VAR_OVERVIEWMMAP, bit);
		if (!bit && boolval != -1) innconf->overviewmmap = boolval;
		SET_CONFIG(CONF_VAR_OVERVIEWMMAP);
	    } else
	    if (EQ(ConfigBuff,_CONF_MTA)) {
		TEST_CONFIG(CONF_VAR_MTA, bit);
		if (!bit) innconf->mta = COPY(p);
		SET_CONFIG(CONF_VAR_MTA);
	    } else
	    if (EQ(ConfigBuff,_CONF_MAILCMD)) {
		TEST_CONFIG(CONF_VAR_MAILCMD, bit);
		if (!bit) innconf->mailcmd = COPY(p);
		SET_CONFIG(CONF_VAR_MAILCMD);
	    } else
	    if (EQ(ConfigBuff,_CONF_CHECK_INC_TEXT)) {
		TEST_CONFIG(CONF_VAR_CHECKINCLUDEDTEXT, bit);
		if (!bit && boolval != -1) innconf->checkincludedtext = boolval;
		SET_CONFIG(CONF_VAR_CHECKINCLUDEDTEXT);
	    } else
	    if (EQ(ConfigBuff,_CONF_MAX_FORKS)) {
		TEST_CONFIG(CONF_VAR_MAXFORKS, bit);
		if (!bit) innconf->maxforks = atoi(p);
		SET_CONFIG(CONF_VAR_MAXFORKS);
	    } else
	    if (EQ(ConfigBuff,_CONF_MAX_ART_SIZE)) {
		TEST_CONFIG(CONF_VAR_MAXARTSIZE, bit);
		if (!bit) innconf->maxartsize = atol(p);
		SET_CONFIG(CONF_VAR_MAXARTSIZE);
	    } else
	    if (EQ(ConfigBuff,_CONF_NICE_KIDS)) {
		TEST_CONFIG(CONF_VAR_NICEKIDS, bit);
		if (!bit) innconf->nicekids = atoi(p);
		SET_CONFIG(CONF_VAR_NICEKIDS);
	    } else
	    if (EQ(ConfigBuff,_CONF_VERIFY_CANCELS)) {
		TEST_CONFIG(CONF_VAR_VERIFYCANCELS, bit);
		if (!bit && boolval != -1) innconf->verifycancels = boolval;
		SET_CONFIG(CONF_VAR_VERIFYCANCELS);
	    } else
	    if (EQ(ConfigBuff,_CONF_LOG_CANCEL_COMM)) {
		TEST_CONFIG(CONF_VAR_LOGCANCELCOMM, bit);
		if (!bit && boolval != -1) innconf->logcancelcomm = boolval;
		SET_CONFIG(CONF_VAR_LOGCANCELCOMM);
	    } else
	    if (EQ(ConfigBuff,_CONF_WANT_TRASH)) {
		TEST_CONFIG(CONF_VAR_WANTTRASH, bit);
		if (!bit && boolval != -1) innconf->wanttrash = boolval;
		SET_CONFIG(CONF_VAR_WANTTRASH);
	    } else
	    if (EQ(ConfigBuff,_CONF_REMEMBER_TRASH)) {
		TEST_CONFIG(CONF_VAR_REMEMBERTRASH, bit);
		if (!bit && boolval != -1) innconf->remembertrash = boolval;
		SET_CONFIG(CONF_VAR_REMEMBERTRASH);
	    } else
	    if (EQ(ConfigBuff,_CONF_LINECOUNT_FUZZ)) {
		TEST_CONFIG(CONF_VAR_LINECOUNTFUZZ, bit);
		if (!bit) innconf->linecountfuzz = atoi(p);
		SET_CONFIG(CONF_VAR_LINECOUNTFUZZ);
	    } else
	    if (EQ(ConfigBuff,_CONF_PEER_TIMEOUT)) {
		TEST_CONFIG(CONF_VAR_PEERTIMEOUT, bit);
		if (!bit) innconf->peertimeout = atoi(p);
		SET_CONFIG(CONF_VAR_PEERTIMEOUT);
	    } else
	    if (EQ(ConfigBuff,_CONF_CLIENT_TIMEOUT)) {
		TEST_CONFIG(CONF_VAR_CLIENTTIMEOUT, bit);
		if (!bit) innconf->clienttimeout = atoi(p);
		SET_CONFIG(CONF_VAR_CLIENTTIMEOUT);
	    } else
	    if (EQ(ConfigBuff,_CONF_ALLOW_READERS)) {
		TEST_CONFIG(CONF_VAR_ALLOWREADERS, bit);
		if (!bit && boolval != -1) {
		    if (boolval == TRUE)
			innconf->allowreaders = FALSE;
		    else
			innconf->allowreaders = TRUE;
		}
		SET_CONFIG(CONF_VAR_ALLOWREADERS);
	    } else
	    if (EQ(ConfigBuff,_CONF_ALLOW_NEWNEWS)) {
		TEST_CONFIG(CONF_VAR_ALLOWNEWNEWS, bit);
		if (!bit && boolval != -1) innconf->allownewnews = boolval;
		SET_CONFIG(CONF_VAR_ALLOWNEWNEWS);
	    } else
	    if (EQ(ConfigBuff,_CONF_LOCAL_MAX_ARTSIZE)) {
		TEST_CONFIG(CONF_VAR_LOCALMAXARTSIZE, bit);
		if (!bit) innconf->localmaxartsize = atoi(p);
		SET_CONFIG(CONF_VAR_LOCALMAXARTSIZE);
	    } else
	    if (EQ(ConfigBuff,_CONF_LOG_ARTSIZE)) {
		TEST_CONFIG(CONF_VAR_LOGARTSIZE, bit);
		if (!bit && boolval != -1) innconf->logartsize = boolval;
		SET_CONFIG(CONF_VAR_LOGARTSIZE);
	    } else
	    if (EQ(ConfigBuff,_CONF_LOG_IPADDR)) {
		TEST_CONFIG(CONF_VAR_LOGIPADDR, bit);
		if (!bit && boolval != -1) innconf->logipaddr = boolval;
		SET_CONFIG(CONF_VAR_LOGIPADDR);
	    } else
	    if (EQ(ConfigBuff,_CONF_CHAN_INACT_TIME)) {
		TEST_CONFIG(CONF_VAR_CHANINACTTIME, bit);
		if (!bit) innconf->chaninacttime = atoi(p);
		SET_CONFIG(CONF_VAR_CHANINACTTIME);
	    } else
	    if (EQ(ConfigBuff,_CONF_MAX_CONNECTIONS)) {
		TEST_CONFIG(CONF_VAR_MAXCONNECTIONS, bit);
		if (!bit) innconf->maxconnections = atoi(p);
		SET_CONFIG(CONF_VAR_MAXCONNECTIONS);
	    } else
	    if (EQ(ConfigBuff,_CONF_CHAN_RETRY_TIME)) {
		TEST_CONFIG(CONF_VAR_CHANRETRYTIME, bit);
		if (!bit) innconf->chanretrytime = atoi(p);
		SET_CONFIG(CONF_VAR_CHANRETRYTIME);
	    } else
	    if (EQ(ConfigBuff,_CONF_ART_CUTOFF)) {
		TEST_CONFIG(CONF_VAR_ARTCUTOFF, bit);
		if (!bit) innconf->artcutoff = atoi(p) * 24 * 60 * 60;
		SET_CONFIG(CONF_VAR_ARTCUTOFF);
	    } else
	    if (EQ(ConfigBuff,_CONF_PAUSE_RETRY_TIME)) {
		TEST_CONFIG(CONF_VAR_PAUSERETRYTIME, bit);
		if (!bit) innconf->pauseretrytime = atoi(p);
		SET_CONFIG(CONF_VAR_PAUSERETRYTIME);
	    } else
	    if (EQ(ConfigBuff,_CONF_NNTPLINK_LOG)) {
		TEST_CONFIG(CONF_VAR_NNTPLINKLOG, bit);
		if (!bit && boolval != -1) innconf->nntplinklog = boolval;
		SET_CONFIG(CONF_VAR_NNTPLINKLOG);
	    } else
	    if (EQ(ConfigBuff,_CONF_NNTP_ACT_SYNC)) {
		TEST_CONFIG(CONF_VAR_NNTPACTSYNC, bit);
		if (!bit) innconf->nntpactsync = atoi(p);
		SET_CONFIG(CONF_VAR_NNTPACTSYNC);
	    } else
	    if (EQ(ConfigBuff,_CONF_BAD_IO_COUNT)) {
		TEST_CONFIG(CONF_VAR_BADIOCOUNT, bit);
		if (!bit) innconf->badiocount = atoi(p);
		SET_CONFIG(CONF_VAR_BADIOCOUNT);
	    } else
	    if (EQ(ConfigBuff,_CONF_BLOCK_BACKOFF)) {
		TEST_CONFIG(CONF_VAR_BLOCKBACKOFF, bit);
		if (!bit) innconf->blockbackoff = atoi(p);
		SET_CONFIG(CONF_VAR_BLOCKBACKOFF);
	    } else
	    if (EQ(ConfigBuff,_CONF_ICD_SYNC_COUNT)) {
		TEST_CONFIG(CONF_VAR_ICDSYNCCOUNT, bit);
		if (!bit) innconf->icdsynccount = atoi(p);
		SET_CONFIG(CONF_VAR_ICDSYNCCOUNT);
	    } else
	    if (EQ(ConfigBuff,_CONF_INNBINDADDR)) {
		TEST_CONFIG(CONF_VAR_BINDADDRESS, bit);
		if (!bit) {
		if (EQ(p,"all") || EQ(p,"any"))
		    innconf->bindaddress =  NULL;
		else
		    innconf->bindaddress = COPY(p);
		}
		SET_CONFIG(CONF_VAR_BINDADDRESS);
	    } else
	    if (EQ(ConfigBuff,_CONF_INNPORT)) {
		TEST_CONFIG(CONF_VAR_PORT, bit);
		if (!bit) innconf->port = atoi(p);
		SET_CONFIG(CONF_VAR_PORT);
	    } else
	    if (EQ(ConfigBuff,_CONF_READERTRACK)) {
		TEST_CONFIG(CONF_VAR_READERTRACK, bit);
		if (!bit && boolval != -1) innconf->readertrack = boolval;
		SET_CONFIG(CONF_VAR_READERTRACK);
	    } else
	    if (EQ(ConfigBuff,_CONF_STRIPPOSTCC)) {
		TEST_CONFIG(CONF_VAR_STRIPPOSTCC, bit);
		if (!bit && boolval != -1) innconf->strippostcc = boolval;
		SET_CONFIG(CONF_VAR_STRIPPOSTCC);
	    } else
	    if (EQ(ConfigBuff,_CONF_OVERVIEWNAME)) {
		TEST_CONFIG(CONF_VAR_OVERVIEWNAME, bit);
		if (!bit) innconf->overviewname = COPY(p);
		SET_CONFIG(CONF_VAR_OVERVIEWNAME);
	    } else
	    if (EQ(ConfigBuff,_CONF_KEYWORDS)) {
		TEST_CONFIG(CONF_VAR_KEYWORDS, bit);
		if (!bit && boolval != -1) innconf->keywords = boolval;
		SET_CONFIG(CONF_VAR_KEYWORDS);
	    } else
            if (EQ(ConfigBuff,_CONF_KEYLIMIT)) {
		TEST_CONFIG(CONF_VAR_KEYLIMIT, bit);
		if (!bit) innconf->keylimit = atoi(p);
		SET_CONFIG(CONF_VAR_KEYLIMIT);
	    } else 
	    if (EQ(ConfigBuff,_CONF_KEYARTLIMIT)) {
		TEST_CONFIG(CONF_VAR_KEYARTLIMIT, bit);
		if (!bit) innconf->keyartlimit = atoi(p);
		SET_CONFIG(CONF_VAR_KEYARTLIMIT);
	    } else  
	    if (EQ(ConfigBuff,_CONF_KEY_MAXWORDS)) {
		TEST_CONFIG(CONF_VAR_KEYMAXWORDS, bit);
		if (!bit) innconf->keymaxwords = atoi(p);
		SET_CONFIG(CONF_VAR_KEYMAXWORDS);
	    } else
 	    if (EQ(ConfigBuff,_CONF_PATHNEWS)) {
		TEST_CONFIG(CONF_VAR_PATHNEWS, bit);
		if (!bit) innconf->pathnews = COPY(p);
		SET_CONFIG(CONF_VAR_PATHNEWS);
	    } else
	    if (EQ(ConfigBuff,_CONF_PATHBIN)) {
		TEST_CONFIG(CONF_VAR_PATHBIN, bit);
		if (!bit) innconf->pathbin = COPY(p);
		SET_CONFIG(CONF_VAR_PATHBIN);
	    } else
	    if (EQ(ConfigBuff,_CONF_PATHFILTER)) {
		TEST_CONFIG(CONF_VAR_PATHFILTER, bit);
		if (!bit) innconf->pathfilter = COPY(p);
		SET_CONFIG(CONF_VAR_PATHFILTER);
	    } else
	    if (EQ(ConfigBuff,_CONF_PATHCONTROL)) {
		TEST_CONFIG(CONF_VAR_PATHCONTROL, bit);
		if (!bit) innconf->pathcontrol = COPY(p);
		SET_CONFIG(CONF_VAR_PATHCONTROL);
	    } else
	    if (EQ(ConfigBuff,_CONF_PATHDB)) {
		TEST_CONFIG(CONF_VAR_PATHDB, bit);
		if (!bit) innconf->pathdb = COPY(p);
		SET_CONFIG(CONF_VAR_PATHDB);
	    } else
	    if (EQ(ConfigBuff,_CONF_PATHETC)) {
		TEST_CONFIG(CONF_VAR_PATHETC, bit);
		if (!bit) innconf->pathetc = COPY(p);
		SET_CONFIG(CONF_VAR_PATHETC);
	    } else
	    if (EQ(ConfigBuff,_CONF_PATHRUN)) {
		TEST_CONFIG(CONF_VAR_PATHRUN, bit);
		if (!bit) innconf->pathrun = COPY(p);
		SET_CONFIG(CONF_VAR_PATHRUN);
	    } else
	    if (EQ(ConfigBuff,_CONF_PATHLOG)) {
		TEST_CONFIG(CONF_VAR_PATHLOG, bit);
		if (!bit) innconf->pathlog = COPY(p);
		SET_CONFIG(CONF_VAR_PATHLOG);
	    } else
	    if (EQ(ConfigBuff,_CONF_PATHHTTP)) {
		TEST_CONFIG(CONF_VAR_PATHHTTP, bit);
		if (!bit) innconf->pathhttp = COPY(p);
		SET_CONFIG(CONF_VAR_PATHHTTP);
	    } else
	    if (EQ(ConfigBuff,_CONF_PATHSPOOL)) {
		TEST_CONFIG(CONF_VAR_PATHSPOOL, bit);
		if (!bit) innconf->pathspool = COPY(p);
		SET_CONFIG(CONF_VAR_PATHSPOOL);
	    } else
	    if (EQ(ConfigBuff,_CONF_PATHARTICLES)) {
		TEST_CONFIG(CONF_VAR_PATHARTICLES, bit);
		if (!bit) innconf->patharticles = COPY(p);
		SET_CONFIG(CONF_VAR_PATHARTICLES);
	    } else
	    if (EQ(ConfigBuff,_CONF_PATHOVERVIEW)) {
		TEST_CONFIG(CONF_VAR_PATHOVERVIEW, bit);
		if (!bit) innconf->pathoverview = COPY(p);
		SET_CONFIG(CONF_VAR_PATHOVERVIEW);
	    } else
	    if (EQ(ConfigBuff,_CONF_PATHOUTGOING)) {
		TEST_CONFIG(CONF_VAR_PATHOUTGOING, bit);
		if (!bit) innconf->pathoutgoing = COPY(p);
		SET_CONFIG(CONF_VAR_PATHOUTGOING);
	    } else
	    if (EQ(ConfigBuff,_CONF_PATHINCOMING)) {
		TEST_CONFIG(CONF_VAR_PATHINCOMING, bit);
		if (!bit) innconf->pathincoming = COPY(p);
		SET_CONFIG(CONF_VAR_PATHINCOMING);
	    } else
	    if (EQ(ConfigBuff,_CONF_PATHARCHIVE)) {
		TEST_CONFIG(CONF_VAR_PATHARCHIVE, bit);
		if (!bit) innconf->patharchive = COPY(p);
		SET_CONFIG(CONF_VAR_PATHARCHIVE);
	    } else
	    if (EQ(ConfigBuff,_CONF_LOGSITENAME)) {
		TEST_CONFIG(CONF_VAR_LOGSITENAME, bit);
		if (!bit && boolval != -1) innconf->logsitename = boolval;
		SET_CONFIG(CONF_VAR_LOGSITENAME);
	    } else
	    if (EQ(ConfigBuff,_CONF_NNRPDPOSTHOST)) {
		TEST_CONFIG(CONF_VAR_NNRPDPOSTHOST, bit);
		if (!bit) innconf->nnrpdposthost = COPY(p);
		SET_CONFIG(CONF_VAR_NNRPDPOSTHOST);
	    } else
	    if (EQ(ConfigBuff,_CONF_EXTENDEDDBZ)) {
		TEST_CONFIG(CONF_VAR_EXTENDEDDBZ, bit);
		if (!bit && boolval != -1) innconf->extendeddbz = boolval;
		SET_CONFIG(CONF_VAR_EXTENDEDDBZ);
	    } else
	    if (EQ(ConfigBuff,_CONF_NNRPDOVERSTATS)) {
		TEST_CONFIG(CONF_VAR_NNRPDOVERSTATS, bit);
		if (!bit && boolval != -1) innconf->nnrpdoverstats = boolval;
		SET_CONFIG(CONF_VAR_NNRPDOVERSTATS);
	    } else
	    if (EQ(ConfigBuff,_CONF_DECNETDOMAIN)) {
		TEST_CONFIG(CONF_VAR_DECNETDOMAIN, bit);
		if (!bit) innconf->decnetdomain = COPY(p);
		SET_CONFIG(CONF_VAR_DECNETDOMAIN);
	    } else
	    if (EQ(ConfigBuff,_CONF_BACKOFFAUTH)) {
		TEST_CONFIG(CONF_VAR_BACKOFFAUTH, bit);
		if (!bit && boolval != -1) innconf->backoff_auth = boolval;
		SET_CONFIG(CONF_VAR_BACKOFFAUTH);
	    } else
	    if (EQ(ConfigBuff,_CONF_BACKOFFDB)) {
		TEST_CONFIG(CONF_VAR_BACKOFFDB, bit);
		if (!bit) innconf->backoff_db = COPY(p);
		SET_CONFIG(CONF_VAR_BACKOFFDB);
	    } else 
	    if (EQ(ConfigBuff,_CONF_BACKOFFK)) {
		TEST_CONFIG(CONF_VAR_BACKOFFK, bit);
		if (!bit) innconf->backoff_k = atol(p);
		SET_CONFIG(CONF_VAR_BACKOFFK);
	    } else 
	    if (EQ(ConfigBuff,_CONF_BACKOFFPOSTFAST)) {
		TEST_CONFIG(CONF_VAR_BACKOFFPOSTFAST, bit);
		if (!bit) innconf->backoff_postfast = atol(p);
		SET_CONFIG(CONF_VAR_BACKOFFPOSTFAST);
	    } else 
	    if (EQ(ConfigBuff,_CONF_BACKOFFPOSTSLOW)) {
		TEST_CONFIG(CONF_VAR_BACKOFFPOSTSLOW, bit);
		if (!bit) innconf->backoff_postslow = atol(p);
		SET_CONFIG(CONF_VAR_BACKOFFPOSTSLOW);
	    } else 
	    if (EQ(ConfigBuff,_CONF_BACKOFFTRIGGER)) {
		TEST_CONFIG(CONF_VAR_BACKOFFTRIGGER, bit);
		if (!bit) innconf->backoff_trigger = atol(p);
		SET_CONFIG(CONF_VAR_BACKOFFTRIGGER);
	    }
	}
	(void)Fclose(F);
    } else {
	syslog(L_FATAL, "Cannot open %s", _PATH_CONFIG);
	(void)fprintf(stderr, "Cannot open %s\n", _PATH_CONFIG);
	return(-1);
    }
    return(CheckInnConf());
}

/*  $Id$
**
*/
#include "config.h"
#include "clibrary.h"
#include <syslog.h> 

#include "innconf.h"
#include "libinn.h"
#include "macros.h"
#include "paths.h"

/* Global and initialized; to work around SunOS -Bstatic bug, sigh. */
static char		ConfigBuff[SMBUF] = "";
static char		*ConfigBit;
static int		ConfigBitsize;
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

	include/innconf.h	:	Add to conf_defaults
	include/libinn.h	:	Add varname to conf_vars struct
	lib/getconfig.c		:	SetDefaults() & ReadInnConf(), ClearInnConf()
	samples/inn.conf.in	:	Set the default value
	scripts/inncheck.in	:	So we can check it
	doc/pod/inn.conf.pod	:	Document it!
	wherever you need it	:	Use as innconf->varname
*/

struct conf_vars	*innconf = NULL;
const char		*innconffile = _PATH_CONFIG;
char			pathbuff[SMBUF];

char *
GetFileConfigValue(char *value)
{
    FILE	        *F;
    int	                i;
    char	        *p, *q;
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
		q = &p[strlen(p)-1];
		while (q>p && ISWHITE(*q))
		    *q-- = '\0';
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
char *
GetConfigValue(char *value)
{
    char	        *p;

    /* Some environment variables override the file. */
    if (EQ(value, _CONF_SERVER)
     && (p = getenv(_ENV_NNTPSERVER)) != NULL)
	return p;
    if (EQ(value, _CONF_ORGANIZATION)
     && (p = getenv(_ENV_ORGANIZATION)) != NULL)
	return p;
    if (EQ(value, _CONF_BINDADDRESS)
     && (p = getenv(_ENV_INNBINDADDR)) != NULL)
	return p;

    if ((p = GetFileConfigValue(value)) != NULL)
	return p;

    /* Some values have defaults if not in the file. */
    if (EQ(value, _CONF_FROMHOST) || EQ(value, _CONF_PATHHOST))
	return GetFQDN(innconf->domain);
    return NULL;
}

/*
**  Get a boolean config value and return it by value
*/
bool
GetBooleanConfigValue(char *key, bool defaultvalue)
{
    char *value;

    if ((value = GetConfigValue(key)) == NULL)
	return defaultvalue;

    if (caseEQ(value, "on") || caseEQ(value, "true") || caseEQ(value, "yes"))
	return TRUE;
    if (caseEQ(value, "off") || caseEQ(value, "false") || caseEQ(value, "no"))
	return FALSE;
    return defaultvalue;
}

static void
SetDefaults(void)
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
    if ((p = getenv(_ENV_FROMHOST)) != NULL) {
	innconf->fromhost = COPY(p);
	SET_CONFIG(CONF_VAR_FROMHOST);
    }
    innconf->server = NULL;
    if ((p = getenv(_ENV_NNTPSERVER)) != NULL) {
	innconf->server = COPY(p);
	SET_CONFIG(CONF_VAR_SERVER);
    }
    innconf->pathhost = NULL;
    innconf->pathalias = NULL;
    innconf->organization = NULL;
    if ((p = getenv(_ENV_ORGANIZATION)) != NULL) {
	innconf->organization = COPY(p);
	SET_CONFIG(CONF_VAR_ORGANIZATION);
    }
    innconf->moderatormailer = NULL;
    innconf->domain = NULL;
    innconf->hiscachesize = 0;
    innconf->xrefslave = FALSE;
    innconf->nfswriter = FALSE;
    innconf->complaints = NULL;
    innconf->spoolfirst = FALSE;
    innconf->immediatecancel = FALSE;
    innconf->timer = 0;
    innconf->status = 0;
    innconf->articlemmap = FALSE;
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
    innconf->readerswhenstopped = FALSE;
    innconf->allownewnews = TRUE;
    innconf->localmaxartsize = 1000000L;
    innconf->logartsize = TRUE;
    innconf->logipaddr = TRUE;
    innconf->chaninacttime = 600;
    innconf->maxconnections = 50;
    innconf->chanretrytime = 300;
    innconf->artcutoff = 10 * 24 * 60 * 60;
    innconf->pauseretrytime = 300;
    innconf->nntplinklog = FALSE;
    innconf->nntpactsync = 200;
    innconf->badiocount = 5;
    innconf->blockbackoff = 120;
    innconf->icdsynccount = 10;
    innconf->bindaddress = NULL;
    if ((p = getenv(_ENV_INNBINDADDR)) != NULL) {
	innconf->bindaddress = COPY(p);
	SET_CONFIG(CONF_VAR_BINDADDRESS);
    }
    innconf->sourceaddress = NULL;
    innconf->bindaddress6 = NULL;
    innconf->sourceaddress6 = NULL;
    innconf->port = NNTP_PORT;
    innconf->readertrack = FALSE;
    innconf->nfsreader = FALSE;
    innconf->tradindexedmmap = TRUE;
    innconf->nnrpdloadlimit = NNRP_LOADLIMIT;
    innconf->strippostcc = FALSE;
    innconf->keywords = FALSE;		
    innconf->keylimit = 512 ;		
    innconf->keyartlimit = 100000;
    innconf->keymaxwords = 250;
    innconf->nnrpdposthost = NULL;
    innconf->nnrpdpostport = NNTP_PORT;
    innconf->nnrpperlauth = FALSE;
    innconf->nnrppythonauth = FALSE;
    innconf->addnntppostinghost = TRUE;
    innconf->addnntppostingdate = TRUE;

    innconf->pathnews = NULL;
    innconf->pathbin = NULL;
    innconf->pathfilter = NULL;
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
    innconf->pathtmp = NULL;

    innconf->logsitename = TRUE;
    innconf->nnrpdoverstats = FALSE;
    innconf->storeonxref = TRUE;
    innconf->backoff_auth = FALSE;
    innconf->backoff_db = NULL;
    innconf->backoff_k = 1L;
    innconf->backoff_postfast = 0L;
    innconf->backoff_postslow = 1L;
    innconf->backoff_trigger = 10000L;
    innconf->refusecybercancels = FALSE;
    innconf->nnrpdcheckart = TRUE;
    innconf->nicenewnews = 0;
    innconf->nicennrpd = 0;
    innconf->mergetogroups = FALSE;
    innconf->noreader = FALSE;
    innconf->nnrpdauthsender = FALSE;
    innconf->cnfscheckfudgesize = 0L;
    innconf->rlimitnofile = -1;
    innconf->ignorenewsgroups = FALSE;
    innconf->overcachesize = 15;
    innconf->enableoverview = TRUE;
    innconf->wireformat = FALSE;
    innconf->ovmethod = NULL;
    innconf->useoverchan = FALSE;
    innconf->ovgrouppat = NULL;
    innconf->groupbaseexpiry = TRUE;
    innconf->wipcheck = 5;
    innconf->wipexpire = 10;
    innconf->dontrejectfiltered = FALSE;
    innconf->keepmmappedthreshold = 1024;
    innconf->maxcmdreadsize = BUFSIZ;
    innconf->datamovethreshold = BIG_BUFFER;
    innconf->stathist = NULL;
    innconf->hismethod = NULL;
}

void
ClearInnConf(void)
{
    if (innconf->fromhost != NULL) DISPOSE(innconf->fromhost);
    if (innconf->server != NULL) DISPOSE(innconf->server);
    if (innconf->pathhost != NULL) DISPOSE(innconf->pathhost);
    if (innconf->pathalias != NULL) DISPOSE(innconf->pathalias);
    if (innconf->organization != NULL) DISPOSE(innconf->organization);
    if (innconf->moderatormailer != NULL) DISPOSE(innconf->moderatormailer);
    if (innconf->domain != NULL) DISPOSE(innconf->domain);
    if (innconf->complaints != NULL) DISPOSE(innconf->complaints);
    if (innconf->mta != NULL) DISPOSE(innconf->mta);
    if (innconf->mailcmd != NULL) DISPOSE(innconf->mailcmd);
    if (innconf->bindaddress != NULL) DISPOSE(innconf->bindaddress);
    if (innconf->sourceaddress != NULL) DISPOSE(innconf->sourceaddress);
    if (innconf->bindaddress6 != NULL) DISPOSE(innconf->bindaddress6);
    if (innconf->sourceaddress6 != NULL) DISPOSE(innconf->sourceaddress6);
    if (innconf->nnrpdposthost != NULL) DISPOSE(innconf->nnrpdposthost);

    if (innconf->pathnews != NULL) DISPOSE(innconf->pathnews);
    if (innconf->pathbin != NULL) DISPOSE(innconf->pathbin);
    if (innconf->pathfilter != NULL) DISPOSE(innconf->pathfilter);
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
    if (innconf->pathtmp != NULL) DISPOSE(innconf->pathtmp);
    if (innconf->backoff_db != NULL) DISPOSE(innconf->backoff_db);
    if (innconf->ovmethod != NULL) DISPOSE(innconf->ovmethod);
    if (innconf->ovgrouppat != NULL) DISPOSE(innconf->ovgrouppat);
    if (innconf->stathist != NULL) DISPOSE(innconf->stathist);
    if (innconf->hismethod != NULL) DISPOSE(innconf->hismethod);
    memset(ConfigBit, '\0', ConfigBitsize);
}

/*
   Make sure some compulsory inn.conf values are set and set them
   to defaults if possible 
*/
static int
CheckInnConf(void)
{
    char *tmpdir;

    if (GetFQDN(innconf->domain) == NULL) {
	syslog(L_FATAL, "Hostname does not resolve or 'domain' in inn.conf is missing");
	(void)fprintf(stderr, "Hostname does not resolve or 'domain' in inn.conf is missing");
	return(-1);
    }
    if (innconf->fromhost == NULL) {
	innconf->fromhost = COPY(GetFQDN(innconf->domain));
    }
    if (innconf->pathhost == NULL && ((innconf->pathhost = COPY(GetFQDN(innconf->domain))) == NULL)) {
	syslog(L_FATAL, "Must set 'pathhost' in inn.conf");
	(void)fprintf(stderr, "Must set 'pathhost' in inn.conf");
	return(-1);
    }
    if (innconf->mta == NULL) {
	syslog(L_FATAL, "Must set 'mta' in inn.conf");
	(void)fprintf(stderr, "Must set 'mta' in inn.conf");
	return(-1);
    }
    if (innconf->pathnews == NULL) {
	syslog(L_FATAL, "Must set 'pathnews' in inn.conf");
	(void)fprintf(stderr, "Must set 'pathnews' in inn.conf");
	return(-1);
    }
    if (innconf->pathbin == NULL)
	innconf->pathbin = concatpath(innconf->pathnews, "bin");
    if (innconf->pathfilter == NULL)
	innconf->pathfilter = concatpath(innconf->pathbin, "filter");
    if (innconf->pathdb == NULL)
	innconf->pathdb = concatpath(innconf->pathnews, "db");
    if (innconf->pathetc == NULL)
	innconf->pathetc = concatpath(innconf->pathnews, "etc");
    if (innconf->pathrun == NULL)
	innconf->pathrun = concatpath(innconf->pathnews, "run");
    if (innconf->pathlog == NULL)
	innconf->pathlog = concatpath(innconf->pathnews, "log");
    if (innconf->pathhttp == NULL)
	innconf->pathhttp = COPY(innconf->pathlog);
    if (innconf->pathspool == NULL)
	innconf->pathspool = concatpath(innconf->pathnews, "spool");
    if (innconf->patharticles == NULL)
	innconf->patharticles = concatpath(innconf->pathspool, "articles");
    if (innconf->pathoverview == NULL)
	innconf->pathoverview = concatpath(innconf->pathspool, "overview");
    if (innconf->pathoutgoing == NULL)
	innconf->pathoutgoing = concatpath(innconf->pathspool, "outgoing");
    if (innconf->pathincoming == NULL)
	innconf->pathincoming = concatpath(innconf->pathspool, "incoming");
    if (innconf->patharchive == NULL)
	innconf->patharchive = concatpath(innconf->pathspool, "archive");
    if (innconf->pathtmp == NULL)
	innconf->pathtmp = COPY(_PATH_TMP);
    if (innconf->mailcmd == NULL)
	innconf->mailcmd = concatpath(innconf->pathbin, "innmail");

    /* Set the TMPDIR variable unconditionally and globally */
    tmpdir = getenv("TMPDIR");
    if (!tmpdir || strcmp(tmpdir, innconf->pathtmp) != 0) {
	if (setenv("TMPDIR", innconf->pathtmp, true) != 0) {
	    syslog(L_FATAL, "can't set TMPDIR in environment");
	    (void)fprintf(stderr, "can't set TMPDIR in environment\n");
	    return(-1);
	}
    }
    if (innconf->enableoverview && innconf->ovmethod == NULL) {
	syslog(L_FATAL, "'ovmethod' must be defined in inn.conf if enableoverview is true");
	(void)fprintf(stderr, "'ovmethod' must be defined in inn.conf if enableoverview is true\n");
	return(-1);
    }
    if (innconf->datamovethreshold <= 0 ||
	innconf->datamovethreshold > 1024 * 1024) {
	/* datamovethreshold is 0 or exceeds 1MB, then maximum threshold is set
	   to 1MB */
	innconf->datamovethreshold = 1024 * 1024;
    }

    return(0);
}

int
ReadInnConf(void)
{
    FILE	        *F;
    char	        *p, *q;
    int			boolval;
    bool		bit;

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
    if (!(innconffile = getenv("INNCONF")))
	innconffile = _PATH_CONFIG;
    syslog(L_TRACE, "Reading config from %s", innconffile); 
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
	    if (!*p) continue;

            /* trim trailing whitespace */
	    q = &p[strlen(p)-1];
	    while (q>p && ISWHITE(*q))
		*q-- = '\0';

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
	    if (EQ(ConfigBuff,_CONF_MODERATORMAILER)) {
		TEST_CONFIG(CONF_VAR_MODERATORMAILER, bit);
		if (!bit) innconf->moderatormailer = COPY(p);
		SET_CONFIG(CONF_VAR_MODERATORMAILER);
	    } else
	    if (EQ(ConfigBuff,_CONF_DOMAIN)) {
		TEST_CONFIG(CONF_VAR_DOMAIN, bit);
		if (!bit) innconf->domain = COPY(p);
		SET_CONFIG(CONF_VAR_DOMAIN);
	    } else
	    if (EQ(ConfigBuff,_CONF_HISCACHESIZE)) {
		TEST_CONFIG(CONF_VAR_HISCACHESIZE, bit);
		if (!bit) innconf->hiscachesize = atoi(p);
		SET_CONFIG(CONF_VAR_HISCACHESIZE);
	    } else
	    if (EQ (ConfigBuff,_CONF_XREFSLAVE)) {
		TEST_CONFIG(CONF_VAR_XREFSLAVE, bit);
		if (!bit && boolval != -1) innconf->xrefslave = boolval;
		SET_CONFIG(CONF_VAR_XREFSLAVE);
	    } else
	    if (EQ (ConfigBuff,_CONF_NFSWRITER)) {
		TEST_CONFIG(CONF_VAR_NFSWRITER, bit);
		if (!bit && boolval != -1) innconf->nfswriter = boolval;
		SET_CONFIG(CONF_VAR_NFSWRITER);
	    } else
	    if (EQ(ConfigBuff,_CONF_COMPLAINTS)) {
		TEST_CONFIG(CONF_VAR_COMPLAINTS, bit);
		if (!bit) innconf->complaints = COPY(p);
		SET_CONFIG(CONF_VAR_COMPLAINTS);
	    } else
	    if (EQ(ConfigBuff,_CONF_SPOOLFIRST)) {
		TEST_CONFIG(CONF_VAR_SPOOLFIRST, bit);
		if (!bit && boolval != -1) innconf->spoolfirst = boolval;
		SET_CONFIG(CONF_VAR_SPOOLFIRST);
	    } else
	    if (EQ(ConfigBuff,_CONF_IMMEDIATECANCEL)) {
		TEST_CONFIG(CONF_VAR_IMMEDIATECANCEL, bit);
		if (!bit && boolval != -1) innconf->immediatecancel = boolval;
		SET_CONFIG(CONF_VAR_IMMEDIATECANCEL);
	    } else
	    if (EQ(ConfigBuff,_CONF_TIMER)) {
		TEST_CONFIG(CONF_VAR_TIMER, bit);
		if (!bit) innconf->timer = (unsigned int) strtoul(p, NULL, 10);
		SET_CONFIG(CONF_VAR_TIMER);
	    } else
	    if (EQ(ConfigBuff,_CONF_STATUS)) {
		TEST_CONFIG(CONF_VAR_STATUS, bit);
		if (!bit) innconf->status = atoi(p);
		SET_CONFIG(CONF_VAR_STATUS);
	    } else
	    if (EQ(ConfigBuff,_CONF_ARTICLEMMAP)) {
		TEST_CONFIG(CONF_VAR_ARTICLEMMAP, bit);
		if (!bit && boolval != -1) innconf->articlemmap = boolval;
		SET_CONFIG(CONF_VAR_ARTICLEMMAP);
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
	    if (EQ(ConfigBuff,_CONF_CHECKINCLUDEDTEXT)) {
		TEST_CONFIG(CONF_VAR_CHECKINCLUDEDTEXT, bit);
		if (!bit && boolval != -1) innconf->checkincludedtext = boolval;
		SET_CONFIG(CONF_VAR_CHECKINCLUDEDTEXT);
	    } else
	    if (EQ(ConfigBuff,_CONF_MAXFORKS)) {
		TEST_CONFIG(CONF_VAR_MAXFORKS, bit);
		if (!bit) innconf->maxforks = atoi(p);
		SET_CONFIG(CONF_VAR_MAXFORKS);
	    } else
	    if (EQ(ConfigBuff,_CONF_MAXARTSIZE)) {
		TEST_CONFIG(CONF_VAR_MAXARTSIZE, bit);
		if (!bit) innconf->maxartsize = atol(p);
		SET_CONFIG(CONF_VAR_MAXARTSIZE);
	    } else
	    if (EQ(ConfigBuff,_CONF_NICEKIDS)) {
		TEST_CONFIG(CONF_VAR_NICEKIDS, bit);
		if (!bit) innconf->nicekids = atoi(p);
		SET_CONFIG(CONF_VAR_NICEKIDS);
	    } else
	    if (EQ(ConfigBuff,_CONF_VERIFYCANCELS)) {
		TEST_CONFIG(CONF_VAR_VERIFYCANCELS, bit);
		if (!bit && boolval != -1) innconf->verifycancels = boolval;
		SET_CONFIG(CONF_VAR_VERIFYCANCELS);
	    } else
	    if (EQ(ConfigBuff,_CONF_LOGCANCELCOMM)) {
		TEST_CONFIG(CONF_VAR_LOGCANCELCOMM, bit);
		if (!bit && boolval != -1) innconf->logcancelcomm = boolval;
		SET_CONFIG(CONF_VAR_LOGCANCELCOMM);
	    } else
	    if (EQ(ConfigBuff,_CONF_WANTTRASH)) {
		TEST_CONFIG(CONF_VAR_WANTTRASH, bit);
		if (!bit && boolval != -1) innconf->wanttrash = boolval;
		SET_CONFIG(CONF_VAR_WANTTRASH);
	    } else
	    if (EQ(ConfigBuff,_CONF_REMEMBERTRASH)) {
		TEST_CONFIG(CONF_VAR_REMEMBERTRASH, bit);
		if (!bit && boolval != -1) innconf->remembertrash = boolval;
		SET_CONFIG(CONF_VAR_REMEMBERTRASH);
	    } else
	    if (EQ(ConfigBuff,_CONF_LINECOUNTFUZZ)) {
		TEST_CONFIG(CONF_VAR_LINECOUNTFUZZ, bit);
		if (!bit) innconf->linecountfuzz = atoi(p);
		SET_CONFIG(CONF_VAR_LINECOUNTFUZZ);
	    } else
	    if (EQ(ConfigBuff,_CONF_PEERTIMEOUT)) {
		TEST_CONFIG(CONF_VAR_PEERTIMEOUT, bit);
		if (!bit) innconf->peertimeout = atoi(p);
		SET_CONFIG(CONF_VAR_PEERTIMEOUT);
	    } else
	    if (EQ(ConfigBuff,_CONF_CLIENTTIMEOUT)) {
		TEST_CONFIG(CONF_VAR_CLIENTTIMEOUT, bit);
		if (!bit) innconf->clienttimeout = atoi(p);
		SET_CONFIG(CONF_VAR_CLIENTTIMEOUT);
	    } else
	    if (EQ(ConfigBuff,_CONF_READERSWHENSTOPPED)) {
		TEST_CONFIG(CONF_VAR_READERSWHENSTOPPED, bit);
		if (!bit && boolval != -1) innconf->readerswhenstopped = boolval;
		SET_CONFIG(CONF_VAR_READERSWHENSTOPPED);
	    } else
	    if (EQ(ConfigBuff,_CONF_ALLOWNEWNEWS)) {
		TEST_CONFIG(CONF_VAR_ALLOWNEWNEWS, bit);
		if (!bit && boolval != -1) innconf->allownewnews = boolval;
		SET_CONFIG(CONF_VAR_ALLOWNEWNEWS);
	    } else
	    if (EQ(ConfigBuff,_CONF_LOCALMAXARTSIZE)) {
		TEST_CONFIG(CONF_VAR_LOCALMAXARTSIZE, bit);
		if (!bit) innconf->localmaxartsize = atoi(p);
		SET_CONFIG(CONF_VAR_LOCALMAXARTSIZE);
	    } else
	    if (EQ(ConfigBuff,_CONF_LOGARTSIZE)) {
		TEST_CONFIG(CONF_VAR_LOGARTSIZE, bit);
		if (!bit && boolval != -1) innconf->logartsize = boolval;
		SET_CONFIG(CONF_VAR_LOGARTSIZE);
	    } else
	    if (EQ(ConfigBuff,_CONF_LOGIPADDR)) {
		TEST_CONFIG(CONF_VAR_LOGIPADDR, bit);
		if (!bit && boolval != -1) innconf->logipaddr = boolval;
		SET_CONFIG(CONF_VAR_LOGIPADDR);
	    } else
	    if (EQ(ConfigBuff,_CONF_CHANINACTTIME)) {
		TEST_CONFIG(CONF_VAR_CHANINACTTIME, bit);
		if (!bit) innconf->chaninacttime = atoi(p);
		SET_CONFIG(CONF_VAR_CHANINACTTIME);
	    } else
	    if (EQ(ConfigBuff,_CONF_MAXCONNECTIONS)) {
		TEST_CONFIG(CONF_VAR_MAXCONNECTIONS, bit);
		if (!bit) innconf->maxconnections = atoi(p);
		SET_CONFIG(CONF_VAR_MAXCONNECTIONS);
	    } else
	    if (EQ(ConfigBuff,_CONF_CHANRETRYTIME)) {
		TEST_CONFIG(CONF_VAR_CHANRETRYTIME, bit);
		if (!bit) innconf->chanretrytime = atoi(p);
		SET_CONFIG(CONF_VAR_CHANRETRYTIME);
	    } else
	    if (EQ(ConfigBuff,_CONF_ARTCUTOFF)) {
		TEST_CONFIG(CONF_VAR_ARTCUTOFF, bit);
		if (!bit) innconf->artcutoff = atoi(p) * 24 * 60 * 60;
		SET_CONFIG(CONF_VAR_ARTCUTOFF);
	    } else
	    if (EQ(ConfigBuff,_CONF_PAUSERETRYTIME)) {
		TEST_CONFIG(CONF_VAR_PAUSERETRYTIME, bit);
		if (!bit) innconf->pauseretrytime = atoi(p);
		SET_CONFIG(CONF_VAR_PAUSERETRYTIME);
	    } else
	    if (EQ(ConfigBuff,_CONF_NNTPLINKLOG)) {
		TEST_CONFIG(CONF_VAR_NNTPLINKLOG, bit);
		if (!bit && boolval != -1) innconf->nntplinklog = boolval;
		SET_CONFIG(CONF_VAR_NNTPLINKLOG);
	    } else
	    if (EQ(ConfigBuff,_CONF_NNTPACTSYNC)) {
		TEST_CONFIG(CONF_VAR_NNTPACTSYNC, bit);
		if (!bit) innconf->nntpactsync = atoi(p);
		SET_CONFIG(CONF_VAR_NNTPACTSYNC);
	    } else
	    if (EQ(ConfigBuff,_CONF_BADIOCOUNT)) {
		TEST_CONFIG(CONF_VAR_BADIOCOUNT, bit);
		if (!bit) innconf->badiocount = atoi(p);
		SET_CONFIG(CONF_VAR_BADIOCOUNT);
	    } else
	    if (EQ(ConfigBuff,_CONF_BLOCKBACKOFF)) {
		TEST_CONFIG(CONF_VAR_BLOCKBACKOFF, bit);
		if (!bit) innconf->blockbackoff = atoi(p);
		SET_CONFIG(CONF_VAR_BLOCKBACKOFF);
	    } else
	    if (EQ(ConfigBuff,_CONF_ICDSYNCCOUNT)) {
		TEST_CONFIG(CONF_VAR_ICDSYNCCOUNT, bit);
		if (!bit) innconf->icdsynccount = atoi(p);
		SET_CONFIG(CONF_VAR_ICDSYNCCOUNT);
	    } else
	    if (EQ(ConfigBuff,_CONF_BINDADDRESS)) {
		TEST_CONFIG(CONF_VAR_BINDADDRESS, bit);
		if (!bit) {
		if (EQ(p,"all") || EQ(p,"any"))
		    innconf->bindaddress =  NULL;
		else
		    innconf->bindaddress = COPY(p);
		}
		SET_CONFIG(CONF_VAR_BINDADDRESS);
	    } else
	    if (EQ(ConfigBuff,_CONF_BINDADDRESS6)) {
		TEST_CONFIG(CONF_VAR_BINDADDRESS6, bit);
		if (!bit) {
		if (EQ(p,"all") || EQ(p,"any"))
		    innconf->bindaddress6 =  NULL;
		else
		    innconf->bindaddress6 = COPY(p);
		}
		SET_CONFIG(CONF_VAR_BINDADDRESS6);
	    } else
	    if (EQ(ConfigBuff,_CONF_PORT)) {
		TEST_CONFIG(CONF_VAR_PORT, bit);
		if (!bit) innconf->port = atoi(p);
		SET_CONFIG(CONF_VAR_PORT);
	    } else
	    if (EQ(ConfigBuff,_CONF_READERTRACK)) {
		TEST_CONFIG(CONF_VAR_READERTRACK, bit);
		if (!bit && boolval != -1) innconf->readertrack = boolval;
		SET_CONFIG(CONF_VAR_READERTRACK);
	    } else 
	    if (EQ(ConfigBuff,_CONF_NFSREADER)) {
		TEST_CONFIG(CONF_VAR_NFSREADER, bit);
		if (!bit && boolval != -1) innconf->nfsreader = boolval;
		SET_CONFIG(CONF_VAR_NFSREADER);
	    } else 
	    if (EQ(ConfigBuff,_CONF_TRADINDEXEDMMAP)) {
		TEST_CONFIG(CONF_VAR_TRADINDEXEDMMAP, bit);
		if (!bit && boolval != -1) innconf->tradindexedmmap = boolval;
		SET_CONFIG(CONF_VAR_TRADINDEXEDMMAP);
	    } else 
	    if (EQ(ConfigBuff,_CONF_NNRPDLOADLIMIT)) {
		TEST_CONFIG(CONF_VAR_NNRPDLOADLIMIT, bit);
		if (!bit) innconf->nnrpdloadlimit = atoi(p);
		SET_CONFIG(CONF_VAR_NNRPDLOADLIMIT);
	    } else 
	    if (EQ(ConfigBuff,_CONF_NNRPPERLAUTH)) {
		TEST_CONFIG(CONF_VAR_NNRPPERLAUTH, bit);
		if (!bit && boolval != -1) innconf->nnrpperlauth = boolval;
		SET_CONFIG(CONF_VAR_NNRPPERLAUTH);
	    } else
	    if (EQ(ConfigBuff,_CONF_NNRPPYTHONAUTH)) {
		TEST_CONFIG(CONF_VAR_NNRPPYTHONAUTH, bit);
		if (!bit && boolval != -1) innconf->nnrppythonauth = boolval;
		SET_CONFIG(CONF_VAR_NNRPPYTHONAUTH);
	    } else
	    if (EQ(ConfigBuff,_CONF_ADDNNTPPOSTINGHOST)) {
		TEST_CONFIG(CONF_VAR_ADDNNTPPOSTINGHOST, bit);
		if (!bit && boolval != -1) innconf->addnntppostinghost = boolval;
		SET_CONFIG(CONF_VAR_ADDNNTPPOSTINGHOST);
	    } else
	    if (EQ(ConfigBuff,_CONF_ADDNNTPPOSTINGDATE)) {
		TEST_CONFIG(CONF_VAR_ADDNNTPPOSTINGDATE, bit);
		if (!bit && boolval != -1) innconf->addnntppostingdate = boolval;
		SET_CONFIG(CONF_VAR_ADDNNTPPOSTINGDATE);
	    } else
	    if (EQ(ConfigBuff,_CONF_STRIPPOSTCC)) {
		TEST_CONFIG(CONF_VAR_STRIPPOSTCC, bit);
		if (!bit && boolval != -1) innconf->strippostcc = boolval;
		SET_CONFIG(CONF_VAR_STRIPPOSTCC);
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
	    if (EQ(ConfigBuff,_CONF_KEYMAXWORDS)) {
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
	    if (EQ(ConfigBuff,_CONF_PATHTMP)) {
		TEST_CONFIG(CONF_VAR_PATHTMP, bit);
		if (!bit) innconf->pathtmp = COPY(p);
		SET_CONFIG(CONF_VAR_PATHTMP);
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
	    if (EQ(ConfigBuff,_CONF_NNRPDPOSTPORT)) {
		TEST_CONFIG(CONF_VAR_NNRPDPOSTPORT, bit);
		if (!bit) innconf->nnrpdpostport = atol(p);
		SET_CONFIG(CONF_VAR_NNRPDPOSTPORT);
	    } else
	    if (EQ(ConfigBuff,_CONF_NNRPDOVERSTATS)) {
		TEST_CONFIG(CONF_VAR_NNRPDOVERSTATS, bit);
		if (!bit && boolval != -1) innconf->nnrpdoverstats = boolval;
		SET_CONFIG(CONF_VAR_NNRPDOVERSTATS);
	    } else
	    if (EQ(ConfigBuff,_CONF_STOREONXREF)) {
		TEST_CONFIG(CONF_VAR_STOREONXREF, bit);
		if (!bit && boolval != -1) innconf->storeonxref = boolval;
		SET_CONFIG(CONF_VAR_STOREONXREF);
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
	    } else
	    if (EQ(ConfigBuff,_CONF_REFUSECYBERCANCELS)) {
		TEST_CONFIG(CONF_VAR_REFUSECYBERCANCELS, bit);
		if (!bit && boolval != -1) innconf->refusecybercancels = boolval;
		SET_CONFIG(CONF_VAR_REFUSECYBERCANCELS);
	    } else
	    if (EQ(ConfigBuff,_CONF_NNRPDCHECKART)) {
		TEST_CONFIG(CONF_VAR_NNRPDCHECKART, bit);
		if (!bit && boolval != -1) innconf->nnrpdcheckart = boolval;
		SET_CONFIG(CONF_VAR_NNRPDCHECKART);
	    } else
	    if (EQ(ConfigBuff,_CONF_NICENEWNEWS)) {
		TEST_CONFIG(CONF_VAR_NICENEWNEWS, bit);
		if (!bit) innconf->nicenewnews = atoi(p);
		SET_CONFIG(CONF_VAR_NICENEWNEWS);
	    } else 
	    if (EQ(ConfigBuff,_CONF_NICENNRPD)) {
		TEST_CONFIG(CONF_VAR_NICENNRPD, bit);
		if (!bit) innconf->nicennrpd = atoi(p);
		SET_CONFIG(CONF_VAR_NICENNRPD);
	    } else 
	    if (EQ(ConfigBuff,_CONF_MERGETOGROUPS)) {
		TEST_CONFIG(CONF_VAR_MERGETOGROUPS, bit);
		if (!bit && boolval != -1) innconf->mergetogroups = boolval;
		SET_CONFIG(CONF_VAR_MERGETOGROUPS);
	    } else
	    if (EQ(ConfigBuff,_CONF_NOREADER)) {
		TEST_CONFIG(CONF_VAR_NOREADER, bit);
		if (!bit && boolval != -1) innconf->noreader = boolval;
		SET_CONFIG(CONF_VAR_NOREADER);
	    } else
	    if (EQ(ConfigBuff,_CONF_SOURCEADDRESS)) {
		TEST_CONFIG(CONF_VAR_SOURCEADDRESS, bit);
		if (!bit) {
		if (EQ(p,"all") || EQ(p,"any"))
		    innconf->sourceaddress =  NULL;
		else
		    innconf->sourceaddress = COPY(p);
		}
		SET_CONFIG(CONF_VAR_SOURCEADDRESS);
	    } else
	    if (EQ(ConfigBuff,_CONF_SOURCEADDRESS6)) {
		TEST_CONFIG(CONF_VAR_SOURCEADDRESS6, bit);
		if (!bit) {
		if (EQ(p,"all") || EQ(p,"any"))
		    innconf->sourceaddress6 =  NULL;
		else
		    innconf->sourceaddress6 = COPY(p);
		}
		SET_CONFIG(CONF_VAR_SOURCEADDRESS6);
	    } else
	    if (EQ(ConfigBuff,_CONF_NNRPDAUTHSENDER)) {
		TEST_CONFIG(CONF_VAR_NNRPDAUTHSENDER, bit);
		if (!bit && boolval != -1) innconf->nnrpdauthsender = boolval;
		SET_CONFIG(CONF_VAR_NNRPDAUTHSENDER);
	    } else
	    if (EQ(ConfigBuff,_CONF_CNFSCHECKFUDGESIZE)) {
		TEST_CONFIG(CONF_VAR_CNFSCHECKFUDGESIZE, bit);
		if (!bit) innconf->cnfscheckfudgesize = atol(p);
		SET_CONFIG(CONF_VAR_CNFSCHECKFUDGESIZE);
	    } else 
	    if (EQ(ConfigBuff,_CONF_RLIMITNOFILE)) {
		TEST_CONFIG(CONF_VAR_RLIMITNOFILE, bit);
		if (!bit) innconf->rlimitnofile = atoi(p);
		SET_CONFIG(CONF_VAR_RLIMITNOFILE);
	    } else 
	    if (EQ(ConfigBuff,_CONF_IGNORENEWSGROUPS)) {
		TEST_CONFIG(CONF_VAR_IGNORENEWSGROUPS, bit);
		if (!bit && boolval != -1) innconf->ignorenewsgroups = boolval;
		SET_CONFIG(CONF_VAR_IGNORENEWSGROUPS);
	    } else 
	    if (EQ(ConfigBuff,_CONF_OVERCACHESIZE)) {
		TEST_CONFIG(CONF_VAR_OVERCACHESIZE, bit);
		if (!bit) innconf->overcachesize = atoi(p);
		SET_CONFIG(CONF_VAR_OVERCACHESIZE);
	    } else 
	    if (EQ(ConfigBuff,_CONF_ENABLEOVERVIEW)) {
		TEST_CONFIG(CONF_VAR_ENABLEOVERVIEW, bit);
		if (!bit && boolval != -1 ) innconf->enableoverview = boolval;
		SET_CONFIG(CONF_VAR_ENABLEOVERVIEW);
	    } else 
	    if (EQ(ConfigBuff,_CONF_WIREFORMAT)) {
		TEST_CONFIG(CONF_VAR_WIREFORMAT, bit);
		if (!bit && boolval != -1 ) innconf->wireformat = boolval;
		SET_CONFIG(CONF_VAR_WIREFORMAT);
	    } else 
	    if (EQ(ConfigBuff,_CONF_OVMETHOD)) {
		TEST_CONFIG(CONF_VAR_OVMETHOD, bit);
		if (!bit) innconf->ovmethod = COPY(p);
		SET_CONFIG(CONF_VAR_OVMETHOD);
	    } else 
	    if (EQ(ConfigBuff,_CONF_USEOVERCHAN)) {
		TEST_CONFIG(CONF_VAR_USEOVERCHAN, bit);
		if (!bit && boolval != -1 ) innconf->useoverchan = boolval;
		SET_CONFIG(CONF_VAR_USEOVERCHAN);
	    } else 
	    if (EQ(ConfigBuff,_CONF_OVGROUPPAT)) {
		TEST_CONFIG(CONF_VAR_OVGROUPPAT, bit);
		if (!bit) innconf->ovgrouppat = COPY(p);
		SET_CONFIG(CONF_VAR_OVGROUPPAT);
	    } else 
	    if (EQ(ConfigBuff,_CONF_GROUPBASEEXPIRY)) {
		TEST_CONFIG(CONF_VAR_GROUPBASEEXPIRY, bit);
		if (!bit && boolval != -1) innconf->groupbaseexpiry = boolval;
		SET_CONFIG(CONF_VAR_GROUPBASEEXPIRY);
	    } else 
	    if (EQ(ConfigBuff,_CONF_WIPCHECK)) {
		TEST_CONFIG(CONF_VAR_WIPCHECK, bit);
		if (!bit) innconf->wipcheck = atoi(p);
		SET_CONFIG(CONF_VAR_WIPCHECK);
	    } else 
	    if (EQ(ConfigBuff,_CONF_WIPEXPIRE)) {
		TEST_CONFIG(CONF_VAR_WIPEXPIRE, bit);
		if (!bit) innconf->wipexpire = atoi(p);
		SET_CONFIG(CONF_VAR_WIPEXPIRE);
	    } else
	    if (EQ(ConfigBuff,_CONF_DONTREJECTFILTERED)) {
		TEST_CONFIG(CONF_VAR_DONTREJECTFILTERED, bit);
		if (!bit && boolval != -1) innconf->dontrejectfiltered = boolval;
		SET_CONFIG(CONF_VAR_DONTREJECTFILTERED);
	    } else 
	    if (EQ(ConfigBuff,_CONF_KEEPMMAPPEDTHRESHOLD)) {
		TEST_CONFIG(CONF_VAR_KEEPMMAPPEDTHRESHOLD, bit);
		if (!bit) innconf->keepmmappedthreshold = atoi(p);
		SET_CONFIG(CONF_VAR_KEEPMMAPPEDTHRESHOLD);
	    } else 
	    if (EQ(ConfigBuff,_CONF_MAXCMDREADSIZE)) {
		TEST_CONFIG(CONF_VAR_MAXCMDREADSIZE, bit);
		if (!bit) innconf->maxcmdreadsize = atoi(p);
		SET_CONFIG(CONF_VAR_MAXCMDREADSIZE);
	    } else 
	    if (EQ(ConfigBuff,_CONF_DATAMOVETHRESHOLD)) {
		TEST_CONFIG(CONF_VAR_DATAMOVETHRESHOLD, bit);
		if (!bit) innconf->datamovethreshold = atoi(p);
		SET_CONFIG(CONF_VAR_DATAMOVETHRESHOLD);
	    } else 
	    if (EQ(ConfigBuff,_CONF_STATHIST)) {
		TEST_CONFIG(CONF_VAR_STATHIST, bit);
		if (!bit) innconf->stathist = COPY(p);
		SET_CONFIG(CONF_VAR_STATHIST);
	    } else 
	    if (EQ(ConfigBuff,_CONF_HISMETHOD)) {
		TEST_CONFIG(CONF_VAR_HISMETHOD, bit);
		if (!bit) innconf->hismethod = COPY(p);
		SET_CONFIG(CONF_VAR_HISMETHOD);
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

/*  $Revision$
**
**  Here be #define's for filenames, socket names, environment variables,
**  and so on.  The numbers refer to sections in the config.dist file.
*/


/*
**  7.  PATHS TO COMMON PROGRAMS
*/
    /* =()<#define _PATH_INND	"@<_PATH_INND>@">()= */
#define _PATH_INND	"/usr/news/bin/innd"
    /* =()<#define _PATH_INNDSTART	"@<_PATH_INNDSTART>@">()= */
#define _PATH_INNDSTART	"/usr/news/bin/inndstart"
    /* =()<#define _PATH_SENDMAIL	"@<_PATH_SENDMAIL>@">()= */
#define _PATH_SENDMAIL	"/usr/sbin/sendmail -oi %s"
    /* =()<#define _PATH_SH		"@<_PATH_SH>@">()= */
#define _PATH_SH		"/bin/sh"
    /* =()<#define _PATH_NNRPD		"@<_PATH_NNRPD>@">()= */
#define _PATH_NNRPD		"/usr/news/bin/nnrpd"
    /* =()<#define _PATH_NNTPD          "@<_PATH_NNTPD>@">()= */
#define _PATH_NNTPD          "/usr/news/bin/nnrpd"
    /* =()<#define _PATH_COMPRESS	"@<_PATH_COMPRESS>@">()= */
#define _PATH_COMPRESS	"/usr/bin/compress"
    /* =()<#define _PATH_RNEWS	"@<_PATH_RNEWS>@">()= */
#define _PATH_RNEWS	"/usr/news/bin/rnews"
    /* =()<#define _PATH_AUTHDIR	"@<_PATH_AUTHDIR>@">()= */
#define _PATH_AUTHDIR	"/usr/news/bin/auth"
    /* =()<#define _PATH_NEWSBIN	"@<_PATH_NEWSBIN>@">()= */
#define _PATH_NEWSBIN	"/usr/news/bin"
    /* =()<#define _PATH_TMP	"@<_PATH_TMP>@">()= */
#define _PATH_TMP	"/var/tmp"
    /* =()<#define _PATH_GZIP	"@<_PATH_GZIP>@">()= */
#define _PATH_GZIP	"/usr/contrib/bin/gzip"


/*
**  8.  PATHS RELATED TO THE SPOOL DIRECTORY
*/
    /* =()<#define _PATH_SPOOL		"@<_PATH_SPOOL>@">()= */
#define _PATH_SPOOL		"/var/news/spool/articles"
    /* =()<#define _PATH_OVERVIEWDIR		"@<_PATH_OVERVIEWDIR>@">()= */
#define _PATH_OVERVIEWDIR		"/var/news/spool/over.view"
    /* =()<#define _PATH_OVERVIEW		"@<_PATH_OVERVIEW>@">()= */
#define _PATH_OVERVIEW		".overview"
    /* =()<#define _PATH_SPOOLNEWS	"@<_PATH_SPOOLNEWS>@">()= */
#define _PATH_SPOOLNEWS	"/var/news/spool/in.coming"
    /* =()<#define _PATH_SPOOLTEMP	"@<_PATH_SPOOLTEMP>@">()= */
#define _PATH_SPOOLTEMP	"/var/tmp"
    /* =()<#define _PATH_BADNEWS	"@<_PATH_BADNEWS>@">()= */
#define _PATH_BADNEWS	"/var/news/spool/in.coming/bad"
    /* =()<#define _PATH_RELBAD		"@<_PATH_RELBAD>@">()= */
#define _PATH_RELBAD		"bad"


/*
**  9.  EXECUTION PATHS FOR INND AND RNEWS
*/
    /* =()<#define _PATH_RNEWS_DUP_LOG	"@<_PATH_RNEWS_DUP_LOG>@">()= */
#define _PATH_RNEWS_DUP_LOG	"/dev/null"
    /* =()<#define _PATH_RNEWSPROGS	"@<_PATH_RNEWSPROGS>@">()= */
#define _PATH_RNEWSPROGS	"/usr/news/bin/rnews.libexec"
    /* =()<#define _PATH_CONTROLPROGS	"@<_PATH_CONTROLPROGS>@">()= */
#define _PATH_CONTROLPROGS	"/usr/news/bin/control"
    /* =()<#define _PATH_BADCONTROLPROG	"@<_PATH_BADCONTROLPROG>@">()= */
#define _PATH_BADCONTROLPROG	"default"


/*
**  10.  SOCKETS CREATED BY INND OR CLIENTS
*/
    /* =()<#define _PATH_INNDDIR	"@<_PATH_INNDDIR>@">()= */
#define _PATH_INNDDIR	"/var/news/run"
    /* =()<#define _PATH_NNTPCONNECT	"@<_PATH_NNTPCONNECT>@">()= */
#define _PATH_NNTPCONNECT	"/var/news/run/nntpin"
    /* =()<#define _PATH_NEWSCONTROL	"@<_PATH_NEWSCONTROL>@">()= */
#define _PATH_NEWSCONTROL	"/var/news/run/control"
    /* =()<#define _PATH_TEMPSOCK	"@<_PATH_TEMPSOCK>@">()= */
#define _PATH_TEMPSOCK	"/var/news/run/ctlinndXXXXXX"


/*
**  11.  LOG AND CONFIG FILES
*/
    /* =()<#define _PATH_NEWSLIB	"@<_PATH_NEWSLIB>@">()= */
#define _PATH_NEWSLIB	"/var/news/etc"
    /* =()<#define _PATH_LOGFILE		"@<_PATH_LOGFILE>@">()= */
#define _PATH_LOGFILE		"/var/log/news/news"
    /* =()<#define _PATH_ERRLOG	"@<_PATH_ERRLOG>@">()= */
#define _PATH_ERRLOG	"/var/log/news/errlog"
    /* =()<#define _PATH_SERVERPID	"@<_PATH_SERVERPID>@">()= */
#define _PATH_SERVERPID	"/var/news/run/innd.pid"
    /* =()<#define _PATH_NEWSFEEDS	"@<_PATH_NEWSFEEDS>@">()= */
#define _PATH_NEWSFEEDS	"/var/news/etc/newsfeeds"
    /* =()<#define _PATH_HISTORY	"@<_PATH_HISTORY>@">()= */
#define _PATH_HISTORY	"/var/news/etc/history"
    /* =()<#define _PATH_INNDHOSTS	"@<_PATH_INNDHOSTS>@">()= */
#define _PATH_INNDHOSTS	"/var/news/etc/hosts.nntp"
    /* =()<#define _PATH_ACTIVE	"@<_PATH_ACTIVE>@">()= */
#define _PATH_ACTIVE	"/var/news/etc/active"
    /* =()<#define _PATH_NEWACTIVE	"@<_PATH_NEWACTIVE>@">()= */
#define _PATH_NEWACTIVE	"/var/news/etc/active.tmp"
    /* =()<#define _PATH_OLDACTIVE	"@<_PATH_OLDACTIVE>@">()= */
#define _PATH_OLDACTIVE	"/var/news/etc/active.old"
    /* =()<#define _PATH_ACTIVETIMES	"@<_PATH_ACTIVETIMES>@">()= */
#define _PATH_ACTIVETIMES	"/var/news/etc/active.times"
    /* =()<#define _PATH_BATCHDIR	"@<_PATH_BATCHDIR>@">()= */
#define _PATH_BATCHDIR	"/var/news/spool/out.going"
    /* =()<#define _PATH_ARCHIVEDIR	"@<_PATH_ARCHIVEDIR>@">()= */
#define _PATH_ARCHIVEDIR	"/var/news/spool/archive"
    /* =()<#define _PATH_DISTPATS	"@<_PATH_DISTPATS>@">()= */
#define _PATH_DISTPATS	"/var/news/etc/distrib.pats"
    /* =()<#define _PATH_NNRPDIST	"@<_PATH_NNRPDIST>@">()= */
#define _PATH_NNRPDIST	"/var/news/etc/distributions"
    /* =()<#define _PATH_NNRPSUBS       "@<_PATH_NNRPSUBS>@">()= */
#define _PATH_NNRPSUBS       "/var/news/etc/subscriptions"
    /* =()<#define _PATH_NEWSGROUPS	"@<_PATH_NEWSGROUPS>@">()= */
#define _PATH_NEWSGROUPS	"/var/news/etc/newsgroups"
    /* =()<#define _PATH_CONFIG	"@<_PATH_CONFIG>@">()= */
#define _PATH_CONFIG	"/var/news/etc/inn.conf"
    /* =()<#define _PATH_CLIENTACTIVE	"@<_PATH_CLIENTACTIVE>@">()= */
#define _PATH_CLIENTACTIVE	"/var/news/etc/active"
    /* =()<#define _PATH_TEMPACTIVE	"@<_PATH_TEMPACTIVE>@">()= */
#define _PATH_TEMPACTIVE	"/var/tmp/activeXXXXXX"
    /* =()<#define _PATH_TEMPMODERATORS	"@<_PATH_TEMPMODERATORS>@">()= */
#define _PATH_TEMPMODERATORS	"/var/tmp/moderatorsXXXXXX"
    /* =()<#define _PATH_MODERATORS	"@<_PATH_MODERATORS>@">()= */
#define _PATH_MODERATORS	"/var/news/etc/moderators"
    /* =()<#define _PATH_SERVER	"@<_PATH_SERVER>@">()= */
#define _PATH_SERVER	"/var/news/etc/server"
    /* =()<#define _PATH_NNTPPASS	"@<_PATH_NNTPPASS>@">()= */
#define _PATH_NNTPPASS	"/var/news/etc/passwd.nntp"
    /* =()<#define _PATH_NNRPACCESS	"@<_PATH_NNRPACCESS>@">()= */
#define _PATH_NNRPACCESS	"/var/news/etc/nnrp.access"
    /* =()<#define _PATH_EXPIRECTL	"@<_PATH_EXPIRECTL>@">()= */
#define _PATH_EXPIRECTL	"/var/news/etc/expire.ctl"
    /* =()<#define _PATH_SCHEMA	"@<_PATH_SCHEMA>@">()= */
#define _PATH_SCHEMA	"/var/news/etc/overview.fmt"
    /* =()<#define _PATH_XBATCHES	"@<_PATH_XBATCHES>@">()= */
#define _PATH_XBATCHES	"/var/news/spool/in.coming"
    /* =()<#define _PATH_MOTD	"@<_PATH_MOTD>@">()= */
#define _PATH_MOTD	"/var/news/etc/motd.news"
    /* =()<#define _PATH_STORAGECTL	"@<_PATH_STORAGECTL>@">()= */
#define _PATH_STORAGECTL	"/var/news/etc/storage.ctl"
    /* =()<#define _PATH_OVERVIEWCTL	"@<_PATH_OVERVIEWCTL>@">()= */
#define _PATH_OVERVIEWCTL	"/var/news/etc/overview.ctl"



/*
**  ENVIRONMENT VARIABLES
*/
    /* The host name of the NNTP server, for client posting. */
#define _ENV_NNTPSERVER		"NNTPSERVER"
    /* The Organization header line, for client posting. */
#define _ENV_ORGANIZATION	"ORGANIZATION"
    /* What to put in the From line, for client posting. */
#define _ENV_FROMHOST		"FROMHOST"
    /* =()<#define _ENV_UUCPHOST	"@<_ENV_UUCPHOST>@">()= */
#define _ENV_UUCPHOST	"UU_MACHINE"


/*
**  PARAMETERS IN THE _PATH_CONFIG FILE.
*/
    /* Host for the From line; default is FQDN. */
#define _CONF_FROMHOST		"fromhost"
    /* NNTP server to post to, if getenv(_ENV_NNTPSERVER) is NULL. */
#define _CONF_SERVER		"server"
    /* Host for the Path line; default is FQDN. */
#define _CONF_PATHHOST		"pathhost"
    /* Data for the Organization line if getenv(_ENV_ORGANIZATION) is NULL. */
#define _CONF_ORGANIZATION	"organization"
    /* Default host to mail moderated articles to. */
#define _CONF_MODMAILER		"moderatormailer"
    /* Default domain of local host. */
#define _CONF_DOMAIN		"domain"
    /* Default mime version. */
#define _CONF_MIMEVERSION	"mime-version"
    /* Default Content-Type */
#define _CONF_CONTENTTYPE	"mime-contenttype"
    /* Default encoding */
#define _CONF_ENCODING		"mime-encoding"
    /* Size of the history cache in kilobytes */
#define _CONF_HISCACHESIZE	"hiscachesize"
    /* Toggle for wireformat articles */
#define _CONF_WIREFORMAT        "wireformat"
    /* Toggle for Xref: slaving */
#define _CONF_XREFSLAVE         "xrefslave"
    /* Toggle for the time spool format */
#define _CONF_TIMESPOOL         "timespool"
    /* Address for X-Complaints-To: header */
#define _CONF_COMPLAINTS        "complaints"
    /* if true, always spool, else, spool only on error */
#define _CONF_NNRP_SPOOLFIRST   "spoolfirst"
   /* if false then don't write crossposts to the history file */
#define _CONF_WRITELINKS        "writelinks"
   /* if 0 or false then don't monitor performance,
      otherwise, the reporting interval */
#define _CONF_TIMER             "timer"
   /* Whether or not to use the storage api */
#define _CONF_STORAGEAPI        "storageapi"
   /* Whether or not to mmap articles */
#define _CONF_ARTMMAP           "articlemmap"
   /* Whether or not to mmap overviews and indices */
#define _CONF_OVERMMAP          "overviewmmap"
     
/*
**  13.  TCL Support
*/

/* =()<#define _PATH_TCL_STARTUP	"@<_PATH_TCL_STARTUP>@">()= */
#define _PATH_TCL_STARTUP	"/usr/news/bin/control/startup.tcl"

/* =()<#define _PATH_TCL_FILTER	"@<_PATH_TCL_FILTER>@">()= */
#define _PATH_TCL_FILTER	"/usr/news/bin/control/filter.tcl"


/*
** 15. Local Configuration
*/

/*  =()<#define _PATH_NEWSHOME	"@<_PATH_NEWSHOME>@">()=  */
#define _PATH_NEWSHOME	"/usr/news"


/*
**  17.  PERL Support
*/

/* =()<#define _PATH_PERL_STARTUP_INND	"@<_PATH_PERL_STARTUP_INND>@">()= */
#define _PATH_PERL_STARTUP_INND	"/usr/news/bin/control/startup_innd.pl"

/* =()<#define _PATH_PERL_FILTER_INND	"@<_PATH_PERL_FILTER_INND>@">()= */
#define _PATH_PERL_FILTER_INND	"/usr/news/bin/control/filter_innd.pl"

/* =()<#define _PATH_PERL_FILTER_NNRPD	"@<_PATH_PERL_FILTER_NNRPD>@">()= */
#define _PATH_PERL_FILTER_NNRPD	"/usr/news/bin/control/filter_nnrpd.pl"

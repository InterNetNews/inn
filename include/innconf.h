/*
    These are the definitions used for inn.conf variables and used
    primarily in lib/getconfig.c

    $Header$
*/

/*
  Note that the value and valtype entries are not used yet (and hence
  set to NULL strings) - they are for future use

  valtype = 1: char
            2: int
            3: long
*/
struct conf_rec {
	const char	*name;
	const char	*value;
	int		valtype;
	int		mustset;
};

struct conf_rec conf_defaults[] = {
#define _CONF_FROMHOST				"fromhost"
#define CONF_VAR_FROMHOST			0
    { _CONF_FROMHOST,				"",	1, 0 },
#define _CONF_SERVER				"server"
#define CONF_VAR_SERVER				1
    { _CONF_SERVER,				"",	1, 1 },
#define _CONF_PATHHOST				"pathhost"
#define CONF_VAR_PATHHOST			2
    { _CONF_PATHHOST,				"",	1, 0 },
#define _CONF_PATHALIAS				"pathalias"
#define CONF_VAR_PATHALIAS			3
    { _CONF_PATHALIAS,				"",	1, 0 },
#define _CONF_ORGANIZATION			"organization"
#define CONF_VAR_ORGANIZATION			4
    { _CONF_ORGANIZATION,			"",	1, 0 },
#define _CONF_MODERATORMAILER			"moderatormailer"
#define CONF_VAR_MODERATORMAILER		5
    { _CONF_MODERATORMAILER,			"",	1, 0 },
#define _CONF_DOMAIN				"domain"
#define CONF_VAR_DOMAIN				6
    { _CONF_DOMAIN,				"",	1, 0 },
#define _CONF_ENABLEOVERVIEW			"enableoverview"
#define CONF_VAR_ENABLEOVERVIEW			7
    { _CONF_ENABLEOVERVIEW,			"",	2, 1 },
#define _CONF_WIREFORMAT			"wireformat"
#define CONF_VAR_WIREFORMAT			8
    { _CONF_WIREFORMAT,				"",	2, 1 },
#define _CONF_OVMETHOD				"ovmethod"
#define CONF_VAR_OVMETHOD			9
    { _CONF_OVMETHOD,				"",	1, 1 },
#define _CONF_HISCACHESIZE			"hiscachesize"
#define CONF_VAR_HISCACHESIZE			10
    { _CONF_HISCACHESIZE,			"",	2, 1 },
#define _CONF_NNRPDAUTHSENDER			"nnrpdauthsender"
#define CONF_VAR_NNRPDAUTHSENDER		11
    { _CONF_NNRPDAUTHSENDER,			"",	2, 1 },
#define _CONF_XREFSLAVE				"xrefslave"
#define CONF_VAR_XREFSLAVE			12
    { _CONF_XREFSLAVE,				"",	2, 1 },
#define _CONF_COMPLAINTS			"complaints"
#define CONF_VAR_COMPLAINTS			13
    { _CONF_COMPLAINTS,				"",	1, 1 },
#define _CONF_SPOOLFIRST			"spoolfirst"
#define CONF_VAR_SPOOLFIRST			14
    { _CONF_SPOOLFIRST,				"",	2, 1 },
#define _CONF_IMMEDIATECANCEL			"immediatecancel"
#define CONF_VAR_IMMEDIATECANCEL		15
    { _CONF_IMMEDIATECANCEL,			"",	2, 1 },
#define _CONF_TIMER				"timer"
#define CONF_VAR_TIMER				16
    { _CONF_TIMER,				"",	2, 1 },
#define _CONF_STATUS				"status"
#define CONF_VAR_STATUS				17
    { _CONF_STATUS,				"",	2, 1 },
#define _CONF_RLIMITNOFILE			"rlimitnofile"
#define CONF_VAR_RLIMITNOFILE			18
    { _CONF_RLIMITNOFILE,			"",     2, 1 },
#define _CONF_ARTICLEMMAP			"articlemmap"
#define CONF_VAR_ARTICLEMMAP			19
    { _CONF_ARTICLEMMAP,			"",	2, 1 },
#define _CONF_IGNORENEWSGROUPS			"ignorenewsgroups"
#define CONF_VAR_IGNORENEWSGROUPS		20
    { _CONF_IGNORENEWSGROUPS,			"",     2, 1 },
#define _CONF_MTA				"mta"
#define CONF_VAR_MTA				21
    { _CONF_MTA,				"",	1, 1 },
#define _CONF_MAILCMD				"mailcmd"
#define CONF_VAR_MAILCMD			22
    { _CONF_MAILCMD,				"",	1, 1 },
#define _CONF_CHECKINCLUDEDTEXT			"checkincludedtext"
#define CONF_VAR_CHECKINCLUDEDTEXT		23
    { _CONF_CHECKINCLUDEDTEXT,			"",	2, 1 },
#define _CONF_MAXFORKS				"maxforks"
#define CONF_VAR_MAXFORKS			24
    { _CONF_MAXFORKS,				"",	2, 1 },
#define _CONF_MAXARTSIZE			"maxartsize"
#define CONF_VAR_MAXARTSIZE			25
    { _CONF_MAXARTSIZE,				"",	3, 1 },
#define _CONF_NICEKIDS				"nicekids"
#define CONF_VAR_NICEKIDS			26
    { _CONF_NICEKIDS,				"",	2, 1 },
#define _CONF_VERIFYCANCELS			"verifycancels"
#define CONF_VAR_VERIFYCANCELS			27
    { _CONF_VERIFYCANCELS,			"",	2, 1 },
#define _CONF_LOGCANCELCOMM			"logcancelcomm"
#define CONF_VAR_LOGCANCELCOMM			28
    { _CONF_LOGCANCELCOMM,			"",	2, 1 },
#define _CONF_WANTTRASH				"wanttrash"
#define CONF_VAR_WANTTRASH			29
    { _CONF_WANTTRASH,				"",	2, 1 },
#define _CONF_REMEMBERTRASH			"remembertrash"
#define CONF_VAR_REMEMBERTRASH			30
    { _CONF_REMEMBERTRASH,			"",	2, 1 },
#define _CONF_LINECOUNTFUZZ			"linecountfuzz"
#define CONF_VAR_LINECOUNTFUZZ			31
    { _CONF_LINECOUNTFUZZ,			"",	2, 1 },
#define _CONF_PEERTIMEOUT			"peertimeout"
#define CONF_VAR_PEERTIMEOUT			32
    { _CONF_PEERTIMEOUT,			"",	2, 1 },
#define _CONF_CLIENTTIMEOUT			"clienttimeout"
#define CONF_VAR_CLIENTTIMEOUT			33
    { _CONF_CLIENTTIMEOUT,			"",	2, 1 },
#define _CONF_READERSWHENSTOPPED		"readerswhenstopped"
#define CONF_VAR_READERSWHENSTOPPED		34
    { _CONF_READERSWHENSTOPPED,			"",	2, 1 },
#define _CONF_ALLOWNEWNEWS			"allownewnews"
#define CONF_VAR_ALLOWNEWNEWS			35
    { _CONF_ALLOWNEWNEWS,			"",	2, 1 },
#define _CONF_LOCALMAXARTSIZE			"localmaxartsize"
#define CONF_VAR_LOCALMAXARTSIZE			36
    { _CONF_LOCALMAXARTSIZE,			"",	3, 1 },
#define _CONF_LOGARTSIZE			"logartsize"
#define CONF_VAR_LOGARTSIZE			37
    { _CONF_LOGARTSIZE,				"",	2, 1 },
#define _CONF_LOGIPADDR				"logipaddr"
#define CONF_VAR_LOGIPADDR			38
    { _CONF_LOGIPADDR,				"",	2, 1 },
#define _CONF_CHANINACTTIME			"chaninacttime"
#define CONF_VAR_CHANINACTTIME			39
    { _CONF_CHANINACTTIME,			"",	2, 1 },
#define _CONF_MAXCONNECTIONS			"maxconnections"
#define CONF_VAR_MAXCONNECTIONS			40
    { _CONF_MAXCONNECTIONS,			"",	2, 1 },
#define _CONF_CHANRETRYTIME			"chanretrytime"
#define CONF_VAR_CHANRETRYTIME			41
    { _CONF_CHANRETRYTIME,			"",	2, 1 },
#define _CONF_ARTCUTOFF				"artcutoff"
#define CONF_VAR_ARTCUTOFF			42
    { _CONF_ARTCUTOFF,				"",	2, 1 },
#define _CONF_PAUSERETRYTIME			"pauseretrytime"
#define CONF_VAR_PAUSERETRYTIME			43
    { _CONF_PAUSERETRYTIME,			"",	2, 1 },
#define _CONF_NNTPLINKLOG			"nntplinklog"
#define CONF_VAR_NNTPLINKLOG			44
    { _CONF_NNTPLINKLOG,			"",	2, 1 },
#define _CONF_NNTPACTSYNC			"nntpactsync"
#define CONF_VAR_NNTPACTSYNC			45
    { _CONF_NNTPACTSYNC,			"",	2, 1 },
#define _CONF_BADIOCOUNT			"badiocount"
#define CONF_VAR_BADIOCOUNT			46
    { _CONF_BADIOCOUNT,				"",	2, 1 },
#define _CONF_BLOCKBACKOFF			"blockbackoff"
#define CONF_VAR_BLOCKBACKOFF			47
    { _CONF_BLOCKBACKOFF,			"",	2, 1 },
#define _CONF_ICDSYNCCOUNT			"icdsynccount"
#define CONF_VAR_ICDSYNCCOUNT			48
    { _CONF_ICDSYNCCOUNT,			"",	2, 1 },
#define _CONF_BINDADDRESS			"bindaddress"
#define CONF_VAR_BINDADDRESS			49
    { _CONF_BINDADDRESS,			"",	1, 1 },
#define _CONF_PORT				"port"
#define CONF_VAR_PORT				50
    { _CONF_PORT,				"",	2, 1 },
#define _CONF_READERTRACK			"readertrack"
#define CONF_VAR_READERTRACK			51
    { _CONF_READERTRACK,			"",	2, 1 },
#define _CONF_STRIPPOSTCC			"strippostcc"
#define CONF_VAR_STRIPPOSTCC			52
    { _CONF_STRIPPOSTCC,			"",	2, 1 },
#define _CONF_OVERCACHESIZE			"overcachesize"
#define CONF_VAR_OVERCACHESIZE			53
    { _CONF_OVERCACHESIZE,			"",	2, 1 },
#define _CONF_KEYWORDS				"keywords"
#define CONF_VAR_KEYWORDS			54
    { _CONF_KEYWORDS,				"",	1, 1 },
#define _CONF_KEYLIMIT				"keylimit"
#define CONF_VAR_KEYLIMIT			55
    { _CONF_KEYLIMIT,				"",	2, 1 },
#define _CONF_KEYARTLIMIT			"keyartlimit"
#define CONF_VAR_KEYARTLIMIT			56
    { _CONF_KEYARTLIMIT,			"",	2, 1 },
#define _CONF_KEYMAXWORDS			"keymaxwords"
#define CONF_VAR_KEYMAXWORDS			57
    { _CONF_KEYMAXWORDS,			"",	2, 1 },
#define _CONF_PATHNEWS				"pathnews"
#define CONF_VAR_PATHNEWS			58
    { _CONF_PATHNEWS,				"",	1, 1 },
#define _CONF_PATHBIN				"pathbin"
#define CONF_VAR_PATHBIN			59
    { _CONF_PATHBIN,				"",	1, 1 },
#define _CONF_PATHFILTER			"pathfilter"
#define CONF_VAR_PATHFILTER			60
    { _CONF_PATHFILTER,				"",	1, 1 },
#define _CONF_PATHDB				"pathdb"
#define CONF_VAR_PATHDB				62
    { _CONF_PATHDB,				"",	1, 1 },
#define _CONF_PATHETC				"pathetc"
#define CONF_VAR_PATHETC			63
    { _CONF_PATHETC,				"",	1, 1 },
#define _CONF_PATHRUN				"pathrun"
#define CONF_VAR_PATHRUN			64
    { _CONF_PATHRUN,				"",	1, 1 },
#define _CONF_PATHLOG				"pathlog"
#define CONF_VAR_PATHLOG			65
    { _CONF_PATHLOG,				"",	1, 1 },
#define _CONF_PATHSPOOL				"pathspool"
#define CONF_VAR_PATHSPOOL			66
    { _CONF_PATHSPOOL,				"",	1, 1 },
#define _CONF_PATHARTICLES			"patharticles"
#define CONF_VAR_PATHARTICLES			67
    { _CONF_PATHARTICLES,			"",	1, 1 },
#define _CONF_PATHOVERVIEW			"pathoverview"
#define CONF_VAR_PATHOVERVIEW			68
    { _CONF_PATHOVERVIEW,			"",	1, 1 },
#define _CONF_PATHOUTGOING			"pathoutgoing"
#define CONF_VAR_PATHOUTGOING			69
    { _CONF_PATHOUTGOING,			"",	1, 1 },
#define _CONF_PATHINCOMING			"pathincoming"
#define CONF_VAR_PATHINCOMING			70
    { _CONF_PATHINCOMING,			"",	1, 1 },
#define _CONF_PATHARCHIVE			"patharchive"
#define CONF_VAR_PATHARCHIVE			71
    { _CONF_PATHARCHIVE,			"",	1, 1 },
#define _CONF_LOGSITENAME			"logsitename"
#define CONF_VAR_LOGSITENAME			72
    { _CONF_LOGSITENAME,			"",	2, 1 },
#define _CONF_PATHHTTP				"pathhttp"
#define CONF_VAR_PATHHTTP			73
    { _CONF_PATHHTTP,				"",	1, 1 },
#define _CONF_NNRPDPOSTHOST			"nnrpdposthost"
#define CONF_VAR_NNRPDPOSTHOST			74
    { _CONF_NNRPDPOSTHOST,			"",	1, 1 },
#define _CONF_CNFSCHECKFUDGESIZE		"cnfscheckfudgesize"
#define CONF_VAR_CNFSCHECKFUDGESIZE		75
    { _CONF_CNFSCHECKFUDGESIZE,			"",	3, 1 },
#define _CONF_NNRPDOVERSTATS			"nnrpdoverstats"
#define CONF_VAR_NNRPDOVERSTATS			76
    { _CONF_NNRPDOVERSTATS,			"",	2, 1 },
#define _CONF_NNRPDPOSTPORT			"nnrpdpostport"
#define CONF_VAR_NNRPDPOSTPORT			77
    { _CONF_NNRPDPOSTPORT,			"",	2, 1 },
#define _CONF_BACKOFFAUTH			"backoffauth"
#define CONF_VAR_BACKOFFAUTH			78
    { _CONF_BACKOFFAUTH,			"",	2, 1 },
#define _CONF_BACKOFFDB				"backoffdb"
#define CONF_VAR_BACKOFFDB			79
    { _CONF_BACKOFFDB,				"",	2, 1 },
#define _CONF_BACKOFFK				"backoffk"
#define CONF_VAR_BACKOFFK			80
    { _CONF_BACKOFFK,				"",	2, 1 },
#define _CONF_BACKOFFPOSTFAST			"backoffpostfast"
#define CONF_VAR_BACKOFFPOSTFAST		81
    { _CONF_BACKOFFPOSTFAST,			"",	2, 1 },
#define _CONF_BACKOFFPOSTSLOW			"backoffpostslow"
#define CONF_VAR_BACKOFFPOSTSLOW		82
    { _CONF_BACKOFFPOSTSLOW,			"",	2, 1 },
#define _CONF_BACKOFFTRIGGER			"backofftrigger"
#define CONF_VAR_BACKOFFTRIGGER			83
    { _CONF_BACKOFFTRIGGER,			"",	2, 1 },
#define _CONF_PATHTMP				"pathtmp"
#define CONF_VAR_PATHTMP			84
    { _CONF_PATHTMP,				"",	1, 1 },
#define _CONF_USEOVERCHAN			"useoverchan"
#define CONF_VAR_USEOVERCHAN			85
    { _CONF_USEOVERCHAN,			"",	1, 1 },
#define _CONF_STOREONXREF			"storeonxref"
#define CONF_VAR_STOREONXREF			86
    { _CONF_STOREONXREF,			"",	2, 1 },
#define _CONF_REFUSECYBERCANCELS		"refusecybercancels"
#define CONF_VAR_REFUSECYBERCANCELS		87
    { _CONF_REFUSECYBERCANCELS,			"",	2, 1 },
#define _CONF_NNRPDCHECKART			"nnrpdcheckart"
#define CONF_VAR_NNRPDCHECKART			88
    { _CONF_NNRPDCHECKART,			"",	2, 1 },
#define _CONF_ADDNNTPPOSTINGHOST		"addnntppostinghost"
#define CONF_VAR_ADDNNTPPOSTINGHOST		89
    { _CONF_ADDNNTPPOSTINGHOST,			"",     2, 1 },
#define _CONF_ADDNNTPPOSTINGDATE		"addnntppostingdate"
#define CONF_VAR_ADDNNTPPOSTINGDATE		90
    { _CONF_ADDNNTPPOSTINGDATE,			"",     2, 1 },
#define _CONF_NICENNRPD				"nicennrpd"
#define CONF_VAR_NICENNRPD			91
    { _CONF_NICENNRPD,				"",     2, 1 },
#define _CONF_NNRPPERLAUTH			"nnrpperlauth"
#define CONF_VAR_NNRPPERLAUTH			92
    { _CONF_NNRPPERLAUTH,			"",	2, 1 },
#define _CONF_NICENEWNEWS			"nicenewnews"
#define CONF_VAR_NICENEWNEWS			93
    { _CONF_NICENEWNEWS,			"",	2, 1 },
#define _CONF_MERGETOGROUPS			"mergetogroups"
#define CONF_VAR_MERGETOGROUPS			95
    { _CONF_MERGETOGROUPS,			"",	2, 1 },
#define _CONF_NOREADER				"noreader"
#define CONF_VAR_NOREADER			96
    { _CONF_NOREADER,				"",	2, 1 },
#define _CONF_SOURCEADDRESS			"sourceaddress"
#define CONF_VAR_SOURCEADDRESS			97
    { _CONF_SOURCEADDRESS,			"",	1, 1 },
#define _CONF_WIPEXPIRE				"wipexpire"
#define CONF_VAR_WIPEXPIRE			98
    { _CONF_WIPEXPIRE,				"",	2, 1 },
#define _CONF_OVGROUPPAT			"ovgrouppat"
#define CONF_VAR_OVGROUPPAT			99
    { _CONF_OVGROUPPAT,				"",	1, 1 },
#define _CONF_GROUPBASEEXPIRY			"groupbaseexpiry"
#define CONF_VAR_GROUPBASEEXPIRY		101
    { _CONF_GROUPBASEEXPIRY,			"",	2, 1 },
#define _CONF_WIPCHECK				"wipcheck"
#define CONF_VAR_WIPCHECK			102
    { _CONF_WIPCHECK,				"",	2, 1 },
#define	_CONF_NNRPPYTHONAUTH			"nnrppythonauth"
#define	CONF_VAR_NNRPPYTHONAUTH			103
    { _CONF_NNRPPYTHONAUTH,			"",     2, 1 },
#define	_CONF_DONTREJECTFILTERED		"dontrejectfiltered"
#define	CONF_VAR_DONTREJECTFILTERED		104
    { _CONF_DONTREJECTFILTERED,			"",     2, 1 },
#define	_CONF_KEEPMMAPPEDTHRESHOLD		"keepmmappedthreshold"
#define	CONF_VAR_KEEPMMAPPEDTHRESHOLD		105
    { _CONF_KEEPMMAPPEDTHRESHOLD,		"",     2, 1 },
#define	_CONF_MAXCMDREADSIZE			"maxcmdreadsize"
#define	CONF_VAR_MAXCMDREADSIZE			106
    { _CONF_MAXCMDREADSIZE,			"",     2, 1 },
#define	_CONF_DATAMOVETHRESHOLD			"datamovethreshold"
#define	CONF_VAR_DATAMOVETHRESHOLD		107
    { _CONF_DATAMOVETHRESHOLD,			"",     2, 1 },
#define _CONF_STATHIST				"stathist"
#define CONF_VAR_STATHIST			108
    { _CONF_STATHIST,				"",     1, 0 },
#define _CONF_HISMETHOD				"hismethod"
#define CONF_VAR_HISMETHOD			109
    { _CONF_HISMETHOD,				"",     1, 1 },
#define _CONF_BINDADDRESS6			"bindaddress6"
#define CONF_VAR_BINDADDRESS6			110
    { _CONF_BINDADDRESS6,			"",	1, 0 },
#define _CONF_SOURCEADDRESS6			"sourceaddress6"
#define CONF_VAR_SOURCEADDRESS6			111
    { _CONF_SOURCEADDRESS6,			"",	1, 0 },
#define _CONF_NFSREADER				"nfsreader"
#define CONF_VAR_NFSREADER			112
    { _CONF_NFSREADER,				"",	2, 0 },
#define _CONF_NFSWRITER				"nfswriter"
#define CONF_VAR_NFSWRITER			113
    { _CONF_NFSWRITER,				"",	2, 0 },
#define _CONF_TRADINDEXEDMMAP			"tradindexedmmap"
#define CONF_VAR_TRADINDEXEDMMAP		114
    { _CONF_TRADINDEXEDMMAP,			"",	2, 0 },
#define _CONF_NNRPDLOADLIMIT			"nnrpdloadlimit"
#define CONF_VAR_NNRPDLOADLIMIT			115
    { _CONF_NNRPDLOADLIMIT,			"",	2, 0 },
};
#define MAX_CONF_VAR 116

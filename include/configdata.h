/*  $Revision$
**
**  Here be configuration data used by various InterNetNews programs.
**  The numbers refer to sections in the config.dist file.
*/

#if ! defined (__configdata_h__)
#define __configdata_h__ 1

/*
**  1.  MAKE CONFIG PARAMETERS
*/
    /* =()<#define @<USE_CHAR_CONST>@_USE_CHAR_CONST>()= */
#define DUNNO_USE_CHAR_CONST
#if	defined(DO_USE_CHAR_CONST)
typedef char const	*STRING;
typedef char * const	CSTRING;
#endif	/* defined(DO_USE_CHAR_CONST) */
#if	defined(DONT_USE_CHAR_CONST)
typedef char		*STRING;
typedef char		*CSTRING;
#endif	/* defined(DONT_USE_CHAR_CONST) */
#if	defined(DUNNO_USE_CHAR_CONST)
#if	defined(__STDC__)
typedef char const	*STRING;
typedef char * const	CSTRING;
#else
typedef char		*STRING;
typedef char		*CSTRING;
#endif	/* defined(__STDC__) */
#endif	/* defined(DUNNO_USE_CHAR_CONST) */

/*
**  Declare a function that doesn't return.
*/
#if	defined(__dead)
    /* BSD4.4 */
#define NORETURN	__dead void
#else
#if	defined(__GNUC__)
    /* GCC */
#define NORETURN	volatile void
#else
    /* Everyone else */
#define NORETURN	void
#endif	/* defined(__GNUC__) */
#endif	/* defined(__dead) */


/*
**  3.  OWNERSHIPS AND FILE MODES
*/
    /* =()<#define NEWSUSER		"@<NEWSUSER>@">()= */
#define NEWSUSER		"news"
    /* =()<#define NEWSGID		"@<NEWSGROUP>@">()= */
#define NEWSGID		"news"
    /* =()<#define NEWSMASTER		"@<NEWSMASTER>@">()= */
#define NEWSMASTER		"usenet"
    /* =()<#define PATHMASTER		"@<PATHMASTER>@">()= */
#define PATHMASTER		"not-for-mail"
    /* Umask to set. */
    /* =()<#define NEWSUMASK		@<NEWSUMASK>@>()= */
#define NEWSUMASK		02
    /* Mode that incoming articles are created under. */
    /* =()<#define ARTFILE_MODE	@<ARTFILE_MODE>@>()= */
#define ARTFILE_MODE	0664
    /* Mode that batch files are created under. */
    /* =()<#define BATCHFILE_MODE	@<BATCHFILE_MODE>@>()= */
#define BATCHFILE_MODE	0664
    /* Mode that directories are created under. */
    /* =()<#define GROUPDIR_MODE	@<GROUPDIR_MODE>@>()= */
#define GROUPDIR_MODE	0775


/*
**  4.  C LIBRARY DIFFERENCES
*/
    /* Use BSD4.2 or Posix directory names? */
    /* =()<#define DIR_@<DIR_STYLE>@>()= */
#define DIR_DIRENT
    /* Use flock, lockf, or nothing to lock files? */
    /* =()<#define LOCK_@<LOCK_STYLE>@>()= */
#define LOCK_FLOCK
    /* Does your (struct tm) have a tm_gmtoff field? */
    /* =()<#define @<HAVE_TM_GMTOFF>@_HAVE_TM_GMTOFF>()= */
#define DONT_HAVE_TM_GMTOFF
    /* Does your (struct stat) have a st_blksize field? */
    /* =()<#define @<HAVE_ST_BLKSIZE>@_HAVE_ST_BLKSIZE>()= */
#define DO_HAVE_ST_BLKSIZE
    /* Use "union wait" instead of int? */
    /* =()<#define @<USE_UNION_WAIT>@_USE_UNION_WAIT>()= */
#define DONT_USE_UNION_WAIT
    /* How to fork? */
    /* =()<#define FORK()	@<FORK>@()>()= */
#define FORK()	vfork()
    /* Does your AF_UNIX bind use sizeof for the socket size? */
    /* =()<#define @<BIND_USE_SIZEOF>@_BIND_USE_SIZEOF>()= */
#define DO_BIND_USE_SIZEOF
    /* Do you have Unix-domain sockets? */
    /* =()<#define @<HAVE_UNIX_DOMAIN>@_HAVE_UNIX_DOMAIN>()= */
#define DO_HAVE_UNIX_DOMAIN
    /* How should close-on-exec be done? */
    /* =()<#define CLX_@<CLX_STYLE>@>()= */
#define CLX_FCNTL
    /* How should non-blocking I/O be done? */
    /* =()<#define NBIO_@<NBIO_STYLE>@>()= */
#define NBIO_FCNTL
    /* How should resource-totalling be done? */
    /* =()<#define RES_@<RES_STYLE>@>()= */
#define RES_RUSAGE
    /* How to get number of available descriptors? */
    /* =()<#define FDCOUNT_@<FDCOUNT_STYLE>@>()= */
#define FDCOUNT_GETRLIMIT

    /* If greater than -1, then use [gs]etrlimit to set that many descriptors. */
    /* If -1, then no [gs]etrlimit calls are done. */
    /* =()<#define NOFILE_LIMIT		@<NOFILE_LIMIT>@>()= */
#define NOFILE_LIMIT		-1
    /* Do you need <time.h> as well as <sys/time.h>? */
    /* =()<#define @<NEED_TIME>@_NEED_TIME>()= */
#define DONT_NEED_TIME
    /* What predicate, if any, the <ctype.h> macros need. */
    /* =()<#define CTYPE(isXXXXX, c)	(@<CTYPE>@)>()= */
#define CTYPE(isXXXXX, c)	((isascii((c)) && isXXXXX((c))))

/*
**  6.  MISCELLANEOUS CONFIG DATA
*/
    /* Use mmap() to read the active file, or read it in? */
    /* =()<#define ACT_@<ACT_STYLE>@>()= */
#define ACT_READ
    /* Should the routines that use mmap() also do a msync(). */
    /* =()<#define @<MMAP_SYNC>@_MMAP_SYNC>()= */
#define DONT_MMAP_SYNC
    /* Does the msync() function take 3 arguments? DO or DONT */
    /* =()<#define @<MSYNC_3_ARG>@_MSYNC_3_ARG>()= */
#define DONT_MSYNC_3_ARG
    /* Use our NNTP-server-open routine, or the one in NNTP? */
    /* INND is nicer, but you must install inn.conf files everywhere; NNTP */
    /* is better if you already have lots of /usr/lib/news/server files. */
    /* =()<#define REM_@<REM_STYLE>@>()= */
#define REM_INND
    /* Should rnews save articles that the server rejects? */
    /* =()<#define @<RNEWS_SAVE_BAD>@_RNEWS_SAVE_BAD>()= */
#define DONT_RNEWS_SAVE_BAD
    /* Should rnews syslog articles innd already has? */
    /* =()<#define @<RNEWS_LOG_DUPS>@_RNEWS_LOG_DUPS>()= */
#define DONT_RNEWS_LOG_DUPS
    /* Look in _PATH_RNEWSPROGS for rnews unpackers? */
    /* =()<#define @<RNEWSPROGS>@_RNEWSPROGS>()= */
#define DO_RNEWSPROGS
    /* Should rnews try the local host? */
    /* =()<#define @<RNEWSLOCALCONNECT>@_RNEWSLOCALCONNECT>()= */
#define DO_RNEWSLOCALCONNECT
    /* Put hosts in the inews Path header? */
    /* =()<#define @<INEWS_PATH>@_INEWS_PATH>()= */
#define DO_INEWS_PATH
    /* Munge the gecos field of password entry? */
    /* =()<#define @<MUNGE_GECOS>@_MUNGE_GECOS>()= */
#define DO_MUNGE_GECOS
    /* Value of dbzincore(FLAG) call in innd. */
    /* =()<#define INND_DBZINCORE	@<INND_DBZINCORE>@>()= */
#define INND_DBZINCORE	1
    /* Null-terminated list of unknown commands to not log to syslog. */
    /* =()<#define INND_QUIET_BADLIST	@<INND_QUIET_BADLIST>@>()= */
#define INND_QUIET_BADLIST	NULL
    /* Null-terminated set of illegal distribution patterns. */
    /* =()<#define BAD_DISTRIBS	@<BAD_DISTRIBS>@>()= */
#define BAD_DISTRIBS	"*.*",NULL
    /* File unknown "to.*" groups into the "to" newsgroup? */
    /* =()<#define @<MERGE_TO_GROUPS>@_MERGE_TO_GROUPS>()= */
#define DONT_MERGE_TO_GROUPS
    /* Have innd throttle itself after this many I/O errors. */
    /* =()<#define IO_ERROR_COUNT	@<IO_ERROR_COUNT>@>()= */
#define IO_ERROR_COUNT	50
    /* Default value for ctlinnd -t flag. */
    /* =()<#define CTLINND_TIMEOUT	@<CTLINND_TIMEOUT>@>()= */
#define CTLINND_TIMEOUT	0
    /* Flush logs (and NNRP connections) if we go this long with no I/O. */
    /* =()<#define DEFAULT_TIMEOUT	@<DEFAULT_TIMEOUT>@>()= */
#define DEFAULT_TIMEOUT	300
    /* Refuse NNTP connections if load is higher then this; -1 disables. */
    /* =()<#define NNRP_LOADLIMIT	@<NNRP_LOADLIMIT>@>()= */
#define NNRP_LOADLIMIT	16
    /* Don't readdir() spool dir if same group within this many seconds. */
    /* =()<#define NNRP_RESCAN_DELAY	@<NNRP_RESCAN_DELAY>@>()= */
#define NNRP_RESCAN_DELAY	60
    /* Do gethostbyaddr on client addresses in nnrp? */
    /* =()<#define @<NNRP_GETHOSTBYADDR>@_NNRP_GETHOSTBYADDR>()= */
#define DO_NNRP_GETHOSTBYADDR
    /*  Strip Sender from posts that did authenticate? */
    /* =()<#define @<NNRP_AUTH_SENDER>@_NNRP_AUTH_SENDER>()= */
#define DO_NNRP_AUTH_SENDER
    /* Tell resolver _res.options to be fast? */
    /* =()<#define @<FAST_RESOLV>@_FAST_RESOLV>()= */
#define DONT_FAST_RESOLV
    /* Free buffers bigger than this when we're done with them. */
    /* =()<#define BIG_BUFFER	@<BIG_BUFFER>@>()= */
#define BIG_BUFFER	(2 * START_BUFF_SIZE)
    /* A general small buffer. */
    /* =()<#define SMBUF	@<SMBUF>@>()= */
#define SMBUF	256
    /* Buffer for a single article name. */
    /* =()<#define MAXARTFNAME	@<MAXARTFNAME>@>()= */
#define MAXARTFNAME	10
    /* Buffer for a single pathname in the spool directory. */
    /* =()<#define SPOOLNAMEBUFF	@<SPOOLNAMEBUFF>@>()= */
#define SPOOLNAMEBUFF	512
    /* Maximum size of a single header. */
    /* =()<#define MAXHEADERSIZE	@<MAXHEADERSIZE>@>()= */
#define MAXHEADERSIZE	1024
    /* Default number of bytes to hold in memory when buffered. */
    /* =()<#define SITE_BUFFER_SIZE	@<SITE_BUFFER_SIZE>@>()= */
#define SITE_BUFFER_SIZE	(16 * 1024)
    /* Should Inn be calling setsockopt() on network fds. */
    /* =()<#define @<SET_SOCKOPT>@_SET_SOCKOPT @<SET_SOCKOPT>@>()= */
#define DO_SET_SOCKOPT DO
    /* How your DBZ be compiled? Use tagged-hash or splitted tables */
    /* =()<#define @<DBZ_TAGGED_HASH>@_TAGGED_HASH>()= */
#define DO_TAGGED_HASH
    /* Do you have shadow file? */
    /* =()<#define @<HAVE_SHADOW>@_HAVE_SHADOW @<HAVE_SHADOW>@>()= */
#define DONT_HAVE_SHADOW DONT


    /* Function that returns no value, and a pointer to it. */
    /* =()<#define FUNCTYPE	@<FUNCTYPE>@>()= */
#define FUNCTYPE	void
typedef FUNCTYPE	(*FUNCPTR)();



/* Some default values that can be overridden by values inn.conf
      These values used to be in config.data */
#define	MAX_FORKS		10
#define DO_VERIFY_CANCELS		/* Incl the code */
#define DO_LOG_CANCEL_COMMANDS		/* Incl the code */

    /* While reading input, if we have less than LOW_WATER bytes free, we
     * use the current buffersize as input to GROW_AMOUNT to determine how
     * much to realloc.  Growth must be at least NNTP_STRLEN bytes! */
#define START_BUFF_SIZE		(4 * 1024)
#define LOW_WATER		(1 * 1024)
#define GROW_AMOUNT(x)		((x) < 128 * 1024 ? (x) : 128 * 1024)

    /* Some debuggers might need this set to an empty string. */
#define STATIC			static

    /* How to store article numbers; note that INN is not int/long clean. */
typedef unsigned long	ARTNUM;

    /* A general convenience; you shouldn't have to change this. */
typedef int		BOOL;

    /* General values that you should not have to change. */
#define MAX_BUILTIN_ARGV	20
#define NNTP_PORT		119
#define TRUE			1
#define FALSE			0
#define MAXLISTEN		25
#define STDIN			0
#define STDOUT			1
#define STDERR			2
#define PIPE_READ		0
#define PIPE_WRITE		1
#define DATE_FUZZ		(24L * 60L * 60L)
#define COMMENT_CHAR		'#'
#define ART_ACCEPT		'+'
#define ART_CANC		'c'
#define ART_JUNK		'j'
#define ART_REJECT		'-'
#define EXP_CONTROL		'!'
#define FEED_MAXFLAGS		20
#define FEED_BYTESIZE		'b'
#define FEED_FULLNAME		'f'
#define FEED_HASH		'h'
#define FEED_HDR_DISTRIB	'D'
#define FEED_HDR_NEWSGROUP	'N'
#define FEED_MESSAGEID		'm'
#define FEED_FNLNAMES		'*'
#define FEED_HEADERS		'H'
#define FEED_NAME		'n'
#define FEED_NEWSGROUP		'g'
#define FEED_OVERVIEW		'O'
#define FEED_PATH		'P'
#define FEED_REPLIC		'R'
#define FEED_SITE		's'
#define FEED_TIMERECEIVED	't'
#define FEED_TIMEPOSTED		'p'
#define HIS_BADCHAR		'_'
#define HIS_FIELDSEP		'\t'
#define HIS_NOEXP		"-"
#define HIS_SUBFIELDSEP		'~'
#define NF_FIELD_SEP		':'
#define NF_FLAG_ALIAS		'='
#define NF_FLAG_EXCLUDED	'j'
#define NF_FLAG_MODERATED	'm'
#define NF_FLAG_OK		'y'
#define NF_FLAG_NOLOCAL		'n'
#define NF_FLAG_IGNORE		'x'
#define NF_SUBFIELD_SEP		'/'
#define NG_SEPARATOR		","
#define NG_ISSEP(c)		((c) == ',')
#define RNEWS_MAGIC1		'#'
#define RNEWS_MAGIC2		'!'
#define SIG_MAXLINES		4
#define SIG_SEPARATOR		"-- \n"
#define SUB_DEFAULT		FALSE
#define SUB_NEGATE		'!'
#define SUB_POISON		'@'
#define LOOPBACK_HOST		"127.0.0.1"

/*
**  13.  TCL Support
*/

    /* =()<#define @<TCL_SUPPORT>@_TCL>()= */
#define DONT_TCL


/*
**  17.  Perl Support
*/

    /* =()<#define @<PERL_SUPPORT>@_PERL>()= */
#define DONT_PERL


#endif /* ! defined (__configdata_h__) */

/*  $Id$
**
**  Here be configuration data used by various InterNetNews programs.  This
**  file is used as source for the autoheader script, which from it and
**  configure.in generates a config.h.in file that will be used by autoconf
**  as the repository of all wisdom.
**
**  Stuff before TOP and after BOTTOM is copied verbatim to config.h.in;
**  the rest of this file is in the desired form for autoconfiscation.
**  Only stuff that autoconf 2.13 couldn't figure out for itself or that
**  needs a larger comment is included here.
*/

#ifndef CONFIG_H
#define CONFIG_H 1

/* Portable defines that don't rely on autoconf results come from here. */
#include "inn/defines.h"

/*
**  GENERAL SETTINGS
**
**  Look over these settings and make sure they're correct for your site.
**  These values don't come from configure and therefore may need manual
**  editing.  The defaults normally should be fine.
**
**  For boolean #defines, uncomment and change #undef to #define to enable,
**  do the reverse to disable.
*/

/* A null-terminated list of uwildmat(3) patterns matching illegal
   distributions.  inews and nnrpd will reject posts with a distribution
   matching one of these patterns. */
#define BAD_DISTRIBS            "*.*", NULL

/* Default timeout period for ctlinnd, overridden by the -t flag.  If set to
   zero, ctlinnd will never time out, but will check every two minutes to
   see if the server is still running so it won't hang forever on a dead
   server. */
#define CTLINND_TIMEOUT         0

/* Reject articles posted more than this many seconds in the future. */
#define DATE_FUZZ               (24L * 60L * 60L)

/* innd will flush the history and active file after this many seconds.
   nnrpd will drop a connection during POST if it goes this long without
   further I/O from the client. */
#define DEFAULT_TIMEOUT         300

/* Define if inews should put hostnames into the Path header itself. */
#define DO_INEWS_PATH

/* Define if inews should munge the GECOS entry of the passwd file when
   attempting to determine a poster's real name.  Use this if your GECOS
   entries have other stuff after trailing commas or before dashes, things
   in parentheses that aren't part of the name, etc.  See frontends/inews.c
   for the full algorithm. */
#define DO_MUNGE_GECOS

/* Define if rnews should try to connect to the local host. */
#define DO_RNEWSLOCALCONNECT

/* Define if rnews should syslog articles rejected as duplicates. */
/* #undef DO_RNEWS_LOG_DUPS */

/* Define if rnews should look in _PATH_RNEWSPROGS for batch unpackers. */
#define DO_RNEWSPROGS

/* Define if rnews should save articles rejected by the server. */
/* #undef DO_RNEWS_SAVE_BAD */

/* Value to pass to dbzincore() inside innd.  Under some bizarre low memory
   circumstance you may want this not to be 1, but normally you always want
   to load the full history indexes into innd's memory.  Has no effect if
   using tagged hash (which is always in core). */
#define INND_DBZINCORE          1

/* A null-terminated list of unknown commands that, when seen by innd,
   shouldn't be logged to syslog.  Normally innd logs all unknown commands,
   but sometimes some are so frequent that it's not worth it. */
#define INND_QUIET_BADLIST      NULL

/* innd will throttle itself after this many I/O errors.  The count is reset
   on a ctlinnd go.  (ENOSPC is special and will always cause an immediate
   throttle.) */
#define IO_ERROR_COUNT          50

/* Length of listen queue for innd. */
#define MAXLISTEN               25

/* The standard NNTP port. */
#define NNTP_PORT               119

/* What to use for a Path tail for local posts. */
#define PATHMASTER              "not-for-mail"


/*
**  CONFIGURE RESULTS
**
**  Things determined automatically by autoconf.  Nothing below this point
**  should require manual editing; if anything here is wrong, see if you
**  should be passing a flag to configure to set it correctly for your
**  system.
**
**  Be aware that success of some tests will cause other tests to be skipped
**  since their results aren't then needed.  For example, if you have
**  standard C headers, INN won't bother looking for stdlib.h, and
**  HAVE_STDLIB_H will be false whether you have it or not.  This is normal.
**
**  Fodder for autoheader is provided in sort -df order (alphabetical,
**  case-insensitive, ignoring punctuation) to make it easier to check
**  whether a given entry is in the file.
*/
@TOP@

/* Define to a suitable 32-bit type if standard headers don't define.  */
#undef int32_t

/* Define to `int' if <signal.h> doesn't define.  */
#undef sig_atomic_t

/* Define to `int' if <sys/socket.h> doesn't define.  */
#undef socklen_t

/* Define to `int' if <sys/types.h> doesn't define.  */
#undef ssize_t

/* Define to a suitable 32-bit type if standard headers don't define.  */
#undef uint32_t

@BOTTOM@


/*
**  BUFFER SIZES AND DATA LENGTHS
**
**  You shouldn't need to change any of the following, and changing some of
**  them may cause other things to break.  Some standard buffer sizes and
**  lengths of data types for various different things.
*/

/* The data type to use for article numbers.  This probably can't be
   increased without a lot of work due to assumptions about the active file
   format, etc. */
typedef unsigned long           ARTNUM;

/* Input buffers start at START_BUFF_SIZE.  While reading input, if we have
   less than LOW_WATER bytes left free in the buffer, use the current
   buffersize as input to GROW_AMOUNT to determine how much to realloc.
   Growth must be at least NNTP_STRLEN bytes!  The default settings provide
   aggressive, exponential buffer growth. */
#define START_BUFF_SIZE         (4 * 1024)
#define LOW_WATER               (1 * 1024)
#define GROW_AMOUNT(x)          ((x) < 128 * 1024 ? (x) : 128 * 1024)

/* The size of a large buffer.  Free dynamically allocated buffers larger
   than this when we're done with them. */
#define BIG_BUFFER              (2 * START_BUFF_SIZE)

/* The maximum length of a single header, used as a good guess at a buffer
   size for some header parsing code.  This is currently also used by innd
   to determine whether to reject a message for an excessively long header;
   this behavior should be fixed. */
#define MAXHEADERSIZE           1024

/* Default buffer size for outgoing feeds from innd. */
#define SITE_BUFFER_SIZE        (16 * 1024)

/* The size of a small buffer. */
#define SMBUF                   256

/* Maximum size of a pathname in the spool directory. */
#define SPOOLNAMEBUFF           512


/*
**  LEGACY
**
**  Everything below this point is here so that parts of INN that haven't
**  been tweaked to use more standard constructs don't break.  Don't count
**  on any of this staying in this file.  If you have a chance, consider
**  following the comments before each item and fixing it.
*/

/* Only used in innd/art.c, should be moved there. */
#define ART_ACCEPT              '+'
#define ART_CANC                'c'
#define ART_STRSTR              '?'
#define ART_JUNK                'j'
#define ART_REJECT              '-'

/* Making this a #define is just confusing; it's never going to change. */
#define COMMENT_CHAR            '#'

/* Used to send commands to exploders.  Should be moved into a more specific
   header file; used by innd/site.c and backends/buffchan.c. */
#define EXP_CONTROL             '!'

/* Only used internal to innd, should be moved to innd/innd.h. */
#define FEED_BYTESIZE           'b'
#define FEED_FULLNAME           'f'
#define FEED_HASH               'h'
#define FEED_HDR_DISTRIB        'D'
#define FEED_HDR_NEWSGROUP      'N'
#define FEED_MESSAGEID          'm'
#define FEED_FNLNAMES           '*'
#define FEED_HEADERS            'H'
#define FEED_NAME               'n'
#define FEED_STOREDGROUP        'G'
#define FEED_NEWSGROUP          'g'
#define FEED_OVERVIEW           'O'
#define FEED_PATH               'P'
#define FEED_REPLIC             'R'
#define FEED_SITE               's'
#define FEED_TIMEEXPIRED        'e'
#define FEED_TIMERECEIVED       't'
#define FEED_TIMEPOSTED         'p'

/* Maximum number of flags for a feed in newsfeeds.  Only used in innd,
   should be moved there (or made dynamic). */
#define FEED_MAXFLAGS           20

/* Moved to inn.conf, but nnrpd is still using this value and the inn.conf
   code is using it as a default.  Maximum number of times to try forking
   before giving up. */
#define MAX_FORKS               10

/* Maximum length of argv vectors used in innd/site.c.  This should be moved
   out of here into that file, or even better hard-coded rather than
   defined; this value isn't affected by user data and the right value can
   be determined by looking at the code and seeing how big of an argv it
   will attempt to construct. */
#define MAX_BUILTIN_ARGV        20

/* Special characters for newsfeeds entries.  Should be moved to a more
   specific header file. */
#define NF_FIELD_SEP            ':'
#define NF_SUBFIELD_SEP         '/'

/* active file flags.  Should be moved to a more specific header file. */
#define NF_FLAG_ALIAS		'='
#define NF_FLAG_EXCLUDED	'j'
#define NF_FLAG_MODERATED	'm'
#define NF_FLAG_OK		'y'
#define NF_FLAG_NOLOCAL		'n'
#define NF_FLAG_IGNORE		'x'

/* Used for parsing the Newsgroups header.  Should be rolled into a library
   for parsing headers, combining all the code that's currently scattered
   all over INN for doing that. */
#define NG_SEPARATOR            ","
#define NG_ISSEP(c)             ((c) == ',')

/* Only used by rnews.  Should be moved there. */
#define RNEWS_MAGIC1            '#'
#define RNEWS_MAGIC2            '!'

/* Only used by inews.  Should be moved there. */
#define SIG_MAXLINES		4
#define SIG_SEPARATOR		"-- \n"

/* Used internally by innd for storing group subscriptions for feeds.
   Should be moved into innd-specific headers. */
#define SUB_DEFAULT		FALSE
#define SUB_NEGATE		'!'
#define SUB_POISON		'@'

/* There's no reason to make all of these #defines except possibly for
   L_CC_CMD and even that's a stretch.  Since we're logging to our own
   distinguished log facility, provided that we spread things out between a
   reasonable variety of log levels, the sysadmin shouldn't have to change
   any of this.  (Some of this is arguably wrong; L_NOTICE should be
   LOG_NOTICE, for example.) */

/* Flags to use in opening the logs; some programs add LOG_PID. */
#define L_OPENLOG_FLAGS         (LOG_CONS | LOG_NDELAY)

/* Fatal error, program is about to exit. */
#define L_FATAL                 LOG_CRIT

/* Log an error that might mean one or more articles get lost. */
#define L_ERROR                 LOG_ERR

/* Informational notice, usually not worth caring about. */
#define L_NOTICE                LOG_WARNING

/* A protocol trace. */
#define L_TRACE                 LOG_DEBUG

/* All incoming control commands (ctlinnd, etc). */
#define L_CC_CMD                LOG_INFO

#endif /* !CONFIG_H */

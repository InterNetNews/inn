/* -*- c -*-
 *
 * Author:      James A. Brister <brister@vix.com> -- berkeley-unix --
 * Start Date:  Sun, 28 Jan 1996 18:37:49 +1100
 * Project:     INN -- innfeed
 * File:        msgs.h
 * RCSId:       $Id$
 *
 * Copyright:   Copyright (c) 1996 by Internet Software Consortium
 *
 *              Permission to use, copy, modify, and distribute this
 *              software for any purpose with or without fee is hereby
 *              granted, provided that the above copyright notice and this
 *              permission notice appear in all copies.
 *
 *              THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE
 *              CONSORTIUM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 *              SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *              MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET
 *              SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 *              INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *              WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 *              WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 *              TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
 *              USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Description: 
 * 
 */

#if ! defined ( msgs_h__ )
#define msgs_h__

/* Messages used in connection.c */

#define ARTICLE_TIMEOUT_MSG     "%s:%d idle tearing down connection"
#define ARTICLE_TIMEOUT_W_Q_MSG "%s:%d idle connection still has articles"

#define CONNECT_ERROR           "%s:%d connect : %m"
#define CONNECTED               "%s:%d connected"
#define CXN_CLOSED              "%s:%d closed"
#define CXN_PERIODIC_CLOSE      "%s:%d periodic close"
#define CXN_REOPEN_FAILED	"%s:%d flush re-connect failed"

#define CXN_BUFFER_EXPAND_ERROR "%s:%d cxnsleep can't expand input buffer"
#define FCNTL_ERROR             "%s:%d cxnsleep can't set socket non-blocking: %m"
#define IHAVE_WRITE_FAILED      "%s:%d cxnsleep can't write IHAVE body : %m"
#define MODE_STREAM_FAILED	"%s:%d cxnsleep can't write MODE STREAM : %m"
#define QUIT_WRITE_FAILED       "%s:%d cxnsleep can't write QUIT : %m"
#define COMMAND_WRITE_FAILED    "%s:%d cxnsleep can't write command : %m"
#define RESPONSE_READ_FAILED    "%s:%d cxnsleep can't read response : %m"
#define BANNER_READ_FAILED      "%s:%d cxnsleep can't read banner : %m"
#define SOCKET_CREATE_ERROR     "%s:%d cxnsleep can't create socket : %m"
#define MODE_WRITE_PENDING	"%s:%d cxnsleep mode stream command still pending"
#define NOMSGID                 "%s:%d cxnsleep message-id missing in reponse code %d: %s"
#define INVALID_MSGID           "%s:%d cxnsleep message-id invalid message-id in reponse code %d : %s"
#define UNKNOWN_RESPONSE        "%s:%d cxnsleep response unknown : %d : %s"
#define UNKNOWN_BANNER          "%s:%d cxnsleep response unknown banner %d : %s"
#define BAD_RESPONSE		"%s:%d cxnsleep response unexpected : %d"

#define BAD_MODE_RESPONSE       "%s:%d cxnsleep response to MODE STREAM : %s"
#define INVALID_RESP_FORMAT     "%s:%d cxnsleep response format : %s"
#define PREPARE_READ_FAILED     "%s:%d cxnsleep prepare read failed"
#define RESPONSE_TIMEOUT        "%s:%d cxnsleep non-responsive connection"
#define WRITE_TIMEOUT           "%s:%d cxnsleep write timeout"
#define GETSOCKOPT_FAILED       "%s:%d cxnsleep internal getsockopt : %m"
#define CONNECTION_FAILURE      "%s:%d cxnsleep connect : %m"
#define IO_FAILED               "%s:%d cxnsleep i/o failed : %m"
#define NO_TRANSFER_NNRPD	"%s:%d cxnsleep transfer permission denied"
#define NO_TALK_NNRPD		"%s:%d cxnsleep no permission to talk: %s"
#define CXN_BAD_STATE		"%s:%d cxnsleep connection in bad state: %s"
#define CXN_STREAM_RESP		"%s:%d cxnsleep unexpected streaming response for non-streaming connection: %s"
#define CXN_NONSTREAM_RESP	"%s:%d cxnsleep unexpected non-streaming response for streaming connection: %s"

#define MISSING_ART_IHAVE_BODY	"%s:%d missing article for IHAVE-body"

#define NOCR_MSG                "%s:%d remote not giving out CR characters"
#define PREPARE_WRITE_FAILED    "%s:%d fatal prepare write failed"
#define QUIT_WHILE_WRITING      "%s:%d internal QUIT while write pending"

#define CXN_BLOCKED             "%s:%d remote cannot accept articles: %s"

			/* key word is "checkpoint" or "final" */
#define STATS_MSG		"%s:%d %s seconds %ld offered %d accepted %d refused %d rejected %d accsize %.0f rejsize %.0f"



  /* messages used in host.c */

#define NO_STATUS		"ME oserr status file open: %s : %m"

#define CONNECTION_DISAPPEARING	"%s:%d connection vanishing"
#define STREAMING_MODE_SWITCH   "%s mode no-CHECK entered [%.2f,%.2f,%.2f]"
#define STREAMING_MODE_UNDO     "%s mode no-CHECK exited [%.2f,%.2f,%.2f]"

#define REALLY_FINAL_STATS	"%s global seconds %ld offered %d accepted %d refused %d rejected %d missing %d %s spooled %d unspooled %d"
#define PROCESS_FINAL_STATS	"ME global seconds %ld offered %ld accepted %ld refused %ld rejected %ld missing %ld %s spooled %ld unspooled %ld"
		/* key word in next two is "checkpoint" or "final" */
#define HOST_STATS_MSG		"%s %s seconds %ld offered %d accepted %d refused %d rejected %d missing %d %s spooled %d on_close %d unspooled %d deferred %d/%.1f requeued %d queue %.1f/%d:%.0f,%.0f,%.0f,%.0f,%.0f,%.0f"
#define HOST_SPOOL_STATS        "%s %s seconds %ld spooled %d on_close %d sleeping %d"

#define HOST_MAX_CONNECTIONS    "%s hostChkCxns - maxConnections was %d now %d"

#define REMOTE_BLOCKED          "%s remote cannot accept articles initial : %s"
#define REMOTE_STILL_BLOCKED    "%s remote cannot accept articles still : %s"
#define CHANGED_REMOTE_BLOCKED  "%s remote cannot accept articles change : %s"
#define SPOOLING                "%s spooling no active connections"
#define BACKLOG_TO_TAPE         "%s spooling some backlog."
#define SPOOL_DEFERRED          "%s spooling deferred articles"
#define REMOTE_DOES_STREAMING   "%s remote MODE STREAM"
#define REMOTE_STREAMING_OFF	"%s remote MODE STREAM disabled"
#define REMOTE_NO_STREAMING     "%s remote MODE STREAM failed"
#define STREAMING_CHANGE        "%s remote MODE STREAM change"
#define HOST_RESOLV_ERROR       "%s can't resolve hostname: %s : %s"

/* messages used in innlistener.c */
#define INN_GONE                "ME source lost . Exiting"
#define INN_IO_ERROR            "ME source read error Exiting : %m"
#define INN_BAD_CMD             "ME source format bad Exiting : %s"
#define INN_MSGID_SIZE          "ME message id exceeds limit of %d octets : %s"
#define TOO_MANY_HOSTS          "ME internal too may hosts. (max is %d)"
#define DYNAMIC_PEER            "ME unconfigured peer %s added"
#define UNKNOWN_PEER            "ME unconfigured peer %s"
#define INVALID_PEER            "ME invalid peername %s"
#define STOPPING_PROGRAM        "ME finishing at %s"
#define FAST_EXIT_PROGRAM       "ME finishing (quickly) at %s"
#define NO_DROPPED_FILE		"ME cant open %s: loosing articles: %m"
#define NO_NULL_FILE		"ME cant open /dev/null: %m"
#define LOST_ARTICLE_COUNT	"ME lost %ld articles"
#define DROPPED_ARTICLE_COUNT	"ME dropped %ld articles"
#define DROPPED_LOCATION	"ME dropping articles into %s"

#define L_BUFFER_EXPAND_ERROR	"ME error expanding input buffer"
#define L_PREPARE_READ_FAILED	"ME error prepare read failed"

#define SHRINK_TEMP_CREATE      "ME error creating temp shrink file for %s: %m"
#define SHRINK_TEMP_OPEN	"ME error opening temp shrink file %s: %m"
#define SHRINK_SEEK		"ME error seeking to point %ld in %s"
#define SHRINK_NONL		"ME no newline in shrinking file %s"
#define SHRINK_WRITETMP		"ME fwrite failed to temp shrink file %s: %m"
#define SHRINK_READ		"ME fread failed on file %s: %m"
#define SHRINK_FREOPEN		"ME freopen on shrink file failed %s %m"
#define SHRINK_WRITEORIG	"ME fwrite failed to shrink file %s %m"


/* messages used in commander.c */

#define CMDR_PREP_RD_FAILED     "ME commander error listen failed"
#define NO_CONNECT              "ME commander connections disallowed"
#define COMMANDER_CONNECT       "ME commander connect %s"


/* endpoint.c */
#define NO_FD_FREE		"ME cant move fd %d above %d" 
#define CLOSE_FAILED		"ME oserr close (%d): %m"
#define DUP2_FAILED		"ME oserr dup2 (%d, %d): %m"
#define FD_TOO_BIG		"ME fd (%d) looks too big (%d -- %s)"
#define BAD_SELECT              "ME exception: select failed: %d %m"
#define GETSOCKOPT_FAILURE      "ME exception: getsockopt (%d): %m"
#define EXCEPTION_NOTICE        "ME exception: fd %d : %m"
#define UNKNOWN_EXCEPTION       "ME exception: fd %d : Unknown error."


/* article.c */

#define EMPTY_ARTICLE		"ME article 0 bytes: %s"
#define DOUBLE_NAME             "ME two filenames for same article: %s: %s"
#define BAD_ART_READ            "ME article read-error : %s : %m"
#define NO_ARTICLE              "ME article missing : %s : %s"
#define FSTAT_FAILURE           "ME oserr fstat %s : %m"
#define REGFILE_FAILURE         "ME article file-type : %s"
#define PREPARED_NEWLINES       "ME newline to file size ratio: %0.2f (%d/%d)"
#define PREPARE_FAILED          "ME internal failed to prepare buffer for NNTP"
#define ACTIVE_ARTICLES         "ME articles active %d bytes %d"
#define ARTICLE_ALLOCS          "ME articles total %d bytes %d"
#define MAX_BYTES_LIMIT         "ME exceeding maximum article byte limit: %d (max), %d (cur)"
#define MMAP_FAILURE		"ME mmap failure %s: %m"
#define MUNGED_ARTICLE		"ME munged article %s"


/* main.c */

			/* first %s is "innfeed v a.b.c" second is ctime fmt */
#define STARTING_PROGRAM        "ME starting %s at %s"
#define NOSUCH_CONFIG           "ME config aborting No such config file: %s"
#define FOPEN_FAILURE           "ME fopen %s : %m"
#define CFG_FOPEN_FAILURE       "ME config aborting fopen %s : %m"
#define PARSE_FAILURE           "ME config aborting Error parsing config file"
#define NO_HOST			"ME locked cannot setup peer %s"
#define NO_X_AND_S              "ME usage aborting Can't use both '-s' and '-x'"
#define SETRLIM_FAILED		"ME oserr setrlimit(RLIM_NOFILE,%d): %m"
#define GETRLIM_FAILED		"ME oserr getrlimit(RLIM_NOFILE): %m"
#define PIPE_FAILURE            "ME fatal pipe: %m"
#define FORK_FAILURE            "ME fatal fork: %m"
#define CD_FAILED               "ME fatal chdir %s : %m"
#define SHUTDOWN_SIGNAL		"ME received shutdown signal"
#define CONFIG_RELOAD		"ME reloading config file %s"
#define INCR_LOGLEVEL		"ME increasing logging level to %d"
#define DECR_LOGLEVEL		"ME decreasing logging level to %d"
#define NO_PATH_MAX		"ME cant determine PATH_MAX. Guessing %ld"
#define IGNORE_SIGALRM		"ME signal SIGALRM in non-funnel-file mode ignored."
/* misc.c */

#define NO_OPEN_LOCK            "ME lock file open: %s : %m"
#define NO_UNLINK_LOCK          "ME lock file unlink: %s : %m"
#define NO_LINK_LOCK            "ME lock file link: %s : %m"
#define NO_WRITE_LOCK_PID       "ME lock file pid-write: %m"
#define BAD_PID                 "ME lock bad-pid info in %s : %s"
#define LOCK_EXISTS		"ME lock in-use already: %s by pid %d"
#define FILE_SHRUNK		"ME file %s shrunk from %ld to %ld"

/* tape.c */

#define FSTAT_NE_FTELL		"ME fstat and ftello do not agree"
#define NO_FACTOR		"%s no backlog-factor or backlog-high-limit"
#define NEW_HAND_FILE		"%s new hand-prepared backlog file."
#define TAPE_OPEN_FAILED        "ME tape open failed (%s) %s: %m"
#define TAPE_SPAGHETTI          "ME tape invalid messageID in %s: %s"
#define FILE_SHORT              "ME tape short : %s %ld %ld"
#define NO_LOCK_TAPE            "ME lock failed for host: %s"
#define TAPE_INPUT_ERROR        "ME ioerr on tape file: %s : %m"
#define FCLOSE_FAILED           "ME ioerr fclose %s : %m"
#define UNLINK_FAILED           "ME oserr unlink %s : %m"
#define RENAME_FAILED           "ME oserr rename %s,%s : %m"
#define FTELL_FAILED            "ME oserr ftello %s : %m"
#define FGETS_FAILED            "ME oserr fgets %s : %m"
#define FSEEK_FAILED            "ME oserr fseeko %s,%ld,SEEK_SET : %m"
#define CHECKPOINT_OPEN         "ME oserr open checkpoint file: %s %m"
#define BAD_CHECKPOINT          "ME internal bad data in checkpoint file: %s"
#define CKPT_BNDRY		"ME internal checkpoint line boundary missed: %s %ld vs. %ld"
#define FLUSHING_TAPES		"ME flushing tapes" 
#define ROTATING_HAND_DROPPED   "%s grabbing external tape file"



#define ADDMISSINGBOOL		"ME config: adding missing key/value %s: %s"
#define ADDMISSINGINT		"ME config: adding missing key/value %s: %ld"
#define ADDMISSINGREAL		"ME config: adding missing key/value %s: %f"
#define BADSPOOL_CHANGE	 	"ME config: definition of news-spool (%s) is a non-existant directory. Using %s"
#define BAD_TAPEDIR_CHANGE	"ME config: definition of backlog-directory (%s) is a non-existant directory. Using %s"
#define BATCH_AND_NO_CXNS	"%s config ignored batch mode with initial connection count of 0"


#define CONFIG_PARSE_FAILED	"%sconfig file error: %s\n"

#define INT_TO_HIGH		"ME config: value of %s (%ld) in %s is higher than maximum of %ld. Using %ld"
#define INT_TO_LOW		"ME config: value of %s (%ld) in %s is lower than minimum of %ld. Using %ld"
#define INT_TOO_LARGE		"ME config: value of %s (%ld) in %s too large. Using %ld"

#define REALTOHIGH 		"ME config: value of %s (%f) is higher than maximum of %f"
#define REALTOLOW		"ME config: value of %s (%f) is lower than minimum of %f"

#define LESS_THAN_ONE		"ME config: value of %s (%ld) in %s cannot be less than 1. Using %ld"
#define LESS_THAN_ZERO		"ME config: value of %s (%ld) in %s cannot be less than 0. Using %ld"

#define NO_PEER_FIELD_BOOL	"ME config: no value for %s in peer %s. Using %s"
#define NO_PEER_FIELD_INT	"ME config: no value for %s in peer %s. Using %ld"
#define NO_PEER_FIELD_REAL	"ME config: no value for %s in peer %s. Using %.2f"

#define DUP_PEER_NAME		"ME config: two peers with the same name: %s"
#define NODEFN			"ME config: no definition for required key %s"
#define NOTBOOLEAN		"ME config: value of %s is not a boolean"
#define NOTINT			"ME config: value of %s is not an integer"
#define NOTREAL			"ME config: value of %s is not a floating point number"


#define NOCK_LOWHIGHCLOSE	"ME config: no-check-low and no-check-high are close together (%f vs %f)"
#define NOCK_LOWVSHIGH		"ME config: no-check-low value greater than no-check-high (%f vs %f). Setting to %f and %f"
#define NOFACTORHIGH		"ME config: must define at least one of backlog-factor and backlog-limit-high. Adding %s: %f"
#define NO_CHANGE_BACKLOG	"ME config: cannot change backlog-directory of a running process"
#define NO_STDIO_FDMAX		"ME config: cannot define stdio-fdmax if FD_SETSIZE is not available"
#define NO_TAPE_DIR		"ME config: no usable value for backlog-directory"
#define SPOOL_NODEF		"ME config: no definition of news-spool, and %s is no good"

#endif


/*  $Id$
**
**  NNTP codes and utility functions for speaking the NNTP protocol.
**
**  The nntp_code enum defines NNTP codes for every response supported by INN.
**  The functions speak the NNTP protocol to a remote server or client.  So
**  far, most of the library support is for servers rather than clients.
**  Buffering is handled internally when reading from the network to allow for
**  easier parsing.
*/

#ifndef INN_NNTP_H
#define INN_NNTP_H 1

#include <sys/types.h>          /* size_t, time_t */

/*
**  NNTP response codes as defined in RFC 977 and elsewhere.
**
**  All NNTP response codes are three digits.  The first digit is one of:
**
**      1xx     Informative message.
**      2xx     Command completed OK.
**      3xx     Command OK so far; send the rest of it.
**      4xx     Command was correct, but couldn't be performed for some
**              reason.
**      5xx     Command unimplemented, or incorrect, or a serious program
**              error occurred.
**
**  The second digit defines the type of command:
**
**      x0x     Connection, setup, and miscellaneous messages
**      x1x     Newsgroup selection
**      x2x     Article selection
**      x3x     Distribution functions
**      x4x     Posting
**      x8x     Reserved for authentication and authorization extensions
**      x9x     Reserved for private use (non-standard extensions)
**
**  Each response code is assigned a symbolic name starting with NNTP_.  The
**  second component of the code is INFO, OK, CONT, FAIL, or ERR,
**  corresponding to the first digit of the response code.  An enum is
**  available for holding NNTP codes specifically.
*/

enum nntp_code {
    NNTP_INFO_HELP              = 100,
    NNTP_INFO_DATE              = 111,
    NNTP_OK_BANNER_POST         = 200,
    NNTP_OK_BANNER_NOPOST       = 201,
    NNTP_OK_EXTENSIONS          = 202,
    NNTP_OK_QUIT                = 205,
    NNTP_OK_GROUP               = 211,
    NNTP_OK_LIST                = 215,
    NNTP_OK_ARTICLE             = 220,
    NNTP_OK_HEAD                = 221,
    NNTP_OK_BODY                = 222,
    NNTP_OK_STAT                = 223,
    NNTP_OK_OVER                = 224,
    NNTP_OK_NEWNEWS             = 230,
    NNTP_OK_NEWGROUPS           = 231,
    NNTP_OK_IHAVE               = 235,
    NNTP_OK_POST                = 240,
    NNTP_CONT_IHAVE             = 335,
    NNTP_CONT_POST              = 340,
    NNTP_FAIL_TERMINATING       = 400,
    NNTP_FAIL_BAD_GROUP         = 411, /* Group unknown */
    NNTP_FAIL_NO_GROUP          = 412, /* Not in a newsgroup */
    NNTP_FAIL_NO_ARTICLE        = 420, /* No current article */
    NNTP_FAIL_NEXT              = 421,
    NNTP_FAIL_PREV              = 422,
    NNTP_FAIL_BAD_ARTICLE       = 423, /* Bad article number */
    NNTP_FAIL_NOTFOUND          = 430, /* Article not found */
    NNTP_FAIL_IHAVE_REFUSE      = 435, /* IHAVE article not wanted */
    NNTP_FAIL_IHAVE_DEFER       = 436, /* IHAVE article deferred */
    NNTP_FAIL_IHAVE_REJECT      = 437, /* IHAVE article rejected */
    NNTP_FAIL_POST_AUTH         = 440, /* Posting not allowed */
    NNTP_FAIL_POST_REJECT       = 441, /* POST article rejected */
    NNTP_ERR_COMMAND            = 500,
    NNTP_ERR_SYNTAX             = 501,
    NNTP_ERR_ACCESS             = 502,
    NNTP_ERR_UNAVAILABLE        = 503,

    /* Streaming extension. */
    NNTP_OK_STREAM              = 203,
    NNTP_OK_CHECK               = 238,
    NNTP_OK_TAKETHIS            = 239,
    NNTP_FAIL_CHECK_DEFER       = 431,
    NNTP_FAIL_CHECK_REFUSE      = 438,
    NNTP_FAIL_TAKETHIS_REJECT   = 439,

    /* Authentication extension. */
    NNTP_OK_AUTHINFO            = 281,
    NNTP_OK_SASL                = 283,
    NNTP_CONT_AUTHINFO          = 381,
    NNTP_CONT_SASL              = 383,
    NNTP_FAIL_AUTH_NEEDED       = 480,
    NNTP_FAIL_AUTHINFO_BAD      = 481,
    NNTP_FAIL_AUTHINFO_REJECT   = 482,

    /* TLS extension.  (These codes will change.) */
    NNTP_CONT_STARTTLS          = 382,
    NNTP_FAIL_STARTTLS          = 483,
    NNTP_ERR_STARTTLS           = 580,

    /* XGTITLE extension (deprecated, use LIST NEWSGROUPS). */
    NNTP_OK_XGTITLE             = 282,
    NNTP_FAIL_XGTITLE           = 481, /* Duplicates NNTP_FAIL_AUTHINFO_BAD */

    /* MODE CANCEL extension.  (These codes should change.) */
    NNTP_OK_MODE_CANCEL         = 284,
    NNTP_OK_CANCEL              = 289,
    NNTP_FAIL_CANCEL            = 484,

    /* XBATCH extension. */
    NNTP_OK_XBATCH              = 239,
    NNTP_CONT_XBATCH            = 339,
    NNTP_FAIL_XBATCH            = 436  /* Duplicates NNTP_FAIL_IHAVE_DEFER */
};


/* Per draft-ietf-nntpext-base-17.txt:

       Command lines MUST NOT exceed 512 octets, which includes the
       terminating US-ASCII CRLF pair.  The arguments MUST NOT exceed 497
       octets.

   Also see below for an additional restriction on message IDs. */

#define NNTP_MAXLEN_COMMAND     512

/* Consensus on the USEFOR mailing list in June of 2000 indicates that the
   next revision of the Usenet article standard will limit the length of the
   message ID to 250 characters.  This is also the limit recommended by
   son-of-1036.

   You can increase this limit if you want, but don't increase it above 497.
   RFC 977 limits each line of the NNTP protocol to 512 octets, including
   the terminating CRLF.  For a message ID to be passed using the TAKETHIS
   command, it can therefore be a maximum of 501 octets.  The November 1999
   draft of the replacement RFC limits it to 497 octets.

   Both Cyclone and DNews are known to reject message IDs longer than 500
   octets as of June of 2000.  DNews has been reported to have problems with
   message IDs of 494 octets. */

#define NNTP_MAXLEN_MSGID       250

/* Forward declaration. */
struct cvector;

/* Opaque struct that holds NNTP connection state. */
struct nntp;

/* Return codes for NNTP reader functions. */
enum nntp_status {
    NNTP_READ_OK,
    NNTP_READ_EOF,
    NNTP_READ_ERROR,
    NNTP_READ_TIMEOUT,
    NNTP_READ_LONG
};

/* Allocate a new nntp struct for a pair of file descriptors.  Takes the
   maximum size for the read buffer; messages longer than this will not be
   read.  Takes the timeout in seconds for subsequent reads (0 means wait
   forever). */
struct nntp *nntp_new(int in, int out, size_t maxsize, time_t timeout);

/* Free an nntp struct and close the connection. */
void nntp_free(struct nntp *);

/* Connect to a remote host and return an nntp struct for that connection. */
struct nntp *nntp_connect(const char *host, unsigned short port,
                          size_t maxsize, time_t timeout);

/* Sets the read timeout in seconds for subsequent reads (0 means wait
   forever). */
void nntp_timeout(struct nntp *, time_t);

/* Read a single line from an NNTP connection with the given timeout, placing
   the nul-terminated line (without the \r\n line ending) in the provided
   pointer.  The string will be invalidated by the next read from that
   connection. */
enum nntp_status nntp_read_line(struct nntp *, char **);

/* Read a response to an NNTP command with the given timeout, placing the
   response code in the third argument and the rest of the line in the fourth
   argument.  If no response code could be found, the code will be set to 0.
   The string will be invalidated by the next read from that connection. */
enum nntp_status nntp_read_response(struct nntp *nntp, enum nntp_code *code,
                                    char **rest);

/* Read a command from an NNTP connection with the given timeout, placing the
   command and its arguments into the provided cvector.  The cvector will be
   invalidated by the next read from that connection. */
enum nntp_status nntp_read_command(struct nntp *, struct cvector *);

/* Read multiline data from an NNTP connection with the given timeout.  Set
   the third argument to a pointer to the data (still in wire format) and the
   fourth argument to its length. */
enum nntp_status nntp_read_multiline(struct nntp *, char **, size_t *);

/* Send a line to the remote connection.  The output is flushed after sending
   the line. */
bool nntp_send_line(struct nntp *, const char *format, ...);

/* Send a response to an NNTP command or an opening banner.  The string may be
   NULL to indicate nothing should follow the response code; otherwise, it is
   a printf-style format.  The noflush variant doesn't flush the output after
   sending it, used for introducing multiline responses. */
bool nntp_respond(struct nntp *, enum nntp_code, const char *, ...);
void nntp_respond_noflush(struct nntp *, enum nntp_code, const char *, ...);

/* Flush NNTP output, returning true on success and false on any error. */
bool nntp_flush(struct nntp *);

#endif /* INN_NNTP_H */

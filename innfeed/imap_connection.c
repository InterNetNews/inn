/*
**  Feed articles to an IMAP server via LMTP and IMAP.
**
**  Written by Tim Martin.
**
**  Instead of feeding articles via nntp to another host this feeds the
**  messages via lmtp to a host and the control messages (cancel's etc..) it
**  performs via IMAP.  This means it has 2 active connections at any given
**  time and 2 queues.
**
**  When an article comes in it is immediately placed in the lmtp queue. When
**  an article is picked off the lmtp queue for processing first check if it's
**  a control message.  If so, place it in the IMAP queue.  If not, attempt to
**  deliver via LMTP.
**
**  This attempts to follow the exact same api as connection.c.
**
**  TODO:
**
**  feed to smtp
**  security layers?  <--punt on for now
**  authname/password per connection object
**  untagged IMAP messages
*/

#include "portable/system.h"

#include "portable/socket.h"
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <syslog.h>
#include <time.h>

#include "inn/libinn.h"
#include "inn/messages.h"

#include "article.h"
#include "buffer.h"
#include "configfile.h"
#include "connection.h"
#include "endpoint.h"
#include "host.h"
#include "innfeed.h"

#ifdef HAVE_SASL
#    include <sasl/sasl.h>
#    include <sasl/saslplug.h>
#    include <sasl/saslutil.h>

/* For Cyrus SASL versions < 2.1.25 (in hexadecimal notation below). */
#    if !defined(SASL_VERSION_FULL) || SASL_VERSION_FULL < 0x020119
typedef int (*sasl_callback_ft)(void);
#    endif
#endif

/* There's a useless "break" when SASL support is not enabled, reported
 * by Clang but GCC warns if it does not see it, so let's just keep it
 * for the time being and silent this warning. */
#if !defined(HAVE_SASL) && (defined(__llvm__) || defined(__clang__))
#    pragma GCC diagnostic ignored "-Wunreachable-code-break"
#endif

#ifndef MAXHOSTNAMELEN
#    define MAXHOSTNAMELEN 1024
#endif

#define IMAP_PORT 143

#ifdef SMTPMODE
#    define LMTP_PORT 25
#else
#    define LMTP_PORT 2003
#endif

#define IMAP_TAGLENGTH      6

#define QUEUE_MAX_SIZE      250

#define DOSOMETHING_TIMEOUT 60


static char hostname[MAXHOSTNAMELEN];
static char *mailfrom_name = NULL; /* default to no return path */

#ifdef HAVE_SASL
static int initialized_sasl =
    0; /* weather sasl_client_init() has been called */
#endif

/* states the imap connection may be in */
typedef enum {

    IMAP_DISCONNECTED = 1,
    IMAP_WAITING,

    IMAP_CONNECTED_NOTAUTH,

    IMAP_READING_INTRO,

    IMAP_WRITING_CAPABILITY,
    IMAP_READING_CAPABILITY,

    IMAP_WRITING_STARTAUTH,
    IMAP_READING_STEPAUTH,
    IMAP_WRITING_STEPAUTH,

    IMAP_IDLE_AUTHED,
    IMAP_WRITING_NOOP,
    IMAP_READING_NOOP,

    IMAP_WRITING_CREATE,
    IMAP_READING_CREATE,

    IMAP_WRITING_DELETE,
    IMAP_READING_DELETE,

    IMAP_WRITING_SELECT,
    IMAP_READING_SELECT,

    IMAP_WRITING_SEARCH,
    IMAP_READING_SEARCH,

    IMAP_WRITING_STORE,
    IMAP_READING_STORE,

    IMAP_WRITING_CLOSE,
    IMAP_READING_CLOSE,

    IMAP_WRITING_QUIT,
    IMAP_READING_QUIT

} imap_state_t;

typedef enum {
    LMTP_DISCONNECTED = 1,
    LMTP_WAITING,

    LMTP_CONNECTED_NOTAUTH,

    LMTP_READING_INTRO,

    LMTP_WRITING_LHLO,
    LMTP_READING_LHLO,

    LMTP_WRITING_STARTAUTH,
    LMTP_READING_STEPAUTH,
    LMTP_WRITING_STEPAUTH,

    LMTP_AUTHED_IDLE,
    LMTP_WRITING_NOOP,
    LMTP_READING_NOOP,

    LMTP_READING_RSET,
    LMTP_READING_MAILFROM,
    LMTP_READING_RCPTTO,
    LMTP_READING_DATA,
    LMTP_READING_CONTENTS,

    LMTP_WRITING_UPTODATA,
    LMTP_WRITING_CONTENTS,

    LMTP_WRITING_QUIT,
    LMTP_READING_QUIT

} lmtp_state_t;

typedef struct imap_capabilities_s {

    int imap4;         /* does server support imap4bis? */
    int logindisabled; /* does the server allow the login command? */

    char *saslmechs; /* supported SASL mechanisms */

} imap_capabilities_t;

typedef struct lmtp_capabilities_s {

    int Eightbitmime;
    int EnhancedStatusCodes;
    int pipelining;

    char *saslmechs;

} lmtp_capabilities_t;

typedef enum {
    STAT_CONT = 0,
    STAT_NO = 1,
    STAT_OK = 2,
    STAT_FAIL = 3
} imt_stat;

/* Message types */
typedef enum {
    DELIVER,
    CREATE_FOLDER,
    CANCEL_MSG,
    DELETE_FOLDER
} control_type_t;

typedef struct control_item_s {

    Article article;
    char *folder;
    char *msgid;       /* only for cancel's */
    unsigned long uid; /* only for cancel's */

} control_item_t;

typedef struct article_queue_s {

    control_type_t type;

    time_t arrived;
    time_t nextsend; /* time we should next try to send article */

    int trys;

    int counts_toward_size;

    union {
        Article article;
        control_item_t *control;
        void *generic;
    } data;

    struct article_queue_s *next;

} article_queue_t;

typedef struct Q_s {

    article_queue_t *head;

    article_queue_t *tail;

    int size;

} Q_t;

typedef struct connection_s {

    /* common stuff */
    char *ServerName;

    char *lmtp_respBuffer; /* buffer all responses are read into */
    Buffer lmtp_rBuffer;   /* buffer all responses are read into */

    Host myHost; /* the host who owns the connection */

    time_t timeCon; /* the time the connect happened
                       (last auth succeeded) */

    int issue_quit; /* Three states:
                     *   0 - don't do anything
                     *   1 - after issue quit enter wait state
                     *   2 - after issue quit reconnect
                     *   3 - after issue quit delete connection
                     *   4 - nuke cxn
                     */

    /* Statistics */
    int lmtp_succeeded;
    int lmtp_failed;

    int cancel_succeeded;
    int cancel_failed;

    int create_succeeded;
    int create_failed;

    int remove_succeeded;
    int remove_failed;


    /* LMTP stuff */
    int lmtp_port;
    lmtp_state_t lmtp_state;
#ifdef HAVE_SASL
    sasl_conn_t *saslconn_lmtp;
#endif /* HAVE_SASL */
    int sockfd_lmtp;

    time_t lmtp_timeCon;

    EndPoint lmtp_endpoint;
    unsigned int ident; /* an identifier for syslogging. */

    lmtp_capabilities_t *lmtp_capabilities;

    int lmtp_disconnects;
    char *lmtp_tofree_str;

    article_queue_t *current_article;
    Buffer *current_bufs;
    int current_rcpts_issued;
    int current_rcpts_okayed;

    /* Timer for the max amount of time to wait for a response from the
       remote */
    unsigned int lmtp_readTimeout;
    TimeoutId lmtp_readBlockedTimerId;

    /* Timer for the max amount of time to wait for a any amount of data
       to be written to the remote */
    unsigned int lmtp_writeTimeout;
    TimeoutId lmtp_writeBlockedTimerId;

    /* Timer for the number of seconds to sleep before attempting a
       reconnect. */
    unsigned int lmtp_sleepTimeout;
    TimeoutId lmtp_sleepTimerId;

    /* Timer for max amount between queueing some articles and trying to send
     * them */
    unsigned int dosomethingTimeout;
    TimeoutId dosomethingTimerId;

    Q_t lmtp_todeliver_q;


    /* IMAP stuff */
    int imap_port;
#ifdef HAVE_SASL
    sasl_conn_t *imap_saslconn;
#endif /* HAVE_SASL */

    char *imap_respBuffer;
    Buffer imap_rBuffer;
    EndPoint imap_endpoint;

    imap_capabilities_t *imap_capabilities;

    int imap_sockfd;

    time_t imap_timeCon;

    imap_state_t imap_state;
    int imap_disconnects;
    char *imap_tofree_str;

    char imap_currentTag[IMAP_TAGLENGTH + 1];
    int imap_tag_num;

    /* Timer for the max amount of time to wait for a response from the
       remote */
    unsigned int imap_readTimeout;
    TimeoutId imap_readBlockedTimerId;

    /* Timer for the max amount of time to wait for a any amount of data
       to be written to the remote */
    unsigned int imap_writeTimeout;
    TimeoutId imap_writeBlockedTimerId;

    /* Timer for the number of seconds to sleep before attempting a
       reconnect. */
    unsigned int imap_sleepTimeout;
    TimeoutId imap_sleepTimerId;

    Q_t imap_controlMsg_q;

    article_queue_t *current_control;

    struct connection_s *next;

} connection_t;

static Connection gCxnList = NULL;
static unsigned int gCxnCount = 0;
unsigned int max_reconnect_period = MAX_RECON_PER;
unsigned int init_reconnect_period = INIT_RECON_PER;

typedef enum {
    RET_OK = 0,
    RET_FAIL = 1,
    RET_QUEUE_EMPTY,
    RET_EXCEEDS_SIZE,
    RET_NO_FULLLINE,
    RET_NO,
    RET_ARTICLE_BAD
} conn_ret;


/********** Private Function Declarations *************/

static void lmtp_readCB(EndPoint e, IoStatus i, Buffer *b, void *d);
static void imap_readCB(EndPoint e, IoStatus i, Buffer *b, void *d);
static void imap_writeCB(EndPoint e, IoStatus i, Buffer *b, void *d);
static void lmtp_writeCB(EndPoint e, IoStatus i, Buffer *b, void *d);

static conn_ret lmtp_Connect(connection_t *cxn);
static conn_ret imap_Connect(connection_t *cxn);

static void prepareReopenCbk(Connection cxn, int type);

static void lmtp_readTimeoutCbk(TimeoutId id, void *data);
static void imap_readTimeoutCbk(TimeoutId id, void *data);

static void dosomethingTimeoutCbk(TimeoutId id, void *data);

static conn_ret WriteToWire_imapstr(connection_t *cxn, char *str, int slen);
static conn_ret WriteToWire_lmtpstr(connection_t *cxn, char *str, int slen);

static conn_ret WriteToWire(connection_t *cxn, EndpRWCB callback,
                            EndPoint endp, Buffer *array);
static void lmtp_sendmessage(connection_t *cxn, Article justadded);
static void imap_ProcessQueue(connection_t *cxn);

static conn_ret FindHeader(Buffer *bufs, const char *header, char **start,
                           char **end);
static conn_ret PopFromQueue(Q_t *q, article_queue_t **item);

enum failure_type {
    MSG_SUCCESS = 0,
    MSG_FAIL_DELIVER = 1,
    MSG_GIVE_BACK = 2,
    MSG_MISSING = 3
};

static void QueueForgetAbout(connection_t *cxn, article_queue_t *item,
                             enum failure_type failed);

static void delConnection(Connection cxn);
static void DeleteIfDisconnected(Connection cxn);
static void DeferAllArticles(connection_t *cxn, Q_t *q);

static void lmtp_Disconnect(connection_t *cxn);
static void imap_Disconnect(connection_t *cxn);
static conn_ret imap_listenintro(connection_t *cxn);

static void imap_writeTimeoutCbk(TimeoutId id, void *data);
static void lmtp_writeTimeoutCbk(TimeoutId id, void *data);

/******************** PRIVATE FUNCTIONS ***************************/

static const char *
imap_stateToString(int state)
{
    switch (state) {
    case IMAP_DISCONNECTED:
        return "disconnected";
    case IMAP_WAITING:
        return "waiting";
    case IMAP_CONNECTED_NOTAUTH:
        return "connected (unauthenticated)";
    case IMAP_READING_INTRO:
        return "reading intro";
    case IMAP_WRITING_CAPABILITY:
        return "writing CAPABILITY";
    case IMAP_READING_CAPABILITY:
        return "reading CAPABILITY";
    case IMAP_WRITING_STARTAUTH:
        return "writing AUTHENTICATE";
    case IMAP_READING_STEPAUTH:
        return "reading stepauth";
    case IMAP_WRITING_STEPAUTH:
        return "writing stepauth";
    case IMAP_IDLE_AUTHED:
        return "idle (authenticated)";
    case IMAP_WRITING_NOOP:
        return "writing NOOP";
    case IMAP_READING_NOOP:
        return "reading NOOP response";
    case IMAP_WRITING_CREATE:
        return "writing CREATE";
    case IMAP_READING_CREATE:
        return "reading CREATE response";
    case IMAP_WRITING_DELETE:
        return "writing DELETE command";
    case IMAP_READING_DELETE:
        return "reading DELETE response";
    case IMAP_WRITING_SELECT:
        return "writing SELECT";
    case IMAP_READING_SELECT:
        return "reading SELECT response";
    case IMAP_WRITING_SEARCH:
        return "writing SEARCH";
    case IMAP_READING_SEARCH:
        return "reading SEARCH response";
    case IMAP_WRITING_STORE:
        return "writing STORE";
    case IMAP_READING_STORE:
        return "reading STORE response";
    case IMAP_WRITING_CLOSE:
        return "writing CLOSE";
    case IMAP_READING_CLOSE:
        return "reading CLOSE response";
    case IMAP_WRITING_QUIT:
        return "writing LOGOUT";
    case IMAP_READING_QUIT:
        return "reading LOGOUT response";
    default:
        return "Unknown state";
    }
}

static const char *
lmtp_stateToString(int state)
{
    switch (state) {
    case LMTP_DISCONNECTED:
        return "disconnected";
    case LMTP_WAITING:
        return "waiting";
    case LMTP_CONNECTED_NOTAUTH:
        return "connected (unauthenticated)";
    case LMTP_READING_INTRO:
        return "reading intro";
    case LMTP_WRITING_LHLO:
        return "writing LHLO";
    case LMTP_READING_LHLO:
        return "reading LHLO response";
    case LMTP_WRITING_STARTAUTH:
        return "writing AUTH";
    case LMTP_READING_STEPAUTH:
        return "reading stepauth";
    case LMTP_WRITING_STEPAUTH:
        return "writing stepauth";
    case LMTP_AUTHED_IDLE:
        return "idle (authenticated)";
    case LMTP_WRITING_NOOP:
        return "writing NOOP";
    case LMTP_READING_NOOP:
        return "reading NOOP response";
    case LMTP_READING_RSET:
        return "reading RSET response";
    case LMTP_READING_MAILFROM:
        return "reading MAIL FROM response";
    case LMTP_READING_RCPTTO:
        return "reading RCPT TO response";
    case LMTP_READING_DATA:
        return "reading DATA response";
    case LMTP_READING_CONTENTS:
        return "reading contents response";
    case LMTP_WRITING_UPTODATA:
        return "writing RSET, MAIL FROM, RCPT TO, DATA commands";
    case LMTP_WRITING_CONTENTS:
        return "writing contents of message";
    case LMTP_WRITING_QUIT:
        return "writing QUIT";
    case LMTP_READING_QUIT:
        return "reading QUIT";
    default:
        return "unknown state";
    }
}

/******************************* Queue functions
 * ***********************************/

/*
 * Add a message to a generic queue
 *
 *  q       - the queue adding to
 *  item    - the data to add to the queue
 *  type    - the type of item it is (i.e. cancel,lmtp,etc..)
 *  addsmsg - weather this should be counted toward the queue size
 *            this is for control msg's that create multiple queue items.
 *            For example a cancel message canceling a message in multiple
 *            newsgroups will create >1 queue item but we only want it to count
 *            once towards the queue
 *  must    - wheather we must take it even though it may put us over our max
 * size
 */

static conn_ret
AddToQueue(Q_t *q, void *item, control_type_t type, int addsmsg, bool must)
{
    article_queue_t *newentry;

    if (must == false) {
        if (q->size >= QUEUE_MAX_SIZE) {
            return RET_EXCEEDS_SIZE;
        }
    } else {
        if (q->size >= QUEUE_MAX_SIZE * 10) {
            d_printf(0, "Queue has grown way too much. Dropping article\n");
            return RET_FAIL;
        }
    }

    /* add to the end of our queue */
    newentry = xmalloc(sizeof(article_queue_t));

    newentry->type = type;

    /* send as soon as possible */
    newentry->nextsend = newentry->arrived = time(NULL);

    newentry->trys = 0;

    newentry->data.generic = item;
    newentry->next = NULL;
    newentry->counts_toward_size = addsmsg;

    /* add to end of queue */
    if (q->tail == NULL) {
        q->head = newentry;
        q->tail = newentry;
    } else {

        q->tail->next = newentry;
        q->tail = newentry;
    }

    q->size += addsmsg;

    return RET_OK;
}

/*
 * Pop an item from the queue
 *
 * q    - the queue to pop from
 * item - where the item shall be placed upon success
 *
 */

static conn_ret
PopFromQueue(Q_t *q, article_queue_t **item)
{
    /* if queue empty return error */
    if (q->head == NULL) {
        return RET_QUEUE_EMPTY;
    }

    /* set what we return */
    *item = q->head;

    q->head = q->head->next;
    if (q->head == NULL)
        q->tail = NULL;

    q->size -= (*item)->counts_toward_size;

    return RET_OK;
}

/*
 * ReQueue an item. Will either put it back in the queue for another try
 * or forget about it
 *
 *  cxn     - our connection object (needed so forget about things)
 *  q       - the queue to requeue to
 *  entry   - the item to put back
 */

static void
ReQueue(connection_t *cxn, Q_t *q, article_queue_t *entry)
{
    /* look at the time it's been here */
    entry->nextsend =
        time(NULL) + (entry->trys * 30); /* xxx better formula? */

    entry->trys++;

    /* give up after 5 tries xxx configurable??? */
    if (entry->trys >= 5) {
        QueueForgetAbout(cxn, entry, MSG_FAIL_DELIVER);
        return;
    }


    /* ok let's add back to the end of the queue */
    entry->next = NULL;

    /* add to end of queue */
    if (q->tail == NULL) {
        q->head = entry;
        q->tail = entry;
    } else {
        q->tail->next = entry;
        q->tail = entry;
    }

    q->size += entry->counts_toward_size;
}


/*
 * Forget about an item. Tells host object if we succeeded/failed/etc with the
 * message
 *
 * cxn    - connection object
 * item   - item
 * failed - type of failure (see below)
 *
 * failed:
 *   0 - succeeded delivering message
 *   1 - failed delivering message
 *   2 - Try to give back to host
 *   3 - Article missing (i.e. can't find on disk)
 */
static void
QueueForgetAbout(connection_t *cxn, article_queue_t *item,
                 enum failure_type failed)
{
    Article art = NULL;

    switch (item->type) {
    case DELIVER:
        if (failed > 0)
            cxn->lmtp_failed++;
        art = item->data.article;
        break;

    case CANCEL_MSG:
        if (failed > 0)
            cxn->cancel_failed++;
        free(item->data.control->msgid);
        free(item->data.control->folder);

        if (item->counts_toward_size == 1)
            art = item->data.control->article;

        free(item->data.control);
        break;

    case CREATE_FOLDER:
        if (failed > 0)
            cxn->create_failed++;
        free(item->data.control->folder);

        art = item->data.control->article;

        free(item->data.control);
        break;

    case DELETE_FOLDER:
        if (failed > 0)
            cxn->remove_failed++;
        free(item->data.control->folder);

        art = item->data.control->article;

        free(item->data.control);
        break;

    default:
        d_printf(0,
                 "%s:%u QueueForgetAbout(): "
                 "Unknown type to forget about\n",
                 hostPeerName(cxn->myHost), cxn->ident);
        break;
    }

    if (art != NULL) {
        switch (failed) {
        case MSG_SUCCESS:
            hostArticleAccepted(cxn->myHost, cxn, art);
            break;

        case MSG_FAIL_DELIVER:
            hostArticleRejected(cxn->myHost, cxn, art);
            break;

        case MSG_GIVE_BACK:
            hostTakeBackArticle(cxn->myHost, cxn, art);
            break;

        case MSG_MISSING:
            hostArticleIsMissing(cxn->myHost, cxn, art);
            break;
        default:
            d_printf(0, "%s:%u QueueForgetAbout(): failure type unknown\n",
                     hostPeerName(cxn->myHost), cxn->ident);
            break;
        }
    }

    free(item);
}

/*
 * How much space is available in the queue
 */

static int
QueueSpace(Q_t *q)
{
    int ret = QUEUE_MAX_SIZE - q->size;
    if (ret < 0)
        ret = 0;
    return ret;
}

/*
 * How many items are in the queue
 */

static int
QueueItems(Q_t *q)
{
    return q->size;
}


/***************************** END Queue functions
 * ***********************************/

/***************************** Generic Parse Functions
 * *******************************/

/* returns the end of the header */

static char *
GetUntil(char *str)
{
    while (((*str) != '\0') && ((*str) != '\r') && ((*str) != '\n')) {
        str++;
    }

    return str;
}

static char *
GotoNextLine(char *str)
{
    while (((*str) != '\0') && ((*str) != '\r') && ((*str) != '\n')) {
        str++;
    }

    if (*str == '\r')
        str++;
    if (*str == '\n')
        str++;

    return str;
}

/*
 * Finds the given header field in the message
 *  Returns NULL if not found
 */
static conn_ret
FindHeader(Buffer *bufs, const char *header, char **start, char **end)
{
    Buffer b;
    int size;
    char *str_base;
    char *str;
    int headerlen = strlen(header);

    if (bufs == NULL) {
        if (start)
            *start = NULL;
        return RET_ARTICLE_BAD;
    }

    b = bufs[0];
    size = bufferSize(b);
    str_base = bufferBase(b);
    str = str_base;

    while ((str - str_base) < size - headerlen) {
        if (*str == header[0]) {
            if ((strncasecmp(header, str, headerlen) == 0)
                && (*(str + headerlen) == ':')) {

                if (start) {
                    *start = str + headerlen + 1;

                    /* get rid of leading whitespace */
                    while (isspace((unsigned char) **start))
                        (*start)++;
                }

                if (end)
                    *end = GetUntil(str + headerlen + 1);

                return RET_OK;
            }
        } else if (*str == '\n') {
            /* end of headers */
            return RET_NO;
        }
        str = GotoNextLine(str);
    }

    return RET_NO;
}

static conn_ret
GetLine(char *buf, char *ret, int retmaxsize)
{
    char *str_base;
    char *str;

    int size = strlen(buf);
    str_base = buf;
    str = str_base;

    while ((*str) != '\0') {
        if ((*str) == '\n') {
            if (str - str_base > retmaxsize) {
                d_printf(0, "Max size exceeded! %s\n", str_base);
                return RET_FAIL;
            }

            /* fill in the return string */
            memcpy(ret, str_base, str - str_base);
            ret[str - str_base - 1] = '\0';

            memcpy(str_base, str_base + (str - str_base) + 1,
                   size - (str - str_base));
            str_base[size - (str - str_base)] = '\0';

            return RET_OK;
        }

        str++;
    }

    /* couldn't find a full line */
    return RET_NO_FULLLINE;
}


/************************** END Generic Parse Functions
 * *******************************/

/************************ Writing to Network functions *****************/

static conn_ret
WriteToWire(connection_t *cxn, EndpRWCB callback, EndPoint endp, Buffer *array)
{

    if (array == NULL)
        return RET_FAIL;

    prepareWrite(endp, array, NULL, callback, cxn);

    return RET_OK;
}

static conn_ret
WriteToWire_str(connection_t *cxn, EndpRWCB callback, EndPoint endp, char *str,
                int slen)
{
    conn_ret result;
    Buffer buff;
    Buffer *writeArr;

    if (slen == -1)
        slen = strlen(str);

    buff = newBufferByCharP(str, slen + 1, slen);
    ASSERT(buff != NULL);

    writeArr = makeBufferArray(buff, NULL);

    result = WriteToWire(cxn, callback, endp, writeArr);

    return result;
}

static conn_ret
WriteToWire_imapstr(connection_t *cxn, char *str, int slen)
{
    /* prepare the timeouts */
    clearTimer(cxn->imap_readBlockedTimerId);

    /* set up the write timer. */
    clearTimer(cxn->imap_writeBlockedTimerId);

    if (cxn->imap_writeTimeout > 0)
        cxn->imap_writeBlockedTimerId =
            prepareSleep(imap_writeTimeoutCbk, cxn->imap_writeTimeout, cxn);
    cxn->imap_tofree_str = str;
    return WriteToWire_str(cxn, imap_writeCB, cxn->imap_endpoint, str, slen);
}

static conn_ret
WriteToWire_lmtpstr(connection_t *cxn, char *str, int slen)
{
    /* prepare the timeouts */
    clearTimer(cxn->lmtp_readBlockedTimerId);

    /* set up the write timer. */
    clearTimer(cxn->lmtp_writeBlockedTimerId);

    if (cxn->lmtp_writeTimeout > 0)
        cxn->lmtp_writeBlockedTimerId =
            prepareSleep(lmtp_writeTimeoutCbk, cxn->lmtp_writeTimeout, cxn);

    cxn->lmtp_tofree_str = str;
    return WriteToWire_str(cxn, lmtp_writeCB, cxn->lmtp_endpoint, str, slen);
}

static conn_ret
WriteArticle(connection_t *cxn, Buffer *array)
{
    conn_ret result;

    /* Just call WriteToWire since it's easy. */
    result = WriteToWire(cxn, lmtp_writeCB, cxn->lmtp_endpoint, array);

    if (result != RET_OK) {
        return result;
    }

    cxn->lmtp_state = LMTP_WRITING_CONTENTS;

    return RET_OK;
}

/************************ END Writing to Network functions *****************/


/*
 * Adds a cancel item to the control queue
 * Cancel item to delete message with <msgid> in <folder>
 *
 * cxn       - connection object
 * folder    - pointer to start of folder string (this is a pointer into the
 * actual message buffer) folderlen - length of folder string msgid     -
 * pointer to start of msgid string (this is a pointer into the actual message
 * buffer) msgidlen  - length of msgid string art       - the article for this
 * control message (NULL if this cancel object lacks one) must      - if must
 * be accepted into queue
 */

static conn_ret
addCancelItem(connection_t *cxn, char *folder, int folderlen, char *msgid,
              int msgidlen, Article art, int must)
{
    control_item_t *item;
    conn_ret result;
    int i;

    ASSERT(folder);
    ASSERT(msgid);
    ASSERT(cxn);

    /* sanity check folder, msgid */
    for (i = 0; i < folderlen; i++)
        ASSERT(!isspace((unsigned char) folder[i]));
    for (i = 0; i < msgidlen; i++)
        ASSERT(!isspace((unsigned char) msgid[i]));

    /* create the object */
    item = xcalloc(1, sizeof(control_item_t));

    item->folder = xcalloc(folderlen + 1, 1);
    memcpy(item->folder, folder, folderlen);
    item->folder[folderlen] = '\0';

    item->msgid = xcalloc(msgidlen + 1, 1);
    memcpy(item->msgid, msgid, msgidlen);
    item->msgid[msgidlen] = '\0';

    item->article = art;

    /* try to add to the queue (counts if art isn't null) */
    result = AddToQueue(&(cxn->imap_controlMsg_q), item, CANCEL_MSG,
                        (art != NULL), must);
    if (result != RET_OK) {
        d_printf(1,
                 "%s:%u addCancelItem(): "
                 "I thought we had in space in [imap] queue "
                 "but apparently not\n",
                 hostPeerName(cxn->myHost), cxn->ident);

        /* cleanup */
        free(item->folder);
        free(item->msgid);
        free(item);

        return result;
    }

    return RET_OK;
}

static conn_ret
AddControlMsg(connection_t *cxn, Article art, Buffer *bufs,
              char *control_header, char *control_header_end, bool must)
{
    char *rcpt_list = NULL, *rcpt_list_end;
    control_item_t *item;
    conn_ret res = RET_OK;
    int t;

    /* make sure contents ok; this also should load it into memory */
    if (!artContentsOk(art)) {
        d_printf(0,
                 "%s:%u AddControlMsg(): "
                 "artContentsOk() said article was bad\n",
                 hostPeerName(cxn->myHost), cxn->ident);
        hostArticleIsMissing(cxn->myHost, cxn, art);
        return RET_FAIL;
    }

    /* now let's look at the control to see what it is */
    if (!strncasecmp(control_header, "newgroup", 8)) {
        control_header += 8;
        t = CREATE_FOLDER;
    } else if (!strncasecmp(control_header, "rmgroup", 7)) {
        /* jump past "rmgroup" */
        control_header += 7;
        t = DELETE_FOLDER;
    } else if (!strncasecmp(control_header, "cancel", 6)) {
        t = CANCEL_MSG;
        control_header += 6;
    } else {
        /* unrecognized type */
        char tmp[100];
        char *endstr;
        size_t clen;

        endstr = strchr(control_header, '\n');
        clen = endstr - control_header;

        if (clen > sizeof(tmp) - 1)
            clen = sizeof(tmp) - 1;

        memcpy(tmp, control_header, clen);
        tmp[clen] = '\0';

        d_printf(0, "%s:%u Don't understand Control header field [%s]\n",
                 hostPeerName(cxn->myHost), cxn->ident, tmp);
        return RET_FAIL;
    }

    switch (t) {
    case CREATE_FOLDER:
    case DELETE_FOLDER: {
        int folderlen;

        /* go past all white space */
        while ((*control_header == ' ')
               && (control_header != control_header_end)) {
            control_header++;
        }

        /* trim trailing whitespace */
        while (control_header_end[-1] == ' ') {
            control_header_end--;
        }

        if (control_header >= control_header_end) {
            d_printf(0,
                     "%s:%u addControlMsg(): "
                     "newgroup/rmgroup Control header field has no group "
                     "specified\n",
                     hostPeerName(cxn->myHost), cxn->ident);
            return RET_FAIL;
        }

        folderlen = control_header_end - control_header;

        item = xcalloc(1, sizeof(control_item_t));

        item->folder = xcalloc(folderlen + 1, 1);
        memcpy(item->folder, control_header, folderlen);
        item->folder[folderlen] = '\0';

        item->article = art;

        if (AddToQueue(&(cxn->imap_controlMsg_q), item, t, 1, must)
            != RET_OK) {
            d_printf(1,
                     "%s:%u addCancelItem(): "
                     "I thought we had in space in [imap] queue"
                     " but apparently not\n",
                     hostPeerName(cxn->myHost), cxn->ident);
            free(item->folder);
            free(item);
            return RET_FAIL;
        }

        break;
    }

    case CANCEL_MSG: {
        char *str, *laststart;

        while (((*control_header) == ' ')
               && (control_header != control_header_end)) {
            control_header++;
        }

        if (control_header == control_header_end) {
            d_printf(0,
                     "%s:%u Control header field contains cancel "
                     "with no msgid specified\n",
                     hostPeerName(cxn->myHost), cxn->ident);
            return RET_FAIL;
        }

        if (FindHeader(bufs, "Newsgroups", &rcpt_list, &rcpt_list_end)
            != RET_OK) {
            d_printf(0,
                     "%s:%u Cancel message contains no Newsgroups header "
                     "field\n",
                     hostPeerName(cxn->myHost), cxn->ident);
            return RET_FAIL;
        }

        str = rcpt_list;
        laststart = rcpt_list;

        while (str != rcpt_list_end) {
            if (*str == ',') {
                /* eliminate leading whitespace */
                while (((*laststart) == ' ') || ((*laststart) == '\t')) {
                    laststart++;
                }

                res = addCancelItem(
                    cxn, laststart, str - laststart, control_header,
                    control_header_end - control_header, NULL, must);
                if (res != RET_OK)
                    return res;

                laststart = str + 1;
            }

            str++;
        }

        if (laststart < str) {

            res =
                addCancelItem(cxn, laststart, str - laststart, control_header,
                              control_header_end - control_header, art, must);
            if (res != RET_OK)
                return res;
        }
        break;
    }
    default:
        /* huh?!? */
        d_printf(0, "%s:%u internal error in addControlMsg()\n",
                 hostPeerName(cxn->myHost), cxn->ident);
    }
    return RET_FAIL;
}

/*
 * Show msg handling statistics
 */

static void
show_stats(connection_t *cxn)
{
    d_printf(0, "%s:%u\n", hostPeerName(cxn->myHost), cxn->ident);
    d_printf(0, "  imap queue = %d lmtp queue = %d\n",
             QueueItems(&(cxn->imap_controlMsg_q)),
             QueueItems(&(cxn->lmtp_todeliver_q)));
    d_printf(0, "  imap state = %s\n", imap_stateToString(cxn->imap_state));
    d_printf(0, "  lmtp state = %s\n", lmtp_stateToString(cxn->lmtp_state));
    d_printf(0, "  delivered:  yes: %d no: %d\n", cxn->lmtp_succeeded,
             cxn->lmtp_failed);
    d_printf(0, "  cancel:     yes: %d no: %d\n", cxn->cancel_succeeded,
             cxn->cancel_failed);
    d_printf(0, "  create:     yes: %d no: %d\n", cxn->create_succeeded,
             cxn->create_failed);
    d_printf(0, "  remove:     yes: %d no: %d\n", cxn->remove_succeeded,
             cxn->remove_failed);
}

/**************************** SASL helper functions
 * ******************************/

#ifdef HAVE_SASL
/* callback to get userid or authid */
static int
getsimple(void *context UNUSED, int id, const char **result, unsigned *len)
{
    char *authid;

    if (!result)
        return SASL_BADPARAM;


    switch (id) {
    case SASL_CB_GETREALM:
        *result = deliver_realm;
        if (len)
            *len = deliver_realm ? strlen(deliver_realm) : 0;
        break;
    case SASL_CB_USER:
        *result = deliver_username;
        if (len)
            *len = deliver_username ? strlen(deliver_username) : 0;
        break;
    case SASL_CB_AUTHNAME:
        authid = deliver_authname;
        *result = authid;
        if (len)
            *len = authid ? strlen(authid) : 0;
        break;
    case SASL_CB_LANGUAGE:
        *result = NULL;
        if (len)
            *len = 0;
        break;
    default:
        return SASL_BADPARAM;
    }
    return SASL_OK;
}

/* callback to get password */
static int
getsecret(sasl_conn_t *conn, void *context UNUSED, int id,
          sasl_secret_t **psecret)
{
    size_t passlen;

    if (!conn || !psecret || id != SASL_CB_PASS)
        return SASL_BADPARAM;

    if (deliver_password == NULL) {
        d_printf(0, "SASL requested a password but I don't have one\n");
        return SASL_FAIL;
    }

    passlen = strlen(deliver_password);
    *psecret = xmalloc(sizeof(sasl_secret_t) + passlen + 1);
    if (!*psecret)
        return SASL_FAIL;

    strlcpy((char *) (*psecret)->data, deliver_password, passlen + 1);
    (*psecret)->len = passlen;

    return SASL_OK;
}

#    if __GNUC__ > 7 || LLVM_VERSION_MAJOR > 12
#        pragma GCC diagnostic ignored "-Wcast-function-type"
#    endif

/* callbacks we support */
static sasl_callback_t saslcallbacks[] = {
    {SASL_CB_GETREALM, (sasl_callback_ft) &getsimple, NULL},
    {SASL_CB_USER,     (sasl_callback_ft) &getsimple, NULL},
    {SASL_CB_AUTHNAME, (sasl_callback_ft) &getsimple, NULL},
    {SASL_CB_PASS,     (sasl_callback_ft) &getsecret, NULL},
    {SASL_CB_LIST_END, NULL,                          NULL}
};

#    if __GNUC__ > 7 || LLVM_VERSION_MAJOR > 12
#        pragma GCC diagnostic warning "-Wcast-function-type"
#    endif

static sasl_security_properties_t *
make_secprops(int min, int max)
{
    sasl_security_properties_t *ret =
        xmalloc(sizeof(sasl_security_properties_t));

    ret->maxbufsize = 1024;
    ret->min_ssf = min;
    ret->max_ssf = max;

    ret->security_flags = 0;
    ret->property_names = NULL;
    ret->property_values = NULL;

    return ret;
}

#    ifndef NI_WITHSCOPEID
#        define NI_WITHSCOPEID 0
#    endif
#    ifndef NI_MAXHOST
#        define NI_MAXHOST 1025
#    endif
#    ifndef NI_MAXSERV
#        define NI_MAXSERV 32
#    endif

static int
iptostring(const struct sockaddr *addr, socklen_t addrlen, char *out,
           unsigned outlen)
{
    char hbuf[NI_MAXHOST], pbuf[NI_MAXSERV];

    if (!addr || !out)
        return SASL_BADPARAM;

    getnameinfo(addr, addrlen, hbuf, sizeof(hbuf), pbuf, sizeof(pbuf),
                NI_NUMERICHOST | NI_WITHSCOPEID | NI_NUMERICSERV);

    if (outlen < strlen(hbuf) + strlen(pbuf) + 2)
        return SASL_BUFOVER;

    snprintf(out, outlen, "%s;%s", hbuf, pbuf);

    return SASL_OK;
}

static conn_ret
SetSASLProperties(sasl_conn_t *conn, int sock, int minssf, int maxssf)
{
    sasl_security_properties_t *secprops = NULL;
    socklen_t addrsize = sizeof(struct sockaddr_in);
    char localip[NI_MAXHOST + NI_MAXSERV + 1];
    char remoteip[NI_MAXHOST + NI_MAXSERV + 1];
    struct sockaddr_in saddr_l;
    struct sockaddr_in saddr_r;

    /* create a security structure and give it to sasl */
    secprops = make_secprops(minssf, maxssf);
    if (secprops != NULL) {
        sasl_setprop(conn, SASL_SEC_PROPS, secprops);
        free(secprops);
    }

    if (getpeername(sock, (struct sockaddr *) &saddr_r, &addrsize) != 0)
        return RET_FAIL;

    if (iptostring((struct sockaddr *) &saddr_r, sizeof(struct sockaddr_in),
                   remoteip, sizeof(remoteip)))
        return RET_FAIL;

    if (sasl_setprop(conn, SASL_IPREMOTEPORT, remoteip) != SASL_OK)
        return RET_FAIL;

    addrsize = sizeof(struct sockaddr_in);
    if (getsockname(sock, (struct sockaddr *) &saddr_l, &addrsize) != 0)
        return RET_FAIL;

    if (iptostring((struct sockaddr *) &saddr_l, sizeof(struct sockaddr_in),
                   localip, sizeof(localip)))
        return RET_FAIL;

    if (sasl_setprop(conn, SASL_IPLOCALPORT, localip) != SASL_OK)
        return RET_FAIL;

    return RET_OK;
}
#endif /* HAVE_SASL */

/************************** END SASL helper functions
 * ******************************/

/************************* Startup functions
 * **********************************/

static conn_ret
Initialize(connection_t *cxn, int respTimeout)
{
#ifdef HAVE_SASL
    conn_ret saslresult;
#endif /* HAVE_SASL */

    d_printf(1, "%s:%u initializing....\n", hostPeerName(cxn->myHost),
             cxn->ident);

#ifdef HAVE_SASL
    /* only call sasl_client_init() once */
    if (initialized_sasl == 0) {
        /* Initialize SASL */
        saslresult = sasl_client_init(saslcallbacks);

        if (saslresult != SASL_OK) {
            d_printf(0,
                     "%s:%u Error initializing SASL (sasl_client_init) (%s)\n",
                     hostPeerName(cxn->myHost), cxn->ident,
                     sasl_errstring(saslresult, NULL, NULL));
            return RET_FAIL;
        } else {
            initialized_sasl = 1;
        }
    }
#endif /* HAVE_SASL */

    cxn->lmtp_rBuffer = newBuffer(4096);
    if (cxn->lmtp_rBuffer == NULL) {
        d_printf(0, "%s:%u Failure allocating buffer for lmtp_rBuffer\n",
                 hostPeerName(cxn->myHost), cxn->ident);
        return RET_FAIL;
    }
    bufferAddNullByte(cxn->lmtp_rBuffer);


    cxn->imap_rBuffer = newBuffer(4096);
    if (cxn->imap_rBuffer == NULL) {
        d_printf(0, "%s:%u Failure allocating buffer for imap_rBuffer \n",
                 hostPeerName(cxn->myHost), cxn->ident);
        return RET_FAIL;
    }
    bufferAddNullByte(cxn->imap_rBuffer);

    /* Initialize timeouts */
    cxn->lmtp_writeTimeout = respTimeout;
    cxn->lmtp_readTimeout = respTimeout;
    cxn->imap_writeTimeout = respTimeout;
    cxn->imap_readTimeout = respTimeout;
    cxn->lmtp_sleepTimerId = 0;
    cxn->lmtp_sleepTimeout = init_reconnect_period;
    cxn->imap_sleepTimerId = 0;
    cxn->imap_sleepTimeout = init_reconnect_period;

    cxn->dosomethingTimeout = DOSOMETHING_TIMEOUT;

    /* set up the write timer. */
    clearTimer(cxn->dosomethingTimerId);

    if (cxn->dosomethingTimeout > 0)
        cxn->dosomethingTimerId =
            prepareSleep(dosomethingTimeoutCbk, cxn->dosomethingTimeout, cxn);


    return RET_OK;
}


/* initialize the network */
static conn_ret
init_net(char *serverFQDN, int port, int *sock)
{
    struct sockaddr_in addr;
    struct hostent *hp;

    if ((hp = gethostbyname(serverFQDN)) == NULL) {
        d_printf(0, "gethostbyname(): %s\n", strerror(errno));
        return RET_FAIL;
    }

    if (((*sock) = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        d_printf(0, "socket(): %s\n", strerror(errno));
        return RET_FAIL;
    }

    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    memcpy(&addr.sin_addr, hp->h_addr, hp->h_length);
    addr.sin_port = htons(port);

    if (connect((*sock), (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        d_printf(0, "connect(): %s\n", strerror(errno));
        return RET_FAIL;
    }

    return RET_OK;
}


static conn_ret
SetupLMTPConnection(connection_t *cxn, char *serverName, int port)
{
#ifdef HAVE_SASL
    int saslresult;
#endif /* HAVE_SASL */
    conn_ret result;

    cxn->lmtp_port = port;

    if (serverName == NULL) {
        d_printf(0, "%s:%u serverName is null\n", hostPeerName(cxn->myHost),
                 cxn->ident);
        return RET_FAIL;
    }

#ifdef HAVE_SASL
    /* Free the SASL connection if we already had one */
    if (cxn->saslconn_lmtp != NULL) {
        sasl_dispose(&cxn->saslconn_lmtp);
    }

    /* Start SASL */
    saslresult = sasl_client_new("lmtp", serverName, NULL, NULL, NULL, 0,
                                 &cxn->saslconn_lmtp);

    if (saslresult != SASL_OK) {

        d_printf(0, "%s:%u:LMTP Error creating a new SASL connection (%s)\n",
                 hostPeerName(cxn->myHost), cxn->ident,
                 sasl_errstring(saslresult, NULL, NULL));
        return RET_FAIL;
    }
#endif /* HAVE_SASL */

    /* Connect the Socket */
    result = init_net(serverName, LMTP_PORT, /*port,*/
                      &(cxn->sockfd_lmtp));

    if (result != RET_OK) {
        d_printf(0, "%s:%u unable to connect to lmtp host\n",
                 hostPeerName(cxn->myHost), cxn->ident);
        return RET_FAIL;
    }

    if (cxn->lmtp_respBuffer)
        free(cxn->lmtp_respBuffer);
    cxn->lmtp_respBuffer = xmalloc(4096);
    cxn->lmtp_respBuffer[0] = '\0';

    /* Free if we had an existing one */
    if (cxn->lmtp_endpoint != NULL) {
        delEndPoint(cxn->lmtp_endpoint);
        cxn->lmtp_endpoint = NULL;
    }

    cxn->lmtp_endpoint = newEndPoint(cxn->sockfd_lmtp);
    if (cxn->lmtp_endpoint == NULL) {
        d_printf(0, "%s:%u:LMTP failure creating endpoint\n",
                 hostPeerName(cxn->myHost), cxn->ident);
        return RET_FAIL;
    }

#ifdef HAVE_SASL
    /* Set the SASL properties */
    result = SetSASLProperties(cxn->saslconn_lmtp, cxn->sockfd_lmtp, 0, 0);

    if (result != RET_OK) {
        d_printf(0, "%s:%u:LMTP error setting SASL properties\n",
                 hostPeerName(cxn->myHost), cxn->ident);
        return RET_FAIL;
    }
#endif /* HAVE_SASL */


    return RET_OK;
}

static conn_ret
SetupIMAPConnection(connection_t *cxn, char *serverName, int port)
{
#ifdef HAVE_SASL
    int saslresult;
#endif /* HAVE_SASL */
    conn_ret result;

    cxn->imap_port = port;

    if (serverName == NULL) {
        d_printf(0, "%s:%u:IMAP Servername is null", hostPeerName(cxn->myHost),
                 cxn->ident);
        return RET_FAIL;
    }

#ifdef HAVE_SASL
    /* Free the SASL connection if we already had one */
    if (cxn->imap_saslconn != NULL) {
        sasl_dispose(&cxn->imap_saslconn);
    }

    /* Start SASL */
    saslresult = sasl_client_new("imap", serverName, NULL, NULL, NULL, 0,
                                 &cxn->imap_saslconn);

    if (saslresult != SASL_OK) {
        d_printf(0, "%s:%u:IMAP Error creating a new SASL connection (%s)",
                 hostPeerName(cxn->myHost), cxn->ident,
                 sasl_errstring(saslresult, NULL, NULL));
        return RET_FAIL;
    }
#endif /* HAVE_SASL */

    /* Connect the Socket */
    result = init_net(serverName, port, &(cxn->imap_sockfd));

    if (result != RET_OK) {
        d_printf(0, "%s:%u:IMAP Unable to start network connection for IMAP",
                 hostPeerName(cxn->myHost), cxn->ident);
        return RET_FAIL;
    }

    if (cxn->imap_respBuffer)
        free(cxn->imap_respBuffer);
    cxn->imap_respBuffer = xmalloc(4096);
    cxn->imap_respBuffer[0] = '\0';

    /* Free if we had an existing one */
    if (cxn->imap_endpoint != NULL) {
        delEndPoint(cxn->imap_endpoint);
        cxn->imap_endpoint = NULL;
    }

    cxn->imap_endpoint = newEndPoint(cxn->imap_sockfd);
    if (cxn->imap_endpoint == NULL) {
        d_printf(0, "%s:%u:IMAP Failure creating imap endpoint\n",
                 hostPeerName(cxn->myHost), cxn->ident);
        return RET_FAIL;
    }

#ifdef HAVE_SASL
    /* Set the SASL properties */
    result = SetSASLProperties(cxn->imap_saslconn, cxn->imap_sockfd, 0, 0);
    if (result != RET_OK) {
        d_printf(0, "%s:%u:IMAP Error setting sasl properties",
                 hostPeerName(cxn->myHost), cxn->ident);
        return result;
    }
#endif /* HAVE_SASL */


    return RET_OK;
}

/************************* END Startup functions
 * **********************************/

/* Return the response code for this line
   -1 if it doesn't seem to have one
*/
static int
ask_code(char *str)
{
    int ret = 0;

    if (str == NULL)
        return -1;

    if (strlen(str) < 3)
        return -1;

    /* check to make sure 0-2 are digits */
    if ((!isdigit((unsigned char) str[0]))
        || (!isdigit((unsigned char) str[1]))
        || (!isdigit((unsigned char) str[2]))) {
        d_printf(0, "Parse error: response does not begin with a code [%s]\n",
                 str);
        return -1;
    }


    ret = ((str[0] - '0') * 100) + ((str[1] - '0') * 10) + (str[2] - '0');

    return ret;
}

/* is this a continuation or not?
   220-fdfsd is        (1)
   220 fdsfs is not    (0)
 */

static int
ask_keepgoing(char *str)
{
    if (str == NULL)
        return 0;
    if (strlen(str) < 4)
        return 0;

    if (str[3] == '-')
        return 1;

    return 0;
}


static conn_ret
lmtp_listenintro(connection_t *cxn)
{
    Buffer *readBuffers;

    /* set up to receive */
    readBuffers = makeBufferArray(bufferTakeRef(cxn->lmtp_rBuffer), NULL);
    prepareRead(cxn->lmtp_endpoint, readBuffers, lmtp_readCB, cxn, 5);

    cxn->lmtp_state = LMTP_READING_INTRO;

    return RET_OK;
}


/************************** IMAP functions ***********************/

static conn_ret
imap_Connect(connection_t *cxn)
{
    conn_ret result;

    ASSERT(cxn->imap_sleepTimerId == 0);

    /* make the IMAP connection */ SetupIMAPConnection(cxn, cxn->ServerName,
                                                       IMAP_PORT);

    /* Listen to the intro and start the authenticating process */
    result = imap_listenintro(cxn);

    return result;
}

/*
 * This is called when the data write timeout for the remote
 * goes off. We tear down the connection and notify our host.
 */
static void
imap_writeTimeoutCbk(TimeoutId id UNUSED, void *data)
{
    connection_t *cxn = (Connection) data;
    const char *peerName;

    peerName = hostPeerName(cxn->myHost);

    syslog(LOG_WARNING, "timeout for %s", peerName);
    d_printf(0, "%s: shutting down non-responsive IMAP connection (%s)\n",
             hostPeerName(cxn->myHost), imap_stateToString(cxn->imap_state));

    cxnLogStats(cxn, true);

    imap_Disconnect(cxn);
}

/*
 * This is called when the timeout for the response from the remote
 * goes off. We tear down the connection and notify our host.
 */
static void
imap_readTimeoutCbk(TimeoutId id, void *data)
{
    Connection cxn = (Connection) data;
    const char *peerName;

    ASSERT(id == cxn->imap_readBlockedTimerId);

    peerName = hostPeerName(cxn->myHost);

    warn("%s:%u cxnsleep non-responsive connection", peerName, cxn->ident);
    d_printf(0, "%s:%u shutting down non-responsive IMAP connection (%s)\n",
             hostPeerName(cxn->myHost), cxn->ident,
             imap_stateToString(cxn->imap_state));

    cxnLogStats(cxn, true);

    if (cxn->imap_state == IMAP_DISCONNECTED) {
        imap_Disconnect(cxn);
        lmtp_Disconnect(cxn);
        delConnection(cxn);
    } else {
        imap_Disconnect(cxn);
    }
}

/*
 * Called by the EndPoint class when the timer goes off
 */
static void
imap_reopenTimeoutCbk(TimeoutId id, void *data)
{
    Connection cxn = (Connection) data;

    ASSERT(id == cxn->imap_sleepTimerId);

    cxn->imap_sleepTimerId = 0;

    d_printf(1,
             "%s:%u:IMAP Reopen timer rang. Try to make new connection now\n",
             hostPeerName(cxn->myHost), cxn->ident);

    if (cxn->imap_state != IMAP_DISCONNECTED) {
        warn("%s:%u cxnsleep connection in bad state: %s",
             hostPeerName(cxn->myHost), cxn->ident,
             imap_stateToString(cxn->imap_state));
    } else {
        if (imap_Connect(cxn) != RET_OK)
            prepareReopenCbk(cxn, 0);
    }
}

static void
imap_Disconnect(connection_t *cxn)
{
    clearTimer(cxn->imap_sleepTimerId);
    cxn->imap_sleepTimerId = 0;
    clearTimer(cxn->imap_readBlockedTimerId);
    clearTimer(cxn->imap_writeBlockedTimerId);

    DeferAllArticles(
        cxn, &(cxn->imap_controlMsg_q)); /* give any articles back to Host */

    cxn->imap_state = IMAP_DISCONNECTED;

    cxn->imap_disconnects++;

    cxn->imap_respBuffer[0] = '\0';

    if (cxn->issue_quit == 0)
        prepareReopenCbk(cxn, 0);

    DeleteIfDisconnected(cxn);
}

/************************** END IMAP functions ***********************/

/************************ LMTP functions **************************/

/*
 * Create a network lmtp connection
 * and start listening for the intro string
 *
 */

static conn_ret
lmtp_Connect(connection_t *cxn)
{
    conn_ret result;

    ASSERT(cxn->lmtp_sleepTimerId == 0);

    /* make the LMTP connection */
    result = SetupLMTPConnection(cxn, cxn->ServerName, LMTP_PORT);

    if (result != RET_OK)
        return result;

    /* Listen to the intro */
    result = lmtp_listenintro(cxn);

    return result;
}


static void
lmtp_Disconnect(connection_t *cxn)
{
    clearTimer(cxn->lmtp_sleepTimerId);
    cxn->lmtp_sleepTimerId = 0;
    clearTimer(cxn->lmtp_readBlockedTimerId);
    clearTimer(cxn->lmtp_writeBlockedTimerId);

    /* give any articles back to Host */
    DeferAllArticles(cxn, &(cxn->lmtp_todeliver_q));

    cxn->lmtp_state = LMTP_DISCONNECTED;

    cxn->lmtp_disconnects++;

    cxn->lmtp_respBuffer[0] = '\0';

    if (cxn->issue_quit == 0)
        prepareReopenCbk(cxn, 1);

    DeleteIfDisconnected(cxn);
}


/*
 * Called by the EndPoint class when the timer goes off
 */
static void
lmtp_reopenTimeoutCbk(TimeoutId id, void *data)
{
    Connection cxn = (Connection) data;

    ASSERT(id == cxn->lmtp_sleepTimerId);

    cxn->lmtp_sleepTimerId = 0;

    d_printf(1,
             "%s:%u:LMTP Reopen timer rang. Try to make new connection now\n",
             hostPeerName(cxn->myHost), cxn->ident);

    if (cxn->lmtp_state != LMTP_DISCONNECTED) {
        warn("%s:%u cxnsleep connection in bad state: %s",
             hostPeerName(cxn->myHost), cxn->ident,
             lmtp_stateToString(cxn->lmtp_state));
    } else {
        if (lmtp_Connect(cxn) != RET_OK)
            prepareReopenCbk(cxn, 1);
    }
}

/*
 * Set up the callback used when the Connection is sleeping (i.e. will try
 * to reopen the connection).
 *
 * type (0 = imap, 1 = lmtp)
 */
static void
prepareReopenCbk(Connection cxn, int type)
{
    /* xxx check state */


    if (type == 0) {

        cxn->imap_sleepTimerId =
            prepareSleep(imap_reopenTimeoutCbk, cxn->imap_sleepTimeout, cxn);
        d_printf(1,
                 "%s:%u IMAP connection error\n"
                 "  will try to reconnect in %u seconds\n",
                 hostPeerName(cxn->myHost), cxn->ident,
                 cxn->imap_sleepTimeout);
    } else {
        cxn->lmtp_sleepTimerId =
            prepareSleep(lmtp_reopenTimeoutCbk, cxn->lmtp_sleepTimeout, cxn);
        d_printf(1,
                 "%s:%u:LMTP connection error\n"
                 "will try to reconnect in %u seconds\n",
                 hostPeerName(cxn->myHost), cxn->ident,
                 cxn->lmtp_sleepTimeout);
    }

    /* bump the sleep timer amount each time to wait longer and longer. Gets
       reset in resetConnection() */
    if (type == 0) {
        cxn->imap_sleepTimeout *= 2;
        if (cxn->imap_sleepTimeout > max_reconnect_period)
            cxn->imap_sleepTimeout = max_reconnect_period;
    } else {
        cxn->lmtp_sleepTimeout *= 2;
        if (cxn->lmtp_sleepTimeout > max_reconnect_period)
            cxn->lmtp_sleepTimeout = max_reconnect_period;
    }
}

/*
 * This is called when the timeout for the response from the remote
 * goes off. We tear down the connection and notify our host.
 */
static void
lmtp_readTimeoutCbk(TimeoutId id, void *data)
{
    Connection cxn = (Connection) data;
    const char *peerName;

    ASSERT(id == cxn->lmtp_readBlockedTimerId);

    peerName = hostPeerName(cxn->myHost);

    warn("%s:%u cxnsleep non-responsive connection", peerName, cxn->ident);
    d_printf(0, "%s:%u shutting down non-responsive LMTP connection (%s)\n",
             hostPeerName(cxn->myHost), cxn->ident,
             lmtp_stateToString(cxn->lmtp_state));

    cxnLogStats(cxn, true);

    if (cxn->lmtp_state == LMTP_DISCONNECTED) {
        imap_Disconnect(cxn);
        lmtp_Disconnect(cxn);
        delConnection(cxn);
    } else {
        lmtp_Disconnect(cxn);
    }
}


/*
 * This is called when the data write timeout for the remote
 * goes off. We tear down the connection and notify our host.
 */
static void
lmtp_writeTimeoutCbk(TimeoutId id UNUSED, void *data)
{
    connection_t *cxn = (Connection) data;
    const char *peerName;

    peerName = hostPeerName(cxn->myHost);

    syslog(LOG_WARNING, "timeout for %s", peerName);
    d_printf(0, "%s:%u shutting down non-responsive LMTP connection (%s)\n",
             hostPeerName(cxn->myHost), cxn->ident,
             lmtp_stateToString(cxn->lmtp_state));

    cxnLogStats(cxn, true);

    lmtp_Disconnect(cxn);
}

/************************ END LMTP functions **************************/

/************************** LMTP write functions ********************/

static conn_ret
lmtp_noop(connection_t *cxn)
{
    conn_ret result;
    char *p;

    p = xstrdup("NOOP\r\n");
    result = WriteToWire_lmtpstr(cxn, p, strlen(p));
    if (result != RET_OK)
        return result;

    cxn->lmtp_state = LMTP_WRITING_NOOP;

    return RET_OK;
}

static conn_ret
lmtp_IssueQuit(connection_t *cxn)
{
    conn_ret result;
    char *p;

    p = xstrdup("QUIT\r\n");
    result = WriteToWire_lmtpstr(cxn, p, strlen(p));
    if (result != RET_OK)
        return result;

    cxn->lmtp_state = LMTP_WRITING_QUIT;

    return RET_OK;
}

static conn_ret
lmtp_getcapabilities(connection_t *cxn)
{
    conn_ret result;
    char *p;

    if (cxn->lmtp_capabilities != NULL) {
        if (cxn->lmtp_capabilities->saslmechs) {
            free(cxn->lmtp_capabilities->saslmechs);
        }
        free(cxn->lmtp_capabilities);
        cxn->lmtp_capabilities = NULL;
    }

    cxn->lmtp_capabilities = xcalloc(1, sizeof(lmtp_capabilities_t));
    cxn->lmtp_capabilities->saslmechs = NULL;

#ifdef SMTPMODE
    p = concat("EHLO ", hostname, "\r\n", (char *) 0);
#else
    p = concat("LHLO ", hostname, "\r\n", (char *) 0);
#endif /* SMTPMODE */

    result = WriteToWire_lmtpstr(cxn, p, strlen(p));
    if (result != RET_OK)
        return result;

    cxn->lmtp_state = LMTP_WRITING_LHLO;

    return RET_OK;
}

#ifdef HAVE_SASL
static conn_ret
lmtp_authenticate(connection_t *cxn)
{
    int saslresult;

    const char *mechusing;
    const char *out;
    unsigned int outlen;
    char *inbase64;
    int inbase64len;
    conn_ret result;
    char *p;

    sasl_interact_t *client_interact = NULL;

    saslresult = sasl_client_start(
        cxn->saslconn_lmtp, cxn->lmtp_capabilities->saslmechs,
        &client_interact, &out, &outlen, &mechusing);

    if ((saslresult != SASL_OK) && (saslresult != SASL_CONTINUE)) {

        d_printf(0, "%s:%u:LMTP Error calling sasl_client_start (%s)\n",
                 hostPeerName(cxn->myHost), cxn->ident,
                 sasl_errstring(saslresult, NULL, NULL));
        return RET_FAIL;
    }

    d_printf(
        1,
        "%s:%u:LMTP Decided to try to authenticate with SASL mechanism=%s\n",
        hostPeerName(cxn->myHost), cxn->ident, mechusing);

    if (!out) {
        /* no initial client response */
        p = concat("AUTH ", mechusing, "\r\n", (char *) 0);
    } else if (!outlen) {
        /* empty initial client response */
        p = concat("AUTH ", mechusing, " =\r\n", (char *) 0);
    } else {
        /* Initial client response - convert to base64.
         * 2n+7 bytes are enough to contain the result of the base64
         * encoding of a string whose length is n bytes.
         * In sasl_encode64() calls, the fourth argument is the length
         * of the third including the null terminator (thus 2n+8 bytes). */
        inbase64 = xmalloc(outlen * 2 + 8);

        saslresult = sasl_encode64(out, outlen, inbase64, outlen * 2 + 8,
                                   (unsigned *) &inbase64len);
        if (saslresult != SASL_OK)
            return RET_FAIL;
        p = concat("AUTH ", mechusing, " ", inbase64, "\r\n", (char *) 0);
        free(inbase64);
    }

    result = WriteToWire_lmtpstr(cxn, p, strlen(p));
    if (result != RET_OK) {
        d_printf(0, "%s:%u:LMTP WriteToWire() failure during AUTH\n",
                 hostPeerName(cxn->myHost), cxn->ident);
        /* Disconnection is handled in the calling function. */
        return result;
    }

    cxn->lmtp_state = LMTP_WRITING_STARTAUTH;

    return RET_OK;
}

static imt_stat
lmtp_getauthline(char *str, char **line, int *linelen)
{
    int saslresult;
    int response_code = -1;

    response_code = ask_code(str);

    if (response_code == 334) {

        /* continue */

    } else if (response_code == 235) {

        /* woohoo! authentication complete */
        return STAT_OK;

    } else {
        /* failure of some sort */
        d_printf(0, "?:?:LMTP Authentication failure (%d) [%s]\n",
                 response_code, str);
        return STAT_NO;
    }

    str += 4; /* jump past the "334 " */

    *line = xmalloc(strlen(str) + 30);
    if ((*line) == NULL) {
        return STAT_NO;
    }

    /* decode this line */
    saslresult = sasl_decode64(str, strlen(str), *line, strlen(str) + 1,
                               (unsigned *) linelen);
    if (saslresult != SASL_OK) {
        d_printf(0, "?:?:LMTP base64 decoding error\n");
        return STAT_NO;
    }

    return STAT_CONT;
}
#endif /* HAVE_SASL */

static void
lmtp_writeCB(EndPoint e UNUSED, IoStatus i UNUSED, Buffer *b, void *d)
{
    connection_t *cxn = (connection_t *) d;
    Buffer *readBuffers;

    clearTimer(cxn->lmtp_writeBlockedTimerId);

    /* Free the string that was written */
    freeBufferArray(b);
    if (cxn->lmtp_tofree_str != NULL) {
        free(cxn->lmtp_tofree_str);
        cxn->lmtp_tofree_str = NULL;
    }

    /* set up to receive */
    readBuffers = makeBufferArray(bufferTakeRef(cxn->lmtp_rBuffer), NULL);
    prepareRead(cxn->lmtp_endpoint, readBuffers, lmtp_readCB, cxn, 5);

    /* set up the response timer. */
    clearTimer(cxn->lmtp_readBlockedTimerId);

    if (cxn->lmtp_readTimeout > 0)
        cxn->lmtp_readBlockedTimerId =
            prepareSleep(lmtp_readTimeoutCbk, cxn->lmtp_readTimeout, cxn);


    switch (cxn->lmtp_state) {

    case LMTP_WRITING_LHLO:
        cxn->lmtp_state = LMTP_READING_LHLO;
        break;

    case LMTP_WRITING_STARTAUTH:
    case LMTP_WRITING_STEPAUTH:
        cxn->lmtp_state = LMTP_READING_STEPAUTH;
        break;

    case LMTP_WRITING_UPTODATA:
        /* expect result to rset */
        cxn->lmtp_state = LMTP_READING_RSET;
        break;

    case LMTP_WRITING_CONTENTS:
        /* so we sent the whole DATA command
           let's see what the server responded */

        cxn->lmtp_state = LMTP_READING_CONTENTS;

        break;

    case LMTP_WRITING_NOOP:
        cxn->lmtp_state = LMTP_READING_NOOP;
        break;

    case LMTP_WRITING_QUIT:
        cxn->lmtp_state = LMTP_READING_QUIT;
        break;

    default:

        d_printf(0, "%s:%u:LMTP Unknown state. Internal error\n",
                 hostPeerName(cxn->myHost), cxn->ident);

        break;
    }
}

/************************** END LMTP write functions ********************/

/************************** IMAP sending functions ************************/


static void
imap_writeCB(EndPoint e UNUSED, IoStatus i UNUSED, Buffer *b, void *d)
{
    connection_t *cxn = (connection_t *) d;
    Buffer *readBuffers;

    clearTimer(cxn->imap_writeBlockedTimerId);

    /* free the string we just wrote out */
    freeBufferArray(b);
    if (cxn->imap_tofree_str != NULL) {
        free(cxn->imap_tofree_str);
        cxn->imap_tofree_str = NULL;
    }

    /* set up to receive */
    readBuffers = makeBufferArray(bufferTakeRef(cxn->imap_rBuffer), NULL);
    prepareRead(cxn->imap_endpoint, readBuffers, imap_readCB, cxn, 5);

    /* set up the response timer. */
    clearTimer(cxn->imap_readBlockedTimerId);

    if (cxn->imap_readTimeout > 0)
        cxn->imap_readBlockedTimerId =
            prepareSleep(imap_readTimeoutCbk, cxn->imap_readTimeout, cxn);

    switch (cxn->imap_state) {
    case IMAP_WRITING_CAPABILITY:
        cxn->imap_state = IMAP_READING_CAPABILITY;
        break;

    case IMAP_WRITING_STEPAUTH:
    case IMAP_WRITING_STARTAUTH:
        cxn->imap_state = IMAP_READING_STEPAUTH;
        break;

    case IMAP_WRITING_CREATE:
        cxn->imap_state = IMAP_READING_CREATE;
        break;

    case IMAP_WRITING_DELETE:
        cxn->imap_state = IMAP_READING_DELETE;
        break;

    case IMAP_WRITING_SELECT:
        cxn->imap_state = IMAP_READING_SELECT;
        break;

    case IMAP_WRITING_SEARCH:
        cxn->imap_state = IMAP_READING_SEARCH;
        break;

    case IMAP_WRITING_STORE:
        cxn->imap_state = IMAP_READING_STORE;
        break;

    case IMAP_WRITING_CLOSE:
        cxn->imap_state = IMAP_READING_CLOSE;
        break;

    case IMAP_WRITING_NOOP:
        cxn->imap_state = IMAP_READING_NOOP;
        break;

    case IMAP_WRITING_QUIT:
        cxn->imap_state = IMAP_READING_QUIT;
        break;

    default:
        d_printf(0, "%s:%u:IMAP invalid connection state\n",
                 hostPeerName(cxn->myHost), cxn->ident);
        imap_Disconnect(cxn);
        break;
    }
}

/*
 * Tag is already allocated
 */

static void
imap_GetTag(connection_t *cxn)
{
    snprintf(cxn->imap_currentTag, IMAP_TAGLENGTH + 1, "%06d",
             cxn->imap_tag_num);
    cxn->imap_tag_num++;
    if (cxn->imap_tag_num >= 999999) {
        cxn->imap_tag_num = 0;
    }
}

#ifdef HAVE_SASL
static conn_ret
imap_sendAuthStep(connection_t *cxn, char *str)
{
    conn_ret result;
    int saslresult;
    char in[4096];
    unsigned int inlen;
    const char *out;
    unsigned int outlen;
    char *inbase64;
    unsigned int inbase64len;

    /* base64 decode it */

    saslresult = sasl_decode64(str, strlen(str), in, strlen(str) + 1, &inlen);
    if (saslresult != SASL_OK) {
        d_printf(0, "%s:%u:IMAP base64 decoding error\n",
                 hostPeerName(cxn->myHost), cxn->ident);
        return RET_FAIL;
    }

    saslresult =
        sasl_client_step(cxn->imap_saslconn, in, inlen, NULL, &out, &outlen);

    /* check if sasl succeeded */
    if (saslresult != SASL_OK && saslresult != SASL_CONTINUE) {

        d_printf(0, "%s:%u:IMAP sasl_client_step failed with %s\n",
                 hostPeerName(cxn->myHost), cxn->ident,
                 sasl_errstring(saslresult, NULL, NULL));
        cxn->imap_state = IMAP_CONNECTED_NOTAUTH;
        return RET_FAIL;
    }
    /* Convert to base64.
     * 2n+7 bytes are enough to contain the result of the base64
     * encoding of a string whose length is n bytes.
     * In sasl_encode64() calls, the fourth argument is the length
     * of the third including the null terminator (thus 2n+8 bytes).
     * And CRLF takes the last two bytes (thus 2n+10 bytes). */
    inbase64 = xmalloc(outlen * 2 + 10);

    saslresult = sasl_encode64(out, outlen, inbase64, outlen * 2 + 8,
                               (unsigned *) &inbase64len);

    if (saslresult != SASL_OK)
        return RET_FAIL;

    /* Append endline. */
    strlcpy(inbase64 + inbase64len, "\r\n", outlen * 2 + 10 - inbase64len);
    inbase64len += 2;

    /* Send to server. */
    result = WriteToWire_imapstr(cxn, inbase64, inbase64len);
    if (result != RET_OK)
        return result;

    cxn->imap_state = IMAP_WRITING_STEPAUTH;

    return RET_OK;
}
#endif /* HAVE_SASL */

static conn_ret
imap_sendAuthenticate(connection_t *cxn)
{
    conn_ret result;
    char *p;

#ifdef HAVE_SASL
    const char *mechusing;
    int saslresult = SASL_NOMECH;

    sasl_interact_t *client_interact = NULL;

    if (cxn->imap_capabilities->saslmechs) {
        saslresult = sasl_client_start(
            cxn->imap_saslconn, cxn->imap_capabilities->saslmechs,
            &client_interact, NULL, NULL, &mechusing);
    }

    /* If no mechs try "login" */
    if (saslresult == SASL_NOMECH) {

#else /* HAVE_SASL */

    { /* always do login */

#endif /* HAVE_SASL */
        d_printf(1, "%s:%u:IMAP No mechanism found. Trying login method\n",
                 hostPeerName(cxn->myHost), cxn->ident);

        if (cxn->imap_capabilities->logindisabled == 1) {
            d_printf(0,
                     "%s:%u:IMAP Login command w/o security layer not allowed "
                     "on this server\n",
                     hostPeerName(cxn->myHost), cxn->ident);
            return RET_FAIL;
        }

        if (deliver_authname == NULL) {
            d_printf(
                0,
                "%s:%u:IMAP Unable to log in because can't find a authname\n",
                hostPeerName(cxn->myHost), cxn->ident);
            return RET_FAIL;
        }

        if (deliver_password == NULL) {
            d_printf(
                0,
                "%s:%u:IMAP Unable to log in because can't find a password\n",
                hostPeerName(cxn->myHost), cxn->ident);
            return RET_FAIL;
        }

        imap_GetTag(cxn);

        p = concat(cxn->imap_currentTag, " LOGIN ", deliver_authname, " \"",
                   deliver_password, "\"\r\n", (char *) 0);

        result = WriteToWire_imapstr(cxn, p, strlen(p));
        if (result != RET_OK)
            return result;

        cxn->imap_state = IMAP_WRITING_STARTAUTH;

        return RET_OK;
    }

#ifdef HAVE_SASL
    if ((saslresult != SASL_OK) && (saslresult != SASL_CONTINUE)) {

        d_printf(
            0,
            "%s:%u:IMAP Error calling sasl_client_start (%s) mechusing = %s\n",
            hostPeerName(cxn->myHost), cxn->ident,
            sasl_errstring(saslresult, NULL, NULL), mechusing);
        return RET_FAIL;
    }

    d_printf(1,
             "%s:%u:IMAP Trying to authenticate to imap with %s mechanism\n",
             hostPeerName(cxn->myHost), cxn->ident, mechusing);

    imap_GetTag(cxn);

    p = concat(cxn->imap_currentTag, " AUTHENTICATE ", mechusing, "\r\n",
               (char *) 0);
    result = WriteToWire_imapstr(cxn, p, strlen(p));
    if (result != RET_OK)
        return result;

    cxn->imap_state = IMAP_WRITING_STARTAUTH;

    return RET_OK;
#endif /* HAVE_SASL */
}

static conn_ret
imap_CreateGroup(connection_t *cxn, char *bboard)
{
    conn_ret result;
    char *tosend;

    d_printf(1, "%s:%u:IMAP Ok creating group [%s]\n",
             hostPeerName(cxn->myHost), cxn->ident, bboard);

    imap_GetTag(cxn);

    tosend =
        concat(cxn->imap_currentTag, " CREATE ", bboard, "\r\n", (char *) 0);

    result = WriteToWire_imapstr(cxn, tosend, -1);
    if (result != RET_OK)
        return result;

    cxn->imap_state = IMAP_WRITING_CREATE;

    return RET_OK;
}

static conn_ret
imap_DeleteGroup(connection_t *cxn, char *bboard)
{
    conn_ret result;
    char *tosend;

    d_printf(1, "%s:%u:IMAP Ok removing bboard [%s]\n",
             hostPeerName(cxn->myHost), cxn->ident, bboard);

    imap_GetTag(cxn);

    tosend =
        concat(cxn->imap_currentTag, " DELETE ", bboard, "\r\n", (char *) 0);
    result = WriteToWire_imapstr(cxn, tosend, -1);
    if (result != RET_OK)
        return result;

    cxn->imap_state = IMAP_WRITING_DELETE;

    return RET_OK;
}

static conn_ret
imap_CancelMsg(connection_t *cxn, char *newsgroup)
{
    conn_ret result;
    char *tosend;

    ASSERT(newsgroup);

    imap_GetTag(cxn);

    /* select mbox */
    tosend = concat(cxn->imap_currentTag, " SELECT ", newsgroup, "\r\n",
                    (char *) 0);
    result = WriteToWire_imapstr(cxn, tosend, -1);
    if (result != RET_OK)
        return result;

    cxn->imap_state = IMAP_WRITING_SELECT;

    hostArticleOffered(cxn->myHost, cxn);

    return RET_OK;
}

static conn_ret
imap_sendSearch(connection_t *cxn, char *msgid)
{
    conn_ret result;
    char *tosend;

    ASSERT(msgid);

    imap_GetTag(cxn);

    /* perform search */
    tosend =
        concat(cxn->imap_currentTag, " UID SEARCH header \"Message-ID\" \"",
               msgid, "\"\r\n", (char *) 0);
    result = WriteToWire_imapstr(cxn, tosend, -1);
    if (result != RET_OK)
        return result;

    cxn->imap_state = IMAP_WRITING_SEARCH;

    return RET_OK;
}

static conn_ret
imap_sendKill(connection_t *cxn, unsigned uid)
{
    conn_ret result;
    char *tosend = NULL;

    imap_GetTag(cxn);

    xasprintf(&tosend, "%s UID STORE %u +FLAGS.SILENT (\\Deleted)\r\n",
              cxn->imap_currentTag, uid);

    result = WriteToWire_imapstr(cxn, tosend, -1);
    if (result != RET_OK)
        return result;

    cxn->imap_state = IMAP_WRITING_STORE;

    return RET_OK;
}

static conn_ret
imap_sendSimple(connection_t *cxn, const char *atom, int st)
{
    char *tosend;
    conn_ret result;

    imap_GetTag(cxn);
    tosend = concat(cxn->imap_currentTag, " ", atom, "\r\n", (char *) 0);

    result = WriteToWire_imapstr(cxn, tosend, -1);
    if (result != RET_OK)
        return result;

    cxn->imap_state = st;

    return RET_OK;
}

static conn_ret
imap_sendClose(connection_t *cxn)
{
    return imap_sendSimple(cxn, "CLOSE", IMAP_WRITING_CLOSE);
}

static conn_ret
imap_sendQuit(connection_t *cxn)
{
    return imap_sendSimple(cxn, "LOGOUT", IMAP_WRITING_QUIT);
}

static conn_ret
imap_noop(connection_t *cxn)
{
    return imap_sendSimple(cxn, "NOOP", IMAP_WRITING_NOOP);
}


static conn_ret
imap_sendCapability(connection_t *cxn)
{
    return imap_sendSimple(cxn, "CAPABILITY", IMAP_WRITING_CAPABILITY);
}

/************************** END IMAP sending functions
 * ************************/

/************************** IMAP reading functions ***************************/

static conn_ret
imap_listenintro(connection_t *cxn)
{
    Buffer *readBuffers;

    /* set up to receive */
    readBuffers = makeBufferArray(bufferTakeRef(cxn->imap_rBuffer), NULL);
    prepareRead(cxn->imap_endpoint, readBuffers, imap_readCB, cxn, 5);

    cxn->imap_state = IMAP_READING_INTRO;

    return RET_OK;
}

static conn_ret
imap_ParseCapability(char *string, imap_capabilities_t **caps)
{
    char *str = string;
    char *start = str;
    size_t mechlen;

    /* allocate the caps structure if it doesn't already exist */
    if ((*caps) == NULL)
        (*caps) = xcalloc(1, sizeof(imap_capabilities_t));

    while ((*str) != '\0') {

        while (((*str) != '\0') && ((*str) != ' ')) {
            str++;
        }

        if ((*str) != '\0') {
            *str = '\0';
            str++;
        }

        if (strcasecmp(start, "IMAP4") == 0) {
            (*caps)->imap4 = 1;
        } else if (strcasecmp(start, "LOGINDISABLED") == 0) {
            (*caps)->logindisabled = 1;
        } else if (strncasecmp(start, "AUTH=", 5) == 0) {
            if ((*caps)->saslmechs == NULL) {
                (*caps)->saslmechs = xstrdup(start + 5);
            } else {
                mechlen = strlen((*caps)->saslmechs) + 1;
                mechlen += strlen(start + 5) + 1;
                (*caps)->saslmechs = xrealloc((*caps)->saslmechs, mechlen);
                strlcat((*caps)->saslmechs, " ", mechlen);
                strlcat((*caps)->saslmechs, start + 5, mechlen);
            }
        }

        start = str;
    }

    if ((*caps)->saslmechs) {
        d_printf(1, "?:?:IMAP parsed capabilities: saslmechs = %s\n",
                 (*caps)->saslmechs);
    }

    return RET_OK;
}


static void
imap_readCB(EndPoint e, IoStatus i, Buffer *b, void *d)
{
    connection_t *cxn = (connection_t *) d;
    Buffer *readBuffers;

    int okno;
    char *str;
    char strbuf[4096];
    conn_ret ret;
    char *p;

    p = bufferBase(b[0]);

    /* Add what we got to our internal read buffer */
    bufferAddNullByte(b[0]);

    if (i != IoDone) {
        errno = endPointErrno(e);

        syslog(LOG_ERR, "%s:%u IMAP i/o failed: %m", hostPeerName(cxn->myHost),
               cxn->ident);
        freeBufferArray(b);
        imap_Disconnect(cxn);
        return;
    }

    if (strchr(p, '\n') == NULL) {
        /* partial read. expand buffer and retry */

        if (expandBuffer(b[0], BUFFER_EXPAND_AMOUNT) == false) {
            d_printf(0, "%s:%u:IMAP expanding buffer returned false\n",
                     hostPeerName(cxn->myHost), cxn->ident);

            imap_Disconnect(cxn);
            return;
        }
        readBuffers = makeBufferArray(bufferTakeRef(b[0]), NULL);

        if (!prepareRead(e, readBuffers, imap_readCB, cxn, 1)) {
            imap_Disconnect(cxn);
        }

        freeBufferArray(b);
        return;
    }

    clearTimer(cxn->imap_readBlockedTimerId);

    /* we got something. add to our buffer and free b */

    strcat(cxn->imap_respBuffer, p);

    bufferSetDataSize(b[0], 0);

    freeBufferArray(b);


    /* goto here to take another step */
reset:

    /* see if we have a full line */
    ret = GetLine(cxn->imap_respBuffer, strbuf, sizeof(strbuf));
    str = strbuf;

    /* if we don't have a full line */
    if (ret == RET_NO_FULLLINE) {

        readBuffers = makeBufferArray(bufferTakeRef(cxn->imap_rBuffer), NULL);

        if (!prepareRead(e, readBuffers, imap_readCB, cxn, 1)) {
            imap_Disconnect(cxn);
        }
        return;

    } else if (ret != RET_OK) {
        return;
    }

    /* if untagged */
    if ((str[0] == '*') && (str[1] == ' ')) {
        str += 2;

        /* now figure out what kind of untagged it is */
        if (strncasecmp(str, "CAPABILITY ", 11) == 0) {
            str += 11;

            imap_ParseCapability(str, &(cxn->imap_capabilities));

        } else if (strncasecmp(str, "SEARCH", 6) == 0) {

            str += 6;

            if ((*str) == ' ') {
                str++;

                cxn->current_control->data.control->uid = atoi(str);

                d_printf(1, "%s:%u:IMAP i think the UID = %lu\n",
                         hostPeerName(cxn->myHost), cxn->ident,
                         cxn->current_control->data.control->uid);
            } else {
                /* it's probably a blank uid (i.e. message doesn't exist) */
                cxn->current_control->data.control->uid = (unsigned long) -1;
            }

        } else if (strncasecmp(str, "OK ", 3) == 0) {

            if (cxn->imap_state == IMAP_READING_INTRO) {
                imap_sendCapability(cxn); /* xxx errors */
                return;

            } else {
            }

        } else {
            /* untagged command not understood */
        }

        /* always might be more to look at */
        goto reset;

    } else if ((str[0] == '+') && (str[1] == ' ')) {

        str += 2;

        if (cxn->imap_state == IMAP_READING_STEPAUTH) {
#ifdef HAVE_SASL
            if (imap_sendAuthStep(cxn, str) != RET_OK) {
                imap_Disconnect(cxn);
            }
#else
            d_printf(
                0,
                "%s:%u:IMAP got a '+ ...' without SASL. Something's wrong\n",
                hostPeerName(cxn->myHost), cxn->ident);
            imap_Disconnect(cxn);
#endif /* HAVE_SASL */

            return;
        } else {
            d_printf(0,
                     "%s:%u:IMAP got a '+ ...' in state where not expected\n",
                     hostPeerName(cxn->myHost), cxn->ident);
            imap_Disconnect(cxn);
            return;
        }

    } else if (strncasecmp(str, cxn->imap_currentTag, IMAP_TAGLENGTH) == 0) {
        /* matches our tag */
        str += IMAP_TAGLENGTH;

        if (str[0] != ' ') {
            d_printf(0,
                     "%s:%u:IMAP Parse error: tag with no space afterward\n",
                     hostPeerName(cxn->myHost), cxn->ident);
            imap_Disconnect(cxn);
            return;
        }
        str++;

        /* should be OK/NO */
        if (strncasecmp(str, "OK", 2) == 0) {
            okno = 1;
        } else {
            okno = 0;
        }

        switch (cxn->imap_state) {
        case IMAP_READING_CAPABILITY:

            if (okno == 1) {
                if (imap_sendAuthenticate(cxn) != RET_OK) {
                    d_printf(0, "%s:%u:IMAP sendauthenticate failed\n",
                             hostPeerName(cxn->myHost), cxn->ident);
                    imap_Disconnect(cxn);
                }
                return;
            } else {
                d_printf(0, "%s:%u:IMAP CAPABILITY gave a NO response\n",
                         hostPeerName(cxn->myHost), cxn->ident);
                imap_Disconnect(cxn);
            }
            return;
            /* NOTREACHED */

        case IMAP_READING_STEPAUTH:

            if (okno == 1) {

                cxn->imap_sleepTimeout = init_reconnect_period;

                cxn->imap_timeCon = theTime();
                cxn->timeCon = theTime();

                d_printf(0, "%s:%u IMAP authentication succeeded\n",
                         hostPeerName(cxn->myHost), cxn->ident);

                cxn->imap_disconnects = 0;

                cxn->imap_state = IMAP_IDLE_AUTHED;

                /* try to send a message if we have one */
                imap_ProcessQueue(cxn);
            } else {
                d_printf(0, "%s:%u:IMAP Authentication failed with [%s]\n",
                         hostPeerName(cxn->myHost), cxn->ident, str);
                imap_Disconnect(cxn);
            }

            return;
            /* NOTREACHED */

        case IMAP_READING_CREATE:

            if (okno == 1) {

                d_printf(1, "%s:%u:IMAP Create of bboard successful\n",
                         hostPeerName(cxn->myHost), cxn->ident);
                cxn->create_succeeded++;

                /* we can delete article now */
                QueueForgetAbout(cxn, cxn->current_control, MSG_SUCCESS);
            } else {
                d_printf(1, "%s:%u:IMAP Create failed with [%s] for %s\n",
                         hostPeerName(cxn->myHost), cxn->ident, str,
                         cxn->current_control->data.control->folder);

                ReQueue(cxn, &(cxn->imap_controlMsg_q), cxn->current_control);
            }

            imap_ProcessQueue(cxn);

            break;
            /* NOTREACHED */

        case IMAP_READING_DELETE:

            if (okno == 1) {
                d_printf(1, "%s:%u:IMAP Delete successful\n",
                         hostPeerName(cxn->myHost), cxn->ident);
                cxn->remove_succeeded++;

                /* we can delete article now */
                QueueForgetAbout(cxn, cxn->current_control, MSG_SUCCESS);
            } else {
                d_printf(1,
                         "%s:%u:IMAP Delete mailbox failed with [%s] for %s\n",
                         hostPeerName(cxn->myHost), cxn->ident, str,
                         cxn->current_control->data.control->folder);

                ReQueue(cxn, &(cxn->imap_controlMsg_q), cxn->current_control);
            }

            imap_ProcessQueue(cxn);
            return;
            /* NOTREACHED */

        case IMAP_READING_SELECT:

            if (okno == 1) {

                imap_sendSearch(cxn,
                                cxn->current_control->data.control->msgid);
                return;

            } else {
                d_printf(1, "%s:%u:IMAP Select failed with [%s] for %s\n",
                         hostPeerName(cxn->myHost), cxn->ident, str,
                         cxn->current_control->data.control->folder);

                ReQueue(cxn, &(cxn->imap_controlMsg_q), cxn->current_control);

                cxn->imap_state = IMAP_IDLE_AUTHED;

                imap_ProcessQueue(cxn);
                return;
            }
            /* NOTREACHED */

        case IMAP_READING_SEARCH:
            /* if no message let's forget about it */
            if (cxn->current_control->data.control->uid
                == (unsigned long) -1) {
                d_printf(2, "%s:%u:IMAP Search didn't find the message\n",
                         hostPeerName(cxn->myHost), cxn->ident);
                QueueForgetAbout(cxn, cxn->current_control, MSG_FAIL_DELIVER);
                if (imap_sendClose(cxn) != RET_OK)
                    imap_Disconnect(cxn);
                return;
            }

            if (okno == 1) {
                /* we got a uid. let's delete it */
                if (imap_sendKill(cxn, cxn->current_control->data.control->uid)
                    != RET_OK)
                    imap_Disconnect(cxn);
                return;
            } else {
                d_printf(0, "%s:%u IMAP Received NO response to SEARCH\n",
                         hostPeerName(cxn->myHost), cxn->ident);
                ReQueue(cxn, &(cxn->imap_controlMsg_q), cxn->current_control);

                if (imap_sendClose(cxn) != RET_OK)
                    imap_Disconnect(cxn);
                return;
            }
            /* NOTREACHED */

        case IMAP_READING_STORE:

            if (okno == 1) {

                d_printf(1, "%s:%u:IMAP Processed a Cancel fully\n",
                         hostPeerName(cxn->myHost), cxn->ident);

                /* we can delete article now */
                QueueForgetAbout(cxn, cxn->current_control, MSG_SUCCESS);

                cxn->cancel_succeeded++;

                if (imap_sendClose(cxn) != RET_OK)
                    imap_Disconnect(cxn);
                return;

            } else {

                d_printf(1, "%s:%u:IMAP Store failed\n",
                         hostPeerName(cxn->myHost), cxn->ident);
                ReQueue(cxn, &(cxn->imap_controlMsg_q), cxn->current_control);

                if (imap_sendClose(cxn) != RET_OK)
                    imap_Disconnect(cxn);
                return;
            }
            /* NOTREACHED */

        case IMAP_READING_NOOP:
            cxn->imap_state = IMAP_IDLE_AUTHED;
            return;
            /* NOTREACHED */

        case IMAP_READING_CLOSE:
            if (!okno) {
                /* we can't do anything about it */
                d_printf(1, "%s:%u:IMAP Close failed\n",
                         hostPeerName(cxn->myHost), cxn->ident);
            }

            cxn->imap_state = IMAP_IDLE_AUTHED;

            imap_ProcessQueue(cxn);
            return;
            /* NOTREACHED */

        case IMAP_READING_QUIT:

            /* we don't care if the server said OK or NO just
               that it said something */

            d_printf(1, "%s:%u:IMAP Read quit response\n",
                     hostPeerName(cxn->myHost), cxn->ident);

            cxn->imap_state = IMAP_DISCONNECTED;

            DeleteIfDisconnected(cxn);
            break;

        default:
            d_printf(0, "%s:%u:IMAP I don't understand state %u [%s]\n",
                     hostPeerName(cxn->myHost), cxn->ident, cxn->imap_state,
                     str);
            imap_Disconnect(cxn);
        }

    } else {
        d_printf(0,
                 "%s:%u:IMAP tag (%s) doesn't match what we gave (%s). What's "
                 "up with that??\n",
                 hostPeerName(cxn->myHost), cxn->ident, str,
                 cxn->imap_currentTag);
        imap_Disconnect(cxn);
    }
}

/************************** END IMAP reading functions
 * ***************************/

/*************************** LMTP reading functions
 * ****************************/

static void
lmtp_readCB(EndPoint e, IoStatus i, Buffer *b, void *d)
{
    connection_t *cxn = (connection_t *) d;
    char str[4096];
    Buffer *readBuffers;
    conn_ret result;
    int response_code;
    conn_ret ret;
#ifdef HAVE_SASL
    int inlen;
    char *in;
    size_t outlen;
    const char *out;
    char *inbase64;
    int inbase64len;
    imt_stat status;
    int saslresult;
    sasl_interact_t *client_interact = NULL;
#endif /* HAVE_SASL */

    char *p = bufferBase(b[0]);

    bufferAddNullByte(b[0]);

    if (i != IoDone) {
        errno = endPointErrno(e);
        syslog(LOG_ERR, "%s:%u LMTP i/o failed: %m", hostPeerName(cxn->myHost),
               cxn->ident);

        freeBufferArray(b);
        lmtp_Disconnect(cxn);
        return;
    }

    if (strchr(p, '\n') == NULL) {
        /* partial read. expand buffer and retry */

        d_printf(0, "%s:%u:LMTP Partial. retry\n", hostPeerName(cxn->myHost),
                 cxn->ident);
        expandBuffer(b[0], BUFFER_EXPAND_AMOUNT);
        readBuffers = makeBufferArray(bufferTakeRef(b[0]), NULL);

        if (!prepareRead(e, readBuffers, lmtp_readCB, cxn, 1)) {
            lmtp_Disconnect(cxn);
        }

        freeBufferArray(b);
        return;
    }

    clearTimer(cxn->lmtp_readBlockedTimerId);

    /* Add what we got to our internal read buffer */
    strcat(cxn->lmtp_respBuffer, p);

    bufferSetDataSize(b[0], 0);

    freeBufferArray(b);

reset:
    /* see if we have a full line */
    ret = GetLine(cxn->lmtp_respBuffer, str, sizeof(str));

    /* get a line */
    if (ret != RET_OK) {
        if (ret != RET_NO_FULLLINE) {
            /* was a more serious error */
            d_printf(0, "%s:%u:LMTP Internal error getting line from server\n",
                     hostPeerName(cxn->myHost), cxn->ident);
            lmtp_Disconnect(cxn);
            return;
        }

        /* set up to receive some more */
        readBuffers = makeBufferArray(bufferTakeRef(cxn->lmtp_rBuffer), NULL);
        prepareRead(cxn->lmtp_endpoint, readBuffers, lmtp_readCB, cxn, 5);
        return;
    }

    switch (cxn->lmtp_state) {

    case LMTP_READING_INTRO:

        if (ask_code(str) != 220) {
            d_printf(0,
                     "%s:%u:LMTP Initial server msg does not start with 220 "
                     "(began with %d)\n",
                     hostPeerName(cxn->myHost), cxn->ident, ask_code(str));
            lmtp_Disconnect(cxn);
            return;
        }

        /* the initial intro could have many lines via
           continuations. see if we need to read more */
        if (ask_keepgoing(str) == 1) {
            goto reset;
        }

        result = lmtp_getcapabilities(cxn);

        if (result != RET_OK) {
            d_printf(0, "%s:%u:LMTP lmtp_getcapabilities() failure\n",
                     hostPeerName(cxn->myHost), cxn->ident);
            lmtp_Disconnect(cxn);
            return;
        }

        break;

    case LMTP_READING_LHLO:
        /* receive the response(s) */
        response_code = ask_code(str);

        if (response_code != 250) /* was none */
        {
            d_printf(0, "%s:%u:LMTP Response code unexpected (%d)\n",
                     hostPeerName(cxn->myHost), cxn->ident, response_code);
            lmtp_Disconnect(cxn);
            return;
        }

        /* look for one we know about; ignore all others */
        if (strncasecmp(str + 4, "8BITMIME", strlen("8BITMIME")) == 0) {
            cxn->lmtp_capabilities->Eightbitmime = 1;
        } else if (strncasecmp(str + 4, "ENHANCEDSTATUSCODES",
                               strlen("ENHANCEDSTATUSCODES"))
                   == 0) {
            cxn->lmtp_capabilities->EnhancedStatusCodes = 1;
        } else if (strncasecmp(str + 4, "AUTH", 4) == 0) {
            cxn->lmtp_capabilities->saslmechs = xstrdup(str + 4 + 5);
        } else if (strncasecmp(str + 4, "PIPELINING", strlen("PIPELINING"))
                   == 0) {
            cxn->lmtp_capabilities->pipelining = 1;
        } else {
            /* don't care; ignore */
        }

        /* see if this is the last line of the capability */
        if (ask_keepgoing(str) == 1) {
            goto reset;
        } else {
            /* we require a few capabilities */
            if (!cxn->lmtp_capabilities->pipelining) {
                d_printf(0, "%s:%u:LMTP We require PIPELINING\n",
                         hostPeerName(cxn->myHost), cxn->ident);

                lmtp_Disconnect(cxn);
                return;
            }
#ifdef HAVE_SASL
            if (cxn->lmtp_capabilities->saslmechs) {
                /* start the authentication */
                result = lmtp_authenticate(cxn);

                if (result != RET_OK) {
                    d_printf(0, "%s:%u:LMTP lmtp_authenticate() error\n",
                             hostPeerName(cxn->myHost), cxn->ident);
                    lmtp_Disconnect(cxn);
                    return;
                }
#else
            if (0) {
                /* noop */
#endif
            } else {
                /* either we can't authenticate or the remote server
                   doesn't support it */
                cxn->lmtp_state = LMTP_AUTHED_IDLE;
                d_printf(1,
                         "%s:%u:LMTP Even though we can't authenticate"
                         " we're going to try to feed anyway\n",
                         hostPeerName(cxn->myHost), cxn->ident);
                /* We just assume we don't need to authenticate
                   (great assumption huh?) */
                hostRemoteStreams(cxn->myHost, cxn, true);

                cxn->lmtp_timeCon = theTime();
                cxn->timeCon = theTime();

                /* try to send a message if we have one */
                lmtp_sendmessage(cxn, NULL);
                return;
            }
        }
        break;

#ifdef HAVE_SASL
    case LMTP_READING_STEPAUTH:
        inlen = 0;
        status = lmtp_getauthline(str, &in, &inlen);

        switch (status) {

        case STAT_CONT:

            saslresult =
                sasl_client_step(cxn->saslconn_lmtp, in, inlen,
                                 &client_interact, &out, (unsigned *) &outlen);

            free(in);

            /* check if sasl succeeded */
            if (saslresult != SASL_OK && saslresult != SASL_CONTINUE) {
                d_printf(0, "%s:%u:LMTP sasl_client_step(): %s\n",
                         hostPeerName(cxn->myHost), cxn->ident,
                         sasl_errstring(saslresult, NULL, NULL));

                lmtp_Disconnect(cxn);
                return;
            }

            /* Convert to base64.
             * 2n+7 bytes are enough to contain the result of the base64
             * encoding of a string whose length is n bytes.
             * In sasl_encode64() calls, the fourth argument is the length
             * of the third including the null terminator (thus 2n+8 bytes).
             * And CRLF takes the last two bytes (thus 2n+10 bytes). */
            inbase64 = xmalloc(outlen * 2 + 10);

            saslresult = sasl_encode64(out, outlen, inbase64, outlen * 2 + 8,
                                       (unsigned *) &inbase64len);

            if (saslresult != SASL_OK) {
                d_printf(0, "%s:%u:LMTP sasl_encode64(): %s\n",
                         hostPeerName(cxn->myHost), cxn->ident,
                         sasl_errstring(saslresult, NULL, NULL));

                lmtp_Disconnect(cxn);
                return;
            }

            /* Add an endline. */
            strlcpy(inbase64 + inbase64len, "\r\n",
                    outlen * 2 + 10 - inbase64len);
            inbase64len += 2;

            /* Send to server. */
            result = WriteToWire_lmtpstr(cxn, inbase64, inbase64len);
            if (result != RET_OK) {
                d_printf(0, "%s:%u:LMTP WriteToWire() failure\n",
                         hostPeerName(cxn->myHost), cxn->ident);
                lmtp_Disconnect(cxn);
                return;
            }

            cxn->lmtp_state = LMTP_WRITING_STEPAUTH;
            break;

        case STAT_OK:
            cxn->lmtp_sleepTimeout = init_reconnect_period;

            d_printf(0, "%s:%u LMTP authentication succeeded\n",
                     hostPeerName(cxn->myHost), cxn->ident);

            cxn->lmtp_disconnects = 0;

            hostRemoteStreams(cxn->myHost, cxn, true);

            cxn->lmtp_timeCon = theTime();
            cxn->timeCon = theTime();

            cxn->lmtp_state = LMTP_AUTHED_IDLE;


            /* try to send a message if we have one */
            lmtp_sendmessage(cxn, NULL);
            return;

        default:
            d_printf(0, "%s:%u:LMTP failed authentication\n",
                     hostPeerName(cxn->myHost), cxn->ident);
            lmtp_Disconnect(cxn);
            return;
        }
        break;
#endif /* HAVE_SASL */

    case LMTP_READING_RSET:
        if (ask_keepgoing(str)) {
            goto reset;
        }
        if (ask_code(str) != 250) {
            d_printf(0, "%s:%u:LMTP RSET failed with (%d)\n",
                     hostPeerName(cxn->myHost), cxn->ident, ask_code(str));
            lmtp_Disconnect(cxn);
            return;
        }

        /* we pipelined so next we receive the mail from response */
        cxn->lmtp_state = LMTP_READING_MAILFROM;
        goto reset;

    case LMTP_READING_MAILFROM:
        if (ask_keepgoing(str)) {
            goto reset;
        }
        if (ask_code(str) != 250) {
            d_printf(0, "%s:%u:LMTP MAILFROM failed with (%d)\n",
                     hostPeerName(cxn->myHost), cxn->ident, ask_code(str));
            lmtp_Disconnect(cxn);
            return;
        }

        /* we pipelined so next we receive the rcpt's */
        cxn->lmtp_state = LMTP_READING_RCPTTO;
        goto reset;
        /* NOTREACHED */

    case LMTP_READING_RCPTTO:
        if (ask_keepgoing(str)) {
            goto reset;
        }
        if (ask_code(str) != 250) {
            d_printf(1, "%s:%u:LMTP RCPT TO failed with (%d) %s\n",
                     hostPeerName(cxn->myHost), cxn->ident, ask_code(str),
                     str);

            /* if got a 5xx don't try to send anymore */
            cxn->current_article->trys = 100;

            cxn->current_rcpts_issued--;
        } else {
            cxn->current_rcpts_okayed++;
        }

        /* if issued equals number okayed then we're done */
        if (cxn->current_rcpts_okayed == cxn->current_rcpts_issued) {
            cxn->lmtp_state = LMTP_READING_DATA;
        } else {
            /* stay in same state */
        }
        goto reset;
        /* NOTREACHED */

    case LMTP_READING_DATA:
        if (ask_keepgoing(str)) {
            goto reset;
        }
        if (cxn->current_rcpts_issued == 0) {
            if (cxn->current_article->trys < 100) {
                d_printf(1,
                         "%s:%u:LMTP None of the rcpts "
                         "were accepted for this message. Re-queueing\n",
                         hostPeerName(cxn->myHost), cxn->ident);
            }

            ReQueue(cxn, &(cxn->lmtp_todeliver_q), cxn->current_article);

            cxn->lmtp_state = LMTP_AUTHED_IDLE;
            lmtp_sendmessage(cxn, NULL);
        } else {
            if (WriteArticle(cxn, cxn->current_bufs) != RET_OK) {
                d_printf(0, "%s:%u:LMTP Error writing article\n",
                         hostPeerName(cxn->myHost), cxn->ident);
                lmtp_Disconnect(cxn);
                return;
            }

            cxn->lmtp_state = LMTP_WRITING_CONTENTS;
        }

        break;

    case LMTP_READING_CONTENTS:
        if (ask_keepgoing(str)) {
            goto reset;
        }

        /* need 1 response from server for every rcpt */
        cxn->current_rcpts_issued--;

        if (ask_code(str) != 250) {
            d_printf(1, "%s:%u:LMTP DATA failed with %d (%s)\n",
                     hostPeerName(cxn->myHost), cxn->ident, ask_code(str),
                     str);
            cxn->current_rcpts_okayed--;
        }

        if (cxn->current_rcpts_issued > 0) {
            goto reset;
        }

        /*
         * current_rcpts_okayed is number that succeeded
         *
         */
        if (cxn->current_rcpts_okayed == 0) {
            cxn->lmtp_state = LMTP_AUTHED_IDLE;
        } else {
            cxn->lmtp_state = LMTP_AUTHED_IDLE;
            cxn->lmtp_succeeded++;
            d_printf(1, "%s:%u:LMTP Woohoo! message accepted\n",
                     hostPeerName(cxn->myHost), cxn->ident);
        }

        /* we can delete article now */
        QueueForgetAbout(cxn, cxn->current_article, MSG_SUCCESS);

        /* try to send another if we have one and we're still idle
         * forgetting the msg might have made us unidle
         */
        if (cxn->lmtp_state == LMTP_AUTHED_IDLE) {
            lmtp_sendmessage(cxn, NULL);
        }

        break;

    case LMTP_READING_NOOP:
        if (ask_keepgoing(str)) {
            goto reset;
        }
        cxn->lmtp_state = LMTP_AUTHED_IDLE;
        break;

    case LMTP_READING_QUIT:
        d_printf(1, "%s:%u:LMTP read quit\n", hostPeerName(cxn->myHost),
                 cxn->ident);

        cxn->lmtp_state = LMTP_DISCONNECTED;

        DeleteIfDisconnected(cxn);
        break;

    default:

        d_printf(0, "%s:%u:LMTP Bad state in lmtp_readCB %u\n",
                 hostPeerName(cxn->myHost), cxn->ident, cxn->lmtp_state);
        lmtp_Disconnect(cxn);
        return;
    }
}

/*
 * Add a rcpt to:<foo> to the string
 *
 */

static void
addrcpt(char *newrcpt, int newrcptlen, char **out, int *outalloc)
{
    int size = strlen(*out);
    int fsize = size;
    int newsize = size + 9 + strlen(deliver_rcpt_to) + newrcptlen + 3;
    int rc;
    char c;

    /* see if we need to grow the string */
    if (newsize > *outalloc) {
        (*outalloc) = newsize;
        (*out) = xrealloc(*out, *outalloc);
    }

    strlcpy((*out) + size, "RCPT TO:<", newsize - size);
    size += 9;

    c = newrcpt[newrcptlen];
    newrcpt[newrcptlen] = '\0';
#if __GNUC__ > 4 || defined(__llvm__) || defined(__clang__)
#    pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
    rc = snprintf((*out) + size, newsize - size, deliver_rcpt_to, newrcpt);
#if __GNUC__ > 4 || defined(__llvm__) || defined(__clang__)
#    pragma GCC diagnostic warning "-Wformat-nonliteral"
#endif
    if (rc < 0) {
        /* Do nothing. */
    } else if (rc >= newsize - size) {
        size = newsize;
    } else {
        size += rc;
    }
    newrcpt[newrcptlen] = c;

    strlcpy((*out) + size, ">\r\n", newsize - size);

    /* has embedded '\n' */
    d_printf(2, "Attempting to send to: %s", (*out) + fsize);
}

/*
 * Takes the Newsgroups header field body and makes it into a list of
 * RCPT TO:'s we can send over the wire
 *
 *  in     - newsgroups header start
 *  in_end - end of newsgroups header
 *  num    - number of rcpt's we created
 */

static char *
ConvertRcptList(char *in, char *in_end, int *num)
{
    int retalloc = 400;
    char *ret = xmalloc(retalloc);
    char *str = in;
    char *laststart = in;

    (*num) = 0;

    /* start it off empty */
    strlcpy(ret, "", retalloc);

    while (str != in_end) {
        if ((*str) == ',') {
            /* eliminate leading whitespace */
            while (((*laststart) == ' ') || ((*laststart) == '\t')) {
                laststart++;
            }

#ifndef SMTPMODE
            addrcpt(laststart, str - laststart, &ret, &retalloc);
            (*num)++;
#endif /* SMTPMODE */
            laststart = str + 1;
        }

        str++;
    }

    if (laststart < str) {
        addrcpt(laststart, str - laststart, &ret, &retalloc);
        (*num)++;
    }

    return ret;
}

static void
addto(char *newrcpt, int newrcptlen, const char *sep, char **out,
      int *outalloc)
{
    int size = strlen(*out);
    int newsize =
        size + strlen(sep) + 1 + strlen(deliver_to_header) + newrcptlen + 1;
    int rc;
    char c;

    /* see if we need to grow the string */
    if (newsize > *outalloc) {
        (*outalloc) = newsize;
        (*out) = xrealloc(*out, *outalloc);
    }

    rc = snprintf((*out) + size, newsize - size, "%s<", sep);
    if (rc < 0) {
        /* Do nothing. */
    } else if (rc >= newsize - size) {
        size = newsize;
    } else {
        size += rc;
    }

    c = newrcpt[newrcptlen];
    newrcpt[newrcptlen] = '\0';
#if __GNUC__ > 4 || defined(__llvm__) || defined(__clang__)
#    pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
    rc = snprintf((*out) + size, newsize - size, deliver_to_header, newrcpt);
#if __GNUC__ > 4 || defined(__llvm__) || defined(__clang__)
#    pragma GCC diagnostic warning "-Wformat-nonliteral"
#endif
    if (rc < 0) {
        /* Do nothing. */
    } else if (rc >= newsize - size) {
        size = newsize;
    } else {
        size += rc;
    }
    newrcpt[newrcptlen] = c;

    strlcpy((*out) + size, ">", newsize - size);
}

/*
 * Takes the Newsgroups header field body and makes it into a To header field
 * body.
 *
 *  in     - newsgroups header start
 *  in_end - end of newsgroups header
 */

static char *
BuildToHeader(char *in, char *in_end)
{
    int retalloc = 400;
    char *ret = xmalloc(retalloc);
    char *str = in;
    char *laststart = in;
    const char *sep = "";

    /* start it off with the header field name */
    strlcpy(ret, "To: ", retalloc);

    while (str != in_end) {
        if ((*str) == ',') {
            /* eliminate leading whitespace */
            while (((*laststart) == ' ') || ((*laststart) == '\t')) {
                laststart++;
            }

            addto(laststart, str - laststart, sep, &ret, &retalloc);
            laststart = str + 1;

            /* separate multiple addresses with a comma */
            sep = ", ";
        }

        str++;
    }

    if (laststart < str) {
        addto(laststart, str - laststart, sep, &ret, &retalloc);
    }

    /* terminate the header field body */
    strlcat(ret, "\n\r", retalloc);
    return ret;
}

/*************************** END LMTP reading functions
 * ****************************/


/*
 * Process the control message queue. If we run out ask the host for more.
 *
 *  cxn       - connection object
 */

static void
imap_ProcessQueue(connection_t *cxn)
{
    article_queue_t *item;
    conn_ret result;

retry:

    /* pull an article off the queue */
    result = PopFromQueue(&(cxn->imap_controlMsg_q), &item);

    if (result == RET_QUEUE_EMPTY) {
        if (cxn->issue_quit) {
            imap_sendQuit(cxn);
            return;
        }

        cxn->imap_state = IMAP_IDLE_AUTHED;

        /* now we wait for articles from our Host, or we have some
           articles already. On infrequently used connections, the
           network link is torn down and rebuilt as needed. So we may
           be rebuilding the connection here in which case we have an
           article to send. */

        /* make sure imap has _lots_ of space too */
        if ((QueueItems(&(cxn->lmtp_todeliver_q)) == 0)
            && (QueueItems(&(cxn->imap_controlMsg_q)) == 0)) {
            if (hostGimmeArticle(cxn->myHost, cxn) == true)
                goto retry;
        }

        return;
    }

    cxn->current_control = item;

    switch (item->type) {
    case CREATE_FOLDER:
        imap_CreateGroup(cxn, item->data.control->folder);
        break;

    case CANCEL_MSG:
        imap_CancelMsg(cxn, item->data.control->folder);
        break;

    case DELETE_FOLDER:
        imap_DeleteGroup(cxn, item->data.control->folder);
        break;
    default:
        break;
    }

    return;
}


/*
 *
 * Pulls a message off the queue and trys to start sending it. If the
 * message is a control message put it in the control queue and grab
 * another message. If the message doesn't exist on disk or something
 * is wrong with it tell the host and try again. If we run out of
 * messages to get tell the host we want more
 *
 * cxn       - connection object
 * justadded - the article that was just added to the queue
 */

static void
lmtp_sendmessage(connection_t *cxn, Article justadded)
{
    bool res;
    conn_ret result;
    char *p;
    Buffer *bufs;
    char *control_header = NULL;
    char *control_header_end = NULL;

    article_queue_t *item;
    char *rcpt_list, *rcpt_list_end;

    /* retry point */
retry:

    /* pull an article off the queue */
    result = PopFromQueue(&(cxn->lmtp_todeliver_q), &item);

    if (result == RET_QUEUE_EMPTY) {
        if (cxn->issue_quit) {
            lmtp_IssueQuit(cxn);
            return;
        }
        /* now we wait for articles from our Host, or we have some
           articles already. On infrequently used connections, the
           network link is torn down and rebuilt as needed. So we may
           be rebuilding the connection here in which case we have an
           article to send. */

        /* make sure imap has space too */
        d_printf(1, "%s:%u stalled waiting for articles\n",
                 hostPeerName(cxn->myHost), cxn->ident);
        if ((QueueItems(&(cxn->lmtp_todeliver_q)) == 0)
            && (QueueItems(&(cxn->imap_controlMsg_q)) == 0)) {
            if (hostGimmeArticle(cxn->myHost, cxn) == true)
                goto retry;
        }

        return;
    }

    /* make sure contents ok; this also should load it into memory */
    res = artContentsOk(item->data.article);
    if (res == false) {
        if (justadded == item->data.article) {
            ReQueue(cxn, &(cxn->lmtp_todeliver_q), item);
            return;
        } else {
            /* tell to reject taking this message */
            QueueForgetAbout(cxn, item, MSG_MISSING);
        }

        goto retry;
    }

    /* Check if it's a control message */
    bufs = artGetNntpBuffers(item->data.article);
    if (bufs == NULL) {
        /* tell to reject taking this message */
        QueueForgetAbout(cxn, item, MSG_MISSING);
        goto retry;
    }

    result = FindHeader(bufs, "Control", &control_header, &control_header_end);
    if (result == RET_OK) {
        result = AddControlMsg(cxn, item->data.article, bufs, control_header,
                               control_header_end, 1);
        if (result != RET_OK) {
            d_printf(1, "%s:%u Error adding to [imap] control queue\n",
                     hostPeerName(cxn->myHost), cxn->ident);
            ReQueue(cxn, &(cxn->lmtp_todeliver_q), item);
            return;
        }

        switch (cxn->imap_state) {
        case IMAP_IDLE_AUTHED:
            /* we're idle. let's process the queue */
            imap_ProcessQueue(cxn);
            break;
        case IMAP_DISCONNECTED:
        case IMAP_WAITING:
            /* Let's connect. Once we're connected we can
               worry about the message */
            if (cxn->imap_sleepTimerId == 0) {
                if (imap_Connect(cxn) != RET_OK)
                    prepareReopenCbk(cxn, 0);
            }
            break;
        default:
            /* we're doing something right now */
            break;
        }

        /* all we did was add a control message.
           we still want to get an lmtp message */
        goto retry;
    }

    if (cxn->current_bufs != NULL) {
        /* freeBufferArray(cxn->current_bufs); */
        cxn->current_bufs = NULL;
    }
    cxn->current_bufs = bufs;
    cxn->current_article = item;

    /* we make use of pipelining here
       send:
         rset
         mail from
         rcpt to
         data
    */

    /* find out who it's going to */
    result = FindHeader(cxn->current_bufs, "Newsgroups", &rcpt_list,
                        &rcpt_list_end);

    if ((result != RET_OK) || (rcpt_list == NULL)) {
        d_printf(1, "%s:%u Didn't find Newsgroups header field\n",
                 hostPeerName(cxn->myHost), cxn->ident);
        QueueForgetAbout(cxn, cxn->current_article, MSG_FAIL_DELIVER);
        goto retry;
    }

    /* free's original rcpt_list */
    rcpt_list =
        ConvertRcptList(rcpt_list, rcpt_list_end, &cxn->current_rcpts_issued);
    cxn->current_rcpts_okayed = 0;

    if (mailfrom_name == NULL)
        mailfrom_name = xstrdup("");
    p = concat("RSET\r\n"
               "MAIL FROM:<",
               mailfrom_name, ">\r\n", rcpt_list, "DATA\r\n", (char *) 0);

    cxn->lmtp_state = LMTP_WRITING_UPTODATA;
    result = WriteToWire_lmtpstr(cxn, p, strlen(p));
    if (result != RET_OK) {
        d_printf(0, "%s:%u failed trying to write\n",
                 hostPeerName(cxn->myHost), cxn->ident);
        lmtp_Disconnect(cxn);
        return;
    }

    /* prepend To header field to article */
    if (deliver_to_header) {
        char *to_list, *to_list_end;
        int i, len;

        result = FindHeader(cxn->current_bufs, "Followup-To", &to_list,
                            &to_list_end);

        if ((result != RET_OK) || (to_list == NULL)) {
            FindHeader(cxn->current_bufs, "Newsgroups", &to_list,
                       &to_list_end);
        }

        /* free's original to_list */
        to_list = BuildToHeader(to_list, to_list_end);

        len = bufferArrayLen(cxn->current_bufs);
        cxn->current_bufs =
            xrealloc(cxn->current_bufs, sizeof(Buffer) * (len + 2));
        cxn->current_bufs[len + 1] = NULL;

        for (i = len; i > 0; i--) {
            cxn->current_bufs[i] = cxn->current_bufs[i - 1];
        }

        cxn->current_bufs[0] =
            newBufferByCharP(to_list, strlen(to_list + 1), strlen(to_list));
    }

    hostArticleOffered(cxn->myHost, cxn);
}

/*
 * Called by the EndPoint class when the timer goes off
 */
static void
dosomethingTimeoutCbk(TimeoutId id, void *data)
{
    Connection cxn = (Connection) data;

    ASSERT(id == cxn->dosomethingTimerId);

    show_stats(cxn);

    /* we're disconnected but there are things to send */
    if ((cxn->lmtp_state == LMTP_DISCONNECTED) && (cxn->lmtp_sleepTimerId == 0)
        && QueueItems(&(cxn->lmtp_todeliver_q)) > 0) {
        if (lmtp_Connect(cxn) != RET_OK)
            prepareReopenCbk(cxn, 1);
    }

    if ((cxn->imap_state == IMAP_DISCONNECTED) && (cxn->imap_sleepTimerId == 0)
        && (QueueItems(&(cxn->imap_controlMsg_q)) > 0)) {
        if (imap_Connect(cxn) != RET_OK)
            prepareReopenCbk(cxn, 0);
    }


    /* if we're idle and there are items to send let's send them */
    if ((cxn->lmtp_state == LMTP_AUTHED_IDLE)
        && QueueItems(&(cxn->lmtp_todeliver_q)) > 0) {
        lmtp_sendmessage(cxn, NULL);
    } else if (cxn->lmtp_state == LMTP_AUTHED_IDLE) {
        lmtp_noop(cxn);
    }

    if ((cxn->imap_state == IMAP_IDLE_AUTHED)
        && (QueueItems(&(cxn->imap_controlMsg_q)) > 0)) {
        imap_ProcessQueue(cxn);
    } else if (cxn->imap_state == IMAP_IDLE_AUTHED) {
        imap_noop(cxn);
    }

    /* set up the timer. */
    clearTimer(cxn->dosomethingTimerId);

    cxn->dosomethingTimerId =
        prepareSleep(dosomethingTimeoutCbk, cxn->dosomethingTimeout, cxn);
}

/* Give all articles in the queue back to the host. We're probably
 * going to exit soon.
 * */

static void
DeferAllArticles(connection_t *cxn, Q_t *q)
{
    article_queue_t *cur;
    conn_ret ret;

    while (1) {
        ret = PopFromQueue(q, &cur);
        if (ret == RET_QUEUE_EMPTY)
            return;

        if (ret == RET_OK) {
            QueueForgetAbout(cxn, cur, MSG_GIVE_BACK);
        } else {
            d_printf(0,
                     "%s:%u Error emptying queue (deffering all articles)\n",
                     hostPeerName(cxn->myHost), cxn->ident);
            return;
        }
    }
}

/*
 * Does the actual deletion of a connection and all its private data.
 */
static void
delConnection(Connection cxn)
{
    bool shutDown;
    Connection c, q;

    if (cxn == NULL)
        return;

    d_printf(1, "Deleting connection: %s:%u\n", hostPeerName(cxn->myHost),
             cxn->ident);

    for (c = gCxnList, q = NULL; c != NULL; q = c, c = c->next)
        if (c == cxn) {
            if (gCxnList == c)
                gCxnList = gCxnList->next;
            else
                q->next = c->next;
            break;
        }

    ASSERT(c != NULL);

    if (cxn->lmtp_endpoint != NULL)
        delEndPoint(cxn->lmtp_endpoint);
    if (cxn->imap_endpoint != NULL)
        delEndPoint(cxn->imap_endpoint);

    delBuffer(cxn->imap_rBuffer);
    delBuffer(cxn->lmtp_rBuffer);

    /* tell the Host we're outta here. */
    shutDown = hostCxnGone(cxn->myHost, cxn);

    cxn->ident = 0;
    cxn->timeCon = 0;

    free(cxn->ServerName);

    clearTimer(cxn->imap_readBlockedTimerId);
    clearTimer(cxn->imap_writeBlockedTimerId);
    clearTimer(cxn->lmtp_readBlockedTimerId);
    clearTimer(cxn->lmtp_writeBlockedTimerId);

    clearTimer(cxn->imap_sleepTimerId);
    cxn->imap_sleepTimerId = 0;
    clearTimer(cxn->lmtp_sleepTimerId);
    cxn->lmtp_sleepTimerId = 0;

    clearTimer(cxn->dosomethingTimerId);

    free(cxn->imap_respBuffer);
    free(cxn->lmtp_respBuffer);

    free(cxn);

    if (shutDown) {
        /* exit program if that was the last connection for the last host */
        /* XXX what about if there are ever multiple listeners?
           XXX    this will be executed if all hosts on only one of the
           XXX    listeners have gone */
        time_t now = theTime();
        char dateString[30];

        timeToString(now, dateString, sizeof(dateString));
        notice("ME finishing at %s", dateString);

        exit(0);
    }
}


/******************** PUBLIC FUNCTIONS ****************************/


/*
 * Create a new Connection.
 *
 * HOST is the host object we're owned by.
 * IDENT is an identifier to be added to syslog entries so we can tell
 *    what's happening on different connections to the same peer.
 * IPNAME is the name (or ip address) of the remote)
 * MAXTOUT is the maximum amount of time to wait for a response before
 *    considering the remote host dead.
 * PORTNUM is the portnum to contact on the remote end.
 * RESPTIMEOUT is the amount of time to wait for a response from a remote
 *    before considering the connection dead.
 * CLOSEPERIOD is the number of seconds after connecting that the
 *     connections should be closed down and reinitialized (due to problems
 *     with old NNTP servers that hold history files open. Value of 0 means
 *     no close down.
 */

Connection
newConnection(Host host, unsigned int ident, const char *ipname,
              unsigned int artTout UNUSED, unsigned int portNum UNUSED,
              unsigned int respTimeout, unsigned int closePeriod UNUSED,
              double lowPassLow UNUSED, double lowPassHigh UNUSED,
              double lowPassFilter UNUSED)
{
    Connection cxn;
    /* check arguments */

    /* allocate connection structure */
    cxn = xcalloc(1, sizeof(connection_t));

    cxn->ident = ident;
    cxn->ServerName = xstrdup(ipname);
    cxn->myHost = host;

    /* setup mailfrom user */
    if (gethostname(hostname, MAXHOSTNAMELEN) != 0) {
        d_printf(0, "%s gethostname failed\n", ipname);
        return NULL;
    }


    mailfrom_name = concat("news@", hostname, (char *) 0);

    cxn->next = gCxnList;
    gCxnList = cxn;
    gCxnCount++;

    /* init stuff */
    Initialize(cxn, respTimeout);

    return cxn;
}


/* Causes the Connection to build the network connection. */
bool
cxnConnect(Connection cxn)
{
    /* make the lmtp connection */
    if (lmtp_Connect(cxn) != RET_OK)
        return false;

    if (imap_Connect(cxn) != RET_OK)
        return false;

    return true;
}


static void
QuitIfIdle(Connection cxn)
{
    if ((cxn->lmtp_state == LMTP_AUTHED_IDLE)
        && (QueueItems(&(cxn->lmtp_todeliver_q)) <= 0)) {
        lmtp_IssueQuit(cxn);
    }
    if ((cxn->imap_state == IMAP_IDLE_AUTHED)
        && (QueueItems(&(cxn->imap_controlMsg_q)) <= 0)) {
        imap_sendQuit(cxn);
    }
}

static void
DeleteIfDisconnected(Connection cxn)
{
    /* we want to shut everything down. if both connections disconnected now we
     * can */
    if ((cxn->issue_quit >= 1) && (cxn->lmtp_state == LMTP_DISCONNECTED)
        && (cxn->imap_state == IMAP_DISCONNECTED)) {

        switch (cxn->issue_quit) {
        case 1:
            if (cxn->lmtp_state == LMTP_DISCONNECTED) {
                cxn->lmtp_state = LMTP_WAITING;
                cxn->imap_state = IMAP_WAITING;
                cxn->issue_quit = 0;
                hostCxnWaiting(cxn->myHost,
                               cxn); /* tell our Host we're waiting */
            }
            break;
        case 2:
            if (cxn->lmtp_state == LMTP_DISCONNECTED) {
                cxn->issue_quit = 0;

                if (imap_Connect(cxn) != RET_OK)
                    prepareReopenCbk(cxn, 0);
                if (lmtp_Connect(cxn) != RET_OK)
                    prepareReopenCbk(cxn, 1);
            }
            break;
        case 3:
            if (cxn->lmtp_state == LMTP_DISCONNECTED) {
                hostCxnDead(cxn->myHost, cxn);
                delConnection(cxn);
            }
            break;
        }
    }
}

/* puts the connection into the wait state (i.e. waits for an article
   before initiating a connect). Can only be called right after
   newConnection returns, or while the Connection is in the (internal)
   Sleeping state. */
void
cxnWait(Connection cxn)
{
    cxn->issue_quit = 1;

    QuitIfIdle(cxn);
}

/* The Connection will disconnect as if cxnDisconnect were called and then
   it automatically reconnects to the remote. */
void
cxnFlush(Connection cxn)
{
    cxn->issue_quit = 2;

    QuitIfIdle(cxn);
}


/* The Connection sends remaining articles, then issues a QUIT and then
   deletes itself */
void
cxnClose(Connection cxn)
{
    d_printf(0, "%s:%u Closing cxn\n", hostPeerName(cxn->myHost), cxn->ident);
    cxn->issue_quit = 3;

    QuitIfIdle(cxn);

    DeleteIfDisconnected(cxn);
}

/* The Connection drops all queueed articles, then issues a QUIT and then
   deletes itself */
void
cxnTerminate(Connection cxn)
{
    d_printf(0, "%s:%u Terminate\n", hostPeerName(cxn->myHost), cxn->ident);

    cxn->issue_quit = 3;

    /* give any articles back to host in both queues */
    DeferAllArticles(cxn, &(cxn->lmtp_todeliver_q));
    DeferAllArticles(cxn, &(cxn->imap_controlMsg_q));

    QuitIfIdle(cxn);
}

/* Blow away the connection gracelessly and immediately clean up */
void
cxnNuke(Connection cxn)
{
    d_printf(0, "%s:%u Nuking connection\n", cxn->ServerName, cxn->ident);

    cxn->issue_quit = 4;

    /* give any articles back to host in both queues */
    DeferAllArticles(cxn, &(cxn->lmtp_todeliver_q));
    DeferAllArticles(cxn, &(cxn->imap_controlMsg_q));

    imap_Disconnect(cxn);
    lmtp_Disconnect(cxn);

    hostCxnDead(cxn->myHost, cxn);
    delConnection(cxn);
}

/*
 * must
 *   true  - must queue article. Don't try sending
 *   false - queue of article may fail. Try sending
 *
 * Always adds to lmtp queue even if control message
 *
 */

static bool
ProcessArticle(Connection cxn, Article art, bool must)
{
    conn_ret result;

    /* Don't accept any articles when we're closing down the connection */
    if (cxn->issue_quit > 1) {
        return false;
    }

    /* if it's a regular message let's add it to the queue */
    result = AddToQueue(&(cxn->lmtp_todeliver_q), art, DELIVER, 1, must);

    if (result == RET_EXCEEDS_SIZE) {
        return false;
    }

    if (result != RET_OK) {
        d_printf(0, "%s:%u Error adding to delivery queue\n",
                 hostPeerName(cxn->myHost), cxn->ident);
        return must;
    }

    if (must)
        return true;

    switch (cxn->lmtp_state) {
    case LMTP_WAITING:
    case LMTP_DISCONNECTED:
        if (cxn->lmtp_sleepTimerId == 0)
            if (lmtp_Connect(cxn) != RET_OK)
                prepareReopenCbk(cxn, 1);
        break;

    case LMTP_AUTHED_IDLE:
        lmtp_sendmessage(cxn, art);
        break;
    default:
        /* currently doing something */
        break;
    }

    return true;
}

/* Tells the Connection to take the article and handle its
   transmission. If it can't (due to queue size or whatever), then the
   function returns false. The connection assumes ownership of the
   article if it accepts it (returns true). */
bool
cxnTakeArticle(Connection cxn, Article art)
{
    /* if we're closing down always refuse */
    if (cxn->issue_quit == 1)
        return false;

    return ProcessArticle(cxn, art, false);
}

/* Tell the Connection to take the article (if it can) for later
   processing. Assumes ownership of it if it takes it. */
bool
cxnQueueArticle(Connection cxn, Article art)
{
    return ProcessArticle(cxn, art, true);
}

/* generate a syslog message for the connections activity. Called by Host. */
void
cxnLogStats(Connection cxn, bool final)
{
    const char *peerName;
    time_t now = theTime();
    int total, bad;

    ASSERT(cxn != NULL);

    peerName = hostPeerName(cxn->myHost);

    total = cxn->lmtp_succeeded + cxn->lmtp_failed;
    total += cxn->cancel_succeeded + cxn->cancel_failed;
    total += cxn->create_succeeded + cxn->create_failed;
    total += cxn->remove_succeeded + cxn->remove_failed;

    bad = cxn->lmtp_failed;
    bad += cxn->cancel_failed;
    bad += cxn->create_failed;
    bad += cxn->remove_failed;

    notice("%s:%u %s seconds %ld accepted %d refused %d rejected %d", peerName,
           cxn->ident, (final ? "final" : "checkpoint"),
           (long) (now - cxn->timeCon), total, 0, bad);
    show_stats(cxn);

    if (final) {
        cxn->lmtp_succeeded = 0;
        cxn->lmtp_failed = 0;
        cxn->cancel_succeeded = 0;
        cxn->cancel_failed = 0;
        cxn->create_succeeded = 0;
        cxn->create_failed = 0;
        cxn->remove_succeeded = 0;
        cxn->remove_failed = 0;

        if (cxn->timeCon > 0)
            cxn->timeCon = theTime();
    }
}

/* return the number of articles the connection can be given. This lets
   the host shovel in as many as possible. May be zero. */
size_t
cxnQueueSpace(Connection cxn)
{
    int lmtpsize;
    int imapsize;

    lmtpsize = QueueSpace(&(cxn->lmtp_todeliver_q));
    imapsize = QueueSpace(&(cxn->imap_controlMsg_q));

    if (lmtpsize >= 1)
        lmtpsize--;
    if (imapsize >= 1)
        imapsize--;

    d_printf(1, "%s:%u Q Space lmtp size = %d state = %u\n",
             hostPeerName(cxn->myHost), cxn->ident, lmtpsize, cxn->lmtp_state);
    d_printf(1, "%s:%u Q Space imap size = %d state = %u\n",
             hostPeerName(cxn->myHost), cxn->ident, imapsize, cxn->imap_state);

    /* return the smaller of our 2 queues */
    if (lmtpsize < imapsize)
        return lmtpsize;
    else
        return imapsize;
}

/* adjust the mode no-CHECK filter values */
void
cxnSetCheckThresholds(Connection cxn, double lowFilter UNUSED,
                      double highFilter UNUSED, double lowPassFilter UNUSED)
{
    d_printf(1, "%s:%u Threshold change. This means nothing to me\n",
             hostPeerName(cxn->myHost), cxn->ident);
}

/* print some debugging info. */
void
gPrintCxnInfo(FILE *fp, unsigned int indentAmt)
{
    char indent[INDENT_BUFFER_SIZE];
    unsigned int i;
    Connection cxn;

    for (i = 0; i < MIN(INDENT_BUFFER_SIZE - 1, indentAmt); i++)
        indent[i] = ' ';
    indent[i] = '\0';

    fprintf(fp, "%sGlobal Connection list : (count %u) {\n", indent,
            gCxnCount);
    for (cxn = gCxnList; cxn != NULL; cxn = cxn->next)
        printCxnInfo(cxn, fp, indentAmt + INDENT_INCR);
    fprintf(fp, "%s}\n", indent);
}

void
printCxnInfo(Connection cxn, FILE *fp, unsigned int indentAmt)
{
    char indent[INDENT_BUFFER_SIZE];
    unsigned int i;
    article_queue_t *artH;

    for (i = 0; i < MIN(INDENT_BUFFER_SIZE - 1, indentAmt); i++)
        indent[i] = ' ';
    indent[i] = '\0';

    fprintf(fp, "%sConnection : %p {\n", indent, (void *) cxn);
    fprintf(fp, "%s    host : %p\n", indent, (void *) cxn->myHost);
    fprintf(fp, "%s    endpoint (imap): %p\n", indent,
            (void *) cxn->imap_endpoint);
    fprintf(fp, "%s    endpoint (lmtp): %p\n", indent,
            (void *) cxn->lmtp_endpoint);
    fprintf(fp, "%s    state (imap) : %s\n", indent,
            imap_stateToString(cxn->imap_state));
    fprintf(fp, "%s    state (lmtp) : %s\n", indent,
            lmtp_stateToString(cxn->lmtp_state));
    fprintf(fp, "%s    ident : %u\n", indent, cxn->ident);
    fprintf(fp, "%s    ip-name (imap): %s\n", indent, cxn->ServerName);
    fprintf(fp, "%s    ip-name (lmtp): %s\n", indent, cxn->ServerName);
    fprintf(fp, "%s    port-number (imap) : %d\n", indent, cxn->imap_port);
    fprintf(fp, "%s    port-number (lmtp) : %d\n", indent, cxn->lmtp_port);

    fprintf(fp, "%s    Issuing Quit : %d\n", indent, cxn->issue_quit);

    fprintf(fp, "%s    time-connected (imap) : %lu\n", indent,
            (unsigned long) cxn->imap_timeCon);
    fprintf(fp, "%s    time-connected (lmtp) : %lu\n", indent,
            (unsigned long) cxn->lmtp_timeCon);
    fprintf(fp, "%s    articles from INN : %d\n", indent,
            cxn->lmtp_succeeded + cxn->lmtp_failed + cxn->cancel_succeeded
                + cxn->cancel_failed + cxn->create_succeeded
                + cxn->create_failed + cxn->remove_succeeded
                + cxn->remove_failed + QueueSpace(&(cxn->lmtp_todeliver_q))
                + QueueSpace(&(cxn->imap_controlMsg_q)));
    fprintf(fp, "%s    LMTP STATS: yes: %d no: %d\n", indent,
            cxn->lmtp_succeeded, cxn->lmtp_failed);
    fprintf(fp, "%s    control:    yes: %d no: %d\n", indent,
            cxn->cancel_succeeded, cxn->cancel_failed);
    fprintf(fp, "%s    create:     yes: %d no: %d\n", indent,
            cxn->create_succeeded, cxn->create_failed);
    fprintf(fp, "%s    remove:     yes: %d no: %d\n", indent,
            cxn->remove_succeeded, cxn->remove_failed);

    fprintf(fp, "%s    response-timeout : %u\n", indent,
            cxn->imap_readTimeout);
    fprintf(fp, "%s    response-callback : %d\n", indent,
            cxn->imap_readBlockedTimerId);

    fprintf(fp, "%s    write-timeout : %u\n", indent, cxn->imap_writeTimeout);
    fprintf(fp, "%s    write-callback : %d\n", indent,
            cxn->imap_writeBlockedTimerId);

    fprintf(fp, "%s    reopen wait : %u\n", indent, cxn->imap_sleepTimeout);
    fprintf(fp, "%s    reopen id : %d\n", indent, cxn->imap_sleepTimerId);

    fprintf(fp, "%s    IMAP queue {\n", indent);
    for (artH = cxn->imap_controlMsg_q.head; artH != NULL; artH = artH->next)
        printArticleInfo(artH->data.article, fp, indentAmt + INDENT_INCR);
    fprintf(fp, "%s    }\n", indent);

    fprintf(fp, "%s    LMTP queue {\n", indent);
    for (artH = cxn->lmtp_todeliver_q.head; artH != NULL; artH = artH->next)
        printArticleInfo(artH->data.control->article, fp,
                         indentAmt + INDENT_INCR);
    fprintf(fp, "%s    }\n", indent);

    fprintf(fp, "%s}\n", indent);
}

/* config file load callback */
int
cxnConfigLoadCbk(void *data UNUSED)
{
    long iv;
    int rval = 1;
    FILE *fp = (FILE *) data;

    if (getInteger(topScope, "max-reconnect-time", &iv, NO_INHERIT)) {
        if (iv < 1) {
            rval = 0;
            logOrPrint(LOG_ERR, fp,
                       "ME config: value of %s (%ld) in %s cannot be less"
                       " than 1. Using %ld",
                       "max-reconnect-time", iv, "global scope",
                       (long) MAX_RECON_PER);
            iv = MAX_RECON_PER;
        }
    } else
        iv = MAX_RECON_PER;
    max_reconnect_period = (unsigned int) iv;

    if (getInteger(topScope, "initial-reconnect-time", &iv, NO_INHERIT)) {
        if (iv < 1) {
            rval = 0;
            logOrPrint(LOG_ERR, fp,
                       "ME config: value of %s (%ld) in %s cannot be less"
                       " than 1. Using %ld",
                       "initial-reconnect-time", iv, "global scope",
                       (long) INIT_RECON_PER);
            iv = INIT_RECON_PER;
        }
    } else
        iv = INIT_RECON_PER;
    init_reconnect_period = (unsigned int) iv;

    return rval;
}

/* check connection state is in cxnWaitingS, cxnConnectingS or cxnIdleS */
bool
cxnCheckstate(Connection cxn)
{
    d_printf(5, "%s:%u Being asked to check state\n",
             hostPeerName(cxn->myHost), cxn->ident);

    /* return false if either connection is doing something */
    if (cxn->imap_state > IMAP_IDLE_AUTHED)
        return false;
    if (cxn->lmtp_state > LMTP_AUTHED_IDLE)
        return false;

    return true;
}

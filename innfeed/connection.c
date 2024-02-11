/*
**  The implementation of the innfeed Connection class.
**
**  Written by James Brister <brister@vix.com>
**
**  The Connection object is what manages the NNTP protocol. If the remote
**  doesn't do streaming, then the standard IHAVE lock-step protcol is
**  performed. In the streaming situation we have two cases. One where we must
**  send CHECK commands, and the other where we can directly send TAKETHIS
**  commands without a prior CHECK.
**
**  The Connection object maintains four article queues. The first one is
**  where new articles are put if they need to have an IHAVE or CHECK command
**  sent for them. The second queue is where the articles move from the first
**  after their IHAVE/CHECK command is sent, but the reply has not yet been
**  seen. The third queue is where articles go after the IHAVE/CHECK reply has
**  been seen (and the reply says to send the article). It is articles in the
**  third queue that have the TAKETHIS command sent, or the body of an IHAVE.
**  The third queue is also where new articles go if the connection is running
**  in no-CHECK mode. The fourth queue is where the articles move to from the
**  third queue after their IHAVE-body or TAKETHIS command has been sent. When
**  the response to the IHAVE-body or TAKETHIS is received the articles are
**  removed from the fourth queue and the Host object controlling this
**  Connection is notified of the success or failure of the transfer.
**
**  The whole system is event-driven by the EndPoint class and the Host via
**  calls to prepareRead() and prepareWrite() and prepareSleep().
**
**
**  We should probably store the results of gethostbyname in the connection so
**  we can rotate through the address when one fails for connecting. Perhaps
**  the gethostbyname should be done in the Host and the connection should
**  just be given the address to use.
**
**  Should we worry about articles being stuck on a queue for ever if the
**  remote forgets to send a response to a CHECK?
**
**  Perhaps instead of killing the connection on some of the more simple
**  errors, we should perhaps try to flush the input and keep going.
**
**  Worry about counter overflow.
**
**  Worry about stats gathering when switch to no-check mode.
**
**  XXX if issueQUIT() has a problem and the state goes to cxnDeadS this is
**  not handled properly everywhere yet.
*/

#include "portable/system.h"

#include "innfeed.h"
#include "portable/socket.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <syslog.h>

#ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#endif
#include <time.h>

#if defined(__FreeBSD__)
#    include <sys/ioctl.h>
#endif

#include "inn/fdflag.h"
#include "inn/innconf.h"
#include "inn/libinn.h"
#include "inn/messages.h"
#include "inn/network.h"

#include "article.h"
#include "buffer.h"
#include "configfile.h"
#include "connection.h"
#include "endpoint.h"
#include "host.h"

#if defined(NDEBUG)
#    define VALIDATE_CONNECTION(x) ((void) 0)
#else
#    define VALIDATE_CONNECTION(x) validateConnection(x)
#endif

/*
 * Private types.
 */

/* We keep a linked list of articles the connection is trying to transmit */
typedef struct art_holder_s {
    Article article;
    struct art_holder_s *next;
} *ArtHolder;


typedef enum {
    cxnStartingS,    /* the connection's start state. */
    cxnWaitingS,     /* not connected. Waiting for an article. */
    cxnConnectingS,  /* in the middle of connecting */
    cxnIdleS,        /* open and ready to feed, has empty queues */
    cxnIdleTimeoutS, /* timed out in the idle state */
    cxnFeedingS,     /* in the processes of feeding articles */
    cxnSleepingS,    /* blocked on reestablishment timer */
    cxnFlushingS,    /* am waiting for queues to drain to bounce connection. */
    cxnClosingS,     /* have been told to close down permanently when queues
                        drained */
    cxnDeadS         /* connection is dead. */
} CxnState;

/* The Connection class */
struct connection_s {
    Host myHost;        /* the host who owns the connection */
    EndPoint myEp;      /* the endpoint the connection talks through */
    unsigned int ident; /* an identifier for syslogging. */
    CxnState state;     /* the state the connection is in */


    /*
     * The Connection maintains 4 queue of articles.
     */
    ArtHolder checkHead;        /* head of article list to do CHECK/IHAVE */
    ArtHolder checkRespHead;    /* head of list waiting on CHECK/IHAVE
                                   response */
    ArtHolder takeHead;         /* head of list of articles to send
                                   TAKETHIS/IHAVE-body */
    ArtHolder takeRespHead;     /* list of articles waiting on
                                   TAKETHIS/IHAVE-body response */
    unsigned int articleQTotal; /* number of articles in all four queues */
    ArtHolder missing;          /* head of missing list */


    Buffer respBuffer; /* buffer all responses are read into */

    char *ipName; /* the ip name (possibly quad) of the remote */

    unsigned int maxCheck; /* the max number of CHECKs to send */
    unsigned short port;   /* the port number to use */

    /*
     * Timeout values and their callback IDs
     */

    /* Timer for max amount of time between receiving articles from the
       Host */
    unsigned int articleReceiptTimeout;
    TimeoutId artReceiptTimerId;

    /* Timer for the max amount of time to wait for a response from the
       remote */
    unsigned int readTimeout;
    TimeoutId readBlockedTimerId;

    /* Timer for the max amount of time to wait for a any amount of data
       to be written to the remote */
    unsigned int writeTimeout;
    TimeoutId writeBlockedTimerId;

    /* Timer for the max number of seconds to keep the network connection
       up (long lasting connections give older nntp servers problems). */
    unsigned int flushTimeout;
    TimeoutId flushTimerId;

    /* Timer for the number of seconds to sleep before attempting a
       reconnect. */
    unsigned int sleepTimeout;
    TimeoutId sleepTimerId;


    bool loggedNoCr;    /* true if we logged the NOCR_MSG */
    bool immedRecon;    /* true if we recon immediately after flushing. */
    bool doesStreaming; /* true if remote will handle streaming */
    bool authenticated; /* true if remote authenticated */
    bool quitWasIssued; /* true if QUIT command was sent. */
    bool needsChecks;   /* true if we issue CHECK commands in
                           streaming mode (rather than just sending
                           TAKETHIS commands) */

    time_t timeCon; /* the time the connect happened (including
                       the MODE STREAM command) */
    time_t timeCon_checkpoint;

    /*
     * STATISTICS
     */
    unsigned int artsTaken; /* the number of articles INN gave this cxn */

    unsigned int checksIssued; /* the number of CHECKs/IHAVEs we
                                   sent.  Note that if we're running in
                                   no-CHECK mode, then we add in the
                                   TAKETHIS commands too */
    unsigned int checksIssued_checkpoint;

    unsigned int checksRefused; /* the number of response 435/438 */
    unsigned int checksRefused_checkpoint;
    unsigned int takesRejected; /* the number of response 437/439 received */
    unsigned int takesRejected_checkpoint;
    unsigned int takesOkayed; /* the number of response 235/239 received */
    unsigned int takesOkayed_checkpoint;

    double takesSizeRejected;
    double takesSizeRejected_checkpoint;
    double takesSizeOkayed;
    double takesSizeOkayed_checkpoint;

    double onThreshold;   /* for no-CHECK mode */
    double offThreshold;  /* for no-CHECK mode */
    double filterValue;   /* current value of IIR filter */
    double lowPassFilter; /* time constant for IIR filter */

    Connection next; /* for global list */
};

static Connection gCxnList = NULL;
static unsigned int gCxnCount = 0;
unsigned int max_reconnect_period = MAX_RECON_PER;
unsigned int init_reconnect_period = INIT_RECON_PER;
static Buffer dotFirstBuffer;
static Buffer dotBuffer;
static Buffer crlfBuffer;


/***************************************************
 *
 * Private function declarations.
 *
 ***************************************************/


/* I/O Callbacks */
static void connectionDone(EndPoint e, IoStatus i, Buffer *b, void *d);
static void getBanner(EndPoint e, IoStatus i, Buffer *b, void *d);
static void getAuthUserResponse(EndPoint e, IoStatus i, Buffer *b, void *d);
static void getAuthPassResponse(EndPoint e, IoStatus i, Buffer *b, void *d);
static void getModeResponse(EndPoint e, IoStatus i, Buffer *b, void *d);
static void responseIsRead(EndPoint e, IoStatus i, Buffer *b, void *d);
static void quitWritten(EndPoint e, IoStatus i, Buffer *b, void *d);
static void ihaveBodyDone(EndPoint e, IoStatus i, Buffer *b, void *d);
static void commandWriteDone(EndPoint e, IoStatus i, Buffer *b, void *d);
static void modeCmdIssued(EndPoint e, IoStatus i, Buffer *b, void *d);
static void authUserIssued(EndPoint e, IoStatus i, Buffer *b, void *d);
static void authPassIssued(EndPoint e, IoStatus i, Buffer *b, void *d);
static void writeProgress(EndPoint e, IoStatus i, Buffer *b, void *d);


/* Timer callbacks */
static void responseTimeoutCbk(TimeoutId id, void *data);
static void writeTimeoutCbk(TimeoutId id, void *data);
static void reopenTimeoutCbk(TimeoutId id, void *data);
static void flushCxnCbk(TimeoutId, void *data);
static void articleTimeoutCbk(TimeoutId id, void *data);

/* Work callbacks */
static void cxnWorkProc(EndPoint ep, void *data);


static void cxnSleepOrDie(Connection cxn);

/* Response processing. */
static void processResponse205(Connection cxn, char *response);
static void processResponse238(Connection cxn, char *response);
static void processResponse431(Connection cxn, char *response);
static void processResponse438(Connection cxn, char *response);
static void processResponse239(Connection cxn, char *response);
static void processResponse439(Connection cxn, char *response);
static void processResponse235(Connection cxn, char *response);
static void processResponse335(Connection cxn, char *response);
static void processResponse400(Connection cxn, char *response);
static void processResponse435(Connection cxn, char *response);
static void processResponse436(Connection cxn, char *response);
static void processResponse437(Connection cxn, char *response);
static void processResponse480(Connection cxn, char *response);
static void processResponse503(Connection cxn, char *response);


/* Misc functions */
static void cxnSleep(Connection cxn);
static void cxnDead(Connection cxn);
static void cxnIdle(Connection cxn);
static void noSuchMessageId(Connection cxn, unsigned int responseCode,
                            const char *msgid, const char *response);
static void abortConnection(Connection cxn);
static void resetConnection(Connection cxn);
static void deferAllArticles(Connection cxn);
static void deferQueuedArticles(Connection cxn);
static void doSomeWrites(Connection cxn);
static bool issueIHAVE(Connection cxn);
static void issueIHAVEBody(Connection cxn);
static bool issueStreamingCommands(Connection cxn);
static Buffer buildCheckBuffer(Connection cxn);
static Buffer *buildTakethisBuffers(Connection cxn, Buffer checkBuffer);
static void issueQUIT(Connection cxn);
static void initReadBlockedTimeout(Connection cxn);
static int prepareWriteWithTimeout(EndPoint endp, Buffer *buffers,
                                   EndpRWCB done, Connection cxn);
static void delConnection(Connection cxn);
static void incrFilter(Connection cxn);
static void decrFilter(Connection cxn);
static bool writesNeeded(Connection cxn);
static void validateConnection(Connection cxn);
static const char *stateToString(CxnState state);

static void issueModeStream(EndPoint e, Connection cxn);
static void issueAuthUser(EndPoint e, Connection cxn);
static void issueAuthPass(EndPoint e, Connection cxn);

static void prepareReopenCbk(Connection cxn);


/* Article queue management routines. */
static ArtHolder newArtHolder(Article art);
static void delArtHolder(ArtHolder artH);
static bool remArtHolder(ArtHolder art, ArtHolder *head, unsigned int *count);
static void appendArtHolder(ArtHolder artH, ArtHolder *head,
                            unsigned int *count);
static ArtHolder artHolderByMsgId(const char *msgid, ArtHolder head);

static int fudgeFactor(int initVal);


/***************************************************
 *
 * Public functions implementation.
 *
 ***************************************************/


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


/*
 * Create a new Connection object and return it. All fields are
 * initialized to reasonable values.
 */
Connection
newConnection(Host host, unsigned int id, const char *ipname,
              unsigned int articleReceiptTimeout, unsigned int portNum,
              unsigned int respTimeout, unsigned int flushTimeout,
              double lowPassLow, double lowPassHigh, double lowPassFilter)
{
    Connection cxn;
    bool croak = false;

    if (ipname == NULL) {
        d_printf(1, "NULL ipname in newConnection\n");
        croak = true;
    }

    if (ipname && strlen(ipname) == 0) {
        d_printf(1, "Empty ipname in newConnection\n");
        croak = true;
    }

    if (croak)
        return NULL;

    cxn = xcalloc(1, sizeof(struct connection_s));

    cxn->myHost = host;
    cxn->myEp = NULL;
    cxn->ident = id;

    cxn->checkHead = NULL;
    cxn->checkRespHead = NULL;
    cxn->takeHead = NULL;
    cxn->takeRespHead = NULL;

    cxn->articleQTotal = 0;
    cxn->missing = NULL;

    cxn->respBuffer = newBuffer(BUFFER_SIZE);
    ASSERT(cxn->respBuffer != NULL);

    cxn->ipName = xstrdup(ipname);
    cxn->port = portNum;

    /* Time out the higher numbered connections faster */
    cxn->articleReceiptTimeout =
        (unsigned int) (articleReceiptTimeout * 10.0 / (10.0 + id));
    cxn->artReceiptTimerId = 0;

    cxn->readTimeout = respTimeout;
    cxn->readBlockedTimerId = 0;

    cxn->writeTimeout = respTimeout; /* XXX should be a separate value */
    cxn->writeBlockedTimerId = 0;

    cxn->flushTimeout = fudgeFactor(flushTimeout);
    cxn->flushTimerId = 0;

    cxn->onThreshold = lowPassHigh * lowPassFilter / 100.0;
    cxn->offThreshold = lowPassLow * lowPassFilter / 100.0;
    cxn->lowPassFilter = lowPassFilter;

    cxn->sleepTimerId = 0;
    cxn->sleepTimeout = init_reconnect_period;

    resetConnection(cxn);

    cxn->next = gCxnList;
    gCxnList = cxn;
    gCxnCount++;

    cxn->state = cxnStartingS;

    return cxn;
}


/* Create a new endpoint hooked to a non-blocking socket that is trying to
 * connect to the host info stored in the Connection. On fast machines
 * connecting locally the connect() may have already succeeded when this
 * returns, but typically the connect will still be running and when it
 * completes. The Connection will be notified via a write callback setup by
 * prepareWrite below. If nothing goes wrong then this will return true
 * (even if the connect() has not yet completed). If something fails
 * (hostname lookup etc.) then it returns false (and the Connection is left
 * in the sleeping state)..
 *
 * Pre-state                Reason cxnConnect called
 * ---------                ------------------------
 * cxnStartingS             Connection owner issued call.
 * cxnWaitingS              Side effect of cxnTakeArticle() call.
 * cxnConnecting            Side effect of cxnFlush() call.
 * cxnSleepingS             Side effect of reopenTimeoutCbk() call.
 */
bool
cxnConnect(Connection cxn)
{
    struct sockaddr *cxnAddr;
    socklen_t len;
    int fd, rval;
    const char *src;
    const char *peerName = hostPeerName(cxn->myHost);

    ASSERT(cxn->myEp == NULL);

    if (!(cxn->state == cxnStartingS || cxn->state == cxnWaitingS
          || cxn->state == cxnFlushingS || cxn->state == cxnSleepingS)) {
        warn("%s:%u cxnsleep connection in bad state: %s",
             hostPeerName(cxn->myHost), cxn->ident, stateToString(cxn->state));
        cxnSleepOrDie(cxn);
        return false;
    }

    if (cxn->state == cxnWaitingS)
        ASSERT(cxn->articleQTotal == 1);
    else
        ASSERT(cxn->articleQTotal == 0);

    cxn->state = cxnConnectingS;

    cxnAddr = hostIpAddr(cxn->myHost);

    if (cxnAddr == NULL) {
        cxnSleepOrDie(cxn);
        return false;
    }

    if (cxnAddr->sa_family == AF_INET) {
        src = hostBindAddr(cxn->myHost);
        len = sizeof(struct sockaddr_in);
    } else {
        src = hostBindAddr6(cxn->myHost);
#if HAVE_INET6
        len = sizeof(struct sockaddr_in6);
#else
        /* This should never happen, but the compiler doesn't know that. */
        len = sizeof(struct sockaddr);
#endif
    }
    if (src && strcmp(src, "none") == 0)
        src = NULL;

    fd = network_client_create(cxnAddr->sa_family, SOCK_STREAM, src);
    if (fd < 0) {
        syswarn("%s:%u cxnsleep can't create socket", peerName, cxn->ident);
        d_printf(1, "Can't get a socket: %s\n", strerror(errno));

        hostIpFailed(cxn->myHost);
        cxnSleepOrDie(cxn);

        return false;
    }

    if (!fdflag_nonblocking(fd, true)) {
        syswarn("%s:%u cxnsleep can't set socket non-blocking", peerName,
                cxn->ident);
        close(fd);

        cxnSleepOrDie(cxn);

        return false;
    }

    rval = connect(fd, cxnAddr, len);
    if (rval < 0 && errno != EINPROGRESS) {
        syswarn("%s:%u connect", peerName, cxn->ident);
        hostIpFailed(cxn->myHost);
        close(fd);

        cxnSleepOrDie(cxn);

        return false;
    }

    if ((cxn->myEp = newEndPoint(fd)) == NULL) {
        /* If this happens, then fd was bigger than what select could handle,
           so endpoint.c refused to create the new object. */
        close(fd);
        cxnSleepOrDie(cxn);
        return false;
    }

    if (rval < 0)
        /* when the write callback gets done the connection went through */
        prepareWrite(cxn->myEp, NULL, NULL, connectionDone, cxn);
    else
        connectionDone(cxn->myEp, IoDone, NULL, cxn);

    /* connectionDone() could set state to sleeping */
    return (cxn->state == cxnConnectingS ? true : false);
}


/* Put the Connection into the wait state.
 *
 * Pre-state                Reason cxnWait called
 * ---------                ------------------------
 * cxnStartingS             - Connection owner called cxnWait().
 * cxnSleepingS             - Side effect of cxnFlush() call.
 * cxnConnectingS           - Side effect of cxnFlush() call.
 * cxnFlushingS             - Side effect of receiving response 205
 *                            and Connection had no articles when
 *                            cxnFlush() was issued.
 *                          - prepareRead failed.
 *                          - I/O failed.
 *
 */
void
cxnWait(Connection cxn)
{
    ASSERT(cxn->state == cxnStartingS || cxn->state == cxnSleepingS
           || cxn->state == cxnConnectingS || cxn->state == cxnFeedingS
           || cxn->state == cxnFlushingS);
    VALIDATE_CONNECTION(cxn);

    abortConnection(cxn);

    cxn->state = cxnWaitingS;

    hostCxnWaiting(cxn->myHost, cxn); /* tell our Host we're waiting */
}


/* Tells the Connection to flush itself (i.e. push out all articles,
 * issue a QUIT and drop the network connection.  If necessary a
 * reconnect will be done immediately after.  Called by the Host, or
 * by the timer callback.
 *
 * Pre-state                Reason cxnFlush called
 * ---------                ------------------------
 * ALL (except cxnDeadS     - Connection owner called cxnFlush().
 *    and cxnStartingS)
 * cxnFeedingS              - Side effect of flushCxnCbk() call.
 */
void
cxnFlush(Connection cxn)
{
    ASSERT(cxn != NULL);
    ASSERT(cxn->state != cxnStartingS);
    ASSERT(cxn->state != cxnDeadS);
    VALIDATE_CONNECTION(cxn);

    switch (cxn->state) {
    case cxnSleepingS:
        cxnWait(cxn);
        break;

    case cxnConnectingS:
        cxnWait(cxn);
        cxnConnect(cxn);
        break;

    case cxnIdleTimeoutS:
    case cxnIdleS:
        ASSERT(cxn->articleQTotal == 0);
        if (cxn->state != cxnIdleTimeoutS)
            clearTimer(cxn->artReceiptTimerId);
        clearTimer(cxn->flushTimerId);
        cxn->state = cxnFlushingS;
        issueQUIT(cxn);
        break;

    case cxnClosingS:
    case cxnFlushingS:
    case cxnWaitingS:
        if (cxn->articleQTotal == 0 && !writeIsPending(cxn->myEp))
            issueQUIT(cxn);
        break;

    case cxnFeedingS:
        /* we only reconnect immediately if we're not idle when cxnFlush()
           is called. */
        if (!cxn->immedRecon) {
            cxn->immedRecon = (cxn->articleQTotal > 0 ? true : false);
            d_printf(1, "%s:%u immediate reconnect for a cxnFlush()\n",
                     hostPeerName(cxn->myHost), cxn->ident);
        }

        clearTimer(cxn->flushTimerId);

        cxn->state = cxnFlushingS;

        if (cxn->articleQTotal == 0 && !writeIsPending(cxn->myEp))
            issueQUIT(cxn);
        break;

    default:
        die("Bad connection state: %s\n", stateToString(cxn->state));
    }
}


/*
 * Tells the Connection to dump all articles that are queued and to issue a
 * QUIT as quickly as possible. Much like cxnClose, except queued articles
 * are not sent, but are given back to the Host.
 */
void
cxnTerminate(Connection cxn)
{
    ASSERT(cxn != NULL);
    ASSERT(cxn->state != cxnDeadS);
    ASSERT(cxn->state != cxnStartingS);
    VALIDATE_CONNECTION(cxn);

    switch (cxn->state) {
    case cxnFeedingS:
        d_printf(1, "%s:%u Issuing terminate\n", hostPeerName(cxn->myHost),
                 cxn->ident);

        clearTimer(cxn->flushTimerId);

        cxn->state = cxnClosingS;

        deferQueuedArticles(cxn);
        if (cxn->articleQTotal == 0)
            issueQUIT(cxn); /* send out the QUIT if we can */
        break;

    case cxnIdleTimeoutS:
    case cxnIdleS:
        ASSERT(cxn->articleQTotal == 0);
        if (cxn->state != cxnIdleTimeoutS)
            clearTimer(cxn->artReceiptTimerId);
        clearTimer(cxn->flushTimerId);
        cxn->state = cxnClosingS;
        issueQUIT(cxn);
        break;

    case cxnFlushingS: /* we are in the middle of a periodic close. */
        d_printf(1, "%s:%u Connection already being flushed\n",
                 hostPeerName(cxn->myHost), cxn->ident);
        cxn->state = cxnClosingS;
        if (cxn->articleQTotal == 0)
            issueQUIT(cxn); /* send out the QUIT if we can */
        break;

    case cxnClosingS:
        d_printf(1, "%s:%u Connection already closing\n",
                 hostPeerName(cxn->myHost), cxn->ident);
        break;

    case cxnWaitingS:
    case cxnConnectingS:
    case cxnSleepingS:
        cxnDead(cxn);
        break;

    default:
        die("Bad connection state: %s\n", stateToString(cxn->state));
    }

    VALIDATE_CONNECTION(cxn);

    if (cxn->state == cxnDeadS) {
        d_printf(1, "%s:%u Deleting connection\n", hostPeerName(cxn->myHost),
                 cxn->ident);

        delConnection(cxn);
    }
}


/* Tells the Connection to do a disconnect and then when it is
 * disconnected to delete itself.
 *
 * Pre-state                Reason cxnClose called
 * ---------                ------------------------
 * ALL (except cxnDeadS     - Connecton owner called directly.
 *   and cxnStartingS).
 */
void
cxnClose(Connection cxn)
{
    ASSERT(cxn != NULL);
    ASSERT(cxn->state != cxnDeadS);
    ASSERT(cxn->state != cxnStartingS);
    VALIDATE_CONNECTION(cxn);

    switch (cxn->state) {
    case cxnFeedingS:
        d_printf(1, "%s:%u Issuing disconnect\n", hostPeerName(cxn->myHost),
                 cxn->ident);

        clearTimer(cxn->flushTimerId);

        cxn->state = cxnClosingS;

        if (cxn->articleQTotal == 0)
            issueQUIT(cxn); /* send out the QUIT if we can */
        break;

    case cxnIdleS:
    case cxnIdleTimeoutS:
        ASSERT(cxn->articleQTotal == 0);
        if (cxn->state != cxnIdleTimeoutS)
            clearTimer(cxn->artReceiptTimerId);
        clearTimer(cxn->flushTimerId);
        cxn->state = cxnClosingS;
        issueQUIT(cxn);
        break;

    case cxnFlushingS: /* we are in the middle of a periodic close. */
        d_printf(1, "%s:%u Connection already being flushed\n",
                 hostPeerName(cxn->myHost), cxn->ident);
        cxn->state = cxnClosingS;
        if (cxn->articleQTotal == 0)
            issueQUIT(cxn); /* send out the QUIT if we can */
        break;

    case cxnClosingS:
        d_printf(1, "%s:%u Connection already closing\n",
                 hostPeerName(cxn->myHost), cxn->ident);
        break;

    case cxnWaitingS:
    case cxnConnectingS:
    case cxnSleepingS:
        cxnDead(cxn);
        break;

    default:
        die("Bad connection state: %s\n", stateToString(cxn->state));
    }

    VALIDATE_CONNECTION(cxn);

    if (cxn->state == cxnDeadS) {
        d_printf(1, "%s:%u Deleting connection\n", hostPeerName(cxn->myHost),
                 cxn->ident);

        delConnection(cxn);
    }
}


/* This is what the Host calls to get us to tranfer an article. If
 * we're running the IHAVE sequence, then we can't take it if we've
 * got an article already. If we're running the CHECK/TAKETHIS
 * sequence, then we'll take as many as we can (up to our MAXCHECK
 * limit).
 */
bool
cxnTakeArticle(Connection cxn, Article art)
{
    bool rval = true;

    ASSERT(cxn != NULL);
    VALIDATE_CONNECTION(cxn);

    if (!cxnQueueArticle(cxn, art)) /* might change cxnIdleS to cxnFeedingS */
        return false;

    if (!(cxn->state == cxnConnectingS || cxn->state == cxnFeedingS
          || cxn->state == cxnWaitingS)) {
        warn("%s:%u cxnsleep connection in bad state: %s",
             hostPeerName(cxn->myHost), cxn->ident, stateToString(cxn->state));
        cxnSleepOrDie(cxn);
        return false;
    }

    if (cxn->state != cxnWaitingS) /* because articleQTotal == 1 */
        VALIDATE_CONNECTION(cxn);
    else
        ASSERT(cxn->articleQTotal == 1);

    switch (cxn->state) {
    case cxnWaitingS:
        cxnConnect(cxn);
        break;

    case cxnFeedingS:
        doSomeWrites(cxn);
        break;

    case cxnConnectingS:
        break;

    default:
        die("Bad connection state: %s\n", stateToString(cxn->state));
    }

    return rval;
}


/* if there's room in the Connection then stick the article on the
 * queue, otherwise return false.
 */
bool
cxnQueueArticle(Connection cxn, Article art)
{
    ArtHolder newArt;
    bool rval = false;

    ASSERT(cxn != NULL);
    ASSERT(cxn->state != cxnStartingS);
    ASSERT(cxn->state != cxnDeadS);
    VALIDATE_CONNECTION(cxn);

    switch (cxn->state) {
    case cxnClosingS:
        d_printf(5, "%s:%u Refusing article due to closing\n",
                 hostPeerName(cxn->myHost), cxn->ident);
        break;

    case cxnFlushingS:
        d_printf(5, "%s:%u Refusing article due to flushing\n",
                 hostPeerName(cxn->myHost), cxn->ident);
        break;

    case cxnSleepingS:
        d_printf(5, "%s:%u Refusing article due to sleeping\n",
                 hostPeerName(cxn->myHost), cxn->ident);
        break;

    case cxnWaitingS:
        rval = true;
        newArt = newArtHolder(art);
        appendArtHolder(newArt, &cxn->checkHead, &cxn->articleQTotal);
        break;

    case cxnConnectingS:
        if (cxn->articleQTotal != 0)
            break;
        rval = true;
        newArt = newArtHolder(art);
        appendArtHolder(newArt, &cxn->checkHead, &cxn->articleQTotal);
        break;

    case cxnIdleS:
    case cxnFeedingS:
        if (cxn->articleQTotal >= cxn->maxCheck)
            d_printf(5,
                     "%s:%u Refusing article due to articleQTotal >= maxCheck "
                     "(%u > %u)\n",
                     hostPeerName(cxn->myHost), cxn->ident, cxn->articleQTotal,
                     cxn->maxCheck);
        else {
            rval = true;
            newArt = newArtHolder(art);
            if (cxn->needsChecks)
                appendArtHolder(newArt, &cxn->checkHead, &cxn->articleQTotal);
            else
                appendArtHolder(newArt, &cxn->takeHead, &cxn->articleQTotal);
            if (cxn->state == cxnIdleS) {
                cxn->state = cxnFeedingS;
                clearTimer(cxn->artReceiptTimerId);
            }
        }
        break;

    default:
        die("Invalid state: %s\n", stateToString(cxn->state));
    }

    if (rval) {
        d_printf(5, "%s:%u accepting article %s\n", hostPeerName(cxn->myHost),
                 cxn->ident, artMsgId(art));

        cxn->artsTaken++;
    }

    return rval;
}


/*
 * Generate a log message for activity.  Usually called by the Connection's
 * owner.
 */
void
cxnLogStats(Connection cxn, bool final)
{
    const char *peerName;
    time_t now = theTime();

    ASSERT(cxn != NULL);

    /* Only log stats when in one of these three states. */
    switch (cxn->state) {
    case cxnFeedingS:
    case cxnFlushingS:
    case cxnClosingS:
        break;

    default:
        return;
    }

    peerName = hostPeerName(cxn->myHost);

    /* Log a checkpoint in any case. */
    notice("%s:%u checkpoint seconds %ld offered %u accepted %u refused %u"
           " rejected %u accsize %.0f rejsize %.0f",
           peerName, cxn->ident, (long) (now - cxn->timeCon_checkpoint),
           cxn->checksIssued - cxn->checksIssued_checkpoint,
           cxn->takesOkayed - cxn->takesOkayed_checkpoint,
           cxn->checksRefused - cxn->checksRefused_checkpoint,
           cxn->takesRejected - cxn->takesRejected_checkpoint,
           cxn->takesSizeOkayed - cxn->takesSizeOkayed_checkpoint,
           cxn->takesSizeRejected - cxn->takesSizeRejected_checkpoint);

    if (final) {
        notice("%s:%u final seconds %ld offered %u accepted %u refused %u"
               " rejected %u accsize %.0f rejsize %.0f",
               peerName, cxn->ident, (long) (now - cxn->timeCon),
               cxn->checksIssued, cxn->takesOkayed, cxn->checksRefused,
               cxn->takesRejected, cxn->takesSizeOkayed,
               cxn->takesSizeRejected);

        cxn->artsTaken = 0;
        cxn->checksIssued = 0;
        cxn->checksRefused = 0;
        cxn->takesRejected = 0;
        cxn->takesOkayed = 0;
        cxn->takesSizeRejected = 0;
        cxn->takesSizeOkayed = 0;

        if (cxn->timeCon > 0) {
            cxn->timeCon = theTime();
        }
    }

    cxn->timeCon_checkpoint = now;
    cxn->checksIssued_checkpoint = cxn->checksIssued;
    cxn->takesOkayed_checkpoint = cxn->takesOkayed;
    cxn->checksRefused_checkpoint = cxn->checksRefused;
    cxn->takesRejected_checkpoint = cxn->takesRejected;
    cxn->takesSizeOkayed_checkpoint = cxn->takesSizeOkayed;
    cxn->takesSizeRejected_checkpoint = cxn->takesSizeRejected;
}


/*
 * return the number of articles the connection will accept.
 */
size_t
cxnQueueSpace(Connection cxn)
{
    int rval = 0;

    ASSERT(cxn != NULL);

    if (cxn->state == cxnFeedingS || cxn->state == cxnIdleS
        || cxn->state == cxnConnectingS || cxn->state == cxnWaitingS)
        rval = cxn->maxCheck - cxn->articleQTotal;

    return rval;
}


/*
 * Print info on all the connections that currently exist.
 */
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


/*
 * Print the info about the given connection.
 */
void
printCxnInfo(Connection cxn, FILE *fp, unsigned int indentAmt)
{
    char indent[INDENT_BUFFER_SIZE];
    unsigned int i;
    ArtHolder artH;

    for (i = 0; i < MIN(INDENT_BUFFER_SIZE - 1, indentAmt); i++)
        indent[i] = ' ';
    indent[i] = '\0';

    fprintf(fp, "%sConnection : %p {\n", indent, (void *) cxn);
    fprintf(fp, "%s    host : %p\n", indent, (void *) cxn->myHost);
    fprintf(fp, "%s    endpoint : %p\n", indent, (void *) cxn->myEp);
    fprintf(fp, "%s    state : %s\n", indent, stateToString(cxn->state));
    fprintf(fp, "%s    ident : %u\n", indent, cxn->ident);
    fprintf(fp, "%s    ip-name : %s\n", indent, cxn->ipName);
    fprintf(fp, "%s    port-number : %u\n", indent, cxn->port);
    fprintf(fp, "%s    max-checks : %u\n", indent, cxn->maxCheck);
    fprintf(fp, "%s    does-streaming : %s\n", indent,
            boolToString(cxn->doesStreaming));
    fprintf(fp, "%s    authenticated : %s\n", indent,
            boolToString(cxn->authenticated));
    fprintf(fp, "%s    quitWasIssued : %s\n", indent,
            boolToString(cxn->quitWasIssued));
    fprintf(fp, "%s    needs-checks : %s\n", indent,
            boolToString(cxn->needsChecks));

    fprintf(fp, "%s    time-connected : %ld\n", indent, (long) cxn->timeCon);
    fprintf(fp, "%s    articles from INN : %u\n", indent, cxn->artsTaken);
    fprintf(fp, "%s    articles offered : %u\n", indent, cxn->checksIssued);
    fprintf(fp, "%s    articles refused : %u\n", indent, cxn->checksRefused);
    fprintf(fp, "%s    articles rejected : %u\n", indent, cxn->takesRejected);
    fprintf(fp, "%s    articles accepted : %u\n", indent, cxn->takesOkayed);
    fprintf(fp, "%s    low-pass upper limit : %0.6f\n", indent,
            cxn->onThreshold);
    fprintf(fp, "%s    low-pass lower limit : %0.6f\n", indent,
            cxn->offThreshold);
    fprintf(fp, "%s    low-pass filter tc : %0.6f\n", indent,
            cxn->lowPassFilter);
    fprintf(fp, "%s    low-pass filter : %0.6f\n", indent, cxn->filterValue);

    fprintf(fp, "%s    article-timeout : %u\n", indent,
            cxn->articleReceiptTimeout);
    fprintf(fp, "%s    article-callback : %d\n", indent,
            cxn->artReceiptTimerId);

    fprintf(fp, "%s    response-timeout : %u\n", indent, cxn->readTimeout);
    fprintf(fp, "%s    response-callback : %d\n", indent,
            cxn->readBlockedTimerId);

    fprintf(fp, "%s    write-timeout : %u\n", indent, cxn->writeTimeout);
    fprintf(fp, "%s    write-callback : %d\n", indent,
            cxn->writeBlockedTimerId);

    fprintf(fp, "%s    flushTimeout : %u\n", indent, cxn->flushTimeout);
    fprintf(fp, "%s    flushTimerId : %d\n", indent, cxn->flushTimerId);

    fprintf(fp, "%s    reopen wait : %u\n", indent, cxn->sleepTimeout);
    fprintf(fp, "%s    reopen id : %d\n", indent, cxn->sleepTimerId);

    fprintf(fp, "%s    CHECK queue {\n", indent);
    for (artH = cxn->checkHead; artH != NULL; artH = artH->next)
        printArticleInfo(artH->article, fp, indentAmt + INDENT_INCR);
    fprintf(fp, "%s    }\n", indent);

    fprintf(fp, "%s    CHECK Response queue {\n", indent);
    for (artH = cxn->checkRespHead; artH != NULL; artH = artH->next)
        printArticleInfo(artH->article, fp, indentAmt + INDENT_INCR);
    fprintf(fp, "%s    }\n", indent);

    fprintf(fp, "%s    TAKE queue {\n", indent);
    for (artH = cxn->takeHead; artH != NULL; artH = artH->next)
        printArticleInfo(artH->article, fp, indentAmt + INDENT_INCR);
    fprintf(fp, "%s    }\n", indent);

    fprintf(fp, "%s    TAKE response queue {\n", indent);
    for (artH = cxn->takeRespHead; artH != NULL; artH = artH->next)
        printArticleInfo(artH->article, fp, indentAmt + INDENT_INCR);
    fprintf(fp, "%s    }\n", indent);

    fprintf(fp, "%s    response buffer {\n", indent);
    printBufferInfo(cxn->respBuffer, fp, indentAmt + INDENT_INCR);
    fprintf(fp, "%s    }\n", indent);

    fprintf(fp, "%s}\n", indent);
}


/*
 * Return whether the connection will accept articles.
 */
bool
cxnCheckstate(Connection cxn)
{
    bool rval = false;

    ASSERT(cxn != NULL);

    if (cxn->state == cxnFeedingS || cxn->state == cxnIdleS
        || cxn->state == cxnConnectingS)
        rval = true;

    return rval;
}


/**********************************************************************/
/**                       STATIC PRIVATE FUNCTIONS                   **/
/**********************************************************************/


/*
 * ENDPOINT CALLBACK AREA.
 *
 * All the functions in this next section are callbacks fired by the
 * EndPoint objects/class (either timers or i/o completion callbacks)..
 */


/*
 * this is the first stage of the NNTP FSM. This function is called
 * when the tcp/ip network connection is setup and we should get
 * ready to read the banner message. When this function returns the
 * state of the Connection will still be cxnConnectingS unless
 * something broken, in which case it probably went into the
 * cxnSleepingS state.
 */
static void
connectionDone(EndPoint e, IoStatus i, Buffer *b, void *d)
{
    Buffer *readBuffers;
    Connection cxn = (Connection) d;
    const char *peerName;
    int optval;
    socklen_t size;

    ASSERT(b == NULL);
    ASSERT(cxn->state == cxnConnectingS);
    ASSERT(!writeIsPending(cxn->myEp));

    size = sizeof(optval);
    peerName = hostPeerName(cxn->myHost);

    if (i != IoDone) {
        errno = endPointErrno(e);
        syswarn("%s:%u cxnsleep i/o failed", peerName, cxn->ident);

        cxnSleepOrDie(cxn);
    } else if (getsockopt(endPointFd(e), SOL_SOCKET, SO_ERROR,
                          (char *) &optval, &size)
               != 0) {
        /* This is bad. Can't even get the SO_ERROR value out of the socket */
        syswarn("%s:%u cxnsleep internal getsockopt", peerName, cxn->ident);

        cxnSleepOrDie(cxn);
    } else if (optval != 0) {
        /* if the connect failed then the only way to know is by getting
           the SO_ERROR value out of the socket. */
        errno = optval;
        syswarn("%s:%u cxnsleep connect", peerName, cxn->ident);
        hostIpFailed(cxn->myHost);

        cxnSleepOrDie(cxn);
    } else {
        readBuffers = makeBufferArray(bufferTakeRef(cxn->respBuffer), NULL);

        if (!prepareRead(e, readBuffers, getBanner, cxn, 1)) {
            warn("%s:%u cxnsleep prepare read failed", peerName, cxn->ident);

            cxnSleepOrDie(cxn);
        } else {
            initReadBlockedTimeout(cxn);

            /* set up the callback for closing down the connection at regular
               intervals (due to problems with long running nntpd). */
            if (cxn->flushTimeout > 0)
                cxn->flushTimerId =
                    prepareSleep(flushCxnCbk, cxn->flushTimeout, cxn);

            /* The state doesn't change yet until we've read the banner and
               tried the MODE STREAM command. */
        }
    }
    VALIDATE_CONNECTION(cxn);
}


/*
 * This is called when we are so far in the connection setup that
 * we're confident it'll work.  If the connection is IPv6, remove
 * the IPv4 addresses from the address list.
 */
static void
connectionIfIpv6DeleteIpv4Addr(Connection cxn)
{
    union {
        struct sockaddr sa;
        struct sockaddr_storage ss;
    } u;
    socklen_t len = sizeof(u);

    if (getpeername(endPointFd(cxn->myEp), &u.sa, &len) < 0)
        return;
    if (u.sa.sa_family == AF_INET)
        return;

    hostDeleteIpv4Addr(cxn->myHost);
}


/*
 * Called when the banner message has been read off the wire and is
 * in the buffer(s). When this function returns the state of the
 * Connection will still be cxnConnectingS unless something broken,
 * in which case it probably went into the cxnSleepiongS state.
 */
static void
getBanner(EndPoint e, IoStatus i, Buffer *b, void *d)
{
    Buffer *readBuffers;
    Connection cxn = (Connection) d;
    char *p = bufferBase(b[0]);
    int code;
    bool isOk = false;
    const char *peerName;
    char *rest;

    ASSERT(e == cxn->myEp);
    ASSERT(b[0] == cxn->respBuffer);
    ASSERT(b[1] == NULL);
    ASSERT(cxn->state == cxnConnectingS);
    ASSERT(!writeIsPending(cxn->myEp));


    peerName = hostPeerName(cxn->myHost);

    bufferAddNullByte(b[0]);

    if (i != IoDone) {
        errno = endPointErrno(cxn->myEp);
        syswarn("%s:%u cxnsleep can't read banner", peerName, cxn->ident);
        hostIpFailed(cxn->myHost);

        cxnSleepOrDie(cxn);
    } else if (strchr(p, '\n')
               == NULL) { /* partial read. expand buffer and retry */
        expandBuffer(b[0], BUFFER_EXPAND_AMOUNT);
        readBuffers = makeBufferArray(bufferTakeRef(b[0]), NULL);

        if (!prepareRead(e, readBuffers, getBanner, cxn, 1)) {
            warn("%s:%u cxnsleep prepare read failed", peerName, cxn->ident);

            cxnSleepOrDie(cxn);
        }
    } else if (!getNntpResponse(p, &code, &rest)) {
        trim_ws(p);

        warn("%s:%u cxnsleep response format: %s", peerName, cxn->ident, p);

        cxnSleepOrDie(cxn);
    } else {
        trim_ws(p);

        switch (code) {
        case 200: /* normal */
        case 201: /* can transfer but not post -- old nntpd */
            isOk = true;
            break;

        case 400:
            cxnSleepOrDie(cxn);
            hostIpFailed(cxn->myHost);
            hostCxnBlocked(cxn->myHost, cxn, rest);
            break;

        case 502:
            warn("%s:%u cxnsleep no permission to talk: %s", peerName,
                 cxn->ident, p);
            cxnSleepOrDie(cxn);
            hostIpFailed(cxn->myHost);
            hostCxnBlocked(cxn->myHost, cxn, rest);
            break;

        default:
            warn("%s:%u cxnsleep response unknown banner: %d %s", peerName,
                 cxn->ident, code, p);
            d_printf(1, "%s:%u Unknown response code: %d: %s\n",
                     hostPeerName(cxn->myHost), cxn->ident, code, p);
            cxnSleepOrDie(cxn);
            hostIpFailed(cxn->myHost);
            hostCxnBlocked(cxn->myHost, cxn, rest);
            break;
        }

        if (isOk) {
            /* If we got this far and the connection is IPv6, remove
               the IPv4 addresses from the address list. */
            connectionIfIpv6DeleteIpv4Addr(cxn);

            if (hostUsername(cxn->myHost) != NULL
                && hostPassword(cxn->myHost) != NULL)
                issueAuthUser(e, cxn);
            else
                issueModeStream(e, cxn);
        }
    }
    freeBufferArray(b);
}


static void
issueAuthUser(EndPoint e, Connection cxn)
{
    Buffer authUserBuffer;
    Buffer *authUserCmdBuffers, *readBuffers;
    size_t lenBuff = 0;
    char *t;

    /* 17 == strlen("AUTHINFO USER \r\n\0") */
    lenBuff = (17 + strlen(hostUsername(cxn->myHost)));
    authUserBuffer = newBuffer(lenBuff);
    t = bufferBase(authUserBuffer);

    sprintf(t, "AUTHINFO USER %s\r\n", hostUsername(cxn->myHost));
    bufferSetDataSize(authUserBuffer, strlen(t));

    authUserCmdBuffers = makeBufferArray(authUserBuffer, NULL);

    if (!prepareWriteWithTimeout(e, authUserCmdBuffers, authUserIssued, cxn)) {
        die("%s:%u fatal prepare write for authinfo user failed",
            hostPeerName(cxn->myHost), cxn->ident);
    }

    bufferSetDataSize(cxn->respBuffer, 0);

    readBuffers = makeBufferArray(bufferTakeRef(cxn->respBuffer), NULL);

    if (!prepareRead(e, readBuffers, getAuthUserResponse, cxn, 1)) {
        warn("%s:%u cxnsleep prepare read failed", hostPeerName(cxn->myHost),
             cxn->ident);
        freeBufferArray(readBuffers);
        cxnSleepOrDie(cxn);
    }
}


static void
issueAuthPass(EndPoint e, Connection cxn)
{
    Buffer authPassBuffer;
    Buffer *authPassCmdBuffers, *readBuffers;
    size_t lenBuff = 0;
    char *t;

    /* 17 == strlen("AUTHINFO PASS \r\n\0") */
    lenBuff = (17 + strlen(hostPassword(cxn->myHost)));
    authPassBuffer = newBuffer(lenBuff);
    t = bufferBase(authPassBuffer);

    sprintf(t, "AUTHINFO PASS %s\r\n", hostPassword(cxn->myHost));
    bufferSetDataSize(authPassBuffer, strlen(t));

    authPassCmdBuffers = makeBufferArray(authPassBuffer, NULL);

    if (!prepareWriteWithTimeout(e, authPassCmdBuffers, authPassIssued, cxn)) {
        die("%s:%u fatal prepare write for authinfo pass failed",
            hostPeerName(cxn->myHost), cxn->ident);
    }

    bufferSetDataSize(cxn->respBuffer, 0);

    readBuffers = makeBufferArray(bufferTakeRef(cxn->respBuffer), NULL);

    if (!prepareRead(e, readBuffers, getAuthPassResponse, cxn, 1)) {
        warn("%s:%u cxnsleep prepare read failed", hostPeerName(cxn->myHost),
             cxn->ident);
        freeBufferArray(readBuffers);
        cxnSleepOrDie(cxn);
    }
}


static void
issueModeStream(EndPoint e, Connection cxn)
{
    Buffer *modeCmdBuffers, *readBuffers;
    Buffer modeBuffer;
    char *p;

#define MODE_CMD "MODE STREAM\r\n"

    modeBuffer = newBuffer(strlen(MODE_CMD) + 1);
    p = bufferBase(modeBuffer);

    /* now issue the MODE STREAM command */
    d_printf(1, "%s:%u Issuing the streaming command\n",
             hostPeerName(cxn->myHost), cxn->ident);

    strlcpy(p, MODE_CMD, bufferSize(modeBuffer));

    bufferSetDataSize(modeBuffer, strlen(p));

    modeCmdBuffers = makeBufferArray(modeBuffer, NULL);

    if (!prepareWriteWithTimeout(e, modeCmdBuffers, modeCmdIssued, cxn)) {
        die("%s:%u fatal prepare write for mode stream failed",
            hostPeerName(cxn->myHost), cxn->ident);
    }

    bufferSetDataSize(cxn->respBuffer, 0);

    readBuffers = makeBufferArray(bufferTakeRef(cxn->respBuffer), NULL);

    if (!prepareRead(e, readBuffers, getModeResponse, cxn, 1)) {
        warn("%s:%u cxnsleep prepare read failed", hostPeerName(cxn->myHost),
             cxn->ident);
        freeBufferArray(readBuffers);
        cxnSleepOrDie(cxn);
    }
}


/*
 *
 */
static void
getAuthUserResponse(EndPoint e, IoStatus i, Buffer *b, void *d)
{
    Connection cxn = (Connection) d;
    int code;
    char *p = bufferBase(b[0]);
    Buffer *buffers;
    const char *peerName;

    ASSERT(e == cxn->myEp);
    ASSERT(b[0] == cxn->respBuffer);
    ASSERT(b[1] == NULL); /* only ever one buffer on this read */
    ASSERT(cxn->state == cxnConnectingS);
    VALIDATE_CONNECTION(cxn);

    peerName = hostPeerName(cxn->myHost);

    bufferAddNullByte(b[0]);

    d_printf(1, "%s:%u Processing authinfo user response: %s", /* no NL */
             hostPeerName(cxn->myHost), cxn->ident, p);

    if (i == IoDone && writeIsPending(cxn->myEp)) {
        /* badness. should never happen */
        warn("%s:%u cxnsleep authinfo command still pending", peerName,
             cxn->ident);

        cxnSleepOrDie(cxn);
    } else if (i != IoDone) {
        if (i != IoEOF) {
            errno = endPointErrno(e);
            syswarn("%s:%u cxnsleep can't read response", peerName,
                    cxn->ident);
        }
        cxnSleepOrDie(cxn);
    } else if (strchr(p, '\n') == NULL) {
        /* partial read */
        expandBuffer(b[0], BUFFER_EXPAND_AMOUNT);

        buffers = makeBufferArray(bufferTakeRef(b[0]), NULL);
        if (!prepareRead(e, buffers, getAuthUserResponse, cxn, 1)) {
            warn("%s:%u cxnsleep prepare read failed", peerName, cxn->ident);
            freeBufferArray(buffers);
            cxnSleepOrDie(cxn);
        }
    } else {
        clearTimer(cxn->readBlockedTimerId);

        if (!getNntpResponse(p, &code, NULL)) {
            warn("%s:%u cxnsleep response to AUTHINFO USER: %s", peerName,
                 cxn->ident, p);

            cxnSleepOrDie(cxn);
        } else {
            notice("%s:%u connected", peerName, cxn->ident);

            switch (code) {
            case 381:
                issueAuthPass(e, cxn);
                break;

            default:
                warn("%s:%u cxnsleep response to AUTHINFO USER: %s", peerName,
                     cxn->ident, p);
                cxn->authenticated = true;
                issueModeStream(e, cxn);
                break;
            }
        }
    }
}


/*
 *
 */
static void
getAuthPassResponse(EndPoint e, IoStatus i, Buffer *b, void *d)
{
    Connection cxn = (Connection) d;
    int code;
    char *p = bufferBase(b[0]);
    Buffer *buffers;
    const char *peerName;

    ASSERT(e == cxn->myEp);
    ASSERT(b[0] == cxn->respBuffer);
    ASSERT(b[1] == NULL); /* only ever one buffer on this read */
    ASSERT(cxn->state == cxnConnectingS);
    VALIDATE_CONNECTION(cxn);

    peerName = hostPeerName(cxn->myHost);

    bufferAddNullByte(b[0]);

    d_printf(1, "%s:%u Processing authinfo pass response: %s", /* no NL */
             hostPeerName(cxn->myHost), cxn->ident, p);

    if (i == IoDone && writeIsPending(cxn->myEp)) {
        /* badness. should never happen */
        warn("%s:%u cxnsleep authinfo command still pending", peerName,
             cxn->ident);

        cxnSleepOrDie(cxn);
    } else if (i != IoDone) {
        if (i != IoEOF) {
            errno = endPointErrno(e);
            syswarn("%s:%u cxnsleep can't read response", peerName,
                    cxn->ident);
        }
        cxnSleepOrDie(cxn);
    } else if (strchr(p, '\n') == NULL) {
        /* partial read */
        expandBuffer(b[0], BUFFER_EXPAND_AMOUNT);

        buffers = makeBufferArray(bufferTakeRef(b[0]), NULL);
        if (!prepareRead(e, buffers, getAuthPassResponse, cxn, 1)) {
            warn("%s:%u cxnsleep prepare read failed", peerName, cxn->ident);
            freeBufferArray(buffers);
            cxnSleepOrDie(cxn);
        }
    } else {
        clearTimer(cxn->readBlockedTimerId);

        if (!getNntpResponse(p, &code, NULL)) {
            warn("%s:%u cxnsleep response to AUTHINFO PASS: %s", peerName,
                 cxn->ident, p);

            cxnSleepOrDie(cxn);
        } else {
            switch (code) {
            case 281:
                notice("%s:%u authenticated", peerName, cxn->ident);
                cxn->authenticated = true;
                issueModeStream(e, cxn);
                break;

            default:
                warn("%s:%u cxnsleep response to AUTHINFO PASS: %s", peerName,
                     cxn->ident, p);
                cxnSleepOrDie(cxn);
                break;
            }
        }
    }
}


/*
 * Process the remote's response to our MODE STREAM command. This is where
 * the Connection moves into the cxnFeedingS state. If the remote has given
 * us a good welcome banner, but then immediately dropped the connection,
 * we'll arrive here with the MODE STREAM command still queued up.
 */
static void
getModeResponse(EndPoint e, IoStatus i, Buffer *b, void *d)
{
    Connection cxn = (Connection) d;
    int code;
    char *p = bufferBase(b[0]);
    Buffer *buffers;
    const char *peerName;

    ASSERT(e == cxn->myEp);
    ASSERT(b[0] == cxn->respBuffer);
    ASSERT(b[1] == NULL); /* only ever one buffer on this read */
    ASSERT(cxn->state == cxnConnectingS);
    VALIDATE_CONNECTION(cxn);

    peerName = hostPeerName(cxn->myHost);

    bufferAddNullByte(b[0]);

    d_printf(1, "%s:%u Processing mode response: %s", /* no NL */
             hostPeerName(cxn->myHost), cxn->ident, p);

    if (i == IoDone
        && writeIsPending(cxn->myEp)) { /* badness. should never happen */
        warn("%s:%u cxnsleep mode stream command still pending", peerName,
             cxn->ident);

        cxnSleepOrDie(cxn);
    } else if (i != IoDone) {
        if (i != IoEOF) {
            errno = endPointErrno(e);
            syswarn("%s:%u cxnsleep can't read response", peerName,
                    cxn->ident);
        }
        cxnSleepOrDie(cxn);
    } else if (strchr(p, '\n') == NULL) { /* partial read */
        expandBuffer(b[0], BUFFER_EXPAND_AMOUNT);

        buffers = makeBufferArray(bufferTakeRef(b[0]), NULL);
        if (!prepareRead(e, buffers, getModeResponse, cxn, 1)) {
            warn("%s:%u cxnsleep prepare read failed", peerName, cxn->ident);
            freeBufferArray(buffers);
            cxnSleepOrDie(cxn);
        }
    } else {
        clearTimer(cxn->readBlockedTimerId);

        if (!getNntpResponse(p, &code, NULL)) {
            warn("%s:%u cxnsleep response to MODE STREAM: %s", peerName,
                 cxn->ident, p);

            cxnSleepOrDie(cxn);
        } else {
            if (!cxn->authenticated)
                notice("%s:%u connected", peerName, cxn->ident);

            switch (code) {
            case 203: /* will do streaming */
                hostRemoteStreams(cxn->myHost, cxn, true);

                if (hostWantsStreaming(cxn->myHost)) {
                    cxn->doesStreaming = true;
                    cxn->maxCheck = hostMaxChecks(cxn->myHost);
                } else
                    cxn->maxCheck = 1;

                break;

            default: /* won't do it */
                hostRemoteStreams(cxn->myHost, cxn, false);
                cxn->maxCheck = 1;
                break;
            }

            /* now we consider ourselves completly connected. */
            cxn->timeCon = theTime();
            cxn->timeCon_checkpoint = theTime();

            if (cxn->articleQTotal == 0)
                cxnIdle(cxn);
            else
                cxn->state = cxnFeedingS;

            /* one for the connection and one for the buffer array */
            ASSERT(cxn->authenticated || bufferRefCount(cxn->respBuffer) == 2);

            /* there was only one line in there, right? */
            bufferSetDataSize(cxn->respBuffer, 0);
            buffers = makeBufferArray(bufferTakeRef(cxn->respBuffer), NULL);

            /* sleepTimeout get changed at each failed attempt, so reset. */
            cxn->sleepTimeout = init_reconnect_period;

            if (!prepareRead(cxn->myEp, buffers, responseIsRead, cxn, 1)) {
                freeBufferArray(buffers);

                cxnSleepOrDie(cxn);
            } else {
                /* now we wait for articles from our Host, or we have some
                   articles already. On infrequently used connections, the
                   network link is torn down and rebuilt as needed. So we may
                   be rebuilding the connection here in which case we have an
                   article to send. */
                if (writesNeeded(cxn) || hostGimmeArticle(cxn->myHost, cxn))
                    doSomeWrites(cxn);
            }
        }
    }

    freeBufferArray(b);
}


/*
 * called when a response has been read from the socket. This is
 * where the bulk of the processing starts.
 */
static void
responseIsRead(EndPoint e, IoStatus i, Buffer *b, void *d)
{
    Connection cxn = (Connection) d;
    char *response;
    char *endr;
    char *bufBase;
    unsigned int respSize;
    int code;
    char *rest = NULL;
    Buffer buf;
    Buffer *bArr;
    const char *peerName;

    ASSERT(e == cxn->myEp);
    ASSERT(b != NULL);
    ASSERT(b[1] == NULL);
    ASSERT(b[0] == cxn->respBuffer);
    ASSERT(cxn->state == cxnFeedingS || cxn->state == cxnIdleS
           || cxn->state == cxnClosingS || cxn->state == cxnFlushingS);
    VALIDATE_CONNECTION(cxn);

    bufferAddNullByte(b[0]);

    peerName = hostPeerName(cxn->myHost);

    if (i != IoDone) { /* uh oh. */
        if (i != IoEOF) {
            errno = endPointErrno(e);
            syswarn("%s:%u cxnsleep can't read response", peerName,
                    cxn->ident);
        }
        freeBufferArray(b);

        cxnLogStats(cxn, true);

        if (cxn->state == cxnClosingS) {
            cxnDead(cxn);
            delConnection(cxn);
        } else
            cxnSleep(cxn);

        return;
    }

    buf = b[0];
    bufBase = bufferBase(buf);

    /* check that we have (at least) a full line response. If not expand
       the buffer and resubmit the read. */
    if (strchr(bufBase, '\n') == 0) {
        if (!expandBuffer(buf, BUFFER_EXPAND_AMOUNT)) {
            warn("%s:%u cxnsleep can't expand input buffer", peerName,
                 cxn->ident);
            freeBufferArray(b);

            cxnSleepOrDie(cxn);
        } else if (!prepareRead(cxn->myEp, b, responseIsRead, cxn, 1)) {
            warn("%s:%u cxnsleep prepare read failed", peerName, cxn->ident);
            freeBufferArray(b);

            cxnSleepOrDie(cxn);
        }

        return;
    }


    freeBufferArray(b); /* connection still has reference to buffer */


    /*
     * Now process all the full responses that we have.
     */
    response = bufBase;
    respSize = bufferDataSize(cxn->respBuffer);

    while ((endr = strchr(response, '\n')) != NULL) {
        char *next = endr + 1;

        if (*next == '\r')
            next++;

        endr--;
        if (*endr != '\r')
            endr++;

        if (next - endr != 2 && !cxn->loggedNoCr) {
            /* only a newline there. we'll live with it */
            warn("%s:%u remote not giving out CR characters", peerName,
                 cxn->ident);
            cxn->loggedNoCr = true;
        }

        *endr = '\0';

        if (!getNntpResponse(response, &code, &rest)) {
            warn("%s:%u cxnsleep response format: %s", peerName, cxn->ident,
                 response);
            cxnSleepOrDie(cxn);

            return;
        }

        d_printf(5, "%s:%u Response %d: %s\n", peerName, cxn->ident, code,
                 response);

        /* now handle the response code. I'm not using symbolic names on
           purpose--the numbers are all you see in the RFC's. */
        switch (code) {
        case 205: /* OK response to QUIT. */
            processResponse205(cxn, response);
            break;


            /* These three are from the CHECK command */
        case 238: /* no such article found */
                  /* Do not incrFilter (cxn) now, wait till after
                     subsequent TAKETHIS */
            processResponse238(cxn, response);
            break;

        case 431: /* try again later (also for TAKETHIS) */
            decrFilter(cxn);
            if (hostDropDeferred(cxn->myHost))
                processResponse438(cxn, response);
            else
                processResponse431(cxn, response);
            break;

        case 438: /* already have it */
            decrFilter(cxn);
            processResponse438(cxn, response);
            break;


            /* These are from the TAKETHIS command */
        case 239: /* article transferred OK */
            incrFilter(cxn);
            processResponse239(cxn, response);
            break;

        case 439: /* article rejected */
            decrFilter(cxn);
            processResponse439(cxn, response);
            break;


            /* These are from the IHAVE command */
        case 335: /* send article */
            processResponse335(cxn, response);
            break;

        case 435: /* article not wanted */
            processResponse435(cxn, response);
            break;

        case 436: /* transfer failed try again later */
            if (cxn->takeRespHead == NULL && hostDropDeferred(cxn->myHost))
                processResponse435(cxn, response);
            else
                processResponse436(cxn, response);
            break;

        case 437: /* article rejected */
            processResponse437(cxn, response);
            break;

        case 400: /* has stopped accepting articles */
            processResponse400(cxn, response);
            break;


        case 235: /* article transfered OK (IHAVE-body) */
            processResponse235(cxn, response);
            break;


        case 480: /* Transfer permission denied. */
            processResponse480(cxn, response);
            break;

        case 503: /* remote timeout. */
            processResponse503(cxn, response);
            break;

        default:
            warn("%s:%u cxnsleep response unknown: %d %s", peerName,
                 cxn->ident, code, response);
            cxnSleepOrDie(cxn);
            break;
        }

        VALIDATE_CONNECTION(cxn);

        if (cxn->state != cxnFeedingS && cxn->state != cxnClosingS
            && cxn->state != cxnFlushingS && cxn->state != cxnIdleS /* XXX */)
            break; /* connection is terminated */

        response = next;
    }

    d_printf(5, "%s:%u done with responses\n", hostPeerName(cxn->myHost),
             cxn->ident);

    switch (cxn->state) {
    case cxnIdleS:
    case cxnFeedingS:
    case cxnClosingS:
    case cxnFlushingS:
        /* see if we need to drop in to or out of no-CHECK mode */
        if (cxn->state == cxnFeedingS && cxn->doesStreaming) {
            if ((cxn->filterValue > cxn->onThreshold) && cxn->needsChecks) {
                cxn->needsChecks = false;
                hostLogNoCheckMode(cxn->myHost, true,
                                   cxn->offThreshold / cxn->lowPassFilter,
                                   cxn->filterValue / cxn->lowPassFilter,
                                   cxn->onThreshold / cxn->lowPassFilter);
                /* on and log */
            } else if ((cxn->filterValue < cxn->offThreshold)
                       && !cxn->needsChecks) {
                cxn->needsChecks = true;
                hostLogNoCheckMode(cxn->myHost, false,
                                   cxn->offThreshold / cxn->lowPassFilter,
                                   cxn->filterValue / cxn->lowPassFilter,
                                   cxn->onThreshold / cxn->lowPassFilter);
                /* off and log */
            }
        }

        /* Now handle possible remaining partial response and set up for
           next read. */
        if (*response != '\0') { /* partial response */
            unsigned int leftAmt = respSize - (response - bufBase);

            d_printf(2, "%s:%u handling a partial response\n",
                     hostPeerName(cxn->myHost), cxn->ident);

            /* first we shift what's left in the buffer down to the
               bottom, if needed, or just expand the buffer */
            if (response != bufBase) {
                /* so next read appends */
                memmove(bufBase, response, leftAmt);
                bufferSetDataSize(cxn->respBuffer, leftAmt);
            } else if (!expandBuffer(cxn->respBuffer, BUFFER_EXPAND_AMOUNT))
                die("%s:%u cxnsleep can't expand input buffer", peerName,
                    cxn->ident);
        } else
            bufferSetDataSize(cxn->respBuffer, 0);

        bArr = makeBufferArray(bufferTakeRef(cxn->respBuffer), NULL);

        if (!prepareRead(e, bArr, responseIsRead, cxn, 1)) {
            warn("%s:%u cxnsleep prepare read failed", peerName, cxn->ident);
            freeBufferArray(bArr);
            cxnWait(cxn);
            return;
        } else {
            /* only setup the timer if we're still waiting for a response
               to something. There's not necessarily a 1-to-1 mapping
               between reads and writes in streaming mode. May have been
               set already above (that would be unlikely I think). */
            VALIDATE_CONNECTION(cxn);

            d_printf(5, "%s:%u about to do some writes\n",
                     hostPeerName(cxn->myHost), cxn->ident);

            doSomeWrites(cxn);

            /* If the read timer is (still) running, update it to give
               those terminally slow hosts that take forever to drain
               the network buffers and just dribble out responses the
               benefit of the doubt.  XXX - maybe should just increase
               timeout for these! */
            if (cxn->readBlockedTimerId)
                cxn->readBlockedTimerId =
                    updateSleep(cxn->readBlockedTimerId, responseTimeoutCbk,
                                cxn->readTimeout, cxn);
        }
        VALIDATE_CONNECTION(cxn);
        break;

    case cxnWaitingS:    /* presumably after a code 205 or 400 */
    case cxnConnectingS: /* presumably after a code 205 or 400 */
    case cxnSleepingS:   /* probably after a 480 */
        break;

    case cxnDeadS:
        delConnection(cxn);
        break;

    case cxnStartingS:
    default:
        die("Bad connection state: %s\n", stateToString(cxn->state));
    }
}


/*
 * called when the write of the QUIT command has completed.
 */
static void
quitWritten(EndPoint e, IoStatus i, Buffer *b, void *d)
{
    Connection cxn = (Connection) d;
    const char *peerName;

    peerName = hostPeerName(cxn->myHost);

    clearTimer(cxn->writeBlockedTimerId);

    ASSERT(cxn->myEp == e);
    VALIDATE_CONNECTION(cxn);

    if (i != IoDone) {
        errno = endPointErrno(e);
        syswarn("%s:%u cxnsleep can't write QUIT", peerName, cxn->ident);
        if (cxn->state == cxnClosingS) {
            cxnDead(cxn);
            delConnection(cxn);
        } else
            cxnWait(cxn);
    } else
        /* The QUIT command has been sent, so start the response timer. */
        initReadBlockedTimeout(cxn);

    freeBufferArray(b);
}


/*
 * called when the write of the IHAVE-body data is finished
 */
static void
ihaveBodyDone(EndPoint e, IoStatus i, Buffer *b, void *d)
{
    Connection cxn = (Connection) d;

    ASSERT(e == cxn->myEp);

    clearTimer(cxn->writeBlockedTimerId);

    if (i != IoDone) {
        errno = endPointErrno(e);
        syswarn("%s:%u cxnsleep can't write IHAVE body",
                hostPeerName(cxn->myHost), cxn->ident);

        cxnLogStats(cxn, true);

        if (cxn->state == cxnClosingS) {
            cxnDead(cxn);
            delConnection(cxn);
        } else
            cxnSleep(cxn);
    } else {
        /* Some hosts return a response even before we're done sending, so
           don't go idle until here. */
        if (cxn->state == cxnFeedingS && cxn->articleQTotal == 0)
            cxnIdle(cxn);
        else
            /* The command set has been sent, so start the response timer. */
            initReadBlockedTimeout(cxn);
    }

    freeBufferArray(b);

    return;
}


/*
 * Called when a command set (IHAVE, CHECK, TAKETHIS) has been
 * written to the remote.
 */
static void
commandWriteDone(EndPoint e, IoStatus i, Buffer *b, void *d)
{
    Connection cxn = (Connection) d;
    const char *peerName;

    ASSERT(e == cxn->myEp);

    peerName = hostPeerName(cxn->myHost);

    freeBufferArray(b);

    clearTimer(cxn->writeBlockedTimerId);

    if (i != IoDone) {
        errno = endPointErrno(e);
        syswarn("%s:%u cxnsleep can't write command", peerName, cxn->ident);

        cxnLogStats(cxn, true);

        if (cxn->state == cxnClosingS) {
            cxnDead(cxn);
            delConnection(cxn);
        } else {
            /* XXX - so cxnSleep() doesn't die in VALIDATE_CONNECTION () */
            deferAllArticles(cxn);
            cxnIdle(cxn);

            cxnSleep(cxn);
        }
    } else {
        /* Some hosts return a response even before we're done sending, so
           don't go idle until here */
        if (cxn->state == cxnFeedingS && cxn->articleQTotal == 0)
            cxnIdle(cxn);
        else
            /* The command set has been sent, so start the response timer.
               XXX - we'd like finer grained control */
            initReadBlockedTimeout(cxn);

        if (cxn->doesStreaming)
            doSomeWrites(cxn); /* pump data as fast as possible */
                               /* XXX - will clear the read timeout */
    }
}


/*
 * Called when the MODE STREAM command has been written down the pipe.
 */
static void
modeCmdIssued(EndPoint e, IoStatus i, Buffer *b, void *d)
{
    Connection cxn = (Connection) d;

    ASSERT(e == cxn->myEp);

    clearTimer(cxn->writeBlockedTimerId);

    /* The mode command has been sent, so start the response timer */
    initReadBlockedTimeout(cxn);

    if (i != IoDone) {
        d_printf(1, "%s:%u MODE STREAM command failed to write\n",
                 hostPeerName(cxn->myHost), cxn->ident);

        syswarn("%s:%u cxnsleep can't write MODE STREAM",
                hostPeerName(cxn->myHost), cxn->ident);

        cxnSleepOrDie(cxn);
    }

    freeBufferArray(b);
}


/*
 * Called when the AUTHINFO USER command has been written down the pipe.
 */
static void
authUserIssued(EndPoint e, IoStatus i, Buffer *b, void *d)
{
    Connection cxn = (Connection) d;

    ASSERT(e == cxn->myEp);

    clearTimer(cxn->writeBlockedTimerId);

    /* The authinfo user command has been sent, so start the response timer */
    initReadBlockedTimeout(cxn);

    if (i != IoDone) {
        d_printf(1, "%s:%u AUTHINFO USER command failed to write\n",
                 hostPeerName(cxn->myHost), cxn->ident);

        syswarn("%s:%u cxnsleep can't write AUTHINFO USER",
                hostPeerName(cxn->myHost), cxn->ident);

        cxnSleepOrDie(cxn);
    }

    freeBufferArray(b);
}


/*
 * Called when the AUTHINFO USER command has been written down the pipe.
 */
static void
authPassIssued(EndPoint e, IoStatus i, Buffer *b, void *d)
{
    Connection cxn = (Connection) d;

    ASSERT(e == cxn->myEp);

    clearTimer(cxn->writeBlockedTimerId);

    /* The authinfo pass command has been sent, so start the response timer */
    initReadBlockedTimeout(cxn);

    if (i != IoDone) {
        d_printf(1, "%s:%u AUTHINFO PASS command failed to write\n",
                 hostPeerName(cxn->myHost), cxn->ident);

        syswarn("%s:%u cxnsleep can't write AUTHINFO PASS",
                hostPeerName(cxn->myHost), cxn->ident);

        cxnSleepOrDie(cxn);
    }

    freeBufferArray(b);
}


/*
 * Called whenever some amount of data has been written to the pipe but
 * more data remains to be written
 */
static void
writeProgress(EndPoint e UNUSED, IoStatus i, Buffer *b UNUSED, void *d)
{
    Connection cxn = (Connection) d;

    ASSERT(i == IoProgress);

    if (cxn->writeTimeout > 0)
        cxn->writeBlockedTimerId = updateSleep(
            cxn->writeBlockedTimerId, writeTimeoutCbk, cxn->writeTimeout, cxn);
}


/*
 * Timers.
 */

/*
 * This is called when the timeout for the response from the remote
 * goes off. We tear down the connection and notify our host.
 */
static void
responseTimeoutCbk(TimeoutId id, void *data)
{
    Connection cxn = (Connection) data;
    const char *peerName;

    ASSERT(id == cxn->readBlockedTimerId);
    ASSERT(cxn->state == cxnConnectingS || cxn->state == cxnFeedingS
           || cxn->state == cxnFlushingS || cxn->state == cxnClosingS);
    VALIDATE_CONNECTION(cxn);

    /* XXX - let abortConnection clear readBlockedTimerId, otherwise
       VALIDATE_CONNECTION() will croak */

    peerName = hostPeerName(cxn->myHost);

    warn("%s:%u cxnsleep non-responsive connection", peerName, cxn->ident);
    d_printf(1, "%s:%u shutting down non-responsive connection\n",
             hostPeerName(cxn->myHost), cxn->ident);

    cxnLogStats(cxn, true);

    if (cxn->state == cxnClosingS) {
        abortConnection(cxn);
        delConnection(cxn);
    } else
        cxnSleep(cxn); /* will notify the Host */
}


/*
 * This is called when the data write timeout for the remote
 * goes off. We tear down the connection and notify our host.
 */
static void
writeTimeoutCbk(TimeoutId id, void *data)
{
    Connection cxn = (Connection) data;
    const char *peerName;

    ASSERT(id == cxn->writeBlockedTimerId);
    ASSERT(cxn->state == cxnConnectingS || cxn->state == cxnFeedingS
           || cxn->state == cxnFlushingS || cxn->state == cxnClosingS);
    VALIDATE_CONNECTION(cxn);

    /* XXX - let abortConnection clear writeBlockedTimerId, otherwise
       VALIDATE_CONNECTION() will croak */

    peerName = hostPeerName(cxn->myHost);

    warn("%s:%u cxnsleep write timeout", peerName, cxn->ident);
    d_printf(1, "%s:%u shutting down non-responsive connection\n",
             hostPeerName(cxn->myHost), cxn->ident);

    cxnLogStats(cxn, true);

    if (cxn->state == cxnClosingS) {
        abortConnection(cxn);
        delConnection(cxn);
    } else
        cxnSleep(cxn); /* will notify the Host */
}


/*
 * Called by the EndPoint class when the timer goes off
 */
static void
reopenTimeoutCbk(TimeoutId id, void *data)
{
    Connection cxn = (Connection) data;

    ASSERT(id == cxn->sleepTimerId);

    cxn->sleepTimerId = 0;

    if (cxn->state != cxnSleepingS) {
        warn("%s:%u cxnsleep connection in bad state: %s",
             hostPeerName(cxn->myHost), cxn->ident, stateToString(cxn->state));
        cxnSleepOrDie(cxn);
    } else
        cxnConnect(cxn);
}


/*
 * timeout callback to close down long running connection.
 */
static void
flushCxnCbk(TimeoutId id, void *data)
{
    Connection cxn = (Connection) data;

    ASSERT(id == cxn->flushTimerId);
    VALIDATE_CONNECTION(cxn);

    cxn->flushTimerId = 0;

    if (!(cxn->state == cxnFeedingS || cxn->state == cxnConnectingS
          || cxn->state == cxnIdleS)) {
        warn("%s:%u cxnsleep connection in bad state: %s",
             hostPeerName(cxn->myHost), cxn->ident, stateToString(cxn->state));
        cxnSleepOrDie(cxn);
    } else {
        d_printf(1, "%s:%u Handling periodic connection close.\n",
                 hostPeerName(cxn->myHost), cxn->ident);

        notice("%s:%u periodic close", hostPeerName(cxn->myHost), cxn->ident);

        cxnFlush(cxn);
    }
}


/*
 * Timer callback for when the connection has not received an
 * article from INN. When that happens we tear down the network
 * connection to help recycle fds
 */
static void
articleTimeoutCbk(TimeoutId id, void *data)
{
    Connection cxn = (Connection) data;
    const char *peerName = hostPeerName(cxn->myHost);

    ASSERT(cxn->artReceiptTimerId == id);
    VALIDATE_CONNECTION(cxn);

    cxn->artReceiptTimerId = 0;

    if (cxn->state != cxnIdleS) {
        warn("%s:%u cxnsleep connection in bad state: %s",
             hostPeerName(cxn->myHost), cxn->ident, stateToString(cxn->state));
        cxnSleepOrDie(cxn);

        return;
    }

    /* it's doubtful (right?) that this timer could go off and there'd
       still be articles in the queue. */
    if (cxn->articleQTotal > 0) {
        warn("%s:%u idle connection still has articles", peerName, cxn->ident);
    } else {
        notice("%s:%u idle tearing down connection", peerName, cxn->ident);
        cxn->state = cxnIdleTimeoutS;
        cxnFlush(cxn);
    }
}


/*
 * function to be called when the fd is not ready for reading, but there is
 * an article on tape or in the queue to be done. Things are done this way
 * so that a Connection doesn't hog time trying to find the next good
 * article for writing. With a large backlog of expired articles that would
 * take a long time. Instead the Connection just tries its next article on
 * tape or queue, and if that's no good then it registers this callback so
 * that other Connections have a chance of being serviced.
 *
 * This function is also put on the callback queue if we called doSomeWrites
 * but there was already a write pending, mostly so that we don't deadlock the
 * connection when we got a response to the body of an IHAVE command before we
 * finished sending the body.
 */
static void
cxnWorkProc(EndPoint ep UNUSED, void *data)
{
    Connection cxn = (Connection) data;

    d_printf(2, "%s:%u calling work proc\n", hostPeerName(cxn->myHost),
             cxn->ident);

    if (writesNeeded(cxn))
        doSomeWrites(cxn); /* may re-register the work proc... */
    else if (cxn->state == cxnFlushingS || cxn->state == cxnClosingS) {
        if (cxn->articleQTotal == 0)
            issueQUIT(cxn);
    } else
        d_printf(2, "%s:%u no writes were needed...\n",
                 hostPeerName(cxn->myHost), cxn->ident);
}


/****************************************************************************
 *
 * END EndPoint callback area.
 *
 ****************************************************************************/


/****************************************************************************
 *
 * RESPONSE CODE PROCESSING.
 *
 ***************************************************************************/


/*
 * A connection needs to sleep, but if it's closing it needs to die instead.
 */
static void
cxnSleepOrDie(Connection cxn)
{
    if (cxn->state == cxnClosingS)
        cxnDead(cxn);
    else
        cxnSleep(cxn);
}


/*
 * Handle the response 205 to our QUIT command, which means the
 * remote is going away and we can happily cleanup
 */
static void
processResponse205(Connection cxn, char *response UNUSED)
{
    bool immedRecon;

    VALIDATE_CONNECTION(cxn);

    if (!(cxn->state == cxnFeedingS || cxn->state == cxnIdleS
          || cxn->state == cxnFlushingS || cxn->state == cxnClosingS)) {
        warn("%s:%u cxnsleep connection in bad state: %s",
             hostPeerName(cxn->myHost), cxn->ident, stateToString(cxn->state));
        cxnSleepOrDie(cxn);
        return;
    }

    switch (cxn->state) {
    case cxnFlushingS:
    case cxnClosingS:
        ASSERT(cxn->articleQTotal == 0);

        cxnLogStats(cxn, true);

        immedRecon = cxn->immedRecon;

        hostCxnDead(cxn->myHost, cxn);

        if (cxn->state == cxnFlushingS && immedRecon) {
            abortConnection(cxn);
            if (!cxnConnect(cxn))
                notice("%s:%u flush re-connect failed",
                       hostPeerName(cxn->myHost), cxn->ident);
        } else if (cxn->state == cxnFlushingS)
            cxnWait(cxn);
        else
            cxnDead(cxn);
        break;

    case cxnIdleS:
    case cxnFeedingS:
        /* this shouldn't ever happen... */
        warn("%s:%u cxnsleep response unexpected: %d",
             hostPeerName(cxn->myHost), cxn->ident, 205);
        cxnSleepOrDie(cxn);
        break;

    default:
        die("Bad connection state: %s\n", stateToString(cxn->state));
    }
}


/*
 * Handle a response code of 238 which is the "no such article"
 * reply to the CHECK command (i.e. remote wants it).
 */
static void
processResponse238(Connection cxn, char *response)
{
    char *msgid;
    ArtHolder artHolder;

    if (!cxn->doesStreaming) {
        warn("%s:%u cxnsleep unexpected streaming response for non-streaming"
             " connection: %s",
             hostPeerName(cxn->myHost), cxn->ident, response);
        cxnSleepOrDie(cxn);
        return;
    }

    if (!(cxn->state == cxnFlushingS || cxn->state == cxnFeedingS
          || cxn->state == cxnClosingS)) {
        warn("%s:%u cxnsleep connection in bad state: %s",
             hostPeerName(cxn->myHost), cxn->ident, stateToString(cxn->state));
        cxnSleepOrDie(cxn);
        return;
    }

    VALIDATE_CONNECTION(cxn);

    msgid = getMsgId(response);

    if (cxn->checkRespHead == NULL) /* peer is confused */
    {
        warn("%s:%u cxnsleep response unexpected: %d",
             hostPeerName(cxn->myHost), cxn->ident, 238);
        cxnSleepOrDie(cxn);
    } else if (msgid == NULL || strlen(msgid) == 0
               || (artHolder = artHolderByMsgId(msgid, cxn->checkRespHead))
                      == NULL)
        noSuchMessageId(cxn, 238, msgid, response);
    else {
        /* now remove the article from the check queue and move it onto the
           transmit queue. Another function wil take care of transmitting */
        remArtHolder(artHolder, &cxn->checkRespHead, &cxn->articleQTotal);
        if (cxn->state != cxnClosingS)
            appendArtHolder(artHolder, &cxn->takeHead, &cxn->articleQTotal);
        else {
            hostTakeBackArticle(cxn->myHost, cxn, artHolder->article);
            delArtHolder(artHolder);
        }
    }

    if (msgid != NULL)
        free(msgid);
}


/*
 * process the response "try again later" to the CHECK command If this
 * returns true then the connection is still usable.
 */
static void
processResponse431(Connection cxn, char *response)
{
    char *msgid;
    ArtHolder artHolder;

    if (!cxn->doesStreaming) {
        warn("%s:%u cxnsleep unexpected streaming response for non-streaming"
             " connection: %s",
             hostPeerName(cxn->myHost), cxn->ident, response);
        cxnSleepOrDie(cxn);
        return;
    }

    if (!(cxn->state == cxnFlushingS || cxn->state == cxnFeedingS
          || cxn->state == cxnClosingS)) {
        warn("%s:%u cxnsleep connection in bad state: %s",
             hostPeerName(cxn->myHost), cxn->ident, stateToString(cxn->state));
        cxnSleepOrDie(cxn);
        return;
    }

    VALIDATE_CONNECTION(cxn);

    msgid = getMsgId(response);

    if (cxn->checkRespHead == NULL) /* peer is confused */
    {
        warn("%s:%u cxnsleep response unexpected: %d",
             hostPeerName(cxn->myHost), cxn->ident, 431);
        cxnSleepOrDie(cxn);
    } else if (msgid == NULL || strlen(msgid) == 0
               || (artHolder = artHolderByMsgId(msgid, cxn->checkRespHead))
                      == NULL)
        noSuchMessageId(cxn, 431, msgid, response);
    else {
        remArtHolder(artHolder, &cxn->checkRespHead, &cxn->articleQTotal);
        if (cxn->articleQTotal == 0 && !writeIsPending(cxn->myEp))
            cxnIdle(cxn);
        hostArticleDeferred(cxn->myHost, cxn, artHolder->article);
        delArtHolder(artHolder);
    }

    if (msgid != NULL)
        free(msgid);
}


/*
 * process the "already have it" response to the CHECK command.  If this
 * returns true then the connection is still usable.
 */
static void
processResponse438(Connection cxn, char *response)
{
    char *msgid;
    ArtHolder artHolder;

    if (!cxn->doesStreaming) {
        warn("%s:%u cxnsleep unexpected streaming response for non-streaming"
             " connection: %s",
             hostPeerName(cxn->myHost), cxn->ident, response);
        cxnSleepOrDie(cxn);
        return;
    }

    if (!(cxn->state == cxnFlushingS || cxn->state == cxnFeedingS
          || cxn->state == cxnClosingS)) {
        warn("%s:%u cxnsleep connection in bad state: %s",
             hostPeerName(cxn->myHost), cxn->ident, stateToString(cxn->state));
        cxnSleepOrDie(cxn);
        return;
    }

    VALIDATE_CONNECTION(cxn);

    msgid = getMsgId(response);

    if (cxn->checkRespHead == NULL) /* peer is confused */
    {
        warn("%s:%u cxnsleep response unexpected: %d",
             hostPeerName(cxn->myHost), cxn->ident, 438);
        cxnSleepOrDie(cxn);
    } else if (msgid == NULL || strlen(msgid) == 0
               || (artHolder = artHolderByMsgId(msgid, cxn->checkRespHead))
                      == NULL)
        noSuchMessageId(cxn, 438, msgid, response);
    else {
        cxn->checksRefused++;

        remArtHolder(artHolder, &cxn->checkRespHead, &cxn->articleQTotal);
        if (cxn->articleQTotal == 0 && !writeIsPending(cxn->myEp))
            cxnIdle(cxn);
        hostArticleNotWanted(cxn->myHost, cxn, artHolder->article);
        delArtHolder(artHolder);
    }

    if (msgid != NULL)
        free(msgid);
}


/*
 * process the "article transferred ok" response to the TAKETHIS.
 */
static void
processResponse239(Connection cxn, char *response)
{
    char *msgid;
    ArtHolder artHolder;

    if (!cxn->doesStreaming) {
        warn("%s:%u cxnsleep unexpected streaming response for non-streaming"
             " connection: %s",
             hostPeerName(cxn->myHost), cxn->ident, response);
        cxnSleepOrDie(cxn);
        return;
    }

    if (!(cxn->state == cxnFlushingS || cxn->state == cxnFeedingS
          || cxn->state == cxnClosingS)) {
        warn("%s:%u cxnsleep connection in bad state: %s",
             hostPeerName(cxn->myHost), cxn->ident, stateToString(cxn->state));
        cxnSleepOrDie(cxn);
        return;
    }

    VALIDATE_CONNECTION(cxn);

    msgid = getMsgId(response);

    if (cxn->takeRespHead == NULL) /* peer is confused */
    {
        warn("%s:%u cxnsleep response unexpected: %d",
             hostPeerName(cxn->myHost), cxn->ident, 239);
        cxnSleepOrDie(cxn);
    } else if (msgid == NULL || strlen(msgid) == 0
               || (artHolder = artHolderByMsgId(msgid, cxn->takeRespHead))
                      == NULL)
        noSuchMessageId(cxn, 239, msgid, response);
    else {
        cxn->takesOkayed++;
        cxn->takesSizeOkayed += artSize(artHolder->article);

        remArtHolder(artHolder, &cxn->takeRespHead, &cxn->articleQTotal);
        if (cxn->articleQTotal == 0 && !writeIsPending(cxn->myEp))
            cxnIdle(cxn);
        hostArticleAccepted(cxn->myHost, cxn, artHolder->article);
        delArtHolder(artHolder);
    }

    if (msgid != NULL)
        free(msgid);
}


/*
 *  Set the thresholds for no-CHECK mode; negative means leave existing value
 */

void
cxnSetCheckThresholds(Connection cxn, double lowFilter, double highFilter,
                      double lowPassFilter)
{
    /* Adjust current value for new scaling */
    if (cxn->lowPassFilter > 0.0)
        cxn->filterValue =
            cxn->filterValue / cxn->lowPassFilter * lowPassFilter;

    /* Stick in new values */
    if (highFilter >= 0)
        cxn->onThreshold = highFilter * lowPassFilter / 100.0;
    if (lowFilter >= 0)
        cxn->offThreshold = lowFilter * lowPassFilter / 100.0;
    cxn->lowPassFilter = lowPassFilter;
}


/*
 *  Blow away the connection gracelessly and immedately clean up
 */
void
cxnNuke(Connection cxn)
{
    abortConnection(cxn);
    hostCxnDead(cxn->myHost, cxn);
    delConnection(cxn);
}


/*
 * process a "article rejected do not try again" response to the
 * TAKETHIS.
 */
static void
processResponse439(Connection cxn, char *response)
{
    char *msgid;
    ArtHolder artHolder;

    if (!cxn->doesStreaming) {
        warn("%s:%u cxnsleep unexpected streaming response for non-streaming"
             " connection: %s",
             hostPeerName(cxn->myHost), cxn->ident, response);
        cxnSleepOrDie(cxn);
        return;
    }

    if (!(cxn->state == cxnFlushingS || cxn->state == cxnFeedingS
          || cxn->state == cxnClosingS)) {
        warn("%s:%u cxnsleep connection in bad state: %s",
             hostPeerName(cxn->myHost), cxn->ident, stateToString(cxn->state));
        cxnSleepOrDie(cxn);
        return;
    }

    VALIDATE_CONNECTION(cxn);

    msgid = getMsgId(response);

    if (cxn->takeRespHead == NULL) /* peer is confused */
    {
        /* NNTPRelay return 439 for check <messid> if messid is bad */
        if (cxn->checkRespHead == NULL) /* peer is confused */
        {
            warn("%s:%u cxnsleep response unexpected: %d",
                 hostPeerName(cxn->myHost), cxn->ident, 439);
            cxnSleepOrDie(cxn);
        } else {
            if ((artHolder = artHolderByMsgId(msgid, cxn->checkRespHead))
                == NULL)
                noSuchMessageId(cxn, 439, msgid, response);
            else {
                cxn->checksRefused++;
                remArtHolder(artHolder, &cxn->checkRespHead,
                             &cxn->articleQTotal);
                if (cxn->articleQTotal == 0 && !writeIsPending(cxn->myEp))
                    cxnIdle(cxn);
                hostArticleNotWanted(cxn->myHost, cxn, artHolder->article);
                delArtHolder(artHolder);
            }
        }
    } else if (msgid == NULL || strlen(msgid) == 0
               || (artHolder = artHolderByMsgId(msgid, cxn->takeRespHead))
                      == NULL)
        noSuchMessageId(cxn, 439, msgid, response);
    else {
        cxn->takesRejected++;
        cxn->takesSizeRejected += artSize(artHolder->article);

        remArtHolder(artHolder, &cxn->takeRespHead, &cxn->articleQTotal);
        /* Some hosts return the 439 response even before we're done sending */
        if (cxn->articleQTotal == 0 && !writeIsPending(cxn->myEp))
            cxnIdle(cxn);
        hostArticleRejected(cxn->myHost, cxn, artHolder->article);
        delArtHolder(artHolder);
    }

    if (msgid != NULL)
        free(msgid);
}


/*
 * process the "article transferred ok" response to the IHAVE-body.
 */
static void
processResponse235(Connection cxn, char *response UNUSED)
{
    ArtHolder artHolder;

    if (cxn->doesStreaming) {
        warn("%s:%u cxnsleep unexpected non-streaming response for"
             " streaming connection: %s",
             hostPeerName(cxn->myHost), cxn->ident, response);
        cxnSleepOrDie(cxn);
        return;
    }

    if (!(cxn->state == cxnFlushingS || cxn->state == cxnFeedingS
          || cxn->state == cxnClosingS)) {
        warn("%s:%u cxnsleep connection in bad state: %s",
             hostPeerName(cxn->myHost), cxn->ident, stateToString(cxn->state));
        cxnSleepOrDie(cxn);
        return;
    }

    ASSERT(cxn->articleQTotal == 1);
    ASSERT(cxn->takeRespHead != NULL);
    VALIDATE_CONNECTION(cxn);

    if (cxn->takeRespHead == NULL) /* peer is confused */
    {
        warn("%s:%u cxnsleep response unexpected: %d",
             hostPeerName(cxn->myHost), cxn->ident, 235);
        cxnSleepOrDie(cxn);
    } else {
        /* now remove the article from the queue and tell the Host to forget
           about it. */
        artHolder = cxn->takeRespHead;

        cxn->takeRespHead = NULL;
        cxn->articleQTotal = 0;
        cxn->takesOkayed++;
        cxn->takesSizeOkayed += artSize(artHolder->article);

        if (cxn->articleQTotal == 0 && !writeIsPending(cxn->myEp))
            cxnIdle(cxn);

        hostArticleAccepted(cxn->myHost, cxn, artHolder->article);
        delArtHolder(artHolder);
    }
}


/*
 * process the "send article to be transfered" response to the IHAVE.
 */
static void
processResponse335(Connection cxn, char *response UNUSED)
{
    if (cxn->doesStreaming) {
        warn("%s:%u cxnsleep unexpected non-streaming response for"
             " streaming connection: %s",
             hostPeerName(cxn->myHost), cxn->ident, response);
        cxnSleepOrDie(cxn);
        return;
    }

    if (!(cxn->state == cxnFlushingS || cxn->state == cxnFeedingS
          || cxn->state == cxnClosingS)) {
        warn("%s:%u cxnsleep connection in bad state: %s",
             hostPeerName(cxn->myHost), cxn->ident, stateToString(cxn->state));
        cxnSleepOrDie(cxn);
        return;
    }

    if (cxn->checkRespHead == NULL) {
        warn("%s:%u cxnsleep response unexpected: %d",
             hostPeerName(cxn->myHost), cxn->ident, 335);
        cxnSleepOrDie(cxn);
    } else {
        VALIDATE_CONNECTION(cxn);
        /* now move the article into the third queue */
        cxn->takeHead = cxn->checkRespHead;
        cxn->checkRespHead = NULL;

        issueIHAVEBody(cxn);
    }
}


/*
 * process the "not accepting articles" response. This could be to any of
 * the IHAVE/CHECK/TAKETHIS command, but not the banner--that's handled
 * elsewhere.
 */
static void
processResponse400(Connection cxn, char *response)
{
    if (!(cxn->state == cxnFlushingS || cxn->state == cxnFeedingS
          || cxn->state == cxnIdleS || cxn->state == cxnClosingS)) {
        warn("%s:%u cxnsleep connection in bad state: %s",
             hostPeerName(cxn->myHost), cxn->ident, stateToString(cxn->state));
        cxnSleepOrDie(cxn);
        return;
    }

    VALIDATE_CONNECTION(cxn);

    /* We may get a response 400 multiple times when in streaming mode. */
    notice("%s:%u remote cannot accept articles: %s",
           hostPeerName(cxn->myHost), cxn->ident, response);

    /* right here there may still be data queued to write and so we'll fail
       trying to issue the quit ('cause a write will be pending). Furthermore,
       the data pending may be half way through an command, and so just
       tossing the buffer is nt sufficient. But figuring out where we are and
       doing a tidy job is hard */
    if (writeIsPending(cxn->myEp))
        cxnSleepOrDie(cxn);
    else {
        if (cxn->articleQTotal > 0) {
            /* Defer the articles here so that cxnFlush() doesn't set up an
               immediate reconnect. */
            deferAllArticles(cxn);
            clearTimer(cxn->readBlockedTimerId);
            /* XXX - so cxnSleep() doesn't die when it validates the connection
             */
            cxnIdle(cxn);
        }
        /* XXX - it would be nice if we QUIT first, but we'd have to go
           into a state where we just search for the 205 response, and
           only go into the sleep state at that point */
        cxnSleepOrDie(cxn);
    }
}


/*
 * process the "not wanted" response to the IHAVE.
 */
static void
processResponse435(Connection cxn, char *response UNUSED)
{
    ArtHolder artHolder;

    if (cxn->doesStreaming) {
        warn("%s:%u cxnsleep unexpected non-streaming response for"
             " streaming connection: %s",
             hostPeerName(cxn->myHost), cxn->ident, response);
        cxnSleepOrDie(cxn);
        return;
    }

    if (!(cxn->state == cxnFlushingS || cxn->state == cxnFeedingS
          || cxn->state == cxnClosingS)) {
        warn("%s:%u cxnsleep connection in bad state: %s",
             hostPeerName(cxn->myHost), cxn->ident, stateToString(cxn->state));
        cxnSleepOrDie(cxn);
        return;
    }

    /* Some servers, such as early versions of Diablo, had a bug where they'd
       respond with a 435 code (which should only be used for refusing an
       article before it was offered) after an article has been sent. */
    if (cxn->checkRespHead == NULL) {
        warn("%s:%u cxnsleep response unexpected: %d",
             hostPeerName(cxn->myHost), cxn->ident, 435);
        cxnSleepOrDie(cxn);
        return;
    }

    ASSERT(cxn->articleQTotal == 1);
    VALIDATE_CONNECTION(cxn);

    cxn->articleQTotal--;
    cxn->checksRefused++;

    artHolder = cxn->checkRespHead;
    cxn->checkRespHead = NULL;

    if (cxn->articleQTotal == 0 && !writeIsPending(cxn->myEp))
        cxnIdle(cxn);

    hostArticleNotWanted(cxn->myHost, cxn, artHolder->article);
    delArtHolder(artHolder);

#if defined(INNFEED_DEBUG)
    d_printf(
        1, "%s:%u On exiting 435 article queue total is %u (%d %d %d %d)\n",
        hostPeerName(cxn->myHost), cxn->ident, cxn->articleQTotal,
        (int) (cxn->checkHead != NULL), (int) (cxn->checkRespHead != NULL),
        (int) (cxn->takeHead != NULL), (int) (cxn->takeRespHead != NULL));
#endif
}


/*
 * process the "transfer failed" response to the IHAVE-body, (seems this
 * can come from the IHAVE too).
 */
static void
processResponse436(Connection cxn, char *response UNUSED)
{
    ArtHolder artHolder;

    if (cxn->doesStreaming) {
        warn("%s:%u cxnsleep unexpected non-streaming response for"
             " streaming connection: %s",
             hostPeerName(cxn->myHost), cxn->ident, response);
        cxnSleepOrDie(cxn);
        return;
    }

    if (!(cxn->state == cxnFlushingS || cxn->state == cxnFeedingS
          || cxn->state == cxnClosingS)) {
        warn("%s:%u cxnsleep connection in bad state: %s",
             hostPeerName(cxn->myHost), cxn->ident, stateToString(cxn->state));
        cxnSleepOrDie(cxn);
        return;
    }

    ASSERT(cxn->articleQTotal == 1);
    ASSERT(cxn->takeRespHead != NULL || cxn->checkRespHead != NULL);
    VALIDATE_CONNECTION(cxn);

    cxn->articleQTotal--;

    if (cxn->takeRespHead != NULL) /* IHAVE-body command barfed */
    {
        artHolder = cxn->takeRespHead;
        cxn->takeRespHead = NULL;
    } else /* IHAVE command barfed */
    {
        artHolder = cxn->checkRespHead;
        cxn->checkRespHead = NULL;
    }

    if (cxn->articleQTotal == 0 && !writeIsPending(cxn->myEp))
        cxnIdle(cxn);

    hostArticleDeferred(cxn->myHost, cxn, artHolder->article);
    delArtHolder(artHolder);
}


/*
 * Process the "article rejected do not try again" response to the
 * IHAVE-body.
 */
static void
processResponse437(Connection cxn, char *response UNUSED)
{
    ArtHolder artHolder;

    if (cxn->doesStreaming) {
        warn("%s:%u cxnsleep unexpected non-streaming response for"
             " streaming connection: %s",
             hostPeerName(cxn->myHost), cxn->ident, response);
        cxnSleepOrDie(cxn);
        return;
    }

    if (!(cxn->state == cxnFlushingS || cxn->state == cxnFeedingS
          || cxn->state == cxnClosingS)) {
        warn("%s:%u cxnsleep connection in bad state: %s",
             hostPeerName(cxn->myHost), cxn->ident, stateToString(cxn->state));
        cxnSleepOrDie(cxn);
        return;
    }

    ASSERT(cxn->articleQTotal == 1);
    ASSERT(cxn->takeRespHead != NULL);
    VALIDATE_CONNECTION(cxn);

    cxn->articleQTotal--;
    cxn->takesRejected++;

    artHolder = cxn->takeRespHead;
    cxn->takeRespHead = NULL;
    cxn->takesSizeRejected += artSize(artHolder->article);

    /* Some servers return the 437 response before we're done sending. */
    if (cxn->articleQTotal == 0 && !writeIsPending(cxn->myEp))
        cxnIdle(cxn);

    hostArticleRejected(cxn->myHost, cxn, artHolder->article);
    delArtHolder(artHolder);
}


/* Process the response 480 Transfer permission defined. We're probably
   talking to a remote nnrpd on a system that forgot to put us in
   the hosts.nntp */
static void
processResponse480(Connection cxn, char *response UNUSED)
{
    if (cxn->doesStreaming) {
        warn("%s:%u cxnsleep unexpected non-streaming response for"
             " streaming connection: %s",
             hostPeerName(cxn->myHost), cxn->ident, response);
        cxnSleepOrDie(cxn);
        return;
    }

    if (!(cxn->state == cxnFlushingS || cxn->state == cxnFeedingS
          || cxn->state == cxnClosingS)) {
        warn("%s:%u cxnsleep connection in bad state: %s",
             hostPeerName(cxn->myHost), cxn->ident, stateToString(cxn->state));
        cxnSleepOrDie(cxn);
        return;
    }

    VALIDATE_CONNECTION(cxn);

    warn("%s:%u cxnsleep transfer permission denied",
         hostPeerName(cxn->myHost), cxn->ident);

    if (cxn->state == cxnClosingS)
        cxnDead(cxn);
    else
        cxnSleep(cxn);
}


/*
 * Handle the response 503, which means the timeout of nnrpd.
 */
static void
processResponse503(Connection cxn, char *response UNUSED)
{
    bool immedRecon;

    VALIDATE_CONNECTION(cxn);

    if (!(cxn->state == cxnFeedingS || cxn->state == cxnIdleS
          || cxn->state == cxnFlushingS || cxn->state == cxnClosingS)) {
        warn("%s:%u cxnsleep connection in bad state: %s",
             hostPeerName(cxn->myHost), cxn->ident, stateToString(cxn->state));
        cxnSleepOrDie(cxn);
        return;
    }

    if (cxn->articleQTotal != 0)
        notice("%s:%u flush re-connect failed", hostPeerName(cxn->myHost),
               cxn->ident);

    cxnLogStats(cxn, true);

    immedRecon = cxn->immedRecon;

    hostCxnDead(cxn->myHost, cxn);

    if (cxn->state == cxnFlushingS && immedRecon) {
        abortConnection(cxn);
        if (!cxnConnect(cxn))
            notice("%s:%u flush re-connect failed", hostPeerName(cxn->myHost),
                   cxn->ident);
    } else if (cxn->state == cxnFlushingS)
        cxnWait(cxn);
    else
        cxnDead(cxn);
}


/****************************************************************************
 *
 * END RESPONSE CODE PROCESSING.
 *
 ***************************************************************************/


/*
 * puts the Connection into the sleep state.
 */
static void
cxnSleep(Connection cxn)
{
    ASSERT(cxn != NULL);
    ASSERT(cxn->state == cxnFlushingS || cxn->state == cxnIdleS
           || cxn->state == cxnFeedingS || cxn->state == cxnConnectingS);
    VALIDATE_CONNECTION(cxn);

    abortConnection(cxn);

    prepareReopenCbk(cxn); /* XXX - we don't want to reopen if idle */
    cxn->state = cxnSleepingS;

    /* tell our Host we're asleep so it doesn't try to give us articles */
    hostCxnSleeping(cxn->myHost, cxn);
}


static void
cxnDead(Connection cxn)
{
    ASSERT(cxn != NULL);
    VALIDATE_CONNECTION(cxn);

    abortConnection(cxn);
    cxn->state = cxnDeadS;
}


/*
 * Sets the idle timer. If no articles arrive before the timer expires, the
 * connection will be closed.
 */
static void
cxnIdle(Connection cxn)
{
    ASSERT(cxn != NULL);
    ASSERT(cxn->state == cxnFeedingS || cxn->state == cxnConnectingS
           || cxn->state == cxnFlushingS || cxn->state == cxnClosingS);
    ASSERT(cxn->articleQTotal == 0);
    ASSERT(cxn->writeBlockedTimerId == 0);
    ASSERT(!writeIsPending(cxn->myEp));
    ASSERT(cxn->sleepTimerId == 0);

    if (cxn->state == cxnFeedingS || cxn->state == cxnConnectingS) {
        if (cxn->articleReceiptTimeout > 0) {
            clearTimer(cxn->artReceiptTimerId);
            cxn->artReceiptTimerId = prepareSleep(
                articleTimeoutCbk, cxn->articleReceiptTimeout, cxn);
        }

        if (cxn->readTimeout > 0 && cxn->state == cxnFeedingS)
            clearTimer(cxn->readBlockedTimerId);

        cxn->state = cxnIdleS;
        ASSERT(cxn->readBlockedTimerId == 0);
    }
}


/*
 * Called when a response from the remote refers to a non-existent
 * message-id. The network connection is aborted and the Connection
 * object goes into sleep mode.
 */
static void
noSuchMessageId(Connection cxn, unsigned int responseCode, const char *msgid,
                const char *response)
{
    const char *peerName = hostPeerName(cxn->myHost);

    if (msgid == NULL || strlen(msgid) == 0)
        warn("%s:%u cxnsleep message-id missing in response code %u: %s",
             peerName, cxn->ident, responseCode, response);
    else
        warn("%s:%u cxnsleep message-id invalid message-id in response code"
             " %u: %s",
             peerName, cxn->ident, responseCode, msgid);

    cxnLogStats(cxn, true);

    if (cxn->state != cxnClosingS)
        cxnSleep(cxn);
    else
        cxnDead(cxn);
}


/*
 * a processing error has occured (for example in parsing a response), or
 * we're at the end of the FSM and we're cleaning up.
 */
static void
abortConnection(Connection cxn)
{
    ASSERT(cxn != NULL);
    VALIDATE_CONNECTION(cxn);

    /* cxn->myEp could be NULL if we get here during cxnConnect (via
       cxnWait()) */
    if (cxn->myEp != NULL) {

        delEndPoint(cxn->myEp);
        cxn->myEp = NULL;
    }

    clearTimer(cxn->sleepTimerId);
    clearTimer(cxn->artReceiptTimerId);
    clearTimer(cxn->readBlockedTimerId);
    clearTimer(cxn->writeBlockedTimerId);
    clearTimer(cxn->flushTimerId);

    deferAllArticles(cxn); /* give any articles back to Host */

    bufferSetDataSize(cxn->respBuffer, 0);

    resetConnection(cxn);

    if (cxn->state == cxnFeedingS || cxn->state == cxnIdleS
        || cxn->state == cxnFlushingS || cxn->state == cxnClosingS)
        hostCxnDead(cxn->myHost, cxn);
}


/*
 * Set up the callback used when the Connection is sleeping (i.e. will try
 * to reopen the connection).
 */
static void
prepareReopenCbk(Connection cxn)
{
    ASSERT(cxn->sleepTimerId == 0);

    if (!(cxn->state == cxnConnectingS || cxn->state == cxnIdleS
          || cxn->state == cxnFeedingS || cxn->state == cxnFlushingS
          || cxn->state == cxnStartingS)) {
        warn("%s:%u cxnsleep connection in bad state: %s",
             hostPeerName(cxn->myHost), cxn->ident, stateToString(cxn->state));
        cxnSleepOrDie(cxn);
        return;
    }

    d_printf(1, "%s:%u Setting up a reopen callback\n",
             hostPeerName(cxn->myHost), cxn->ident);

    cxn->sleepTimerId = prepareSleep(reopenTimeoutCbk, cxn->sleepTimeout, cxn);

    /* bump the sleep timer amount each time to wait longer and longer. Gets
       reset in resetConnection() */
    cxn->sleepTimeout *= 2;
    if (cxn->sleepTimeout > max_reconnect_period)
        cxn->sleepTimeout = max_reconnect_period;
}


/*
 * (re)set all state variables to inital condition.
 */
static void
resetConnection(Connection cxn)
{
    ASSERT(cxn != NULL);

    bufferSetDataSize(cxn->respBuffer, 0);

    cxn->loggedNoCr = false;
    cxn->maxCheck = 1;
    cxn->immedRecon = false;
    cxn->doesStreaming = false; /* who knows, next time around maybe... */
    cxn->authenticated = false;
    cxn->quitWasIssued = false;
    cxn->needsChecks = true;
    cxn->timeCon = 0;

    cxn->artsTaken = 0;
    cxn->checksIssued = 0;
    cxn->checksRefused = 0;
    cxn->takesRejected = 0;
    cxn->takesOkayed = 0;
    cxn->takesSizeRejected = 0;
    cxn->takesSizeOkayed = 0;

    cxn->timeCon_checkpoint = 0;
    cxn->checksIssued_checkpoint = 0;
    cxn->checksRefused_checkpoint = 0;
    cxn->takesRejected_checkpoint = 0;
    cxn->takesOkayed_checkpoint = 0;
    cxn->takesSizeRejected_checkpoint = 0;
    cxn->takesSizeOkayed_checkpoint = 0;

    cxn->filterValue = 0.0;
}


/*
 * Give back all articles that are queued, but not actually in progress.
 * XXX merge come of this with deferAllArticles
 */
static void
deferQueuedArticles(Connection cxn)
{
    ArtHolder p, q;

    for (q = NULL, p = cxn->checkHead; p != NULL; p = q) {
        q = p->next;
        hostTakeBackArticle(cxn->myHost, cxn, p->article);
        delArtHolder(p);
        cxn->articleQTotal--;
    }
    cxn->checkHead = NULL;

    for (q = NULL, p = cxn->takeHead; cxn->doesStreaming && p != NULL; p = q) {
        q = p->next;
        hostTakeBackArticle(cxn->myHost, cxn, p->article);
        delArtHolder(p);
        cxn->articleQTotal--;
    }
    cxn->takeHead = NULL;
}


/*
 * Give back any articles we have to the Host for later retrys.
 */
static void
deferAllArticles(Connection cxn)
{
    ArtHolder p, q;

    for (q = NULL, p = cxn->checkHead; p != NULL; p = q) {
        q = p->next;
        hostTakeBackArticle(cxn->myHost, cxn, p->article);
        delArtHolder(p);
        cxn->articleQTotal--;
    }
    cxn->checkHead = NULL;

    for (q = NULL, p = cxn->checkRespHead; p != NULL; p = q) {
        q = p->next;
        hostTakeBackArticle(cxn->myHost, cxn, p->article);
        delArtHolder(p);
        cxn->articleQTotal--;
    }
    cxn->checkRespHead = NULL;

    for (q = NULL, p = cxn->takeHead; p != NULL; p = q) {
        q = p->next;
        hostTakeBackArticle(cxn->myHost, cxn, p->article);
        delArtHolder(p);
        cxn->articleQTotal--;
    }
    cxn->takeHead = NULL;

    for (q = NULL, p = cxn->takeRespHead; p != NULL; p = q) {
        q = p->next;
        hostTakeBackArticle(cxn->myHost, cxn, p->article);
        delArtHolder(p);
        cxn->articleQTotal--;
    }
    cxn->takeRespHead = NULL;

    ASSERT(cxn->articleQTotal == 0);
}


/*
 * Called when there's an article to be pushed out to the remote. Even if
 * the Connection has an article it's possible that nothing will be written
 * (e.g. if the article on the queue doesn't exist any more)
 */
static void
doSomeWrites(Connection cxn)
{
    bool doneSome = false;

    /* If there's a write pending we can't do anything now.
     * No addWorkCallback here (otherwise innfeed consumes too much CPU). */
    if (writeIsPending(cxn->myEp)) {
        return;
    } else if (writesNeeded(cxn)) /* something on a queue. */
    {
        if (cxn->doesStreaming)
            doneSome = issueStreamingCommands(cxn);
        else
            doneSome = issueIHAVE(cxn);

        /* doneSome will be false if article(s) were gone, but if the Host
           has something available, then it would have been put on the queue
           for next time around. */
        if (!doneSome) {
            if (writesNeeded(cxn)) /* Host gave us something */
                addWorkCallback(cxn->myEp, cxnWorkProc,
                                cxn); /* for next time. */
            else if (cxn->articleQTotal == 0) {
                /* if we were in cxnFeedingS, then issueStreamingCommands
                   already called cxnIdle(). */
                if (cxn->state == cxnClosingS || cxn->state == cxnFlushingS)
                    issueQUIT(cxn); /* and nothing to wait for... */
            }
        }
    } else if (cxn->state == cxnClosingS
               || cxn->state == cxnFlushingS) { /* nothing to do... */
        if (cxn->articleQTotal == 0)
            issueQUIT(cxn); /* and nothing to wait for before closing */
    }
}


/* Queue up a buffer with the IHAVE command in it for the article at
 * the head of the transmisson queue.
 *
 * If the article is missing, then the Host will be notified and
 * another article may be put on the Connections queue. This new
 * article is ignored for now, but a work callback is registered so
 * that it can be looked at later.
 */
static bool
issueIHAVE(Connection cxn)
{
    Buffer ihaveBuff, *writeArr;
    ArtHolder artH;
    Article article;
    const char *msgid;
    char *p;
    unsigned int tmp;
    size_t bufLen = 256;
    bool rval = false;

    ASSERT(!cxn->doesStreaming);
    ASSERT(cxn->state == cxnFlushingS || cxn->state == cxnFeedingS
           || cxn->state == cxnClosingS);
    ASSERT(cxn->articleQTotal == 1);
    ASSERT(cxn->checkHead != NULL);
    ASSERT(writeIsPending(cxn->myEp) == false);
    VALIDATE_CONNECTION(cxn);

    artH = cxn->checkHead;
    article = cxn->checkHead->article;
    msgid = artMsgId(artH->article);

    ASSERT(msgid != NULL);
    ASSERT(article != NULL);

    if ((tmp = (strlen(msgid) + 10)) > bufLen)
        bufLen = tmp;

    ihaveBuff = newBuffer(bufLen);

    ASSERT(ihaveBuff != NULL);

    p = bufferBase(ihaveBuff);
    sprintf(p, "IHAVE %s\r\n", msgid);
    bufferSetDataSize(ihaveBuff, strlen(p));

    d_printf(5, "%s:%u Command IHAVE %s\n", hostPeerName(cxn->myHost),
             cxn->ident, msgid);

    writeArr = makeBufferArray(ihaveBuff, NULL);
    if (!prepareWriteWithTimeout(cxn->myEp, writeArr, commandWriteDone, cxn)) {
        die("%s:%u fatal prepare write for IHAVE failed",
            hostPeerName(cxn->myHost), cxn->ident);
    }

    /* now move the article to the second queue */
    cxn->checkRespHead = cxn->checkHead;
    cxn->checkHead = NULL;

    cxn->checksIssued++;
    hostArticleOffered(cxn->myHost, cxn);

    rval = true;

    return rval;
}


/*
 * Do a prepare write with the article body as the body portion of the
 * IHAVE command
 */
static void
issueIHAVEBody(Connection cxn)
{
    Buffer *writeArray;
    Article article;

    ASSERT(cxn != NULL);
    ASSERT(!cxn->doesStreaming);
    ASSERT(cxn->state == cxnFlushingS || cxn->state == cxnFeedingS
           || cxn->state == cxnClosingS);
    ASSERT(cxn->articleQTotal == 1);
    ASSERT(cxn->takeHead != NULL);
    VALIDATE_CONNECTION(cxn);

    article = cxn->takeHead->article;
    ASSERT(article != NULL);

    if (cxn->state != cxnClosingS)
        writeArray = artGetNntpBuffers(article);
    else
        writeArray = NULL;

    if (writeArray == NULL) {
        /* missing article (expired for example) will get us here. */
        if (dotBuffer == NULL) {
            dotBuffer = newBufferByCharP(".\r\n", 3, 3);
            dotFirstBuffer = newBufferByCharP("\r\n.", 3, 3);
            crlfBuffer = newBufferByCharP("\r\n", 2, 2);
        }

        /* we'll just write the empty buffer and the remote will complain
           with 437 */
        writeArray = makeBufferArray(bufferTakeRef(dotBuffer), NULL);
    }


    if (!prepareWriteWithTimeout(cxn->myEp, writeArray, ihaveBodyDone, cxn)) {
        die("%s:%u fatal prepare write failed in issueIHAVEBody",
            hostPeerName(cxn->myHost), cxn->ident);
    } else {
        d_printf(5, "%s:%u prepared write for IHAVE body.\n",
                 hostPeerName(cxn->myHost), cxn->ident);
    }

    /* now move the article to the last queue */
    cxn->takeRespHead = cxn->takeHead;
    cxn->takeHead = NULL;

    return;
}


/* Process the two command queues. Slaps all the CHECKs together and
 * then does the TAKETHIS commands.
 *
 * If no articles on the queue(s) are valid, then the Host is
 * notified. It may queue up new articles on the Connection, but
 * these are ignored for now. A work proc is registered so the
 * articles can be processed later.
 */
static bool
issueStreamingCommands(Connection cxn)
{
    Buffer checkBuffer = NULL; /* the buffer with the CHECK commands in it. */
    Buffer *writeArray = NULL;
    ArtHolder p, q;
    bool rval = false;

    ASSERT(cxn != NULL);
    ASSERT(cxn->myEp != NULL);
    ASSERT(cxn->doesStreaming);
    VALIDATE_CONNECTION(cxn);

    checkBuffer = buildCheckBuffer(cxn); /* may be null if none to issue */

    if (checkBuffer != NULL) {
        /* Now shift the articles to their new queue. */
        for (p = cxn->checkRespHead; p != NULL && p->next != NULL; p = p->next)
            /* nada--finding end of queue*/;

        if (p == NULL)
            cxn->checkRespHead = cxn->checkHead;
        else
            p->next = cxn->checkHead;

        cxn->checkHead = NULL;
    }


    writeArray = buildTakethisBuffers(cxn, checkBuffer); /* may be null */

    /* If not null, then writeArray will have checkBuffer (if it wasn't NULL)
       in the first spot and the takethis buffers after that. */
    if (writeArray) {
        if (!prepareWriteWithTimeout(cxn->myEp, writeArray, commandWriteDone,
                                     cxn)) {
            die("%s:%u fatal prepare write for STREAMING commands failed",
                hostPeerName(cxn->myHost), cxn->ident);
        }

        rval = true;

        /* now shift articles over to their new queue. */
        for (p = cxn->takeRespHead; p != NULL && p->next != NULL; p = p->next)
            /* nada--finding end of queue */;

        if (p == NULL)
            cxn->takeRespHead = cxn->takeHead;
        else
            p->next = cxn->takeHead;

        cxn->takeHead = NULL;
    }

    /* we defer the missing article notification to here because if there
       was a big backlog of missing articles *and* we're running in
       no-CHECK mode, then the Host would be putting bad articles on the
       queue we're taking them off of. */
    if (cxn->missing && cxn->articleQTotal == 0 && !writeIsPending(cxn->myEp))
        cxnIdle(cxn);
    for (p = cxn->missing; p != NULL; p = q) {
        hostArticleIsMissing(cxn->myHost, cxn, p->article);
        q = p->next;
        delArtHolder(p);
    }
    cxn->missing = NULL;

    return rval;
}


/*
 * build up the buffer of all the CHECK commands.
 */
static Buffer
buildCheckBuffer(Connection cxn)
{
    ArtHolder p;
    size_t lenBuff = 0;
    Buffer checkBuffer = NULL;
    const char *peerName = hostPeerName(cxn->myHost);

    p = cxn->checkHead;
    while (p != NULL) {
        Article article = p->article;
        const char *msgid;

        msgid = artMsgId(article);

        lenBuff += (8 + strlen(msgid)); /* 8 == strlen("CHECK \r\n") */
        p = p->next;
    }

    if (lenBuff > 0)
        lenBuff++; /* for the null byte */

    /* now build up the single buffer that contains all the CHECK commands */
    if (lenBuff > 0) {
        char *t;
        size_t tlen = 0;

        checkBuffer = newBuffer(lenBuff);
        t = bufferBase(checkBuffer);

        p = cxn->checkHead;
        while (p != NULL) {
            const char *msgid = artMsgId(p->article);

            sprintf(t, "CHECK %s\r\n", msgid);
            d_printf(5, "%s:%u Command %s\n", peerName, cxn->ident, t);

            tlen += strlen(t);

            while (*t)
                t++;

            cxn->checksIssued++;
            hostArticleOffered(cxn->myHost, cxn);

            p = p->next;
        }

        ASSERT(tlen + 1 == lenBuff);

        bufferSetDataSize(checkBuffer, tlen);
    }

    return checkBuffer;
}


/*
 * Construct and array of TAKETHIS commands and the command bodies. Any
 * articles on the queue that are missing will be removed and the Host will
 * be informed.
 */
static Buffer *
buildTakethisBuffers(Connection cxn, Buffer checkBuffer)
{
    size_t lenArray = 0;
    ArtHolder p, q;
    Buffer *rval = NULL;
    const char *peerName = hostPeerName(cxn->myHost);

    if (checkBuffer != NULL)
        lenArray++;

    if (cxn->takeHead != NULL) /* some TAKETHIS commands to be done. */
    {
        Buffer takeBuffer;
        unsigned int takeBuffLen;
        unsigned int writeIdx = 0;

        /* count up all the buffers we'll be writing. One extra each time for
           the TAKETHIS command buffer*/
        for (p = cxn->takeHead; p != NULL; p = p->next)
            if (artContentsOk(p->article))
                lenArray += (1 + artNntpBufferCount(p->article));

        /* now allocate the array for the buffers and put them all in it */
        /* 1 for the terminator */
        rval = xmalloc(sizeof(Buffer) * (lenArray + 1));

        if (checkBuffer != NULL)
            rval[writeIdx++] = checkBuffer;

        q = NULL;
        p = cxn->takeHead;
        while (p != NULL) {
            char *t;
            const char *msgid;
            Article article;
            Buffer *articleBuffers;
            int i, nntpLen;

            article = p->article;
            nntpLen = artNntpBufferCount(article);
            msgid = artMsgId(article);

            if (nntpLen == 0) { /* file no longer valid so drop from queue */
                ArtHolder ta = p;

                if (q == NULL) /* it's the first in the queue */
                    cxn->takeHead = p->next;
                else
                    q->next = p->next;

                p = p->next;
                ASSERT(cxn->articleQTotal > 0);
                cxn->articleQTotal--;

                ta->next = cxn->missing;
                cxn->missing = ta;
            } else {
                articleBuffers = artGetNntpBuffers(article);

                /* set up the buffer with the TAKETHIS command in it.
                   12 == strlen ("TAKETHIS \n\r") */
                takeBuffLen = 12 + strlen(msgid);
                takeBuffer = newBuffer(takeBuffLen);
                t = bufferBase(takeBuffer);

                sprintf(t, "TAKETHIS %s\r\n", msgid);
                bufferSetDataSize(takeBuffer, strlen(t));

                d_printf(5, "%s:%u Command %s\n", peerName, cxn->ident, t);

                ASSERT(writeIdx <= lenArray);
                rval[writeIdx++] = takeBuffer;

                /* now add all the buffers that make up the body of the
                   TAKETHIS command */
                for (i = 0; i < nntpLen; i++) {
                    ASSERT(writeIdx <= lenArray);
                    rval[writeIdx++] = bufferTakeRef(articleBuffers[i]);
                }

                freeBufferArray(articleBuffers);

                if (!cxn->needsChecks) {
                    /* this isn't quite right. An article may be counted
                       twice if we switch to no-CHECK mode after its
                       CHECK was issued, but before its TAKETHIS was done
                       just now. I'm not going to worry unless someone
                       complains. */

                    cxn->checksIssued++;
                    hostArticleOffered(cxn->myHost, cxn);
                }

                q = p;
                p = p->next;
            }
        }

        if (writeIdx > 0)
            rval[writeIdx] = NULL;
        else { /* all articles were missing and no CHECKS */
            free(rval);
            rval = NULL;
        }
    } else if (checkBuffer != NULL) /* no TAKETHIS to do, but some CHECKS */
        rval = makeBufferArray(checkBuffer, NULL);

    return rval;
}


/*
 * for one reason or another we need to disconnect gracefully. We send a
 * QUIT command.
 */
static void
issueQUIT(Connection cxn)
{
    Buffer quitBuffer, *writeArray;
    const char *peerName = hostPeerName(cxn->myHost);

    ASSERT(cxn->takeHead == NULL);
    ASSERT(cxn->checkHead == NULL);
    VALIDATE_CONNECTION(cxn);

    if (cxn->quitWasIssued)
        return;

    if (writeIsPending(cxn->myEp)) {
        warn("%s:%u internal QUIT while write pending", peerName, cxn->ident);

        if (cxn->state == cxnClosingS)
            cxnDead(cxn);
        else
            cxnWait(cxn);
    } else {
        quitBuffer = newBuffer(7);
        strlcpy(bufferBase(quitBuffer), "QUIT\r\n", 7);
        bufferSetDataSize(quitBuffer, 6);

        writeArray = makeBufferArray(quitBuffer, NULL);

        d_printf(1, "%s:%u Sending a quit command\n",
                 hostPeerName(cxn->myHost), cxn->ident);

        cxn->quitWasIssued = true; /* not exactly true, but good enough */

        if (!prepareWriteWithTimeout(cxn->myEp, writeArray, quitWritten,
                                     cxn)) {
            die("%s:%u fatal prepare write for QUIT command failed", peerName,
                cxn->ident);
        }
    }
}


/*
 * Set up the timer for the blocked reads
 */
static void
initReadBlockedTimeout(Connection cxn)
{
    ASSERT(cxn != NULL);
    ASSERT(cxn->state != cxnIdleS);

    /* set up the response timer. */
    clearTimer(cxn->readBlockedTimerId);

    if (cxn->readTimeout > 0)
        cxn->readBlockedTimerId =
            prepareSleep(responseTimeoutCbk, cxn->readTimeout, cxn);
}


/*
 * Set up the timer for the blocked reads
 */
static int
prepareWriteWithTimeout(EndPoint endp, Buffer *buffers, EndpRWCB done,
                        Connection cxn)
{
    /* Clear the read timer, since we can't expect a response until everything
       is sent.
       XXX - would be nice to have a timeout for responses if we're sending a
       string of commands. */
    clearTimer(cxn->readBlockedTimerId);

    /* set up the write timer. */
    clearTimer(cxn->writeBlockedTimerId);

    if (cxn->writeTimeout > 0)
        cxn->writeBlockedTimerId =
            prepareSleep(writeTimeoutCbk, cxn->writeTimeout, cxn);

    /* set up the write. */
    return prepareWrite(endp, buffers, writeProgress, done, cxn);
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

    if (cxn->myEp != NULL)
        delEndPoint(cxn->myEp);

    ASSERT(cxn->checkHead == NULL);
    ASSERT(cxn->checkRespHead == NULL);
    ASSERT(cxn->takeHead == NULL);
    ASSERT(cxn->takeRespHead == NULL);

    delBuffer(cxn->respBuffer);

    /* tell the Host we're outta here. */
    shutDown = hostCxnGone(cxn->myHost, cxn);

    cxn->ident = 0;
    cxn->timeCon = 0;

    free(cxn->ipName);

    clearTimer(cxn->artReceiptTimerId);
    clearTimer(cxn->readBlockedTimerId);
    clearTimer(cxn->writeBlockedTimerId);
    clearTimer(cxn->flushTimerId);

    free(cxn);

    if (shutDown) {
        /* exit program if that was the last connexion for the last host */
        /* XXX what about if there are ever multiple listeners?
           XXX    this will be executed if all hosts on only one of the
           XXX    listeners have gone */
        time_t now = theTime();
        char dateString[30];
        char **p = PointersFreedOnExit;

        /* finish out all outstanding memory */
        while (*p)
            free(*p++);
        free(PointersFreedOnExit);
        freeTimeoutQueue();

        timeToString(now, dateString, sizeof(dateString));
        notice("ME finishing at %s", dateString);

        exit(0);
    }
}


/*
 * Bump up the value of the low pass filter on the connection.
 */
static void
incrFilter(Connection cxn)
{
    cxn->filterValue *= (1.0 - (1.0 / cxn->lowPassFilter));
    cxn->filterValue += 1.0;
}


/*
 * decrement the value of the low pass filter on the connection.
 */
static void
decrFilter(Connection cxn)
{
    cxn->filterValue *= (1.0 - (1.0 / cxn->lowPassFilter));
}


/*
 * return true if we have articles we need to issue commands for.
 */
static bool
writesNeeded(Connection cxn)
{
    return (cxn->checkHead != NULL || cxn->takeHead != NULL ? true : false);
}


/*
 * do some simple tests to make sure it's OK.
 */
static void
validateConnection(Connection cxn)
{
    unsigned int i;
    unsigned int old;
    ArtHolder p;

    i = 0;

    /* count up the articles the Connection has and make sure that matches. */
    for (p = cxn->takeHead; p != NULL; p = p->next)
        i++;
    d_printf(4, "TAKE queue: %u\n", i);
    old = i;

    for (p = cxn->takeRespHead; p != NULL; p = p->next)
        i++;
    d_printf(4, "TAKE response queue: %u\n", i - old);
    old = i;

    for (p = cxn->checkHead; p != NULL; p = p->next)
        i++;
    d_printf(4, "CHECK queue: %u\n", i - old);
    old = i;

    for (p = cxn->checkRespHead; p != NULL; p = p->next)
        i++;
    d_printf(4, "CHECK response queue: %u\n", i - old);

    ASSERT(i == cxn->articleQTotal);

    switch (cxn->state) {
    case cxnConnectingS:
        ASSERT(cxn->doesStreaming == false);
        ASSERT(cxn->articleQTotal <= 1);
        ASSERT(cxn->artReceiptTimerId == 0);
        ASSERT(cxn->sleepTimerId == 0);
        /* ASSERT (cxn->timeCon == 0) ; */
        break;

    case cxnWaitingS:
        ASSERT(cxn->articleQTotal == 0);
        ASSERT(cxn->myEp == NULL);
        ASSERT(cxn->artReceiptTimerId == 0);
        ASSERT(cxn->readBlockedTimerId == 0);
        ASSERT(cxn->writeBlockedTimerId == 0);
        ASSERT(cxn->flushTimerId == 0);
        ASSERT(cxn->sleepTimerId == 0);
        ASSERT(cxn->timeCon == 0);
        break;

    case cxnFlushingS:
    case cxnClosingS:
        if (!cxn->doesStreaming)
            ASSERT(cxn->articleQTotal <= 1);
        ASSERT(cxn->artReceiptTimerId == 0);
        ASSERT(cxn->flushTimerId == 0);
        ASSERT(cxn->sleepTimerId == 0);
        ASSERT(cxn->timeCon != 0);
        ASSERT(cxn->doesStreaming || cxn->maxCheck == 1);
        break;

    case cxnFeedingS:
        if (cxn->doesStreaming)
            /* Some(?) hosts return the 439 response even before we're done
               sending, so don't go idle until here */
            ASSERT(cxn->articleQTotal > 0 || writeIsPending(cxn->myEp));
        else
            ASSERT(cxn->articleQTotal == 1);
        if (cxn->readTimeout > 0 && !writeIsPending(cxn->myEp)
            && cxn->checkRespHead != NULL && cxn->takeRespHead != NULL)
            ASSERT(cxn->readBlockedTimerId != 0);
        if (cxn->writeTimeout > 0 && writeIsPending(cxn->myEp))
            ASSERT(cxn->writeBlockedTimerId != 0);
        ASSERT(cxn->sleepTimerId == 0);
        ASSERT(cxn->timeCon != 0);
        ASSERT(cxn->doesStreaming || cxn->maxCheck == 1);
        break;

    case cxnIdleS:
        ASSERT(cxn->articleQTotal == 0);
        if (cxn->articleReceiptTimeout > 0)
            ASSERT(cxn->artReceiptTimerId != 0);
        ASSERT(cxn->readBlockedTimerId == 0);
        ASSERT(cxn->writeBlockedTimerId == 0);
        ASSERT(cxn->sleepTimerId == 0);
        ASSERT(cxn->timeCon != 0);
        ASSERT(!writeIsPending(cxn->myEp));
        break;

    case cxnIdleTimeoutS:
        ASSERT(cxn->articleQTotal == 0);
        ASSERT(cxn->artReceiptTimerId == 0);
        ASSERT(cxn->writeBlockedTimerId == 0);
        ASSERT(cxn->sleepTimerId == 0);
        ASSERT(cxn->timeCon != 0);
        ASSERT(!writeIsPending(cxn->myEp));
        break;

    case cxnSleepingS:
        ASSERT(cxn->articleQTotal == 0);
        ASSERT(cxn->myEp == NULL);
        ASSERT(cxn->artReceiptTimerId == 0);
        ASSERT(cxn->readBlockedTimerId == 0);
        ASSERT(cxn->writeBlockedTimerId == 0);
        ASSERT(cxn->flushTimerId == 0);
        ASSERT(cxn->timeCon == 0);
        break;

    case cxnStartingS:
        ASSERT(cxn->articleQTotal == 0);
        ASSERT(cxn->myEp == NULL);
        ASSERT(cxn->artReceiptTimerId == 0);
        ASSERT(cxn->readBlockedTimerId == 0);
        ASSERT(cxn->writeBlockedTimerId == 0);
        ASSERT(cxn->flushTimerId == 0);
        ASSERT(cxn->sleepTimerId == 0);
        ASSERT(cxn->timeCon == 0);
        break;

    case cxnDeadS:
        break;
    }
}


/*
 * Generate a printable string of the parameter.
 */
static const char *
stateToString(CxnState state)
{
    static char rval[64];

    switch (state) {
    case cxnStartingS:
        strlcpy(rval, "cxnStartingS", sizeof(rval));
        break;

    case cxnWaitingS:
        strlcpy(rval, "cxnWaitingS", sizeof(rval));
        break;

    case cxnConnectingS:
        strlcpy(rval, "cxnConnectingS", sizeof(rval));
        break;

    case cxnIdleS:
        strlcpy(rval, "cxnIdleS", sizeof(rval));
        break;

    case cxnIdleTimeoutS:
        strlcpy(rval, "cxnIdleTimeoutS", sizeof(rval));
        break;

    case cxnFeedingS:
        strlcpy(rval, "cxnFeedingS", sizeof(rval));
        break;

    case cxnSleepingS:
        strlcpy(rval, "cxnSleepingS", sizeof(rval));
        break;

    case cxnFlushingS:
        strlcpy(rval, "cxnFlushingS", sizeof(rval));
        break;

    case cxnClosingS:
        strlcpy(rval, "cxnClosingS", sizeof(rval));
        break;

    case cxnDeadS:
        strlcpy(rval, "cxnDeadS", sizeof(rval));
        break;

    default:
        snprintf(rval, sizeof(rval), "UNKNOWN STATE: %u", state);
        break;
    }

    return rval;
}


/****************************************************************************
 *
 * Functions for managing the internal queue of Articles on each Connection.
 *
 ****************************************************************************/

static ArtHolder
newArtHolder(Article article)
{
    ArtHolder a = xmalloc(sizeof(struct art_holder_s));

    a->article = article;
    a->next = NULL;

    return a;
}


/*
 * Deletes the article holder
 */
static void
delArtHolder(ArtHolder artH)
{
    if (artH != NULL)
        free(artH);
}


/*
 * remove the article holder from the queue. Adjust the count and if nxtPtr
 * points at the element then adjust that too.
 */
static bool
remArtHolder(ArtHolder artH, ArtHolder *head, unsigned int *count)
{
    ArtHolder h, i;

    ASSERT(head != NULL);
    ASSERT(count != NULL);

    h = *head;
    i = NULL;
    while (h != NULL && h != artH) {
        i = h;
        h = h->next;
    }

    if (h == NULL)
        return false;

    if (i == NULL)
        *head = (*head)->next;
    else
        i->next = artH->next;

    (*count)--;

    return true;
}


/*
 * append the ArticleHolder to the queue
 */
static void
appendArtHolder(ArtHolder artH, ArtHolder *head, unsigned int *count)
{
    ArtHolder p;

    ASSERT(head != NULL);
    ASSERT(count != NULL);

    for (p = *head; p != NULL && p->next != NULL; p = p->next)
        /* nada */;

    if (p == NULL)
        *head = artH;
    else
        p->next = artH;

    artH->next = NULL;
    (*count)++;
}


/*
 * find the article holder on the queue by comparing the message-id.
 */
static ArtHolder
artHolderByMsgId(const char *msgid, ArtHolder head)
{
    while (head != NULL) {
        if (strcmp(msgid, artMsgId(head->article)) == 0)
            return head;

        head = head->next;
    }

    return NULL;
}


/*
 * Randomize a numeber by the given percentage
 */

static int
fudgeFactor(int initVal)
{
    int newValue;
    static bool seeded;

    if (!seeded) {
        time_t t = theTime();

        /* this may have been done already in endpoint.c. Is that a problem???
         */
        srand(t);
        seeded = true;
    }

    newValue = initVal + (initVal / 10 - (rand() % (initVal / 5)));

    return newValue;
}

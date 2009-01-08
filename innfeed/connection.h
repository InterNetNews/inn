/*  $Id$
**
**  The public interface to the Connection class.
**
**  Written by James Brister <brister@vix.com>
**
**  The Connection class encapulates an NNTP protocol endpoint (either regular
**  or extended with the streaming protocol).  Each Connection is owned by a
**  single Host object.
**
**  It manages the network connection (via an EndPoint) the the pumping of
**  articles to the remote host.  It gets these articles from its Host object.
**  If the remote doesn't handle the streaming extension, then the Connection
**  will only manage one article at a time.  If the remote handles the
**  extension, then the connection will queue up articles while sending the
**  CHECK and TAKETHIS commands.
**
**  If the network connection drops while the Connection object has articles
**  queued up, then it will hand them back to its Host object.
*/

#if ! defined ( connection_h__ )
#define connection_h__


#include <time.h>
#include <stdio.h>

#include "misc.h"


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
Connection newConnection (Host host,
                          unsigned int ident,
                          const char *ipname,
                          unsigned int artTout,
                          unsigned int portNum,
                          unsigned int respTimeout,
                          unsigned int closePeriod,
                          double lowPassLow,
                          double lowPassHigh,
			  double lowPassFilter) ;

  /* Causes the Connection to build the network connection. */
bool cxnConnect (Connection cxn) ;

  /* puts the connection into the wait state (i.e. waits for an article
     before initiating a connect). Can only be called right after
     newConnection returns, or while the Connection is in the (internal)
     Sleeping state. */
void cxnWait (Connection cxn) ;

  /* The Connection will disconnect as if cxnDisconnect were called and then
     it automatically reconnects to the remote. */
void cxnFlush (Connection cxn) ;

  /* The Connection sends remaining articles, then issues a QUIT and then
     deletes itself */
void cxnClose (Connection cxn) ;

  /* The Connection drops all queueed articles, then issues a QUIT and then
     deletes itself */
void cxnTerminate (Connection cxn) ;

  /* Blow away the connection gracelessly and immedately clean up */
void cxnNuke (Connection cxn) ;

  /* Tells the Connection to take the article and handle its
     transmission. If it can't (due to queue size or whatever), then the
     function returns false. The connection assumes ownership of the
     article if it accepts it (returns true). */
bool cxnTakeArticle (Connection cxn, Article art) ;

  /* Tell the Connection to take the article (if it can) for later
     processing. Assumes ownership of it if it takes it. */
bool cxnQueueArticle (Connection cxn, Article art) ;

  /* generate a syslog message for the connections activity. Called by Host. */
void cxnLogStats (Connection cxn, bool final) ;

  /* return the number of articles the connection can be given. This lets
     the host shovel in as many as possible. May be zero. */
size_t cxnQueueSpace (Connection cxn) ;

  /* adjust the mode no-CHECK filter values */
void cxnSetCheckThresholds (Connection cxn,
			    double lowFilter, double highFilter,
			    double lowPassFilter) ;

  /* print some debugging info. */
void gPrintCxnInfo (FILE *fp, unsigned int indentAmt) ;
void printCxnInfo (Connection cxn, FILE *fp, unsigned int indentAmt) ;

/* config file load callback */
int cxnConfigLoadCbk (void *data) ;

/* Check connection state is in cxnFeedingS, cxnIdleS or cxnConnectingS. */
bool cxnCheckstate (Connection cxn) ;

#endif /* connection_h__ */

/* -*- c -*-
 *
 * Author:      James Brister <brister@vix.com> -- berkeley-unix --
 * Start Date:  Wed Dec 27 08:44:20 1995
 * Project:     INN (innfeed)
 * File:        host.h
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
 * Description: The public interface to the Host class.
 *
 *              The Host class represents the remote news system
 *              that we're feeding. A Host object has possibly
 *              multiple connections to the remote system which it
 *              sends articles down. It is given the articles by
 *              other objects (typically the InnListener), and once
 *              taken it assumes all responsibility for transmission
 *              or temporary storage on network failures etc.
 *
 */

#if ! defined ( host_h__ )
#define host_h__


#include <stdio.h>

#include "misc.h"

/*
 * Functions from elsewhere used by host.c
 */

extern void mainLogStatus (FILE *fp) ;


/*
 * Functions used by the InnListener
 */


/*
 * Create a new Host object.
 *
 * NAME is the name that INN uses.
 * IPNAME is the name the networking code uses (or the ascii dotted quad
 *    IP address).
 * ARTTIMEOUT is the max amount of time we'll wait for a new article
 *    from INN before considering the connection unused and we'll close
 *    down.
 * RESPTIMEOUT is the max amount of time we'll wait for any reponse
 *    from a remote. Past this we'll close down the network connection.
 * INITIALCXNS is the number of Connections to create at Host creation time.
 * MAXCXNS is the maximum number of parallel connections to the
 *    remote host we can run at any one time.
 * MAXCHECK is the maximum number of nntp CHECK commands to be outstanding
 *    on a connection before opening up a new connection (or refusing
 *    new articles if we get to MAXCXNS).
 * PORTNUM is the port number on the remote host we should talk to.
 * CLOSEPERIOD is the number of seconds after connecting that the
 *     connections should be closed down and reinitialized (due to problems
 *     with old NNTP servers that hold history files open. Value of 0 means
 *     dont close down.
 * STREAMING is a boolean flag to tell if the Host wants its Connections to
 *     do streaming or not.
 * LOWPASSHIGH is the high value for the low-pass filter.
 * LOWPASSLOW is the low value for the low-pass filter.
 */

void configHosts (bool talkSelf) ;

/* print some debugging info. */
void gPrintHostInfo (FILE *fp, unsigned int indentAmt) ;
void printHostInfo (Host host, FILE *fp, unsigned int indentAmt) ;

/* Delete the host object. Drops all the active connections immediately
   (i.e. no QUIT) . */
void delHost (Host host) ;

/* Get a new default host object */
Host newDefaultHost (InnListener listener,
		     const char *name); 

/* gently close down all the host's connections (issue QUITs). */
void hostClose (Host host) ;

/* gently close down all active connections (issue QUITs) and recreate
   them immediately */
void hostFlush (Host host) ;

/* have the HOST transmit the ARTICLE, or, failing that, store article
   information for later attempts. */
void hostSendArticle (Host host, Article article) ;

/* return an IP address for the host */
struct sockaddr *hostIpAddr (Host host) ;

/* mark the current IP address as failed and rotate to the next one */
void hostIpFailed (Host host) ;

/*
 * Functions used by the Connection to indicate Connection state.
 */

/* called by the Host's connection when the remote is refusing
   postings. Code 400 in the banner */
void hostCxnBlocked (Host host, Connection cxn, char *reason) ;

/* called by the Connection when it has determined if the remote supports
   the streaming extension or not. */
void hostRemoteStreams (Host host, Connection cxn, bool doesStream) ;

/* Called by the connection when it is no longer connected to the
   remote. Perhaps due to getting a code 400 to an IHAVE. */
void hostCxnDead (Host host, Connection cxn) ;

/* Called when the Connection deletes itself */
bool hostCxnGone (Host host, Connection cxn) ;

/* Called when the Connection goes to sleep. */
void hostCxnSleeping (Host host, Connection cxn) ;

/* Called when the Connection starts waiting for articles. */
void hostCxnWaiting (Host host, Connection cxn) ;



/* Called when the connection has sent an IHAVE or a CHECK, or a TAKETHIS
   when in no-check mode.*/
void hostArticleOffered (Host host, Connection cxn) ;

/* called by the Connection when the article was transferred. */
void hostArticleAccepted (Host host, Connection cxn, Article article) ;

/* Called by the connection when the remote answered 435 or 438 */
void hostArticleNotWanted (Host host, Connection cxn, Article article) ;

/* Called by the connection when the remote answered 437 or 439 */
void hostArticleRejected (Host host, Connection cxn, Article article) ;

/* Called when the connection when the remote answered 400 or 431 or 436 */
void hostArticleDeferred (Host host, Connection cxn, Article article) ;

/* Called by the connection if it discovers the file is gone. */
void hostArticleIsMissing (Host host, Connection cxn, Article article) ;


/* Called by the connection when it wants to defer articles, but it
   doesn't want the Host to queue any news on it. */
void hostTakeBackArticle (Host host, Connection cxn, Article article) ;


/* called by the Connection when it is idle and wants to get things
   moving. Returns true if there was something to do and the Host called
   cxnQueueArticle() . */
bool hostGimmeArticle (Host host, Connection cxn) ;

/* get the name that INN uses for this host */
const char *hostPeerName (Host host) ;

/* get the username and password for authentication */
const char *hostUsername (Host host) ;
const char *hostPassword (Host host) ;

/* if VAL is true then each time the host logs its stats all its
   connections will too. */
void hostLogConnectionStats (bool val) ;
bool hostLogConnectionStatsP (void) ;

#if 0
/* Set the frequency (in seconds) with which we log statistics */
void hostSetStatsPeriod (unsigned int period) ;
#endif

/* return whether or not the Connections should attempt to stream. */
bool hostWantsStreaming (Host host) ;

/* return maxChecks */
unsigned int hostmaxChecks (Host host);

/* return if we should drop deferred articles */
bool hostDropDeferred (Host host);

/* return the maximum number of CHECKs that can be outstanding */
unsigned int hostMaxChecks (Host host) ;

/* Called by the Host's connections when they go into (true) or out of
   (false) no-CHECK mode. */
void hostLogNoCheckMode (Host host, bool on, double low, double cur, double high) ;

/* calculate host backlog statistics */
void gCalcHostBlStat (void) ;

/* calculate host global statistics */
void gHostStats (void) ;

/* set the pathname of the file to use instead of innfeed.status */
void hostSetStatusFile (const char *filename) ;

/* function called when config file is loaded. */
int hostConfigLoadCbk (void *data) ;

#endif /* host_h__ */

void hostChkCxns(TimeoutId tid, void *data);






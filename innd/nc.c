/*  $Id$
**
**  Routines for the NNTP channel.  Other channels get the descriptors which
**  we turn into NNTP channels, and over which we speak NNTP.
*/

#include "config.h"
#include "clibrary.h"

#include "inn/innconf.h"
#include "innd.h"

#define BAD_COMMAND_COUNT	10


/*
**  An entry in the dispatch table.  The name, and implementing function,
**  of every command we support.
*/
typedef struct _NCDISPATCH {
    const char *        Name;
    innd_callback_t     Function;
    int                 Size;
} NCDISPATCH;

/* The functions that implement the various commands. */
static void NCauthinfo(CHANNEL *cp);
static void NCcancel(CHANNEL *cp);
static void NCcheck(CHANNEL *cp);
static void NChead(CHANNEL *cp);
static void NChelp(CHANNEL *cp);
static void NCihave(CHANNEL *cp);
static void NClist(CHANNEL *cp);
static void NCmode(CHANNEL *cp);
static void NCquit(CHANNEL *cp);
static void NCstat(CHANNEL *cp);
static void NCtakethis(CHANNEL *cp);
static void NCxbatch(CHANNEL *cp);

/* Handlers for unimplemented commands.  We need two handlers so that we can
   return the right status code; reader commands that are required by the
   standard must return a 502 error rather than a 500 error. */
static void NC_reader(CHANNEL *cp);
static void NC_unimp(CHANNEL *cp);

/* Supporting functions. */
static void NCwritedone(CHANNEL *cp);

/* Set up the dispatch table for all of the commands. */
#define COMMAND(name, func) { name, func, sizeof(name) - 1 }
static NCDISPATCH NCcommands[] = {
    COMMAND("authinfo",  NCauthinfo),
    COMMAND("check",     NCcheck),
    COMMAND("head",      NChead),
    COMMAND("help",      NChelp),
    COMMAND("ihave",     NCihave),
    COMMAND("list",      NClist),
    COMMAND("mode",      NCmode),
    COMMAND("quit",      NCquit),
    COMMAND("stat",      NCstat),
    COMMAND("takethis",  NCtakethis),
    COMMAND("xbatch",    NCxbatch),

    /* Unimplemented reader commands which may become available after a MODE
       READER command. */
    COMMAND("article",   NC_reader),
    COMMAND("body",      NC_reader),
    COMMAND("group",     NC_reader),
    COMMAND("last",      NC_reader),
    COMMAND("newgroups", NC_reader),
    COMMAND("newnews",   NC_reader),
    COMMAND("next",      NC_reader),
    COMMAND("post",      NC_reader),

    /* Other unimplemented standard commands. */
    COMMAND("date",      NC_unimp),
    COMMAND("slave",     NC_unimp)
};
#undef COMMAND

/* Number of open connections. */
static int NCcount;

static char		*NCquietlist[] = { INND_QUIET_BADLIST };
static const char	NCterm[] = "\r\n";
static const char 	NCdot[] = "." ;
static const char	NCbadcommand[] = NNTP_BAD_COMMAND;
static const char       NCbadsubcommand[] = NNTP_BAD_SUBCMD;

/*
** Clear the WIP entry for the given channel
*/
void
NCclearwip(CHANNEL *cp)
{
    WIPfree(WIPbyhash(cp->CurrentMessageIDHash));
    HashClear(&cp->CurrentMessageIDHash);
    cp->ArtBeg = 0;
}

/*
**  Write an NNTP reply message.
**
**  Tries to do the actual write immediately if it will not block and if there
**  is not already other buffered output.  Then, if the write is successful,
**  calls NCwritedone (which does whatever is necessary to accommodate state
**  changes).  Else, NCwritedone will be called from the main select loop
**  later.
**
**  If the reply that we are writing now is associated with a state change,
**  then cp->State must be set to its new value *before* NCwritereply is
**  called.
*/
void
NCwritereply(CHANNEL *cp, const char *text)
{
    struct buffer *bp;
    int i;

    /* XXX could do RCHANremove(cp) here, as the old NCwritetext() used to
     * do, but that would be wrong if the channel is sreaming (because it
     * would zap the channell's input buffer).  There's no harm in
     * never calling RCHANremove here.  */

    bp = &cp->Out;
    i = bp->left;
    WCHANappend(cp, text, strlen(text));	/* text in buffer */
    WCHANappend(cp, NCterm, STRLEN(NCterm));	/* add CR NL to text */

    /* FIXME: Something is wrong with this code.  At the least, it's
       confusing.  bp->used can be incremented without decrementing bp->left,
       which shouldn't be allowed. */
    if (i == 0) {	/* if only data then try to write directly */
	i = write(cp->fd, &bp->data[bp->used], bp->left);
	if (Tracing || cp->Tracing)
	    syslog(L_TRACE, "%s NCwritereply %d=write(%d, \"%.15s\", %d)",
		CHANname(cp), i, cp->fd, &bp->data[bp->used], bp->left);
	if (i > 0) bp->used += i;
	if (bp->used == bp->left) {
	    /* all the data was written */
	    bp->used = bp->left = 0;
	    NCwritedone(cp);
	}
	else i = 0;
    } else i = 0;
    if (i <= 0) {	/* write failed, queue it for later */
	WCHANadd(cp);
    }
    if (Tracing || cp->Tracing)
	syslog(L_TRACE, "%s > %s", CHANname(cp), text);
}

/*
**  Tell the NNTP channel to go away.
*/
void
NCwriteshutdown(CHANNEL *cp, const char *text)
{
    cp->State = CSwritegoodbye;
    RCHANremove(cp); /* we're not going to read anything more */
    WCHANappend(cp, NNTP_GOODBYE, STRLEN(NNTP_GOODBYE));
    WCHANappend(cp, " ", 1);
    WCHANappend(cp, text, (int)strlen(text));
    WCHANappend(cp, NCterm, STRLEN(NCterm));
    WCHANadd(cp);
}


/*
**  If a Message-ID is bad, write a reject message and return TRUE.
*/
static bool
NCbadid(CHANNEL *cp, char *p)
{
    if (ARTidok(p))
	return FALSE;

    NCwritereply(cp, NNTP_HAVEIT_BADID);
    syslog(L_NOTICE, "%s bad_messageid %s", CHANname(cp), MaxLength(p, p));
    return TRUE;
}


/*
**  We have an entire article collected; try to post it.  If we're
**  not running, drop the article or just pause and reschedule.
*/
static void
NCpostit(CHANNEL *cp)
{
  bool	postok;
  const char	*response;
  char	buff[SMBUF];

  /* Note that some use break, some use return here. */
  if ((postok = ARTpost(cp)) != 0) {
    cp->Received++;
    if (cp->Sendid.size > 3) { /* We be streaming */
      cp->Takethis_Ok++;
      snprintf(buff, sizeof(buff), "%d", NNTP_OK_RECID_VAL);
      cp->Sendid.data[0] = buff[0];
      cp->Sendid.data[1] = buff[1];
      cp->Sendid.data[2] = buff[2];
      response = cp->Sendid.data;
    } else
      response = NNTP_TOOKIT;
  } else {
    cp->Rejected++;
    if (cp->Sendid.size)
      response = cp->Sendid.data;
    else
      response = cp->Error;
  }
  cp->Reported++;
  if (cp->Reported >= innconf->nntpactsync) {
    snprintf(buff, sizeof(buff), "accepted size %.0f duplicate size %.0f",
             cp->Size, cp->DuplicateSize);
    syslog(L_NOTICE,
      "%s checkpoint seconds %ld accepted %ld refused %ld rejected %ld duplicate %ld %s",
      CHANname(cp), (long)(Now.time - cp->Started),
    cp->Received, cp->Refused, cp->Rejected,
    cp->Duplicate, buff);
    cp->Reported = 0;
  }
  if (Mode == OMthrottled) {
    NCwriteshutdown(cp, ModeReason);
    return;
  }
  cp->State = CSgetcmd;
  NCwritereply(cp, response);
}


/*
**  Write-done function.  Close down or set state for what we expect to
**  read next.
*/
static void
NCwritedone(CHANNEL *cp)
{
    switch (cp->State) {
    default:
	syslog(L_ERROR, "%s internal NCwritedone state %d",
	    CHANname(cp), cp->State);
	break;

    case CSwritegoodbye:
	if (NCcount > 0)
	    NCcount--;
	CHANclose(cp, CHANname(cp));
	break;

    case CSgetcmd:
    case CSgetauth:
    case CSgetheader:
    case CSgetbody:
    case CSgetxbatch:
    case CScancel:
	RCHANadd(cp);
	break;
    }
}



/*
**  The "head" command.
*/
static void
NChead(CHANNEL *cp)
{
    char	        *p;
    TOKEN		token;
    ARTHANDLE		*art;

    /* Snip off the Message-ID. */
    for (p = cp->In.data + cp->Start + STRLEN("head"); ISWHITE(*p); p++)
	continue;
    cp->Start = cp->Next;
    if (NCbadid(cp, p))
	return;

    /* Get the article token and retrieve it. */
    if (!HISlookup(History, p, NULL, NULL, NULL, &token)) {
	NCwritereply(cp, NNTP_DONTHAVEIT);
	return;
    }
    if ((art = SMretrieve(token, RETR_HEAD)) == NULL) {
	NCwritereply(cp, NNTP_DONTHAVEIT);
	return;
    }

    /* Write it. */
    WCHANappend(cp, NNTP_HEAD_FOLLOWS, STRLEN(NNTP_HEAD_FOLLOWS));
    WCHANappend(cp, " 0 ", 3);
    WCHANappend(cp, p, strlen(p));
    WCHANappend(cp, NCterm, STRLEN(NCterm));
    WCHANappend(cp, art->data, art->len);

    /* Write the terminator. */
    NCwritereply(cp, NCdot);
    SMfreearticle(art);
}


/*
**  The "stat" command.
*/
static void
NCstat(CHANNEL *cp)
{
    char	        *p;
    TOKEN		token;
    ARTHANDLE		*art;
    char		*buff;
    size_t              length;

    /* Snip off the Message-ID. */
    for (p = cp->In.data + cp->Start + STRLEN("stat"); ISWHITE(*p); p++)
	continue;
    cp->Start = cp->Next;
    if (NCbadid(cp, p))
	return;

    /* Get the article filenames; open the first file (to make sure
     * the article is still here). */
    if (!HISlookup(History, p, NULL, NULL, NULL, &token)) {
	NCwritereply(cp, NNTP_DONTHAVEIT);
	return;
    }
    if ((art = SMretrieve(token, RETR_STAT)) == NULL) {
	NCwritereply(cp, NNTP_DONTHAVEIT);
	return;
    }
    SMfreearticle(art);

    /* Write the message. */
    length = snprintf(NULL, 0, "%d 0 %s", NNTP_NOTHING_FOLLOWS_VAL, p) + 1;
    buff = xmalloc(length);
    snprintf(buff, length, "%d 0 %s", NNTP_NOTHING_FOLLOWS_VAL, p);
    NCwritereply(cp, buff);
    free(buff);
}


/*
**  The "authinfo" command.  Actually, we come in here whenever the
**  channel is in CSgetauth state and we just got a command.
*/
static void
NCauthinfo(CHANNEL *cp)
{
    static char		AUTHINFO[] = "authinfo ";
    static char		PASS[] = "pass ";
    static char		USER[] = "user ";
    char		*p;

    p = cp->In.data + cp->Start;
    cp->Start = cp->Next;

    /* Allow the poor sucker to quit. */
    if (caseEQ(p, "quit")) {
	NCquit(cp);
	return;
    }

    /* Otherwise, make sure we're only getting "authinfo" commands. */
    if (!caseEQn(p, AUTHINFO, STRLEN(AUTHINFO))) {
	NCwritereply(cp, NNTP_AUTH_NEEDED);
	return;
    }
    for (p += STRLEN(AUTHINFO); ISWHITE(*p); p++)
	continue;

    /* Ignore "authinfo user" commands, since we only care about the
     * password. */
    if (caseEQn(p, USER, STRLEN(USER))) {
	NCwritereply(cp, NNTP_AUTH_NEXT);
	return;
    }

    /* Now make sure we're getting only "authinfo pass" commands. */
    if (!caseEQn(p, PASS, STRLEN(PASS))) {
	NCwritereply(cp, NNTP_AUTH_NEEDED);
	return;
    }
    for (p += STRLEN(PASS); ISWHITE(*p); p++)
	continue;

    /* Got the password -- is it okay? */
    if (!RCauthorized(cp, p)) {
	cp->State = CSwritegoodbye;
	NCwritereply(cp, NNTP_AUTH_BAD);
    } else {
	cp->State = CSgetcmd;
	NCwritereply(cp, NNTP_AUTH_OK);
    }
}

/*
**  The "help" command.
*/
static void
NChelp(CHANNEL *cp)
{
    static char		LINE1[] = "For more information, contact \"";
    static char		LINE2[] = "\" at this machine.";
    NCDISPATCH		*dp;

    WCHANappend(cp, NNTP_HELP_FOLLOWS,STRLEN(NNTP_HELP_FOLLOWS));
    WCHANappend(cp, NCterm,STRLEN(NCterm));
    for (dp = NCcommands; dp < ENDOF(NCcommands); dp++)
	if (dp->Function != NC_unimp) {
            if ((!StreamingOff && cp->Streaming) ||
                (dp->Function != NCcheck && dp->Function != NCtakethis)) {
                WCHANappend(cp, "\t", 1);
                WCHANappend(cp, dp->Name, dp->Size);
                WCHANappend(cp, NCterm, STRLEN(NCterm));
            }
	}
    WCHANappend(cp, LINE1, STRLEN(LINE1));
    WCHANappend(cp, NEWSMASTER, STRLEN(NEWSMASTER));
    WCHANappend(cp, LINE2, STRLEN(LINE2));
    WCHANappend(cp, NCterm, STRLEN(NCterm));
    NCwritereply(cp, NCdot) ;
    cp->Start = cp->Next;
}

/*
**  The "ihave" command.  Check the Message-ID, and see if we want the
**  article or not.  Set the state appropriately.
*/
static void
NCihave(CHANNEL *cp)
{
    char	*p;
#if defined(DO_PERL) || defined(DO_PYTHON)
    char	*filterrc;
    int		msglen;
#endif /*defined(DO_PERL) || defined(DO_PYTHON) */

    cp->Ihave++;
    /* Snip off the Message-ID. */
    for (p = cp->In.data + cp->Start + STRLEN("ihave"); ISWHITE(*p); p++)
	continue;
    cp->Start = cp->Next;
    if (NCbadid(cp, p))
	return;

    if ((innconf->refusecybercancels) && (strncmp(p, "<cancel.", 8) == 0)) {
	cp->Refused++;
	cp->Ihave_Cybercan++;
	NCwritereply(cp, NNTP_HAVEIT);
	return;
    }

#if defined(DO_PERL)
    /*  Invoke a perl message filter on the message ID. */
    filterrc = PLmidfilter(p);
    if (filterrc) {
        cp->Refused++;
        msglen = strlen(p) + 5; /* 3 digits + space + id + null */
        if (cp->Sendid.size < msglen) {
            if (cp->Sendid.size > 0) free(cp->Sendid.data);
            if (msglen > MAXHEADERSIZE)
                cp->Sendid.size = msglen;
            else
                cp->Sendid.size = MAXHEADERSIZE;
            cp->Sendid.data = xmalloc(cp->Sendid.size);
        }
        snprintf(cp->Sendid.data, cp->Sendid.size, "%d %.200s",
                 NNTP_HAVEIT_VAL, filterrc);
        NCwritereply(cp, cp->Sendid.data);
        free(cp->Sendid.data);
        cp->Sendid.size = 0;
        return;
    }
#endif

#if defined(DO_PYTHON)
    /*  invoke a Python message filter on the message id */
    msglen = strlen(p);
    TMRstart(TMR_PYTHON);
    filterrc = PYmidfilter(p, msglen);
    TMRstop(TMR_PYTHON);
    if (filterrc) {
	cp->Refused++;
	msglen += 5; /* 3 digits + space + id + null */
	if (cp->Sendid.size < msglen) {
	    if (cp->Sendid.size > 0)
		free(cp->Sendid.data);
	    if (msglen > MAXHEADERSIZE)
		cp->Sendid.size = msglen;
	    else
		cp->Sendid.size = MAXHEADERSIZE;
	    cp->Sendid.data = xmalloc(cp->Sendid.size);
	}
	snprintf(cp->Sendid.data, cp->Sendid.size, "%d %.200s",
                 NNTP_HAVEIT_VAL, filterrc);
	NCwritereply(cp, cp->Sendid.data);
	free(cp->Sendid.data);
	cp->Sendid.size = 0;
	return;
    }
#endif

    if (HIScheck(History, p)) {
	cp->Refused++;
	cp->Ihave_Duplicate++;
	NCwritereply(cp, NNTP_HAVEIT);
    }
    else if (WIPinprogress(p, cp, FALSE)) {
	cp->Ihave_Deferred++;
	if (cp->NoResendId) {
	    cp->Refused++;
	    NCwritereply(cp, NNTP_HAVEIT);
	} else {
	    NCwritereply(cp, NNTP_RESENDIT_LATER);
	}
    }
    else {
	if (cp->Sendid.size > 0) {
            free(cp->Sendid.data);
	    cp->Sendid.size = 0;
	}
	cp->Ihave_SendIt++;
	NCwritereply(cp, NNTP_SENDIT);
	cp->ArtBeg = Now.time;
	cp->State = CSgetheader;
	ARTprepare(cp);
    }
}

/* 
** The "xbatch" command. Set the state appropriately.
*/

static void
NCxbatch(CHANNEL *cp)
{
    char	*p;

    /* Snip off the batch size */
    for (p = cp->In.data + cp->Start + STRLEN("xbatch"); ISWHITE(*p); p++)
	continue;
    cp->Start = cp->Next;

    if (cp->XBatchSize) {
        syslog(L_FATAL, "NCxbatch(): oops, cp->XBatchSize already set to %d",
	       cp->XBatchSize);
    }

    cp->XBatchSize = atoi(p);
    if (Tracing || cp->Tracing)
        syslog(L_TRACE, "%s will read batch of size %d",
	       CHANname(cp), cp->XBatchSize);

    if (cp->XBatchSize <= 0 || ((innconf->maxartsize != 0) && (innconf->maxartsize < cp->XBatchSize))) {
        syslog(L_NOTICE, "%s got bad xbatch size %d",
	       CHANname(cp), cp->XBatchSize);
	NCwritereply(cp, NNTP_XBATCH_BADSIZE);
	return;
    }

    /* we prefer not to touch the buffer, NCreader() does enough magic
     * with it
     */
    cp->State = CSgetxbatch;
    NCwritereply(cp, NNTP_CONT_XBATCH);
}

/*
**  The "list" command.  Send the active file.
*/
static void
NClist(CHANNEL *cp)
{
    char *p, *q, *trash, *end, *path;

    for (p = cp->In.data + cp->Start + STRLEN("list"); ISWHITE(*p); p++)
	continue;
    cp->Start = cp->Next;
    if (cp->Nolist) {
	NCwritereply(cp, NCbadcommand);
	return;
    }
    if (caseEQ(p, "newsgroups")) {
        path = concatpath(innconf->pathdb, _PATH_NEWSGROUPS);
	trash = p = ReadInFile(path, NULL);
        free(path);
	if (p == NULL) {
	    NCwritereply(cp, NCdot);
	    return;
	}
	end = p + strlen(p);
    }
    else if (caseEQ(p, "active.times")) {
        path = concatpath(innconf->pathdb, _PATH_ACTIVETIMES);
	trash = p = ReadInFile(path, NULL);
        free(path);
	if (p == NULL) {
	    NCwritereply(cp, NCdot);
	    return;
	}
	end = p + strlen(p);
    }
    else if (*p == '\0' || (caseEQ(p, "active"))) {
	p = ICDreadactive(&end);
	trash = NULL;
    }
    else {
	NCwritereply(cp, NCbadsubcommand);
	return;
    }

    /* Loop over all lines, sending the text and \r\n. */
    WCHANappend(cp, NNTP_LIST_FOLLOWS,STRLEN(NNTP_LIST_FOLLOWS));
    WCHANappend(cp, NCterm, STRLEN(NCterm)) ;
    for (; p < end && (q = strchr(p, '\n')) != NULL; p = q + 1) {
	WCHANappend(cp, p, q - p);
	WCHANappend(cp, NCterm, STRLEN(NCterm));
    }
    NCwritereply(cp, NCdot);
    if (trash)
	free(trash);
}


/*
**  The "mode" command.  Hand off the channel.
*/
static void
NCmode(CHANNEL *cp)
{
    char		*p;
    HANDOFF		h;

    /* Skip the first word, get the argument. */
    for (p = cp->In.data + cp->Start + STRLEN("mode"); ISWHITE(*p); p++)
	continue;
    cp->Start = cp->Next;

    if (caseEQ(p, "reader") && !innconf->noreader)
	h = HOnnrpd;
    else if (caseEQ(p, "stream") &&
             (!StreamingOff && cp->Streaming)) {
	char buff[16];

	snprintf(buff, sizeof(buff), "%d StreamOK.", NNTP_OK_STREAM_VAL);
	NCwritereply(cp, buff);
	syslog(L_NOTICE, "%s NCmode \"mode stream\" received",
		CHANname(cp));
	return;
    } else if (caseEQ(p, "cancel") && cp->privileged) {
       char buff[16];

       cp->State = CScancel;
       snprintf(buff, sizeof(buff), "%d CancelOK.", NNTP_OK_CANCEL_VAL);
       NCwritereply(cp, buff);
       syslog(L_NOTICE, "%s NCmode \"mode cancel\" received",
               CHANname(cp));
       return;
    } else {
	NCwritereply(cp, NCbadsubcommand);
	return;
    }
    RChandoff(cp->fd, h);
    if (NCcount > 0)
	NCcount--;
    CHANclose(cp, CHANname(cp));
}


/*
**  The "quit" command.  Acknowledge, and set the state to closing down.
*/
static void
NCquit(CHANNEL *cp)
{
    cp->State = CSwritegoodbye;
    NCwritereply(cp, NNTP_GOODBYE_ACK);
}


/*
**  The catch-all for reader commands, which should return a different status
**  than just "unrecognized command" since a change of state may make them
**  available.
*/
static void
NC_reader(CHANNEL *cp)
{
    cp->Start = cp->Next;
    NCwritereply(cp, NNTP_ACCESS);
}


/*
**  The catch-all for inimplemented commands.
*/
static void
NC_unimp(CHANNEL *cp)
{
    char		*p, *q;
    char		buff[SMBUF];

    /* Nip off the first word. */
    for (p = q = cp->In.data + cp->Start; *p && !ISWHITE(*p); p++)
	continue;
    cp->Start = cp->Next;
    *p = '\0';
    snprintf(buff, sizeof(buff), "%d \"%s\" not implemented; try \"help\".",
             NNTP_BAD_COMMAND_VAL, MaxLength(q, q));
    NCwritereply(cp, buff);
}



/*
**  Check whatever data is available on the channel.  If we got the
**  full amount (i.e., the command or the whole article) process it.
*/
static void
NCproc(CHANNEL *cp)
{
  char	        *p, *q;
  NCDISPATCH   	*dp;
  struct buffer	*bp;
  char		buff[SMBUF];
  int		i, j;
  bool		readmore, movedata;
  ARTDATA	*data = &cp->Data;
  HDRCONTENT    *hc = data->HdrContent;

  readmore = movedata = FALSE;
  if (Tracing || cp->Tracing)
    syslog(L_TRACE, "%s NCproc Used=%d", CHANname(cp), cp->In.used);

  bp = &cp->In;
  if (bp->used == 0)
    return;

  for ( ; ; ) {
    if (Tracing || cp->Tracing) {
      syslog(L_TRACE, "%s cp->Start=%d cp->Next=%d bp->Used=%d", CHANname(cp),
	cp->Start, cp->Next, bp->used);
      if (bp->used > 15)
	syslog(L_TRACE, "%s NCproc state=%d next \"%.15s\"", CHANname(cp),
	  cp->State, &bp->data[cp->Next]);
    }
    switch (cp->State) {
    default:
      syslog(L_ERROR, "%s internal NCproc state %d", CHANname(cp), cp->State);
      movedata = FALSE;
      readmore = TRUE;
      break;

    case CSwritegoodbye:
      movedata = FALSE;
      readmore = TRUE;
      break;

    case CSgetcmd:
    case CSgetauth:
    case CScancel:
      /* Did we get the whole command, terminated with "\r\n"? */
      for (i = cp->Next; (i < bp->used) && (bp->data[i] != '\n'); i++) ;
      if (i == bp->used) {
	/* Check for too long command. */
	if ((j = bp->used - cp->Start) > NNTP_STRLEN) {
	  /* Make some room, saving only the last few bytes. */
	  for (p = bp->data, i = 0; i < SAVE_AMT; i++)
	    p[i] = p[bp->used - SAVE_AMT + i];
	  cp->LargeCmdSize += j - SAVE_AMT;
	  bp->used = cp->Next = SAVE_AMT;
	  bp->left = bp->size - SAVE_AMT;
	  cp->Start = 0;
	  cp->State = CSeatcommand;
	  /* above means moving data already */
	  movedata = FALSE;
	} else {
	  cp->Next = bp->used;
	  /* move data to the begining anyway */
	  movedata = TRUE;
	}
	readmore = TRUE;
	break;
      }
      /* i points where '\n" and go forward */
      cp->Next = ++i;
      /* never move data so long as "\r\n" is found, since subsequent
	 data may also include command line */
      movedata = FALSE;
      readmore = FALSE;
      if (i - cp->Start < 3) {
	break;
      }
      p = &bp->data[i];
      if (p[-2] != '\r') { /* probably in an article */
	char *tmpstr;

	tmpstr = xmalloc(i - cp->Start + 1);
	memcpy(tmpstr, bp->data + cp->Start, i - cp->Start);
	tmpstr[i - cp->Start] = '\0';
	
	syslog(L_NOTICE, "%s bad_command %s", CHANname(cp),
	  MaxLength(tmpstr, tmpstr));
	free(tmpstr);

	if (++(cp->BadCommands) >= BAD_COMMAND_COUNT) {
	  cp->State = CSwritegoodbye;
	  NCwritereply(cp, NCbadcommand);
	  break;
	}
	NCwritereply(cp, NCbadcommand);
	/* still some data left, go for it */
	cp->Start = cp->Next;
	break;
      }

      q = &bp->data[cp->Start];
      /* Ignore blank lines. */
      if (*q == '\0' || i - cp->Start == 2) {
	cp->Start = cp->Next;
	break;
      }
      p[-2] = '\0';
      if (Tracing || cp->Tracing)
	syslog(L_TRACE, "%s < %s", CHANname(cp), q);

      /* We got something -- stop sleeping (in case we were). */
      SCHANremove(cp);
      if (cp->Argument != NULL) {
	free(cp->Argument);
	cp->Argument = NULL;
      }

      if (cp->State == CSgetauth) {
	if (caseEQn(q, "mode", 4))
	  NCmode(cp);
	else
	  NCauthinfo(cp);
	break;
      } else if (cp->State == CScancel) {
	NCcancel(cp);
	break;
      }

      /* Loop through the command table. */
      for (p = q, dp = NCcommands; dp < ENDOF(NCcommands); dp++) {
	if (caseEQn(p, dp->Name, dp->Size)) {
	  /* ignore the streaming commands if necessary. */
	  if (!StreamingOff || cp->Streaming ||
	    (dp->Function != NCcheck && dp->Function != NCtakethis)) {
	    (*dp->Function)(cp);
	    cp->BadCommands = 0;
	    break;
	  }
	}
      }
      if (dp == ENDOF(NCcommands)) {
	if (++(cp->BadCommands) >= BAD_COMMAND_COUNT)
	    cp->State = CSwritegoodbye;
	NCwritereply(cp, NCbadcommand);
	cp->Start = cp->Next;

	/* Channel could have been freed by above NCwritereply if 
	   we're writing-goodbye */
	if (cp->Type == CTfree)
	  return;
	for (i = 0; (p = NCquietlist[i]) != NULL; i++)
	  if (caseEQ(p, q))
	    break;
	if (p == NULL)
	  syslog(L_NOTICE, "%s bad_command %s", CHANname(cp),
	    MaxLength(q, q));
      }
      break;

    case CSgetheader:
    case CSgetbody:
    case CSeatarticle:
      TMRstart(TMR_ARTPARSE);
      ARTparse(cp);
      TMRstop(TMR_ARTPARSE);
      if (cp->State == CSgetbody || cp->State == CSgetheader ||
	      cp->State == CSeatarticle) {
	if (cp->Next - cp->Start > innconf->datamovethreshold ||
	  (innconf->maxartsize > 0 && cp->Size > innconf->maxartsize)) {
	  /* avoid buffer extention for ever */
	  movedata = TRUE;
	} else {
	  movedata = FALSE;
	}
	readmore = TRUE;
	break;
      }

      if (*cp->Error != '\0') {
	cp->Rejected++;
	cp->State = CSgetcmd;
	cp->Start = cp->Next;
	NCclearwip(cp);
	if (cp->Sendid.size > 3)
	  NCwritereply(cp, cp->Sendid.data);
	else
	  NCwritereply(cp, cp->Error);
	readmore = FALSE;
	movedata = FALSE;
	break;
      }

      if (cp->State == CSgotlargearticle) {
	syslog(L_NOTICE, "%s internal rejecting huge article (%d > %ld)",
	  CHANname(cp), cp->LargeArtSize, innconf->maxartsize);
	if (cp->Sendid.size)
	  NCwritereply(cp, cp->Sendid.data);
	else {
	  snprintf(buff, sizeof(buff),
                   "%d Article exceeds local limit of %ld bytes",
                   NNTP_REJECTIT_VAL, innconf->maxartsize);
	  NCwritereply(cp, buff);
        }
	cp->LargeArtSize = 0;
	cp->State = CSgetcmd;
	cp->Rejected++;
	cp->Start = cp->Next;

	/* Write a local cancel entry so nobody else gives it to us. */
	if (HDR_FOUND(HDR__MESSAGE_ID)) {
	  HDR_PARSE_START(HDR__MESSAGE_ID);
	  if (!HIScheck(History, HDR(HDR__MESSAGE_ID)) &&
	      !InndHisRemember(HDR(HDR__MESSAGE_ID)))
	    syslog(L_ERROR, "%s cant write %s", LogName, HDR(HDR__MESSAGE_ID)); 
	}
	/* Clear the work-in-progress entry. */
	NCclearwip(cp);
	readmore = FALSE;
	movedata = FALSE;
	break;
      }

      if (cp->State == CSnoarticle) {
	/* this should happen when parsing header */
	cp->Rejected++;
	cp->State = CSgetcmd;
	cp->Start = cp->Next;
	/* Clear the work-in-progress entry. */
	NCclearwip(cp);
	if (cp->Sendid.size > 3) { /* We be streaming */
	  cp->Takethis_Err++;
	  snprintf(buff, sizeof(buff), "%d", NNTP_ERR_FAILID_VAL);
	  cp->Sendid.data[0] = buff[0];
	  cp->Sendid.data[1] = buff[1];
	  cp->Sendid.data[2] = buff[2];
	  NCwritereply(cp, cp->Sendid.data);
	} else
	  NCwritereply(cp, NNTP_REJECTIT_EMPTY);
	readmore = FALSE;
	movedata = FALSE;
	break;
      }
    case CSgotarticle: /* in case caming back from pause */
      /* never move data so long as "\r\n.\r\n" is found, since subsequent data
	 may also include command line */
      readmore = FALSE;
      movedata = FALSE;
      if (Mode == OMpaused) { /* defer processing while paused */
	RCHANremove(cp); /* don't bother trying to read more for now */
	SCHANadd(cp, Now.time + innconf->pauseretrytime, &Mode, NCproc, NULL);
	return;
      } else if (Mode == OMthrottled) {
	/* Clear the work-in-progress entry. */
	NCclearwip(cp);
	NCwriteshutdown(cp, ModeReason);
	cp->Rejected++;
	return;
      }

      SCHANremove(cp);
      if (cp->Argument != NULL) {
	free(cp->Argument);
	cp->Argument = NULL;
      }
      NCpostit(cp);
      /* Clear the work-in-progress entry. */
      NCclearwip(cp);
      if (cp->State == CSwritegoodbye)
	break;
      cp->State = CSgetcmd;
      cp->Start = cp->Next;
      break;

    case CSeatcommand:
      /* Eat the command line and then complain that it was too large */
      /* Reading a line; look for "\r\n" terminator. */
      /* cp->Next should be SAVE_AMT(10) */
      for (i = cp->Next ; i < bp->used; i++) {
	if ((bp->data[i - 1] == '\r') && (bp->data[i] == '\n')) {
	  cp->Next = i + 1;
	  break;
	}
      }
      if (i < bp->used) {	/* did find terminator */
	/* Reached the end of the command line. */
	SCHANremove(cp);
	if (cp->Argument != NULL) {
	  free(cp->Argument);
	  cp->Argument = NULL;
	}
	i += cp->LargeCmdSize;
	syslog(L_NOTICE, "%s internal rejecting too long command line (%d > %d)",
	  CHANname(cp), i, NNTP_STRLEN);
	cp->LargeCmdSize = 0;
	snprintf(buff, sizeof(buff), "%d command exceeds limit of %d bytes",
                 NNTP_BAD_COMMAND_VAL, NNTP_STRLEN);
	cp->State = CSgetcmd;
	cp->Start = cp->Next;
	NCwritereply(cp, buff);
        readmore = FALSE;
        movedata = FALSE;
      } else {
	cp->LargeCmdSize += bp->used - cp->Next;
	bp->used = cp->Next = SAVE_AMT;
	bp->left = bp->size - SAVE_AMT;
	cp->Start = 0;
        readmore = TRUE;
        movedata = FALSE;
      }
      break;

    case CSgetxbatch:
      /* if the batch is complete, write it out into the in.coming
       * directory with an unique timestamp, and start rnews on it.
       */
      if (Tracing || cp->Tracing)
	syslog(L_TRACE, "%s CSgetxbatch: now %d of %d bytes", CHANname(cp),
	  bp->used, cp->XBatchSize);

      if (cp->Next != 0) {
	/* data must start from the begining of the buffer */
        movedata = TRUE;
	readmore = FALSE;
	break;
      }
      if (bp->used < cp->XBatchSize) {
	movedata = FALSE;
	readmore = TRUE;
	break;	/* give us more data */
      }
      movedata = FALSE;
      readmore = FALSE;

      /* now do something with the batch */
      {
	char buff2[SMBUF];
	int fd, oerrno, failed;
	long now;

	now = time(NULL);
	failed = 0;
	/* time+channel file descriptor should make an unique file name */
	snprintf(buff, sizeof(buff), "%s/%ld%d.tmp", innconf->pathincoming,
                 now, cp->fd);
	fd = open(buff, O_WRONLY|O_CREAT|O_EXCL, ARTFILE_MODE);
	if (fd < 0) {
	  oerrno = errno;
	  failed = 1;
	  syslog(L_ERROR, "%s cannot open outfile %s for xbatch: %m",
	    CHANname(cp), buff);
	  snprintf(buff, sizeof(buff), "%s cant create file: %s",
                   NNTP_RESENDIT_XBATCHERR, strerror(oerrno));
	  NCwritereply(cp, buff);
	} else {
	  if (write(fd, cp->In.data, cp->XBatchSize) != cp->XBatchSize) {
	    oerrno = errno;
	    syslog(L_ERROR, "%s cant write batch to file %s: %m", CHANname(cp),
	      buff);
	    snprintf(buff, sizeof(buff), "%s cant write batch to file: %s",
                     NNTP_RESENDIT_XBATCHERR, strerror(oerrno));
	    NCwritereply(cp, buff);
	    failed = 1;
	  }
	}
	if (fd >= 0 && close(fd) != 0) {
	  oerrno = errno;
	  syslog(L_ERROR, "%s error closing batch file %s: %m", CHANname(cp),
	    failed ? "" : buff);
	  snprintf(buff, sizeof(buff), "%s error closing batch file: %s",
                   NNTP_RESENDIT_XBATCHERR, strerror(oerrno));
	  NCwritereply(cp, buff);
	  failed = 1;
	}
	snprintf(buff2, sizeof(buff2), "%s/%ld%d.x", innconf->pathincoming,
                 now, cp->fd);
	if (rename(buff, buff2)) {
	  oerrno = errno;
	  syslog(L_ERROR, "%s cant rename %s to %s: %m", CHANname(cp),
	    failed ? "" : buff, buff2);
	  snprintf(buff, sizeof(buff), "%s cant rename batch to %s: %s",
                   NNTP_RESENDIT_XBATCHERR, buff2, strerror(oerrno));
	  NCwritereply(cp, buff);
	  failed = 1;
	}
	cp->Reported++;
	if (!failed) {
	  NCwritereply(cp, NNTP_OK_XBATCHED);
	  cp->Received++;
	} else
	  cp->Rejected++;
      }
      syslog(L_NOTICE, "%s accepted batch size %d", CHANname(cp),
	cp->XBatchSize);
      cp->State = CSgetcmd;
      cp->Start = cp->Next = cp->XBatchSize;
      break;
    }
    if (cp->State == CSwritegoodbye || cp->Type == CTfree)
      break;
    if (Tracing || cp->Tracing)
      syslog(L_TRACE, "%s NCproc state=%d Start=%d Next=%d Used=%d",
	CHANname(cp), cp->State, cp->Start, cp->Next, bp->used);

    if (movedata) { /* move data rather than extend buffer */
      TMRstart(TMR_DATAMOVE);
      movedata = FALSE;
      if (cp->Start > 0)
	memmove(bp->data, &bp->data[cp->Start], bp->used - cp->Start);
      bp->used -= cp->Start;
      bp->left += cp->Start;
      cp->Next -= cp->Start;
      if (cp->State == CSgetheader || cp->State == CSgetbody ||
	cp->State == CSeatarticle) {
	/* adjust offset only in CSgetheader, CSgetbody or CSeatarticle */
	data->CurHeader -= cp->Start;
	data->LastTerminator -= cp->Start;
	data->LastCR -= cp->Start;
	data->LastCRLF -= cp->Start;
	data->Body -= cp->Start;
	if (data->BytesHeader != NULL)
	  data->BytesHeader -= cp->Start;
	for (i = 0 ; i < MAX_ARTHEADER ; i++, hc++) {
	  if (hc->Value != NULL)
	    hc->Value -= cp->Start;
	}
      }
      cp->Start = 0;
      TMRstop(TMR_DATAMOVE);
    }
    if (readmore)
      /* need to read more */
      break;
  }
}


/*
**  Read whatever data is available on the channel.  If we got the
**  full amount (i.e., the command or the whole article) process it.
*/
static void
NCreader(CHANNEL *cp)
{
    int			i;

    if (Tracing || cp->Tracing)
	syslog(L_TRACE, "%s NCreader Used=%d",
	    CHANname(cp), cp->In.used);

    /* Read any data that's there; ignore errors (retry next time it's our
     * turn) and if we got nothing, then it's EOF so mark it closed. */
    if ((i = CHANreadtext(cp)) <= 0) {
        /* Return of -2 indicates we got EAGAIN even though the descriptor
           selected true for reading, probably due to the Solaris select
           bug.  Drop back out to the main loop as if the descriptor never
           selected true. */
        if (i == -2) {
            return;
        }
	if (i == 0 || cp->BadReads++ >= innconf->badiocount) {
	    if (NCcount > 0)
		NCcount--;
	    CHANclose(cp, CHANname(cp));
	}
	return;
    }

    NCproc(cp);	    /* check and process data */
}



/*
**  Set up the NNTP channel state.
*/
void
NCsetup(void)
{
    char		*p;
    char		buff[SMBUF];

    /* Set the greeting message. */
    p = innconf->pathhost;
    if (p == NULL)
	/* Worked in main, now it fails?  Curious. */
	p = Path.data;
    snprintf(buff, sizeof(buff), "%d %s InterNetNews server %s ready",
	    NNTP_POSTOK_VAL, p, inn_version_string);
    NCgreeting = xstrdup(buff);
}


/*
**  Tear down our state.
*/
void
NCclose(void)
{
    CHANNEL		*cp;
    int			j;

    /* Close all incoming channels. */
    for (j = 0; (cp = CHANiter(&j, CTnntp)) != NULL; ) {
	if (NCcount > 0)
	    NCcount--;
	CHANclose(cp, CHANname(cp));
    }
}


/*
**  Create an NNTP channel and print the greeting message.
*/
CHANNEL *
NCcreate(int fd, bool MustAuthorize, bool IsLocal)
{
    CHANNEL		*cp;
    int			i;

    /* Create the channel. */
    cp = CHANcreate(fd, CTnntp, MustAuthorize ? CSgetauth : CSgetcmd,
	    NCreader, NCwritedone);

    NCclearwip(cp);
    cp->privileged = IsLocal;
#if defined(SOL_SOCKET) && defined(SO_SNDBUF) && defined(SO_RCVBUF) 
    if (!IsLocal) {
	i = 24 * 1024;
	if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char *)&i, sizeof i) < 0)
	    syslog(L_ERROR, "%s cant setsockopt(SNDBUF) %m", CHANname(cp));
	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char *)&i, sizeof i) < 0)
	    syslog(L_ERROR, "%s cant setsockopt(RCVBUF) %m", CHANname(cp));
    }
#endif	/* defined(SOL_SOCKET) && defined(SO_SNDBUF) && defined(SO_RCVBUF) */

#if	defined(SOL_SOCKET) && defined(SO_KEEPALIVE)
    if (!IsLocal) {
	/* Set KEEPALIVE to catch broken socket connections. */
	i = 1;
	if (setsockopt(fd, SOL_SOCKET,  SO_KEEPALIVE, (char *)&i, sizeof i) < 0)
	    syslog(L_ERROR, "%s cant setsockopt(KEEPALIVE) %m", CHANname(cp));
    }
#endif /* defined(SOL_SOCKET) && defined(SO_KEEPALIVE) */

    /* Now check our operating mode. */
    NCcount++;
    if (Mode == OMthrottled) {
	NCwriteshutdown(cp, ModeReason);
	return NULL;
    }
    if (RejectReason) {
	NCwriteshutdown(cp, RejectReason);
	return NULL;
    }

    /* See if we have too many channels. */
    if (!IsLocal && innconf->maxconnections && 
			NCcount >= innconf->maxconnections && !RCnolimit(cp)) {
	/* Recount, just in case we got out of sync. */
	for (NCcount = 0, i = 0; CHANiter(&i, CTnntp) != NULL; )
	    NCcount++;
	if (NCcount >= innconf->maxconnections) {
	    NCwriteshutdown(cp, "Too many connections");
	    return NULL;
	}
    }
    cp->BadReads = 0;
    cp->BadCommands = 0;
    return cp;
}



/* These modules support the streaming option to tranfer articles
** faster.
*/

/*
**  The "check" command.  Check the Message-ID, and see if we want the
**  article or not.  Stay in command state.
*/
static void
NCcheck(CHANNEL *cp)
{
    char		*p;
    int			idlen, msglen;
#if defined(DO_PERL) || defined(DO_PYTHON)
    char		*filterrc;
#endif /* DO_PERL || DO_PYTHON */

    cp->Check++;
    /* Snip off the Message-ID. */
    for (p = cp->In.data + cp->Start; *p && !ISWHITE(*p); p++)
	continue;
    cp->Start = cp->Next;
    for ( ; ISWHITE(*p); p++)
	continue;
    idlen = strlen(p);
    msglen = idlen + 5; /* 3 digits + space + id + null */
    if (cp->Sendid.size < msglen) {
	if (cp->Sendid.size > 0) free(cp->Sendid.data);
	if (msglen > MAXHEADERSIZE) cp->Sendid.size = msglen;
	else cp->Sendid.size = MAXHEADERSIZE;
	cp->Sendid.data = xmalloc(cp->Sendid.size);
    }
    if (!ARTidok(p)) {
	snprintf(cp->Sendid.data, cp->Sendid.size, "%d %s",
                 NNTP_ERR_GOTID_VAL, p);
	NCwritereply(cp, cp->Sendid.data);
	syslog(L_NOTICE, "%s bad_messageid %s", CHANname(cp), MaxLength(p, p));
	return;
    }

    if ((innconf->refusecybercancels) && (strncmp(p, "<cancel.", 8) == 0)) {
	cp->Refused++;
	cp->Check_cybercan++;
	snprintf(cp->Sendid.data, cp->Sendid.size, "%d %s",
                 NNTP_ERR_GOTID_VAL, p);
	NCwritereply(cp, cp->Sendid.data);
	return;
    }

#if defined(DO_PERL)
    /*  Invoke a perl message filter on the message ID. */
    filterrc = PLmidfilter(p);
    if (filterrc) {
	cp->Refused++;
	snprintf(cp->Sendid.data, cp->Sendid.size, "%d %s",
                 NNTP_ERR_GOTID_VAL, p);
	NCwritereply(cp, cp->Sendid.data);
	return;
    }
#endif /* defined(DO_PERL) */

#if defined(DO_PYTHON)
    /*  invoke a python message filter on the message id */
    filterrc = PYmidfilter(p, idlen);
    if (filterrc) {
	cp->Refused++;
	snprintf(cp->Sendid.data, cp->Sendid.size, "%d %s",
                 NNTP_ERR_GOTID_VAL, p);
	NCwritereply(cp, cp->Sendid.data);
	return;
    }
#endif /* defined(DO_PYTHON) */

    if (HIScheck(History, p)) {
	cp->Refused++;
	cp->Check_got++;
	snprintf(cp->Sendid.data, cp->Sendid.size, "%d %s",
                 NNTP_ERR_GOTID_VAL, p);
	NCwritereply(cp, cp->Sendid.data);
    } else if (WIPinprogress(p, cp, TRUE)) {
	cp->Check_deferred++;
	if (cp->NoResendId) {
	    cp->Refused++;
	    snprintf(cp->Sendid.data, cp->Sendid.size, "%d %s",
                     NNTP_ERR_GOTID_VAL, p);
	} else {
	    snprintf(cp->Sendid.data, cp->Sendid.size, "%d %s",
                     NNTP_RESENDID_VAL, p);
	}
	NCwritereply(cp, cp->Sendid.data);
    } else {
	cp->Check_send++;
	snprintf(cp->Sendid.data, cp->Sendid.size, "%d %s",
                 NNTP_OK_SENDID_VAL, p);
	NCwritereply(cp, cp->Sendid.data);
    }
    /* stay in command mode */
}

/*
**  The "takethis" command.  Article follows.
**  Remember <id> for later ack.
*/
static void
NCtakethis(CHANNEL *cp)
{
    char	        *p;
    int			msglen;
    WIP                 *wp;

    cp->Takethis++;
    /* Snip off the Message-ID. */
    for (p = cp->In.data + cp->Start + STRLEN("takethis"); ISWHITE(*p); p++)
	continue;
    cp->Start = cp->Next;
    for ( ; ISWHITE(*p); p++)
	continue;
    if (!ARTidok(p)) {
	syslog(L_NOTICE, "%s bad_messageid %s", CHANname(cp), MaxLength(p, p));
    }
    msglen = strlen(p) + 5; /* 3 digits + space + id + null */
    if (cp->Sendid.size < msglen) {
	if (cp->Sendid.size > 0) free(cp->Sendid.data);
	if (msglen > MAXHEADERSIZE) cp->Sendid.size = msglen;
	else cp->Sendid.size = MAXHEADERSIZE;
	cp->Sendid.data = xmalloc(cp->Sendid.size);
    }
    /* save ID for later NACK or ACK */
    snprintf(cp->Sendid.data, cp->Sendid.size, "%d %s", NNTP_ERR_FAILID_VAL,
             p);

    cp->ArtBeg = Now.time;
    cp->State = CSgetheader;
    ARTprepare(cp);
    /* set WIP for benefit of later code in NCreader */
    if ((wp = WIPbyid(p)) == (WIP *)NULL)
	wp = WIPnew(p, cp);
    cp->CurrentMessageIDHash = wp->MessageID;
}

/*
**  Process a cancel ID from a "mode cancel" channel.
*/
static void
NCcancel(CHANNEL *cp)
{
    char *av[2] = { NULL, NULL };
    const char *res;

    ++cp->Received;
    av[0] = cp->In.data + cp->Start;
    cp->Start = cp->Next;
    res = CCcancel(av);
    if (res) {
        char buff[SMBUF];

        snprintf(buff, sizeof(buff), "%d %s", NNTP_ERR_CANCEL_VAL,
                 MaxLength(res, res));
        syslog(L_NOTICE, "%s cant_cancel %s", CHANname(cp),
               MaxLength(res, res));
        NCwritereply(cp, buff);
    } else {
        NCwritereply(cp, NNTP_OK_CANCELLED);
    }
}

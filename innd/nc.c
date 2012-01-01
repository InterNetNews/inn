/*  $Id$
**
**  Routines for the NNTP channel.  Other channels get the descriptors which
**  we turn into NNTP channels, and over which we speak NNTP.
*/

#include "config.h"
#include "clibrary.h"

#include "inn/innconf.h"
#include "inn/qio.h"
#include "inn/version.h"
#include "innd.h"

#define BAD_COMMAND_COUNT	10


/*
**  An entry in the dispatch table.  The name, and implementing function,
**  of every command we support.
*/
typedef struct _NCDISPATCH {
    const char *             Name;
    innd_callback_nntp_func  Function;
    bool                     Needauth;
    int                      Minac;
    int                      Maxac;
    bool                     Stripspaces;
    const char *             Help;
} NCDISPATCH;

/* The functions that implement the various commands. */
static void NCauthinfo       (CHANNEL *cp);
static void NCcancel         (CHANNEL *cp);
static void NCcapabilities   (CHANNEL *cp);
static void NCcheck          (CHANNEL *cp);
static void NChead           (CHANNEL *cp);
static void NChelp           (CHANNEL *cp);
static void NCihave          (CHANNEL *cp);
static void NClist           (CHANNEL *cp);
static void NCmode           (CHANNEL *cp);
static void NCquit           (CHANNEL *cp);
static void NCstat           (CHANNEL *cp);
static void NCtakethis       (CHANNEL *cp);
static void NCxbatch         (CHANNEL *cp);

/* Handlers for unimplemented commands.  We need two handlers so that we can
   return the right status code; reader commands that are required by the
   standard must return a 401 response code rather than a 500 error. */
static void NC_reader        (CHANNEL *cp);
static void NC_unimp         (CHANNEL *cp);

/* Supporting functions. */
static void NCwritedone      (CHANNEL *cp);

/* Set up the dispatch table for all of the commands. */
#define NC_any -1
#define COMMAND(name, func, auth, min, max, strip, help) { name, func, auth, min, max, strip, help }
#define COMMAND_READER(name) { name, NC_reader, false, 1, NC_any, true, NULL }
#define COMMAND_UNIMP(name)  { name, NC_unimp,  false, 1, NC_any, true, NULL }
static NCDISPATCH NCcommands[] = {
    COMMAND("AUTHINFO",      NCauthinfo,      false, 3,  3, false,
            "USER name|PASS password"),
    COMMAND("CAPABILITIES",  NCcapabilities,  false, 1,  2, true,
            "[keyword]"),
    COMMAND("CHECK",         NCcheck,         true,  2,  2, false,
            "message-ID"),
    COMMAND("HEAD",          NChead,          true,  1,  2, true,
            "message-ID"),
    COMMAND("HELP",          NChelp,          false, 1,  1, true,
            NULL),
    COMMAND("IHAVE",         NCihave,         true,  2,  2, true,
            "message-ID"),
    COMMAND("LIST",          NClist,          true,  1,  3, true,
            "[ACTIVE [wildmat]|ACTIVE.TIMES [wildmat]|MOTD|NEWSGROUPS [wildmat]]"),
    COMMAND("MODE",          NCmode,          false, 2,  2, true,
            "READER"),
    COMMAND("QUIT",          NCquit,          false, 1,  1, true,
            NULL),
    COMMAND("STAT",          NCstat,          true,  1,  2, true,
            "message-ID"),
    COMMAND("TAKETHIS",      NCtakethis,      true,  2,  2, false,
            "message-ID"),
    COMMAND("XBATCH",        NCxbatch,        true,  2,  2, true,
            "size"),

    /* Unimplemented reader commands which may become available after a MODE
       READER command. */
    COMMAND_READER("ARTICLE"),
    COMMAND_READER("BODY"),
    COMMAND_READER("DATE"),
    COMMAND_READER("GROUP"),
    COMMAND_READER("HDR"),
    COMMAND_READER("LAST"),
    COMMAND_READER("LISTGROUP"),
    COMMAND_READER("NEWGROUPS"),
    COMMAND_READER("NEWNEWS"),
    COMMAND_READER("NEXT"),
    COMMAND_READER("OVER"),
    COMMAND_READER("POST"),
#ifdef HAVE_SSL
    COMMAND_READER("STARTTLS"),
#endif
    COMMAND_READER("XGTITLE"),
    COMMAND_READER("XHDR"),
    COMMAND_READER("XOVER"),
    COMMAND_READER("XPAT"),

    /* Other unimplemented standard commands.
       SLAVE (which was ill-defined in RFC 977) was removed from the NNTP
       protocol in RFC 3977. */
    COMMAND_UNIMP("SLAVE")
};
#undef COMMAND

/* Number of open connections. */
static unsigned long NCcount;

static char		*NCquietlist[] = { INND_QUIET_BADLIST };
static const char	NCterm[] = "\r\n";
static const char 	NCdot[] = "." ;

/*
** Clear the WIP entry for the given channel.
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
     * do, but that would be wrong if the channel is streaming (because it
     * would zap the channel's input buffer).  There's no harm in
     * never calling RCHANremove here.  */

    bp = &cp->Out;
    i = bp->left;
    WCHANappend(cp, text, strlen(text));	/* Text in buffer. */
    WCHANappend(cp, NCterm, strlen(NCterm));	/* Add CR LF to text. */

    if (i == 0) {	/* If only data, then try to write directly. */
	i = write(cp->fd, &bp->data[bp->used], bp->left);
	if (Tracing || cp->Tracing)
	    syslog(L_TRACE, "%s NCwritereply %d=write(%d, \"%.15s\", %lu)",
		CHANname(cp), i, cp->fd, &bp->data[bp->used],
		(unsigned long) bp->left);
	if (i > 0)
            bp->used += i;
	if (bp->used == bp->left) {
	    /* All the data was written. */
	    bp->used = bp->left = 0;
	    NCwritedone(cp);
	} else {
            bp->left -= i;
            i = 0;
        }
    } else i = 0;
    if (i <= 0) {	/* Write failed, queue it for later. */
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
    char buff[SMBUF];

    snprintf(buff, sizeof(buff), "%d %s%s", NNTP_FAIL_TERMINATING, text,
             NCterm);

    cp->State = CSwritegoodbye;
    RCHANremove(cp); /* We're not going to read anything more. */
    WCHANappend(cp, buff, strlen(buff));
    WCHANadd(cp);
}


/*
**  We have an entire article collected; try to post it.  If we're
**  not running, drop the article or just pause and reschedule.
*/
static void
NCpostit(CHANNEL *cp)
{
  const char	*response;
  char	buff[SMBUF];

  if (Mode == OMthrottled) {
    cp->Reported++;
    NCwriteshutdown(cp, ModeReason);
    return;
  } else if (Mode == OMpaused) {
    cp->Reported++;
    if (cp->Sendid.size > 3) {
      /* In streaming mode, there is no NNTP_FAIL_TAKETHIS_DEFER and RFC 4644
       * mentions that we MUST send 400 here and close the connection so
       * as not to reject the article.
       * Yet, we could have sent NNTP_FAIL_ACTION without closing the
       * connection... */
      cp->State = CSwritegoodbye;
      snprintf(buff, sizeof(buff), "%d %s", NNTP_FAIL_TERMINATING, ModeReason);
    } else {
      cp->State = CSgetcmd;
      snprintf(buff, sizeof(buff), "%d %s", NNTP_FAIL_IHAVE_DEFER, ModeReason);
    }
    NCwritereply(cp, buff);
    return;
  }

  /* Return an error without trying to post the article if the TAKETHIS
   * command was not correct in the first place (code which does not start
   * with a '2'). */
  if ((cp->Sendid.size > 3) && (cp->Sendid.data[0] != NNTP_CLASS_OK)) {
    cp->State = CSgetcmd;
    NCwritereply(cp, cp->Sendid.data);
    return;
  }

  /* Note that some use break, some use return here. */
  if (ARTpost(cp)) {
    cp->Received++;
    if (cp->Sendid.size > 3) { /* We are streaming. */
      cp->Takethis_Ok++;
      snprintf(buff, sizeof(buff), "%d", NNTP_OK_TAKETHIS);
      cp->Sendid.data[0] = buff[0];
      cp->Sendid.data[1] = buff[1];
      cp->Sendid.data[2] = buff[2];
      response = cp->Sendid.data;
    } else {
      snprintf(buff, sizeof(buff), "%d Article transferred OK", NNTP_OK_IHAVE);
      response = buff;
    }
  } else {
    /* The answer to TAKETHIS is a response code followed by a message-ID. */
    if (cp->Sendid.size > 3) {
      snprintf(buff, sizeof(buff), "%d", NNTP_FAIL_TAKETHIS_REJECT);
      cp->Sendid.data[0] = buff[0];
      cp->Sendid.data[1] = buff[1];
      cp->Sendid.data[2] = buff[2];
      response = cp->Sendid.data;
    } else {
      response = cp->Error;
    }
  }
  cp->Reported++;
  if (cp->Reported >= innconf->incominglogfrequency) {
    syslog(L_NOTICE,
           "%s checkpoint seconds %ld accepted %ld refused %ld rejected %ld duplicate %ld"
           " accepted size %.0f duplicate size %.0f rejected size %.0f",
           CHANname(cp),
           (long)(Now.tv_sec - cp->Started_checkpoint),
           cp->Received - cp->Received_checkpoint,
           cp->Refused - cp->Refused_checkpoint,
           cp->Rejected - cp->Rejected_checkpoint,
           cp->Duplicate - cp->Duplicate_checkpoint,
           cp->Size - cp->Size_checkpoint,
           cp->DuplicateSize - cp->DuplicateSize_checkpoint,
           cp->RejectSize - cp->RejectSize_checkpoint);
    cp->Reported = 0;
    cp->Started_checkpoint = Now.tv_sec;
    cp->Received_checkpoint = cp->Received;
    cp->Refused_checkpoint = cp->Refused;
    cp->Rejected_checkpoint = cp->Rejected;
    cp->Duplicate_checkpoint = cp->Duplicate;
    cp->Size_checkpoint = cp->Size;
    cp->DuplicateSize_checkpoint = cp->DuplicateSize;
    cp->RejectSize_checkpoint = cp->RejectSize;
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
    case CSgetheader:
    case CSgetbody:
    case CSgetxbatch:
    case CSgotlargearticle:
    case CScancel:
	RCHANadd(cp);
	break;
    }
}



/*
**  The HEAD command.
*/
static void
NChead(CHANNEL *cp)
{
    TOKEN		token;
    ARTHANDLE		*art;
    char                *buff = NULL;

    cp->Start = cp->Next;

    /* No argument given, or an article number. */
    if (cp->ac == 1 || IsValidArticleNumber(cp->av[1])) {
        xasprintf(&buff, "%d Not in a newsgroup", NNTP_FAIL_NO_GROUP);
        NCwritereply(cp, buff);
        free(buff);
        return;
    }

    if (!IsValidMessageID(cp->av[1], true)) {
        xasprintf(&buff, "%d Syntax error in message-ID", NNTP_ERR_SYNTAX);
        NCwritereply(cp, buff);
        free(buff);
        syslog(L_NOTICE, "%s bad_messageid %s", CHANname(cp),
               MaxLength(cp->av[1], cp->av[1]));
	return;
    }

    if (Mode == OMthrottled) {
        NCwriteshutdown(cp, ModeReason);
        return;
    } else if (Mode == OMpaused) {
        xasprintf(&buff, "%d %s", NNTP_FAIL_ACTION, ModeReason);
        NCwritereply(cp, buff);
        free(buff);
        return;
    }

    /* Get the article token and retrieve it (to make sure
     * the article is still here). */
    if (!HISlookup(History, cp->av[1], NULL, NULL, NULL, &token)) {
        xasprintf(&buff, "%d No such article", NNTP_FAIL_MSGID_NOTFOUND);
        NCwritereply(cp, buff);
        free(buff);
	return;
    }
    if ((art = SMretrieve(token, RETR_HEAD)) == NULL) {
        xasprintf(&buff, "%d No such article", NNTP_FAIL_MSGID_NOTFOUND);
        NCwritereply(cp, buff);
        free(buff);
	return;
    }

    /* Write it. */
    xasprintf(&buff, "%d 0 %s head%s", NNTP_OK_HEAD, cp->av[1], NCterm);
    WCHANappend(cp, buff, strlen(buff));
    WCHANappend(cp, art->data, art->len);

    /* Write the terminator. */
    NCwritereply(cp, NCdot);
    free(buff);
    SMfreearticle(art);
}


/*
**  The STAT command.
*/
static void
NCstat(CHANNEL *cp)
{
    TOKEN		token;
    ARTHANDLE		*art;
    char		*buff = NULL;

    cp->Start = cp->Next;

    /* No argument given, or an article number. */
    if (cp->ac == 1 || IsValidArticleNumber(cp->av[1])) {
        xasprintf(&buff, "%d Not in a newsgroup", NNTP_FAIL_NO_GROUP);
        NCwritereply(cp, buff);
        free(buff);
        return;
    }

    if (!IsValidMessageID(cp->av[1], true)) {
        xasprintf(&buff, "%d Syntax error in message-ID", NNTP_ERR_SYNTAX);
        NCwritereply(cp, buff);
        free(buff);
        syslog(L_NOTICE, "%s bad_messageid %s", CHANname(cp),
               MaxLength(cp->av[1], cp->av[1]));
	return;
    }

    if (Mode == OMthrottled) {
        NCwriteshutdown(cp, ModeReason);
        return;
    } else if (Mode == OMpaused) {
        xasprintf(&buff, "%d %s", NNTP_FAIL_ACTION, ModeReason);
        NCwritereply(cp, buff);
        free(buff);
        return;
    }

    /* Get the article token and retrieve it (to make sure
     * the article is still here). */
    if (!HISlookup(History, cp->av[1], NULL, NULL, NULL, &token)) {
        xasprintf(&buff, "%d No such article", NNTP_FAIL_MSGID_NOTFOUND);
        NCwritereply(cp, buff);
        free(buff);
	return;
    }
    if ((art = SMretrieve(token, RETR_STAT)) == NULL) {
        xasprintf(&buff, "%d No such article", NNTP_FAIL_MSGID_NOTFOUND);
        NCwritereply(cp, buff);
        free(buff);
	return;
    }
    SMfreearticle(art);

    /* Write the message. */
    xasprintf(&buff, "%d 0 %s status", NNTP_OK_STAT, cp->av[1]);
    NCwritereply(cp, buff);
    free(buff);
}


/*
**  The AUTHINFO command.
*/
static void
NCauthinfo(CHANNEL *cp)
{
    char *buff = NULL;
    cp->Start = cp->Next;

    /* Make sure we're getting only AUTHINFO USER/PASS commands. */
    if (strcasecmp(cp->av[1], "USER") != 0
        && strcasecmp(cp->av[1], "PASS") != 0) {
        xasprintf(&buff, "%d Bad AUTHINFO param", NNTP_ERR_SYNTAX);
        NCwritereply(cp, buff);
        free(buff);
        return;
    }

    if (cp->IsAuthenticated) {
        /* 502 if authentication will fail. */
        if (cp->CanAuthenticate)
            xasprintf(&buff, "%d Authentication will fail", NNTP_ERR_ACCESS);
        else
            xasprintf(&buff, "%d Already authenticated", NNTP_ERR_ACCESS);
        NCwritereply(cp, buff);
        free(buff);
        return;
    }

    /* Ignore AUTHINFO USER commands, since we only care about the
     * password. */
    if (strcasecmp(cp->av[1], "USER") == 0) {
        cp->HasSentUsername = true;
        xasprintf(&buff, "%d Enter password", NNTP_CONT_AUTHINFO);
        NCwritereply(cp, buff);
        free(buff);
	return;
    }

    /* AUTHINFO PASS cannot be sent before AUTHINFO USER. */
    if (!cp->HasSentUsername) {
        xasprintf(&buff, "%d Authentication commands issued out of sequence",
                  NNTP_FAIL_AUTHINFO_REJECT);
        NCwritereply(cp, buff);
        free(buff);
        return;
    }

    /* Got the password -- is it okay? */
    if (!RCauthorized(cp, cp->av[2])) {
        xasprintf(&buff, "%d Authentication failed", NNTP_FAIL_AUTHINFO_BAD);
    } else {
        xasprintf(&buff, "%d Authentication succeeded", NNTP_OK_AUTHINFO);
        cp->CanAuthenticate = false;
        cp->IsAuthenticated = true;
    }
    NCwritereply(cp, buff);
    free(buff);
}

/*
**  The HELP command.
**  As MODE STREAM is recognized, we still display MODE when
**  noreader is set to true or the server is paused or throttled
**  with readerswhenstopped set to false.
*/
static void
NChelp(CHANNEL *cp)
{
    static char		LINE1[] = "For more information, contact \"";
    static char		LINE2[] = "\" at this machine.";
    char                *buff = NULL;
    NCDISPATCH		*dp;

    cp->Start = cp->Next;

    xasprintf(&buff, "%d Legal commands%s", NNTP_INFO_HELP, NCterm);
    WCHANappend(cp, buff, strlen(buff));
    for (dp = NCcommands; dp < ARRAY_END(NCcommands); dp++)
	if (dp->Function != NC_unimp && dp->Function != NC_reader) {
            /* Ignore the streaming commands if necessary. */
            if ((!StreamingOff && cp->Streaming) ||
                (dp->Function != NCcheck && dp->Function != NCtakethis)) {
                WCHANappend(cp, "  ", 2);
                WCHANappend(cp, dp->Name, strlen(dp->Name));
                if (dp->Help != NULL) {
                    WCHANappend(cp, " ", 1);
                    WCHANappend(cp, dp->Help, strlen(dp->Help));
                }
                WCHANappend(cp, NCterm, strlen(NCterm));
            }
	}
    WCHANappend(cp, LINE1, strlen(LINE1));
    WCHANappend(cp, NEWSMASTER, strlen(NEWSMASTER));
    WCHANappend(cp, LINE2, strlen(LINE2));
    WCHANappend(cp, NCterm, strlen(NCterm));
    NCwritereply(cp, NCdot);
    free(buff);
}

/*
**  The CAPABILITIES command.
*/
static void
NCcapabilities(CHANNEL *cp)
{
    char *buff = NULL;

    cp->Start = cp->Next;

    if (cp->ac == 2 && !IsValidKeyword(cp->av[1])) {
        xasprintf(&buff, "%d Syntax error in keyword", NNTP_ERR_SYNTAX);
        NCwritereply(cp, buff);
        free(buff);
        return;
    }

    xasprintf(&buff, "%d Capability list:", NNTP_INFO_CAPABILITIES);
    NCwritereply(cp, buff);
    free(buff);

    WCHANappend(cp, "VERSION 2", 9);
    WCHANappend(cp, NCterm, strlen(NCterm));

    xasprintf(&buff, "IMPLEMENTATION %s", INN_VERSION_STRING);
    NCwritereply(cp, buff);
    free(buff);

    if (cp->CanAuthenticate) {
        WCHANappend(cp, "AUTHINFO", 8);
        if (!cp->IsAuthenticated)
            WCHANappend(cp, " USER", 5);
        WCHANappend(cp, NCterm, strlen(NCterm));
    }

    if (cp->IsAuthenticated) {
        WCHANappend(cp, "IHAVE", 5);
        WCHANappend(cp, NCterm, strlen(NCterm));
    }

    if (cp->IsAuthenticated && !cp->Nolist) {
        WCHANappend(cp, "LIST ACTIVE ACTIVE.TIMES MOTD NEWSGROUPS", 40);
        WCHANappend(cp, NCterm, strlen(NCterm));
    }

    if (cp->CanAuthenticate && !innconf->noreader
        && (NNRPReason == NULL || innconf->readerswhenstopped)) {
        WCHANappend(cp, "MODE-READER", 11);
        WCHANappend(cp, NCterm, strlen(NCterm));
    }

    if (cp->IsAuthenticated && !StreamingOff && cp->Streaming) {
        WCHANappend(cp, "STREAMING", 9);
        WCHANappend(cp, NCterm, strlen(NCterm));
    }

    NCwritereply(cp, NCdot);
}


/*
**  The IHAVE command.  Check the message-ID, and see if we want the
**  article or not.  Set the state appropriately.
*/
static void
NCihave(CHANNEL *cp)
{
    char        *buff = NULL;
#if defined(DO_PERL) || defined(DO_PYTHON)
    char	*filterrc;
    size_t	msglen;
#endif /*defined(DO_PERL) || defined(DO_PYTHON) */

    cp->Ihave++;
    cp->Start = cp->Next;

    if (!IsValidMessageID(cp->av[1], false)) {
        /* Return 435 here instead of 501 for compatibility reasons. */
        xasprintf(&buff, "%d Syntax error in message-ID", NNTP_FAIL_IHAVE_REFUSE);
        NCwritereply(cp, buff);
        free(buff);
        syslog(L_NOTICE, "%s bad_messageid %s", CHANname(cp),
               MaxLength(cp->av[1], cp->av[1]));
	return;
    }

    if (Mode == OMthrottled) {
        NCwriteshutdown(cp, ModeReason);
        return;
    } else if (Mode == OMpaused) {
        cp->Ihave_Deferred++;
        xasprintf(&buff, "%d %s", NNTP_FAIL_IHAVE_DEFER, ModeReason);
        NCwritereply(cp, buff);
        free(buff);
        return;
    }

    if ((innconf->refusecybercancels) && (strncmp(cp->av[1], "<cancel.", 8) == 0)) {
	cp->Refused++;
	cp->Ihave_Cybercan++;
        xasprintf(&buff, "%d Cyberspam cancel", NNTP_FAIL_IHAVE_REFUSE);
        NCwritereply(cp, buff);
        free(buff);
	return;
    }

#if defined(DO_PERL)
    /* Invoke a Perl message filter on the message-ID. */
    filterrc = PLmidfilter(cp->av[1]);
    if (filterrc) {
        cp->Refused++;
        msglen = strlen(cp->av[1]) + 5; /* 3 digits + space + id + null. */
        if (cp->Sendid.size < msglen) {
            if (cp->Sendid.size > 0)
                free(cp->Sendid.data);
            if (msglen > MED_BUFFER)
                cp->Sendid.size = msglen;
            else
                cp->Sendid.size = MED_BUFFER;
            cp->Sendid.data = xmalloc(cp->Sendid.size);
        }
        snprintf(cp->Sendid.data, cp->Sendid.size, "%d %.200s",
                 NNTP_FAIL_IHAVE_REFUSE, filterrc);
        NCwritereply(cp, cp->Sendid.data);
        free(cp->Sendid.data);
        cp->Sendid.size = 0;
        return;
    }
#endif

#if defined(DO_PYTHON)
    /* Invoke a Python message filter on the message-ID. */
    msglen = strlen(cp->av[1]);
    TMRstart(TMR_PYTHON);
    filterrc = PYmidfilter(cp->av[1], msglen);
    TMRstop(TMR_PYTHON);
    if (filterrc) {
        cp->Refused++;
        msglen += 5; /* 3 digits + space + id + null. */
        if (cp->Sendid.size < msglen) {
	    if (cp->Sendid.size > 0)
		free(cp->Sendid.data);
	    if (msglen > MED_BUFFER)
		cp->Sendid.size = msglen;
	    else
		cp->Sendid.size = MED_BUFFER;
	    cp->Sendid.data = xmalloc(cp->Sendid.size);
	}
	snprintf(cp->Sendid.data, cp->Sendid.size, "%d %.200s",
                 NNTP_FAIL_IHAVE_REFUSE, filterrc);
	NCwritereply(cp, cp->Sendid.data);
	free(cp->Sendid.data);
	cp->Sendid.size = 0;
	return;
    }
#endif

    if (HIScheck(History, cp->av[1]) || cp->Ignore) {
	cp->Refused++;
	cp->Ihave_Duplicate++;
        xasprintf(&buff, "%d Duplicate", NNTP_FAIL_IHAVE_REFUSE);
        NCwritereply(cp, buff);
        free(buff);
    } else if (WIPinprogress(cp->av[1], cp, false)) {
	cp->Ihave_Deferred++;
	if (cp->NoResendId) {
	    cp->Refused++;
            xasprintf(&buff, "%d Do not resend", NNTP_FAIL_IHAVE_REFUSE);
            NCwritereply(cp, buff);
            free(buff);
	} else {
            xasprintf(&buff, "%d Retry later", NNTP_FAIL_IHAVE_DEFER);
            NCwritereply(cp, buff);
            free(buff);
	}
    } else {
	if (cp->Sendid.size > 0) {
            free(cp->Sendid.data);
	    cp->Sendid.size = 0;
	}
	cp->Ihave_SendIt++;
        xasprintf(&buff, "%d Send it", NNTP_CONT_IHAVE);
        NCwritereply(cp, buff);
        free(buff);
	cp->ArtBeg = Now.tv_sec;
	cp->State = CSgetheader;
	ARTprepare(cp);
    }
}

/*
** The XBATCH command.  Set the state appropriately.
*/
static void
NCxbatch(CHANNEL *cp)
{
    char *buff = NULL;

    cp->Start = cp->Next;

    if (cp->XBatchSize) {
        syslog(L_FATAL, "NCxbatch(): oops, cp->XBatchSize already set to %d",
	       cp->XBatchSize);
    }

    cp->XBatchSize = atoi(cp->av[1]);
    if (Tracing || cp->Tracing)
        syslog(L_TRACE, "%s will read batch of size %d",
	       CHANname(cp), cp->XBatchSize);

    if (cp->XBatchSize <= 0 || ((innconf->maxartsize != 0) &&
                                (innconf->maxartsize < (unsigned long) cp->XBatchSize))) {
        syslog(L_NOTICE, "%s got bad xbatch size %d",
               CHANname(cp), cp->XBatchSize);
        xasprintf(&buff, "%d Invalid size for XBATCH", NNTP_ERR_SYNTAX);
        NCwritereply(cp, buff);
        free(buff);
        return;
    }

    /* We prefer not to touch the buffer; NCreader() does enough magic
     * with it. */
    cp->State = CSgetxbatch;
    xasprintf(&buff, "%d Send batch", NNTP_CONT_XBATCH);
    NCwritereply(cp, buff);
    free(buff);
}

/*
**  The LIST command.  Send relevant lines of required file.
*/
static void
NClist(CHANNEL *cp)
{
    QIOSTATE    *qp;
    char        *p, *path, *save;
    char        savec;
    char        *buff = NULL;
    bool        checkutf8 = false;

    cp->Start = cp->Next;

    if (cp->Nolist) {
        /* Even authenticated, a peer that has nolist: set will not
         * be able to use the LIST command. */
        if (!cp->CanAuthenticate || innconf->noreader
            || (NNRPReason != NULL && !innconf->readerswhenstopped))
            xasprintf(&buff, "%d Permission denied", NNTP_ERR_ACCESS);
        else
            xasprintf(&buff, "%d MODE-READER", NNTP_FAIL_WRONG_MODE);
        NCwritereply(cp, buff);
        free(buff);
	return;
    }

    /* ACTIVE when no argument given. */
    if (cp->ac == 1 || (strcasecmp(cp->av[1], "ACTIVE") == 0)) {
        path = concatpath(innconf->pathdb, INN_PATH_ACTIVE);
        qp = QIOopen(path);
        free(path);
        if (qp == NULL) {
            xasprintf(&buff, "%d No list of active newsgroups available",
                      NNTP_ERR_UNAVAILABLE);
            NCwritereply(cp, buff);
            free(buff);
            return;
        } else {
            xasprintf(&buff, "%d Newsgroups in form \"group high low status\"",
                      NNTP_OK_LIST);
            NCwritereply(cp, buff);
            free(buff);
        }
    } else if (strcasecmp(cp->av[1], "NEWSGROUPS") == 0) {
        path = concatpath(innconf->pathdb, INN_PATH_NEWSGROUPS);
	qp = QIOopen(path);
        free(path);
	if (qp == NULL) {
            xasprintf(&buff, "%d No list of newsgroup descriptions available",
                      NNTP_ERR_UNAVAILABLE);
            NCwritereply(cp, buff);
            free(buff);
	    return;
	} else {
            xasprintf(&buff, "%d Newsgroup descriptions in form \"group description\"",
                      NNTP_OK_LIST);
            NCwritereply(cp, buff);
            free(buff);
        }
    } else if (strcasecmp(cp->av[1], "ACTIVE.TIMES") == 0) {
        path = concatpath(innconf->pathdb, INN_PATH_ACTIVETIMES);
	qp = QIOopen(path);
        free(path);
	if (qp == NULL) {
            xasprintf(&buff, "%d No list of newsgroup creation times available",
                      NNTP_ERR_UNAVAILABLE);
            NCwritereply(cp, buff);
            free(buff);
	    return;
	} else {
            xasprintf(&buff, "%d Newsgroup creation times in form \"group time who\"",
                      NNTP_OK_LIST);
            NCwritereply(cp, buff);
            free(buff);
        }
    } else if (strcasecmp(cp->av[1], "MOTD") == 0) {
        checkutf8 = true;
        if (cp->ac > 2) {
            xasprintf(&buff, "%d Unexpected wildmat or argument",
                      NNTP_ERR_SYNTAX);
            NCwritereply(cp, buff);
            free(buff);
            return;
        }
        path = concatpath(innconf->pathetc, INN_PATH_MOTD_INND);
        qp = QIOopen(path);
        free(path);
        if (qp == NULL) {
            xasprintf(&buff, "%d No message of the day available",
                      NNTP_ERR_UNAVAILABLE);
            NCwritereply(cp, buff);
            free(buff);
            return;
        } else {
            xasprintf(&buff, "%d Message of the day text in UTF-8",
                      NNTP_OK_LIST);
            NCwritereply(cp, buff);
            free(buff);
        }
    } else {
        xasprintf(&buff, "%d Unknown LIST keyword", NNTP_ERR_SYNTAX);
        NCwritereply(cp, buff);
        free(buff);
	return;
    }

    /* Loop over all lines, sending the text and "\r\n". */
    while ((p = QIOread(qp)) != NULL) {
        /* Check that the output does not break the NNTP protocol. */
        if (p[0] == '.' && p[1] != '.') {
            syslog(L_ERROR, "%s NClist bad dot-stuffing in file %s",
                   CHANname(cp), cp->av[1]);
            continue;
        }

        if (checkutf8) {
            if (!is_valid_utf8(p)) {
                syslog(L_ERROR, "%s NClist bad encoding in %s (UTF-8 expected)",
                       CHANname(cp), cp->av[1]);
                continue;
            }
        }

        /* Check whether the newsgroup matches the wildmat pattern,
         * if given. */
        if (cp->ac == 3) {
            savec = '\0';
            for (save = p; *save != '\0'; save++) {
                if (*save == ' ' || *save == '\t') {
                    savec = *save;
                    *save = '\0';
                    break;
                }
            }

            if (!uwildmat(p, cp->av[2]))
                continue;
        
            if (savec != '\0')
                *save = savec;
        }

        /* Write the line. */
        WCHANappend(cp, p, strlen(p));
        WCHANappend(cp, NCterm, strlen(NCterm));
    }

    QIOclose(qp);

    /* Write the terminator. */
    NCwritereply(cp, NCdot);
}


/*
**  The MODE command.  Hand off the channel.
*/
static void
NCmode(CHANNEL *cp)
{
    char buff[SMBUF];
    HANDOFF h;

    cp->Start = cp->Next;

    if (strcasecmp(cp->av[1], "READER") == 0 && !innconf->noreader) {
        /* MODE READER. */
        syslog(L_NOTICE, "%s NCmode \"MODE READER\" received",
               CHANname(cp));
        if (!cp->CanAuthenticate) {
            /* AUTHINFO has already been successfully used. */
            snprintf(buff, sizeof(buff), "%d Already authenticated as a feeder",
                     NNTP_ERR_ACCESS);
            NCwritereply(cp, buff);
            return;
        }
        if (NNRPReason != NULL && !innconf->readerswhenstopped) {
            /* Server paused or throttled. */
            snprintf(buff, sizeof(buff), "%d %s", NNTP_FAIL_ACTION, NNRPReason);
            NCwritereply(cp, buff);
            return;
        } else {
            /* We will hand off the channel to nnrpd. */
            h = HOnnrpd;
        }
    } else if (strcasecmp(cp->av[1], "STREAM") == 0 &&
               (!StreamingOff && cp->Streaming)) {
        /* MODE STREAM. */
        snprintf(buff, sizeof(buff), "%d Streaming permitted", NNTP_OK_STREAM);
	NCwritereply(cp, buff);
	syslog(L_NOTICE, "%s NCmode \"MODE STREAM\" received",
               CHANname(cp));
        return;
    } else if (strcasecmp(cp->av[1], "CANCEL") == 0 && cp->privileged) {
        /* MODE CANCEL */
        cp->State = CScancel;
        snprintf(buff, sizeof(buff), "%d Cancels permitted", NNTP_OK_MODE_CANCEL);
        NCwritereply(cp, buff);
        syslog(L_NOTICE, "%s NCmode \"MODE CANCEL\" received",
                CHANname(cp));
        return;
    } else {
        /* Unknown MODE command or readers not allowed. */
        snprintf(buff, sizeof(buff), "%d Unknown MODE variant", NNTP_ERR_SYNTAX);
        NCwritereply(cp, buff);
        syslog(L_NOTICE, "%s bad_command MODE %s", CHANname(cp),
               MaxLength(cp->av[1], cp->av[1]));
        return;
    }

    /* Hand off if reached. */
    RChandoff(cp->fd, h);
    if (NCcount > 0)
        NCcount--;
    CHANclose(cp, CHANname(cp));
}


/*
**  The QUIT command.  Acknowledge, and set the state to closing down.
*/
static void
NCquit(CHANNEL *cp)
{
    char buff[SMBUF];

    cp->Start = cp->Next;

    snprintf(buff, sizeof(buff), "%d Bye!", NNTP_OK_QUIT);

    cp->State = CSwritegoodbye;
    NCwritereply(cp, buff);
}


/*
**  The catch-all for reader commands, which should return a different status
**  than just "unrecognized command" since a change of state may make them
**  available.
*/
static void
NC_reader(CHANNEL *cp)
{
    char buff[SMBUF];

    cp->Start = cp->Next;

    if (!cp->CanAuthenticate || innconf->noreader
     || (NNRPReason != NULL && !innconf->readerswhenstopped))
        snprintf(buff, sizeof(buff), "%d Permission denied",
                 NNTP_ERR_ACCESS);
    else
        snprintf(buff, sizeof(buff), "%d MODE-READER",
                 NNTP_FAIL_WRONG_MODE);
    NCwritereply(cp, buff);
}


/*
**  The catch-all for unimplemented commands.
*/
static void
NC_unimp(CHANNEL *cp)
{
    char buff[SMBUF];

    cp->Start = cp->Next;

    snprintf(buff, sizeof(buff), "%d \"%s\" not implemented; try \"HELP\"",
             NNTP_ERR_COMMAND, MaxLength(cp->av[0], cp->av[0]));
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
  char		buff[NNTP_MAXLEN_COMMAND];  /* For our (long) answers for CHECK/TAKETHIS,
                                             * we need at least the length of the command
                                             * (512 bytes). */
  size_t	i, j;
  bool		readmore, movedata;
  ARTDATA	*data = &cp->Data;
  HDRCONTENT    *hc = data->HdrContent;
  char          **v;
  bool          validcommandtoolong;
  int           syntaxerrorcode = NNTP_ERR_SYNTAX;

  readmore = movedata = false;
  if (Tracing || cp->Tracing)
    syslog(L_TRACE, "%s NCproc Used=%lu", CHANname(cp),
           (unsigned long) cp->In.used);

  bp = &cp->In;
  if (bp->used == 0)
    return;

  for ( ; ; ) {
    if (Tracing || cp->Tracing) {
      syslog(L_TRACE, "%s cp->Start=%lu cp->Next=%lu bp->Used=%lu",
        CHANname(cp), (unsigned long) cp->Start, (unsigned long) cp->Next,
        (unsigned long) bp->used);
      if (bp->used > 15)
	syslog(L_TRACE, "%s NCproc state=%d next \"%.15s\"", CHANname(cp),
	  cp->State, &bp->data[cp->Next]);
    }
    switch (cp->State) {
    default:
      syslog(L_ERROR, "%s internal NCproc state %d", CHANname(cp), cp->State);
      movedata = false;
      readmore = true;
      break;

    case CSwritegoodbye:
      movedata = false;
      readmore = true;
      break;

    case CSgetcmd:
    case CScancel:
      /* Did we get the whole command, terminated with "\r\n"? */
      for (i = cp->Next; (i < bp->used) && (bp->data[i] != '\n'); i++) ;
      if (i == bp->used) {
	/* Check for too long command. */
	if ((j = bp->used - cp->Start) > NNTP_MAXLEN_COMMAND) {
	  /* Make some room, saving only the last few bytes. */
	  for (p = bp->data, i = 0; i < SAVE_AMT; i++)
	    p[i] = p[bp->used - SAVE_AMT + i];
	  cp->LargeCmdSize += j - SAVE_AMT;
	  bp->used = cp->Next = SAVE_AMT;
	  bp->left = bp->size - SAVE_AMT;
	  cp->Start = 0;
	  cp->State = CSeatcommand;
	  /* Above means moving data already. */
	  movedata = false;
	} else {
	  cp->Next = bp->used;
	  /* Move data to the beginning anyway. */
	  movedata = true;
	}
	readmore = true;
	break;
      }
      /* i points where "\n" is; go forward. */
      cp->Next = ++i;

      /* Never move data so long as "\r\n" is found, since subsequent
       * data may also include a command line. */
      movedata = false;
      readmore = false;

      /* Ignore too small lines. */
      if (i - cp->Start < 3) {
        cp->Start = cp->Next;
	break;
      }

      p = &bp->data[i];
      q = &bp->data[cp->Start];

      /* Ignore blank lines. */
      if (*q == '\0' || i - cp->Start == 2) {
	cp->Start = cp->Next;
	break;
      }

      /* Guarantee null-termination.  Usually, "\r\n" is found at the end
       * of the command line.  In case we only have "\n", we also accept
       * the command. */
      if (p[-2] == '\r')
          p[-2] = '\0';
      else
          p[-1] = '\0';

      p = q;
      cp->ac = nArgify(p, &cp->av, 1);

      /* Ignore empty lines. */
      if (cp->ac == 0) {
        cp->Start = cp->Next;
        break;
      }

      if (Tracing || cp->Tracing)
	syslog(L_TRACE, "%s < %s", CHANname(cp), q);

      /* We got something -- stop sleeping (in case we were). */
      SCHANremove(cp);
      if (cp->Argument != NULL) {
	free(cp->Argument);
	cp->Argument = NULL;
      }

      /* When MODE CANCEL is used... */
      if (cp->State == CScancel) {
          NCcancel(cp);
          break;
      }

      /* If the line is too long, we have to make sure that
       * no recognized command has been sent. */
      validcommandtoolong = false;
      if (i - cp->Start > NNTP_MAXLEN_COMMAND) {
        for (dp = NCcommands; dp < ARRAY_END(NCcommands); dp++) {
          if ((dp->Function != NC_unimp) &&
              (strcasecmp(cp->av[0], dp->Name) == 0)) {
            if ((!StreamingOff && cp->Streaming) ||
                (dp->Function != NCcheck && dp->Function != NCtakethis)) {
                validcommandtoolong = true;
            }
            /* Return 435/438 instead of 501 to IHAVE/CHECK commands
             * for compatibility reasons. */
            if (strcasecmp(cp->av[0], "IHAVE") == 0) {
              syntaxerrorcode = NNTP_FAIL_IHAVE_REFUSE;
            } else if (strcasecmp(cp->av[0], "CHECK") == 0
                       && (!StreamingOff && cp->Streaming)) {
              syntaxerrorcode = NNTP_FAIL_CHECK_REFUSE;
            }
          }
        }
        /* If TAKETHIS, we have to read the entire multi-line response
         * block before answering. */
        if (strcasecmp(cp->av[0], "TAKETHIS") != 0
            || (StreamingOff || !cp->Streaming)) {
          if (syntaxerrorcode == NNTP_FAIL_CHECK_REFUSE) {
            snprintf(buff, sizeof(buff), "%d %s", syntaxerrorcode,
                     cp->ac > 1 ? cp->av[1] : "");
          } else {
            snprintf(buff, sizeof(buff), "%d Line too long",
                     validcommandtoolong ? syntaxerrorcode : NNTP_ERR_COMMAND);
          }
          NCwritereply(cp, buff);
          cp->Start = cp->Next;

          syslog(L_NOTICE, "%s bad_command %s", CHANname(cp),
                 MaxLength(q, q));
          break;
        }
      }

      /* Loop through the command table. */
      for (dp = NCcommands; dp < ARRAY_END(NCcommands); dp++) {
	if (strcasecmp(cp->av[0], dp->Name) == 0) {
          /* Return 435/438 instead of 501 to IHAVE/CHECK commands
           * for compatibility reasons. */
          if (strcasecmp(cp->av[0], "IHAVE") == 0) {
              syntaxerrorcode = NNTP_FAIL_IHAVE_REFUSE;
          } else if (strcasecmp(cp->av[0], "CHECK") == 0
                     && (!StreamingOff && cp->Streaming)) {
              syntaxerrorcode = NNTP_FAIL_CHECK_REFUSE;
          }
	  /* Ignore the streaming commands if necessary. */
	  if ((!StreamingOff && cp->Streaming) ||
	    (dp->Function != NCcheck && dp->Function != NCtakethis)) {
	    break;
	  }
	}
      }

      /* If no command has been recognized. */
      if (dp == ARRAY_END(NCcommands)) {
	if (++(cp->BadCommands) >= BAD_COMMAND_COUNT) {
          cp->State = CSwritegoodbye;
          snprintf(buff, sizeof(buff), "%d Too many unrecognized commands",
                   NNTP_FAIL_TERMINATING);
          NCwritereply(cp, buff);
          break;
        }
        snprintf(buff, sizeof(buff), "%d What?", NNTP_ERR_COMMAND);
	NCwritereply(cp, buff);
	cp->Start = cp->Next;

	/* Channel could have been freed by above NCwritereply if
	   we're writing-goodbye */
	if (cp->Type == CTfree)
	  return;
	for (i = 0; (p = NCquietlist[i]) != NULL; i++)
	  if (strcasecmp(p, q) == 0)
	    break;
	if (p == NULL)
	  syslog(L_NOTICE, "%s bad_command %s", CHANname(cp),
	    MaxLength(q, q));
        break;
      }

      /* Go on parsing the command.
       * For instance:
       * - "CHECK     <bad mid>  " will give the message-ID "<bad mid>  "
       *   with only leading whitespaces stripped.
       * - "AUTHINFO USER  test " will give the username " test " with
       *   no whitespaces stripped. */
      cp->ac--;
      cp->ac += reArgify(cp->av[cp->ac], &cp->av[cp->ac],
                     dp->Stripspaces ? -1 : dp->Minac - cp->ac - 1,
                     dp->Stripspaces);

      /* Check whether all arguments do not exceed their allowed size. */
      if (cp->ac > 1) {
          validcommandtoolong = false;
          for (v = cp->av; *v; v++)
              if (strlen(*v) > NNTP_MAXLEN_ARG) {
                  validcommandtoolong = true;
                  if (syntaxerrorcode == NNTP_FAIL_CHECK_REFUSE) {
                      snprintf(buff, sizeof(buff), "%d %s", syntaxerrorcode,
                               cp->ac > 1 ? cp->av[1] : "");
                  } else {
                      snprintf(buff, sizeof(buff), "%d Argument too long",
                               syntaxerrorcode);
                  }
                  break;
              }
          if (validcommandtoolong) {
              /* If TAKETHIS, we have to read the entire multi-line response
               * block before answering. */
              if (strcasecmp(cp->av[0], "TAKETHIS") != 0
                  || (StreamingOff || !cp->Streaming)) {
                  NCwritereply(cp, buff);
                  cp->Start = cp->Next;

                  syslog(L_NOTICE, "%s bad_command %s", CHANname(cp),
                         MaxLength(q, q));
                  break;
              }
          }
      }

      /* Check usage. */
      if ((dp->Minac != NC_any && cp->ac < dp->Minac)
          || (dp->Maxac != NC_any && cp->ac > dp->Maxac)) {
          if (syntaxerrorcode == NNTP_FAIL_CHECK_REFUSE) {
              snprintf(buff, sizeof(buff), "%d %s", syntaxerrorcode,
                       cp->ac > 1 ? cp->av[1] : "");
          } else {
              snprintf(buff, sizeof(buff), "%d Syntax is:  %s %s",
                       syntaxerrorcode, dp->Name, dp->Help ? dp->Help : "(no argument allowed)");
          }
          /* If TAKETHIS, we have to read the entire multi-line response
           * block before answering. */
          if (strcasecmp(cp->av[0], "TAKETHIS") != 0
              || (StreamingOff || !cp->Streaming)) {
              NCwritereply(cp, buff);
              cp->Start = cp->Next;

              syslog(L_NOTICE, "%s bad_command %s", CHANname(cp),
                     MaxLength(q, q));
              break;
          }
      }

      /* Check permissions and dispatch. */
      if (dp->Needauth && !cp->IsAuthenticated) {
          if (cp->CanAuthenticate) {
              snprintf(buff, sizeof(buff),
                       "%d Authentication required for command",
                       NNTP_FAIL_AUTH_NEEDED);
          } else {
              snprintf(buff, sizeof(buff),
                       "%d Access denied", NNTP_ERR_ACCESS);
          }
          /* If TAKETHIS, we have to read the entire multi-line response
           * block before answering. */
          if (strcasecmp(cp->av[0], "TAKETHIS") != 0
              || (StreamingOff || !cp->Streaming)) {
              NCwritereply(cp, buff);
              cp->Start = cp->Next;
              break;
          }
      }

      (*dp->Function)(cp);
      cp->BadCommands = 0;

      break;

    case CSgetheader:
    case CSgetbody:
    case CSeatarticle:
      TMRstart(TMR_ARTPARSE);
      ARTparse(cp);
      TMRstop(TMR_ARTPARSE);
      if (cp->State == CSgetbody || cp->State == CSgetheader ||
	      cp->State == CSeatarticle) {
        if (cp->Next - cp->Start > innconf->datamovethreshold
            || (innconf->maxartsize != 0 && cp->Size > innconf->maxartsize)) {
	  /* avoid buffer extention for ever */
	  movedata = true;
	} else {
	  movedata = false;
	}
	readmore = true;
	break;
      }

      /* If error is set, we're rejecting this article.  There is no need
       * to call ARTlog() as it has already been done during ARTparse(). */
      if (*cp->Error != '\0') {
        if (innconf->remembertrash && (Mode == OMrunning)
            && HDR_FOUND(HDR__MESSAGE_ID)) {
            HDR_LASTCHAR_SAVE(HDR__MESSAGE_ID);
            HDR_PARSE_START(HDR__MESSAGE_ID);
            /* The article posting time has not been parsed.  We cannot
             * give it to InndHisRemember. */
            if (!HIScheck(History, HDR(HDR__MESSAGE_ID))
                && !InndHisRemember(HDR(HDR__MESSAGE_ID), 0))
                syslog(L_ERROR, "%s cant write history %s %m",
                       LogName, HDR(HDR__MESSAGE_ID));
            HDR_PARSE_END(HDR__MESSAGE_ID);
        }
	ARTreject(REJECT_OTHER, cp);
	cp->State = CSgetcmd;
	cp->Start = cp->Next;
	NCclearwip(cp);
        /* The answer to TAKETHIS is a response code followed by a
         * message-ID. */
	if (cp->Sendid.size > 3) {
          /* Return the right response code if the TAKETHIS command
           * was not correct in the first place (code which does not
           * start with a '2').  Otherwise, change it to reject the
           * article. */
          if (cp->Sendid.data[0] == NNTP_CLASS_OK) {
            snprintf(buff, sizeof(buff), "%d", NNTP_FAIL_TAKETHIS_REJECT);
            cp->Sendid.data[0] = buff[0];
            cp->Sendid.data[1] = buff[1];
            cp->Sendid.data[2] = buff[2];
          }
	  NCwritereply(cp, cp->Sendid.data);
        } else {
	  NCwritereply(cp, cp->Error);
        }
	readmore = false;
	movedata = false;
	break;
      }
      /* fall thru */
    case CSgotarticle: /* in case caming back from pause */
      /* never move data so long as "\r\n.\r\n" is found, since subsequent data
	 may also include command line */
      readmore = false;
      movedata = false;
      if (Mode == OMthrottled) {
        /* We do not remember the message-ID of this article because it has not
         * been stored. */
        ARTreject(REJECT_OTHER, cp);
        ARTlogreject(cp, ModeReason);
	/* Clear the work-in-progress entry. */
	NCclearwip(cp);
	NCwriteshutdown(cp, ModeReason);
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
	syslog(L_NOTICE, "%s internal rejecting too long command line (%lu > %d)",
	  CHANname(cp), (unsigned long) i, NNTP_MAXLEN_COMMAND);
	cp->LargeCmdSize = 0;
        /* Command eaten; we do not know whether it was valid (500 or 501). */
	snprintf(buff, sizeof(buff), "%d command exceeds limit of %d bytes",
                 NNTP_ERR_COMMAND, NNTP_MAXLEN_COMMAND);
	cp->State = CSgetcmd;
	cp->Start = cp->Next;
	NCwritereply(cp, buff);
        readmore = false;
        movedata = false;
      } else {
	cp->LargeCmdSize += bp->used - cp->Next;
	bp->used = cp->Next = SAVE_AMT;
	bp->left = bp->size - SAVE_AMT;
	cp->Start = 0;
        readmore = true;
        movedata = false;
      }
      break;

    case CSgetxbatch:
      /* if the batch is complete, write it out into the in.coming
       * directory with an unique timestamp, and start rnews on it.
       */
      if (Tracing || cp->Tracing)
	syslog(L_TRACE, "%s CSgetxbatch: now %lu of %d bytes", CHANname(cp),
	  (unsigned long) bp->used, cp->XBatchSize);

      if (cp->Next != 0) {
	/* data must start from the beginning of the buffer */
        movedata = true;
	readmore = false;
	break;
      }
      if (bp->used < (size_t) cp->XBatchSize) {
	movedata = false;
	readmore = true;
	break;	/* give us more data */
      }
      movedata = false;
      readmore = false;

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
          snprintf(buff, sizeof(buff), "%d XBATCH failed -- cant create file: %s",
                   NNTP_FAIL_XBATCH, strerror(oerrno));
	  NCwritereply(cp, buff);
	} else {
	  if (write(fd, cp->In.data, cp->XBatchSize) != cp->XBatchSize) {
	    oerrno = errno;
	    syslog(L_ERROR, "%s cant write batch to file %s: %m", CHANname(cp),
	      buff);
            snprintf(buff, sizeof(buff), "%d XBATCH failed -- cant write batch to file: %s",
                     NNTP_FAIL_XBATCH, strerror(oerrno));
	    NCwritereply(cp, buff);
	    failed = 1;
	  }
	}
	if (fd >= 0 && close(fd) != 0) {
	  oerrno = errno;
	  syslog(L_ERROR, "%s error closing batch file %s: %m", CHANname(cp),
	    failed ? "" : buff);
          snprintf(buff, sizeof(buff), "%d XBATCH failed -- error closing batch file: %s",
                   NNTP_FAIL_XBATCH, strerror(oerrno));
	  NCwritereply(cp, buff);
	  failed = 1;
	}
	snprintf(buff2, sizeof(buff2), "%s/%ld%d.x", innconf->pathincoming,
                 now, cp->fd);
	if (rename(buff, buff2)) {
	  oerrno = errno;
	  syslog(L_ERROR, "%s cant rename %s to %s: %m", CHANname(cp),
	    failed ? "" : buff, buff2);
          snprintf(buff, sizeof(buff), "%d XBATCH failed -- cant rename batch to %s: %s",
                   NNTP_FAIL_XBATCH, buff2, strerror(oerrno));
	  NCwritereply(cp, buff);
	  failed = 1;
	}
	cp->Reported++;
	if (!failed) {
          snprintf(buff, sizeof(buff), "%d Batch transferred OK",
                   NNTP_OK_XBATCH);
          NCwritereply(cp, buff);
	  cp->Received++;
	} else {
          /* Only reject, no call to ARTlog() because it will not be
           * useful for XBATCH -- errors were logged to news.err. */
          ARTreject(REJECT_OTHER, cp);
        }
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
      syslog(L_TRACE, "%s NCproc state=%d Start=%lu Next=%lu Used=%lu",
	CHANname(cp), cp->State, (unsigned long) cp->Start,
        (unsigned long) cp->Next, (unsigned long) bp->used);

    if (movedata) { /* move data rather than extend buffer */
      TMRstart(TMR_DATAMOVE);
      movedata = false;
      if (cp->Start > 0)
	memmove(bp->data, &bp->data[cp->Start], bp->used - cp->Start);
      bp->used -= cp->Start;
      bp->left += cp->Start;
      cp->Next -= cp->Start;
      if (cp->State == CSgetheader || cp->State == CSgetbody ||
	cp->State == CSeatarticle) {
	/* adjust offset only in CSgetheader, CSgetbody or CSeatarticle */
	data->CurHeader -= cp->Start;
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
	syslog(L_TRACE, "%s NCreader Used=%lu",
	    CHANname(cp), (unsigned long) cp->In.used);

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

    /* Set the greeting message.  We always send 200 because we cannot know
     * for sure that the client will not be able to use POST during its
     * entire session. */
    p = innconf->pathhost;
    if (p == NULL)
	/* Worked in main, now it fails?  Curious. */
	p = Path.data;
    snprintf(buff, sizeof(buff), "%d %s InterNetNews server %s ready (transit mode)",
             NNTP_OK_BANNER_POST, p, INN_VERSION_STRING);
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
    cp = CHANcreate(fd, CTnntp, CSgetcmd, NCreader, NCwritedone);

    cp->IsAuthenticated = !MustAuthorize;
    cp->HasSentUsername = false;

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
    if (!IsLocal && innconf->maxconnections != 0 &&
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



/*
**  These modules support the streaming option to tranfer articles
**  faster.
*/

/*
**  The CHECK command.  Check the message-ID, and see if we want the
**  article or not.  Stay in command state.
*/
static void
NCcheck(CHANNEL *cp)
{
    char                *buff = NULL;
    size_t		idlen, msglen;
#if defined(DO_PERL) || defined(DO_PYTHON)
    char		*filterrc;
#endif /* DO_PERL || DO_PYTHON */

    cp->Check++;
    cp->Start = cp->Next;

    idlen = strlen(cp->av[1]);
    msglen = idlen + 5; /* 3 digits + space + id + null. */
    if (cp->Sendid.size < msglen) {
        if (cp->Sendid.size > 0)
            free(cp->Sendid.data);
        if (msglen > MED_BUFFER)
            cp->Sendid.size = msglen;
        else
            cp->Sendid.size = MED_BUFFER;
        cp->Sendid.data = xmalloc(cp->Sendid.size);
    }
    if (!IsValidMessageID(cp->av[1], false)) {
	snprintf(cp->Sendid.data, cp->Sendid.size, "%d %s",
                 NNTP_FAIL_CHECK_REFUSE, cp->av[1]);
	NCwritereply(cp, cp->Sendid.data);
	syslog(L_NOTICE, "%s bad_messageid %s", CHANname(cp),
               MaxLength(cp->av[1], cp->av[1]));
	return;
    }

    if ((innconf->refusecybercancels) && (strncmp(cp->av[1], "<cancel.", 8) == 0)) {
	cp->Refused++;
	cp->Check_cybercan++;
	snprintf(cp->Sendid.data, cp->Sendid.size, "%d %s",
                 NNTP_FAIL_CHECK_REFUSE, cp->av[1]);
	NCwritereply(cp, cp->Sendid.data);
	return;
    }

    if (Mode == OMthrottled) {
        NCwriteshutdown(cp, ModeReason);
        return;
    } else if (Mode == OMpaused) {
        cp->Check_deferred++;
        xasprintf(&buff, "%d %s %s", NNTP_FAIL_CHECK_DEFER, cp->av[1],
                  ModeReason);
        NCwritereply(cp, buff);
        free(buff);
        return;
    }

#if defined(DO_PERL)
    /* Invoke a Perl message filter on the message-ID. */
    filterrc = PLmidfilter(cp->av[1]);
    if (filterrc) {
	cp->Refused++;
	snprintf(cp->Sendid.data, cp->Sendid.size, "%d %s",
                 NNTP_FAIL_CHECK_REFUSE, cp->av[1]);
	NCwritereply(cp, cp->Sendid.data);
	return;
    }
#endif /* defined(DO_PERL) */

#if defined(DO_PYTHON)
    /* Invoke a Python message filter on the message-ID. */
    filterrc = PYmidfilter(cp->av[1], idlen);
    if (filterrc) {
	cp->Refused++;
	snprintf(cp->Sendid.data, cp->Sendid.size, "%d %s",
                 NNTP_FAIL_CHECK_REFUSE, cp->av[1]);
	NCwritereply(cp, cp->Sendid.data);
	return;
    }
#endif /* defined(DO_PYTHON) */

    if (HIScheck(History, cp->av[1]) || cp->Ignore) {
	cp->Refused++;
	cp->Check_got++;
	snprintf(cp->Sendid.data, cp->Sendid.size, "%d %s",
                 NNTP_FAIL_CHECK_REFUSE, cp->av[1]);
	NCwritereply(cp, cp->Sendid.data);
    } else if (WIPinprogress(cp->av[1], cp, true)) {
	cp->Check_deferred++;
	if (cp->NoResendId) {
	    cp->Refused++;
	    snprintf(cp->Sendid.data, cp->Sendid.size, "%d %s",
                     NNTP_FAIL_CHECK_REFUSE, cp->av[1]);
	} else {
	    snprintf(cp->Sendid.data, cp->Sendid.size, "%d %s",
                     NNTP_FAIL_CHECK_DEFER, cp->av[1]);
	}
	NCwritereply(cp, cp->Sendid.data);
    } else {
	cp->Check_send++;
	snprintf(cp->Sendid.data, cp->Sendid.size, "%d %s",
                 NNTP_OK_CHECK, cp->av[1]);
	NCwritereply(cp, cp->Sendid.data);
    }
    /* Stay in command mode. */
}

/*
**  The TAKETHIS command.  Article follows.
**  Remember <id> for later ack.
*/
static void
NCtakethis(CHANNEL *cp)
{
    char    *mid;
    static char empty[] = "";
    int     returncode; /* Will *not* be changed in NCpostit()
                           if it does *not* start with '2'. */
    size_t  idlen, msglen;
    WIP     *wp;
#if defined(DO_PERL) || defined(DO_PYTHON)
    char    *filterrc;
#endif /* DO_PERL || DO_PYTHON */

    cp->Takethis++;
    cp->Start = cp->Next;

    /* Check the syntax and authentication here because
     * it is not done before (TAKETHIS has to eat
     * the whole multi-line block before responding). */
    if (cp->ac == 1) {
        mid = empty;
        returncode = NNTP_FAIL_TAKETHIS_REJECT;
    } else {
        mid = cp->av[1];
        returncode = NNTP_OK_TAKETHIS; /* Default code. */
    }

    idlen = strlen(mid);
    msglen = idlen + 5; /* 3 digits + space + id + null. */

    if (!IsValidMessageID(mid, false)) {
        syslog(L_NOTICE, "%s bad_messageid %s", CHANname(cp),
               MaxLength(mid, mid));
        returncode = NNTP_FAIL_TAKETHIS_REJECT;
    } else {
#if defined(DO_PERL)
        /* Invoke a Perl message filter on the message-ID. */
        filterrc = PLmidfilter(mid);
        if (filterrc) {
            returncode = NNTP_FAIL_TAKETHIS_REJECT;
        }
#endif /* defined(DO_PERL) */

#if defined(DO_PYTHON)
        /* Invoke a Python message filter on the message-ID. */
        filterrc = PYmidfilter(mid, idlen);
        if (filterrc) {
            returncode = NNTP_FAIL_TAKETHIS_REJECT;
        }
#endif /* defined(DO_PYTHON) */
    }

    /* Check authentication after everything else. */
    if (!cp->IsAuthenticated) {
        returncode = cp->CanAuthenticate ?
            NNTP_FAIL_AUTH_NEEDED : NNTP_ERR_ACCESS;
    }

    if (cp->Sendid.size < msglen) {
        if (cp->Sendid.size > 0)
            free(cp->Sendid.data);
        if (msglen > MED_BUFFER)
            cp->Sendid.size = msglen;
        else
            cp->Sendid.size = MED_BUFFER;
        cp->Sendid.data = xmalloc(cp->Sendid.size);
    }
    /* Save ID for later NACK or ACK. */
    snprintf(cp->Sendid.data, cp->Sendid.size, "%d %s",
             returncode, mid);

    cp->ArtBeg = Now.tv_sec;
    cp->State = CSgetheader;
    ARTprepare(cp);
    /* Set WIP for benefit of later code in NCreader. */
    if ((wp = WIPbyid(mid)) == (WIP *)NULL)
	wp = WIPnew(mid, cp);
    cp->CurrentMessageIDHash = wp->MessageID;
}

/*
**  Process a cancel ID from a MODE CANCEL channel.
*/
static void
NCcancel(CHANNEL *cp)
{
    char *argv[2] = { NULL, NULL };
    char buff[SMBUF];
    const char *res;

    ++cp->Received;
    argv[0] = cp->In.data + cp->Start;
    cp->Start = cp->Next;
    res = CCcancel(argv);
    if (res) {
        snprintf(buff, sizeof(buff), "%d %s", NNTP_FAIL_CANCEL,
                 MaxLength(res, res));
        syslog(L_NOTICE, "%s cant_cancel %s", CHANname(cp),
               MaxLength(res, res));
        NCwritereply(cp, buff);
    } else {
        snprintf(buff, sizeof(buff), "%d Article cancelled OK",
                 NNTP_OK_CANCEL);
        NCwritereply(cp, buff);
    }
}

/*  $Id$
**
**  Routines for the NNTP channel.  Other channels get the descriptors which
**  we turn into NNTP channels, and over which we speak NNTP.
*/
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "configdata.h"
#include "clibrary.h"
#include "innd.h"
#include "dbz.h"
#include "qio.h"

#define BAD_COMMAND_COUNT	10
#define SAVE_AMT		10
#define ART_EOF(c, s)		\
    ((c) >= 5 && (s)[-5] == '\r' && (s)[-4] == '\n' && (s)[-3] == '.' \
     && (s)[-2] == '\r' && (s)[-1] == '\n')


/*
**  An entry in the dispatch table.  The name, and implementing function,
**  of every command we support.
*/
typedef struct _NCDISPATCH {
    STRING	Name;
    FUNCPTR	Function;
    int		Size;
} NCDISPATCH;

static FUNCTYPE	NCauthinfo();
static FUNCTYPE	NChead();
static FUNCTYPE	NChelp();
static FUNCTYPE	NCihave();
static FUNCTYPE	NClist();
static FUNCTYPE	NCmode();
static FUNCTYPE	NCquit();
static FUNCTYPE	NCstat();
static FUNCTYPE	NCxpath();
static FUNCTYPE	NC_unimp();
/* new modules for streaming */
static FUNCTYPE	NCxbatch();
static FUNCTYPE	NCcheck();
static FUNCTYPE	NCtakethis();
static FUNCTYPE NCwritedone();

STATIC int		NCcount;	/* Number of open connections	*/
STATIC NCDISPATCH	NCcommands[] = {
#if	0
    {	"article",	NCarticle },
#else
    {	"article",	NC_unimp },
#endif	/* 0 */
    {	"authinfo",	NCauthinfo },
    {	"help",		NChelp	},
    {	"ihave",	NCihave	},
    {	"check",	NCcheck	},
    {	"takethis",	NCtakethis },
    {	"list",		NClist	},
    {	"mode",		NCmode	},
    {	"quit",		NCquit	},
    {	"head",		NChead	},
    {	"stat",		NCstat	},
    {	"body",		NC_unimp },
    {	"group",	NC_unimp },
    {	"last",		NC_unimp },
    {	"newgroups",	NC_unimp },
    {	"newnews",	NC_unimp },
    {	"next",		NC_unimp },
    {	"post",		NC_unimp },
    {	"slave",	NC_unimp },
    {	"xbatch",	NCxbatch },
    {	"xhdr",		NC_unimp },
    {	"xpath",	NCxpath	},
};
STATIC char		*NCquietlist[] = {
    INND_QUIET_BADLIST
};
STATIC char		NCterm[] = "\r\n";
STATIC char 		NCdot[] = "." ;
STATIC char		NCbadcommand[] = NNTP_BAD_COMMAND;

/*
** Clear the WIP entry for the given channel
*/
void NCclearwip(CHANNEL *cp) {
    WIPfree(WIPbyhash(cp->CurrentMessageIDHash));
    HashClear(&cp->CurrentMessageIDHash);
    cp->ArtBeg = 0;
}

/*
**  Write an NNTP reply message.
**
**  Tries to do the actual write immediately if it will not block
**  and if there is not already other buffered output.  Then, if the
**  write is successful, calls NCwritedone (which does whatever is
**  necessary to accommodate state changes).  Else, NCwritedone will
**  be called from the main select loop later.
**
**  If the reply that we are writing now is associated with a
**  state change, then cp->State must be set to its new value
**  *before* NCwritereply is called.
*/
void
NCwritereply(CHANNEL *cp, char *text)
{
    BUFFER	*bp;
    int		i;

    /* XXX could do RCHANremove(cp) here, as the old NCwritetext() used to
     * do, but that would be wrong if the channel is sreaming (because it
     * would zap the channell's input buffer).  There's no harm in
     * never calling RCHANremove here.  */

    bp = &cp->Out;
    i = bp->Left;
    WCHANappend(cp, text, (int)strlen(text));	/* text in buffer */
    WCHANappend(cp, NCterm, STRLEN(NCterm));	/* add CR NL to text */
    if (i == 0) {	/* if only data then try to write directly */
	i = write(cp->fd, &bp->Data[bp->Used], bp->Left);
	if (Tracing || cp->Tracing)
	    syslog(L_TRACE, "%s NCwritereply %d=write(%d, \"%.15s\", %d)",
		CHANname(cp), i, cp->fd,  &bp->Data[bp->Used], bp->Left);
	if (i > 0) bp->Used += i;
	if (bp->Used == bp->Left) {
	    /* all the data was written */
	    bp->Used = bp->Left = 0;
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
NCwriteshutdown(CHANNEL *cp, char *text)
{
    cp->State = CSwritegoodbye;
    RCHANremove(cp); /* we're not going to read anything more */
#if 0
    /* XXX why would we want to zap whatever was already
     * in the output buffer? */
    WCHANset(cp, NNTP_GOODBYE, STRLEN(NNTP_GOODBYE));
#else
    WCHANappend(cp, NNTP_GOODBYE, STRLEN(NNTP_GOODBYE));
#endif
    WCHANappend(cp, " ", 1);
    WCHANappend(cp, text, (int)strlen(text));
    WCHANappend(cp, NCterm, STRLEN(NCterm));
    WCHANadd(cp);
}


/*
**  If a Message-ID is bad, write a reject message and return TRUE.
*/
STATIC BOOL
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
STATIC void
NCpostit(CHANNEL *cp)
{
    STRING		response;

    /* Note that some use break, some use return here. */
    switch (Mode) {
    default:
	syslog(L_ERROR, "%s internal NCpostit mode %d", CHANname(cp), Mode);
	return;
    case OMpaused:
	SCHANadd(cp, (time_t)(Now.time + innconf->pauseretrytime),
			(POINTER)&Mode, NCpostit, (POINTER)NULL);
	return;
    case OMrunning:
	response = ARTpost(cp);
	if (atoi(response) == NNTP_TOOKIT_VAL) {
	    cp->Received++;
	    if (cp->Sendid.Size > 3) { /* We be streaming */
		char buff[4];
		cp->Takethis_Ok++;
		(void)sprintf(buff, "%d", NNTP_OK_RECID_VAL);
		cp->Sendid.Data[0] = buff[0];
		cp->Sendid.Data[1] = buff[1];
		cp->Sendid.Data[2] = buff[2];
		response = cp->Sendid.Data;
	    }
	} else {
            cp->Rejected++;
	    if (cp->Sendid.Size) response = cp->Sendid.Data;
        }
	cp->Reported++;
	if (cp->Reported >= innconf->nntpactsync) {
	    syslog(L_NOTICE,
	    "%s checkpoint seconds %ld accepted %ld refused %ld rejected %ld duplicate %ld accepted size %.0f duplicate size %.0f",
		CHANname(cp), (long)(Now.time - cp->Started),
		cp->Received, cp->Refused, cp->Rejected,
		cp->Duplicate, cp->Size, cp->DuplicateSize);
	    cp->Reported = 0;
	}
	if (Mode == OMthrottled) {
	    NCwriteshutdown(cp, ModeReason);
	    break;
	}
	cp->State = CSgetcmd;
	NCwritereply(cp, (char *)response);
	break;

    case OMthrottled:
	NCwriteshutdown(cp, ModeReason);
	cp->Rejected++;
	break;
    }

    /* Clear the work-in-progress entry. */
    NCclearwip(cp);
}


/*
**  Write-done function.  Close down or set state for what we expect to
**  read next.
*/
STATIC FUNCTYPE
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
    case CSgetarticle:
    case CSgetxbatch:
	RCHANadd(cp);
	break;
    }
}



/*
**  The "head" command.
*/
STATIC FUNCTYPE NChead(CHANNEL *cp)
{
    char	        *p;
    TOKEN		*token;
    ARTHANDLE		*art;

    /* Snip off the Message-ID. */
    for (p = cp->In.Data + STRLEN("head"); ISWHITE(*p); p++)
	continue;
    if (NCbadid(cp, p))
	return;

    /* Get the article filenames; open the first file */
    if ((token = HISfilesfor(HashMessageID(p))) == NULL) {
	NCwritereply(cp, NNTP_DONTHAVEIT);
	return;
    }
    if ((art = SMretrieve(*token, RETR_HEAD)) == NULL) {
	NCwritereply(cp, NNTP_DONTHAVEIT);
	return;
    }

    /* Write it. */
    WCHANappend(cp, NNTP_HEAD_FOLLOWS, STRLEN(NNTP_HEAD_FOLLOWS));
    WCHANappend(cp, NCterm, STRLEN(NCterm));
    WCHANappend(cp, art->data, art->len);

    /* Write the terminator. */
    NCwritereply(cp, NCdot);
    SMfreearticle(art);
}


/*
**  The "stat" command.
*/
STATIC FUNCTYPE NCstat(CHANNEL *cp)
{
    char	        *p;
    TOKEN		*token;
    ARTHANDLE		*art;
    char		*buff;

    /* Snip off the Message-ID. */
    for (p = cp->In.Data + STRLEN("stat"); ISWHITE(*p); p++)
	continue;
    if (NCbadid(cp, p))
	return;

    /* Get the article filenames; open the first file (to make sure
     * the article is still here). */
    if ((token = HISfilesfor(HashMessageID(p))) == NULL) {
	NCwritereply(cp, NNTP_DONTHAVEIT);
	return;
    }
    if ((art = SMretrieve(*token, RETR_STAT)) == NULL) {
	NCwritereply(cp, NNTP_DONTHAVEIT);
	return;
    }
    SMfreearticle(art);

    /* Write the message. */
    buff = NEW(char, strlen(p) + 16);
    (void)sprintf(buff, "%d 0 %s", NNTP_NOTHING_FOLLOWS_VAL, p);
    NCwritereply(cp, buff);
    DISPOSE(buff);
}


/*
**  The "authinfo" command.  Actually, we come in here whenever the
**  channel is in CSgetauth state and we just got a command.
*/
STATIC FUNCTYPE
NCauthinfo(CHANNEL *cp)
{
    static char		AUTHINFO[] = "authinfo ";
    static char		PASS[] = "pass ";
    static char		USER[] = "user ";
    char		*p;

    p = cp->In.Data;

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
    }
    else {
	cp->State = CSgetcmd;
	NCwritereply(cp, NNTP_AUTH_OK);
    }
}

/*
**  The "help" command.
*/
STATIC FUNCTYPE
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
}

/*
**  The "ihave" command.  Check the Message-ID, and see if we want the
**  article or not.  Set the state appropriately.
*/
STATIC FUNCTYPE
NCihave(CHANNEL *cp)
{
    char	*p;
#if defined(DO_PERL) || defined(DO_PYTHON)
    char	*filterrc;
    int		msglen;
#endif /*defined(DO_PERL) || defined(DO_PYTHON) */

    cp->Ihave++;
    /* Snip off the Message-ID. */
    for (p = cp->In.Data + STRLEN("ihave"); ISWHITE(*p); p++)
	continue;
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
        if (cp->Sendid.Size < msglen) {
            if (cp->Sendid.Size > 0) DISPOSE(cp->Sendid.Data);
            if (msglen > MAXHEADERSIZE)
                cp->Sendid.Size = msglen;
            else
                cp->Sendid.Size = MAXHEADERSIZE;
            cp->Sendid.Data = NEW(char, cp->Sendid.Size);
        }
        sprintf(cp->Sendid.Data, "%d %.200s", NNTP_HAVEIT_VAL, filterrc);
        NCwritereply(cp, cp->Sendid.Data);
        DISPOSE(cp->Sendid.Data);
        cp->Sendid.Size = 0;
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
	if (cp->Sendid.Size < msglen) {
	    if (cp->Sendid.Size > 0)
		DISPOSE(cp->Sendid.Data);
	    if (msglen > MAXHEADERSIZE)
		cp->Sendid.Size = msglen;
	    else
		cp->Sendid.Size = MAXHEADERSIZE;
	    cp->Sendid.Data = NEW(char, cp->Sendid.Size);
	}
	sprintf(cp->Sendid.Data, "%d %.200s", NNTP_HAVEIT_VAL, filterrc);
	NCwritereply(cp, cp->Sendid.Data);
	DISPOSE(cp->Sendid.Data);
	cp->Sendid.Size = 0;
	return;
    }
#endif

    if (HIShavearticle(HashMessageID(p))) {
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
	cp->Ihave_SendIt++;
	NCwritereply(cp, NNTP_SENDIT);
	cp->ArtBeg = Now.time;
	cp->State = CSgetarticle;
    }
}

/* 
** The "xbatch" command. Set the state appropriately.
*/

STATIC FUNCTYPE
NCxbatch(CHANNEL *cp)
{
    char	*p;

    /* Snip off the batch size */
    for (p = cp->In.Data + STRLEN("xbatch"); ISWHITE(*p); p++)
	continue;

    if (cp->XBatchSize) {
        syslog(L_FATAL, "NCxbatch(): oops, cp->XBatchSize already set to %ld",
	       cp->XBatchSize);
    }

    cp->XBatchSize = atoi(p);
    if (Tracing || cp->Tracing)
        syslog(L_TRACE, "%s will read batch of size %ld",
	       CHANname(cp), cp->XBatchSize);

    if (cp->XBatchSize <= 0 || ((innconf->maxartsize != 0) && (innconf->maxartsize < cp->XBatchSize))) {
        syslog(L_NOTICE, "%s got bad xbatch size %ld",
	       CHANname(cp), cp->XBatchSize);
	NCwritereply(cp, NNTP_XBATCH_BADSIZE);
	return;
    }

#if 1
    /* we prefer not to touch the buffer, NCreader() does enough magic
     * with it
     */
#else
    /* We already know how much data will arrive, so we can allocate
     * sufficient buffer space for the batch in advance. We allocate
     * LOW_WATER + 1 more than needed, so that CHANreadtext() will never
     * reallocate it.
     */
    if (cp->In.Size < cp->XBatchSize + LOW_WATER + 1) {
        DISPOSE(cp->In.Data);
	cp->In.Left = cp->In.Size = cp->XBatchSize + LOW_WATER + 1;
	cp->In.Used = 0;
	cp->In.Data = NEW(char, cp->In.Size);
    }
#endif
    cp->State = CSgetxbatch;
    NCwritereply(cp, NNTP_CONT_XBATCH);
}

/*
**  The "list" command.  Send the active file.
*/
STATIC FUNCTYPE
NClist(CHANNEL *cp)
{
    char		*p;
    char		*q;
    char		*trash;
    char		*end;

    for (p = cp->In.Data + STRLEN("list"); ISWHITE(*p); p++)
	continue;
    if (caseEQ(p, "newsgroups")) {
	trash = p = ReadInFile(cpcatpath(innconf->pathdb, _PATH_NEWSGROUPS),
			(struct stat *)NULL);
	if (p == NULL) {
	    NCwritereply(cp, NCdot);
	    return;
	}
	end = p + strlen(p);
    }
    else if (caseEQ(p, "active.times")) {
	trash = p = ReadInFile(cpcatpath(innconf->pathdb, _PATH_ACTIVETIMES),
			(struct stat *)NULL);
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
	NCwritereply(cp, NCbadcommand);
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
	DISPOSE(trash);
}


/*
**  The "mode" command.  Hand off the channel.
*/
STATIC FUNCTYPE
NCmode(CHANNEL *cp)
{
    char		*p;
    HANDOFF		h;

    /* Skip the first word, get the argument. */
    for (p = cp->In.Data + STRLEN("mode"); ISWHITE(*p); p++)
	continue;

    if (caseEQ(p, "reader") && !innconf->noreader)
	h = HOnnrpd;
    else if (caseEQ(p, "stream") &&
             (!StreamingOff && cp->Streaming)) {
	char buff[16];
	(void)sprintf(buff, "%d StreamOK.", NNTP_OK_STREAM_VAL);
	NCwritereply(cp, buff);
	syslog(L_NOTICE, "%s NCmode \"mode stream\" received",
		CHANname(cp));
	return;
    } else {
	NCwritereply(cp, NCbadcommand);
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
STATIC FUNCTYPE NCquit(CHANNEL *cp)
{
    cp->State = CSwritegoodbye;
    NCwritereply(cp, NNTP_GOODBYE_ACK);
}


/*
**  The "xpath" command.  Return the paths for an article is.
*/
STATIC FUNCTYPE
NCxpath(CHANNEL *cp)
{
    /* not available for storageapi */
    NCwritereply(cp, NNTP_BAD_COMMAND);
    return;
}

/*
**  The catch-all for inimplemented commands.
*/
STATIC FUNCTYPE
NC_unimp(CHANNEL *cp)
{
    char		*p;
    char		buff[SMBUF];

    /* Nip off the first word. */
    for (p = cp->In.Data; *p && !ISWHITE(*p); p++)
	continue;
    *p = '\0';
    (void)sprintf(buff, "%d \"%s\" not implemented; try \"help\".",
	    NNTP_BAD_COMMAND_VAL, MaxLength(cp->In.Data, cp->In.Data));
    NCwritereply(cp, buff);
}



/*
**  Remove the \r\n and leading dot escape that the NNTP protocol adds.
*/
STATIC void
NCclean(BUFFER *bp)
{
    char	*end;
    char	*p;
    char	*dest;

    for (p = bp->Data, dest = p, end = p + bp->Used; p < end; ) {
	if (p[0] == '\r' && p[1] == '\n') {
	    p += 2;
	    *dest++ = '\n';
	    if (p[0] == '.' && p[1] == '.') {
		p += 2;
		*dest++ = '.';
	    }
	}
	else
	    *dest++ = *p++;
    }
    *dest = '\0';
    bp->Used = dest - bp->Data;
}


/*
**  Check whatever data is available on the channel.  If we got the
**  full amount (i.e., the command or the whole article) process it.
*/
STATIC FUNCTYPE NCproc(CHANNEL *cp)
{
    char	        *p;
    NCDISPATCH   	*dp;
    BUFFER	        *bp;
    char		buff[SMBUF];
    int			i;

    if (Tracing || cp->Tracing)
	syslog(L_TRACE, "%s NCproc Used=%d",
	    CHANname(cp), cp->In.Used);

    bp = &cp->In;
    if (bp->Used == 0)
	return;

    p = &bp->Data[bp->Used];
    for ( ; ; ) {
	cp->Rest = 0;
	cp->SaveUsed = bp->Used;
	if (Tracing || cp->Tracing)
	    if (bp->Used > 15)
		syslog(L_TRACE, "%s NCproc state=%d next \"%.15s\"",
		    CHANname(cp), cp->State, bp->Data);
	switch (cp->State) {
	default:
	    syslog(L_ERROR, "%s internal NCproc state %d",
		CHANname(cp), cp->State);
	    break;

	case CSgetcmd:
	case CSgetauth:
	    /* Did we get the whole command, terminated with "\r\n"? */
	    for (i = 0; (i < bp->Used) && (bp->Data[i] != '\n'); i++) ;
	    if (i < bp->Used) cp->Rest = bp->Used = ++i;
	    else {
		cp->Rest = 0;
		/* Check for too long command. */
		if (bp->Used > NNTP_STRLEN) {
		    /* Make some room, saving only the last few bytes. */
		    for (p = bp->Data, i = 0; i < SAVE_AMT; p++, i++)
			p[0] = p[bp->Used - SAVE_AMT];
		    cp->LargeCmdSize += bp->Used - SAVE_AMT;
		    bp->Used = cp->Lastch = SAVE_AMT;
		    cp->State = CSeatcommand;
		}
		break;	/* come back later for rest of line */
	    }
	    if (cp->Rest < 2) break;
	    p = &bp->Data[cp->Rest];
	    if (p[-2] != '\r' || p[-1] != '\n') { /* probably in an article */
		int j;
		char *tmpstr;

		tmpstr = NEW(char, bp->Used + 1);
		memcpy(tmpstr, bp->Data, bp->Used);
		tmpstr[bp->Used] = '\0';
		
		syslog(L_NOTICE, "%s bad_command %s",
		    CHANname(cp), MaxLength(tmpstr, tmpstr));
		DISPOSE(tmpstr);

		if (++(cp->BadCommands) >= BAD_COMMAND_COUNT) {
		    cp->State = CSwritegoodbye;
		    NCwritereply(cp, NCbadcommand);
		    cp->Rest = cp->SaveUsed;
		    break;
		} else
		    NCwritereply(cp, NCbadcommand);
		for (j = i + 1; j < cp->SaveUsed; j++)
		    if (bp->Data[j] == '\n') {
			if (bp->Data[j - 1] == '\r') break;
			else cp->Rest = bp->Used = j + 1;
		    }
		break;
	    }
	    p[-2] = '\0';
	    bp->Used -= 2;

	    /* Ignore blank lines. */
	    if (bp->Data[0] == '\0')
		break;
	    if (Tracing || cp->Tracing)
		syslog(L_TRACE, "%s < %s", CHANname(cp), bp->Data);

	    /* We got something -- stop sleeping (in case we were). */
	    SCHANremove(cp);
	    if (cp->Argument != NULL) {
		DISPOSE(cp->Argument);
		cp->Argument = NULL;
	    }

	    if (cp->State == CSgetauth) {
		if (caseEQn(bp->Data, "mode", 4))
		    NCmode(cp);
		else
		    NCauthinfo(cp);
		break;
	    }

	    /* Loop through the command table. */
	    for (p = bp->Data, dp = NCcommands; dp < ENDOF(NCcommands); dp++)
		if (caseEQn(p, dp->Name, dp->Size)) {
                    /* ignore the streaming commands if necessary. */
                    if (!StreamingOff || cp->Streaming ||
                        (dp->Function != NCcheck && dp->Function != NCtakethis)) {
                        (*dp->Function)(cp);
                        cp->BadCommands = 0;
                        break;
                    }
		}
            
	    if (dp == ENDOF(NCcommands)) {
		if (++(cp->BadCommands) >= BAD_COMMAND_COUNT) {
		    cp->State = CSwritegoodbye;
		    NCwritereply(cp, NCbadcommand);
		    cp->Rest = cp->SaveUsed;
		} else
		    NCwritereply(cp, NCbadcommand);

                /* Channel could have been freed by above NCwritereply if 
                   we're writing-goodbye */
		if (cp->Type == CTfree) {
                    return;
                }
                
		for (i = 0; (p = NCquietlist[i]) != NULL; i++)
		    if (caseEQ(p, bp->Data))
			break;
		if (p == NULL)
		    syslog(L_NOTICE, "%s bad_command %s",
			CHANname(cp), MaxLength(bp->Data, bp->Data));
	    }
	    break;

	case CSgetarticle:
	    /* Check for the null article. */
	    if ((bp->Used >= 3) && (bp->Data[0] == '.')
	     && (bp->Data[1] == '\r') && (bp->Data[2] == '\n')) {
		cp->Rest = 3;	/* null article (canceled?) */
		cp->Rejected++;
		cp->State = CSgetcmd;
		if (cp->Sendid.Size > 3) { /* We be streaming */
		    char buff[4];
		    cp->Takethis_Err++;
		    (void)sprintf(buff, "%d", NNTP_ERR_FAILID_VAL);
		    cp->Sendid.Data[0] = buff[0];
		    cp->Sendid.Data[1] = buff[1];
		    cp->Sendid.Data[2] = buff[2];
		    NCwritereply(cp, cp->Sendid.Data);
		}
		else NCwritereply(cp, NNTP_REJECTIT_EMPTY);
		bp->Used = 0;

		/* Clear the work-in-progress entry. */
		NCclearwip(cp);
		break;
	    }
	    /* Reading an article; look for "\r\n.\r\n" terminator. */
	    if (cp->Lastch > 5) i = cp->Lastch; /* only look at new data */
	    else		i = 5;
	    for ( ; i <= bp->Used; i++) {
		if ((bp->Data[i - 5] == '\r')
		 && (bp->Data[i - 4] == '\n')
		 && (bp->Data[i - 3] == '.')
		 && (bp->Data[i - 2] == '\r')
		 && (bp->Data[i - 1] == '\n')) {
		    cp->Rest = bp->Used = i;
		    p = &bp->Data[i];
		    break;
		}
	    }
	    cp->Lastch = i;
	    if (i > bp->Used) {	/* did not find terminator */
		/* Check for big articles. */
		if (innconf->maxartsize > SAVE_AMT && bp->Used > innconf->maxartsize) {
		    /* Make some room, saving only the last few bytes. */
		    for (p = bp->Data, i = 0; i < SAVE_AMT; p++, i++)
			p[0] = p[bp->Used - SAVE_AMT];
		    cp->LargeArtSize += bp->Used - SAVE_AMT;
		    bp->Used = cp->Lastch = SAVE_AMT;
		    cp->State = CSeatarticle;
		}
		cp->Rest = 0;
		break;
	    }
	    if (Mode == OMpaused) { /* defer processing while paused */
		cp->Rest = 0;
		bp->Used = cp->SaveUsed;
		RCHANremove(cp); /* don't bother trying to read more for now */
		SCHANadd(cp, (time_t)(Now.time + innconf->pauseretrytime),
		    (POINTER)&Mode, NCproc, (POINTER)NULL);
		return;
	    }

	    /* Strip article terminator and post the article. */
	    p[-3] = '\0';
	    bp->Used -= 2;
	    SCHANremove(cp);
	    if (cp->Argument != NULL) {
		DISPOSE(cp->Argument);
		cp->Argument = NULL;
	    }
	    NCpostit(cp);
	    if (cp->State == CSwritegoodbye)
		break;
	    cp->State = CSgetcmd;
	    break;

	case CSeatarticle:
	    /* Eat the article and then complain that it was too large */
	    /* Reading an article; look for "\r\n.\r\n" terminator. */
	    if (cp->Lastch > 5) i = cp->Lastch; /* only look at new data */
	    else		i = 5;
	    for ( ; i <= bp->Used; i++) {
		if ((bp->Data[i - 5] == '\r')
		 && (bp->Data[i - 4] == '\n')
		 && (bp->Data[i - 3] == '.')
		 && (bp->Data[i - 2] == '\r')
		 && (bp->Data[i - 1] == '\n')) {
		    cp->Rest = bp->Used = i;
		    p = &bp->Data[i];
		    break;
		}
	    }
	    if (i <= bp->Used) {	/* did find terminator */
		/* Reached the end of the article. */
		SCHANremove(cp);
		if (cp->Argument != NULL) {
		    DISPOSE(cp->Argument);
		    cp->Argument = NULL;
		}
		i = cp->LargeArtSize + bp->Used;
		syslog(L_NOTICE, "%s internal rejecting huge article (%d > %d)",
		    CHANname(cp), i, innconf->maxartsize);
		cp->LargeArtSize = 0;
		(void)sprintf(buff, "%d Article exceeds local limit of %ld bytes",
			NNTP_REJECTIT_VAL, innconf->maxartsize);
		cp->State = CSgetcmd;
		if (cp->Sendid.Size)
		    NCwritereply(cp, cp->Sendid.Data);
		else
		    NCwritereply(cp, buff);
		cp->Rejected++;

		/* Write a local cancel entry so nobody else gives it to us. */
		    if (!HIShavearticle(cp->CurrentMessageIDHash))
			if (!HISremember(cp->CurrentMessageIDHash)) 
			    syslog(L_ERROR, "%s cant write %s", LogName, 
			        HashToText(cp->CurrentMessageIDHash)); 

		/* Clear the work-in-progress entry. */
		NCclearwip(cp);

		/*
		 * only free and allocate the buffer back to
		 * START_BUFF_SIZE if there's nothing in the buffer we
		 * need to save (i.e., following commands.
		 * if there is, then we're probably in streaming mode,
		 * so probably not much point in trying to keep the
		 * buffers minimal anyway...
		 */
		if (bp->Used == cp->SaveUsed) {
		    /* Reset input buffer to the default size; don't let realloc
		     * be lazy. */
		    DISPOSE(bp->Data);
		    bp->Size = START_BUFF_SIZE;
		    bp->Used = 0;
		    bp->Data = NEW(char, bp->Size);
		    cp->SaveUsed = cp->Rest = cp->Lastch = 0;
		}
	    }
	    else if (bp->Used > 8 * 1024) {
		/* Make some room; save the last few bytes of the article */
		for (p = bp->Data, i = 0; i < SAVE_AMT; p++, i++)
		    p[0] = p[bp->Used - SAVE_AMT + 0];
		cp->LargeArtSize += bp->Used - SAVE_AMT;
		bp->Used = cp->Lastch = SAVE_AMT;
		cp->Rest = 0;
	    }
	    break;
	case CSeatcommand:
	    /* Eat the command line and then complain that it was too large */
	    /* Reading a line; look for "\r\n" terminator. */
	    if (cp->Lastch > 5) i = cp->Lastch; /* only look at new data */
	    else		i = 5;
	    for ( ; i <= bp->Used; i++) {
		if ((bp->Data[i - 2] == '\r') && (bp->Data[i - 1] == '\n')) {
		    cp->Rest = bp->Used = i;
		    p = &bp->Data[i];
		    break;
		}
	    }
	    if (i <= bp->Used) {	/* did find terminator */
		/* Reached the end of the command line. */
		SCHANremove(cp);
		if (cp->Argument != NULL) {
		    DISPOSE(cp->Argument);
		    cp->Argument = NULL;
		}
		i = cp->LargeCmdSize + bp->Used;
		syslog(L_NOTICE, "%s internal rejecting too long command line (%d > %d)",
		    CHANname(cp), i, NNTP_STRLEN);
		cp->LargeCmdSize = 0;
		(void)sprintf(buff, "%d command exceeds local limit of %ld bytes",
			NNTP_BAD_COMMAND_VAL, NNTP_STRLEN);
		cp->State = CSgetcmd;
		NCwritereply(cp, buff);

		/*
		 * only free and allocate the buffer back to
		 * START_BUFF_SIZE if there's nothing in the buffer we
		 * need to save (i.e., following commands.
		 * if there is, then we're probably in streaming mode,
		 * so probably not much point in trying to keep the
		 * buffers minimal anyway...
		 */
		if (bp->Used == cp->SaveUsed) {
		    /* Reset input buffer to the default size; don't let realloc
		     * be lazy. */
		    DISPOSE(bp->Data);
		    bp->Size = START_BUFF_SIZE;
		    bp->Used = 0;
		    bp->Data = NEW(char, bp->Size);
		    cp->SaveUsed = cp->Rest = cp->Lastch = 0;
		}
	    }
	    else if (bp->Used > 8 * 1024) {
		/* Make some room; save the last few bytes of the article */
		for (p = bp->Data, i = 0; i < SAVE_AMT; p++, i++)
		    p[0] = p[bp->Used - SAVE_AMT + 0];
		cp->LargeCmdSize += bp->Used - SAVE_AMT;
		bp->Used = cp->Lastch = SAVE_AMT;
		cp->Rest = 0;
	    }
	    break;
	case CSgetxbatch:
	    /* if the batch is complete, write it out into the in.coming
	    * directory with an unique timestamp, and start rnews on it.
	    */
	    if (Tracing || cp->Tracing)
		syslog(L_TRACE, "%s CSgetxbatch: now %ld of %ld bytes",
			CHANname(cp), bp->Used, cp->XBatchSize);

	    if (bp->Used < cp->XBatchSize) {
		cp->Rest = 0;
		break;	/* give us more data */
	    }
	    cp->Rest = cp->XBatchSize;

	    /* now do something with the batch */
	    {
		char buff[SMBUF], buff2[SMBUF];
		int fd, oerrno, failed;
		long now;

		now = time(NULL);
		failed = 0;
		/* time+channel file descriptor should make an unique file name */
		sprintf(buff, "%s/%ld%d.tmp", innconf->pathincoming,
						now, cp->fd);
		fd = open(buff, O_WRONLY|O_CREAT|O_EXCL, ARTFILE_MODE);
		if (fd < 0) {
		    oerrno = errno;
		    failed = 1;
		    syslog(L_ERROR, "%s cannot open outfile %s for xbatch: %m",
			    CHANname(cp), buff);
		    sprintf(buff, "%s cant create file: %s",
			    NNTP_RESENDIT_XBATCHERR, strerror(oerrno));
		    NCwritereply(cp, buff);
		} else {
		    if (write(fd, cp->In.Data, cp->XBatchSize) != cp->XBatchSize) {
			oerrno = errno;
			syslog(L_ERROR, "%s cant write batch to file %s: %m",
				CHANname(cp), buff);
			sprintf(buff, "%s cant write batch to file: %s",
				NNTP_RESENDIT_XBATCHERR, strerror(oerrno));
			NCwritereply(cp, buff);
			failed = 1;
		    }
		}
		if (fd >= 0 && close(fd) != 0) {
		    oerrno = errno;
		    syslog(L_ERROR, "%s error closing batch file %s: %m",
			    CHANname(cp), failed ? "" : buff);
		    sprintf(buff, "%s error closing batch file: %s",
			    NNTP_RESENDIT_XBATCHERR, strerror(oerrno));
		    NCwritereply(cp, buff);
		    failed = 1;
		}
		sprintf(buff2, "%s/%ld%d.x", innconf->pathincoming,
						now, cp->fd);
		if (rename(buff, buff2)) {
		    oerrno = errno;
		    syslog(L_ERROR, "%s cant rename %s to %s: %m",
			    CHANname(cp), failed ? "" : buff, buff2);
		    sprintf(buff, "%s cant rename batch to %s: %s",
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
	    syslog(L_NOTICE, "%s accepted batch size %ld",
		   CHANname(cp), cp->XBatchSize);
	    cp->State = CSgetcmd;
	    
	    /* Clear the work-in-progress entry. */
	    NCclearwip(cp);

#if 1
	    /* leave the input buffer as it is, there is a fair chance
		* that we will get another xbatch on this channel.
		* The buffer will finally be disposed at connection shutdown.
		*/
#else
	    /* Reset input buffer to the default size; don't let realloc 
		* be lazy. */
	    DISPOSE(bp->Data);
	    bp->Size = START_BUFF_SIZE;
	    bp->Used = 0;
	    bp->Data = NEW(char, bp->Size);
#endif
	    cp->State = CSgetcmd;
	    break;
	}
	if (cp->State == CSwritegoodbye)
	    break;
	if (Tracing || cp->Tracing)
		syslog(L_TRACE, "%s NCproc Rest=%d Used=%d SaveUsed=%d",
		    CHANname(cp), cp->Rest, bp->Used, cp->SaveUsed);

	if (cp->Rest > 0) {
	    if (cp->Rest < cp->SaveUsed) { /* more commands in buffer */
		bp->Used = cp->SaveUsed = cp->SaveUsed - cp->Rest;
		/* It would be nice to avoid this copy but that
		** would require changes to the bp structure and
		** the way it is used.
		*/
		(void)memmove((POINTER)bp->Data, (POINTER)&bp->Data[cp->Rest], (SIZE_T)bp->Used);
		cp->Rest = cp->Lastch = 0;
	    } else {
		bp->Used = cp->Lastch = 0;
		break;
	    }
	} else break;
    }
}


/*
**  Read whatever data is available on the channel.  If we got the
**  full amount (i.e., the command or the whole article) process it.
*/
STATIC FUNCTYPE
NCreader(CHANNEL *cp)
{
    int			i;

    if (Tracing || cp->Tracing)
	syslog(L_TRACE, "%s NCreader Used=%d",
	    CHANname(cp), cp->In.Used);

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
NCsetup(int i)
{
    NCDISPATCH		*dp;
    char		*p;
    char		buff[SMBUF];

    /* Set the greeting message. */
    p = innconf->pathhost;
    if (p == NULL)
	/* Worked in main, now it fails?  Curious. */
	p = Path.Data;
    (void)sprintf(buff, "%d %s InterNetNews server %s ready",
	    NNTP_POSTOK_VAL, p, inn_version_string);
    NCgreeting = COPY(buff);

    /* Get the length of every command. */
    for (dp = NCcommands; dp < ENDOF(NCcommands); dp++)
	dp->Size = strlen(dp->Name);
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
NCcreate(int fd, BOOL MustAuthorize, BOOL IsLocal)
{
    CHANNEL		*cp;
    int			i;

    /* Create the channel. */
    cp = CHANcreate(fd, CTnntp, MustAuthorize ? CSgetauth : CSgetcmd,
	    NCreader, NCwritedone);

    NCclearwip(cp);
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
STATIC FUNCTYPE
NCcheck(CHANNEL *cp)
{
    char		*p;
    int			idlen, msglen;
#if defined(DO_PERL) || defined(DO_PYTHON)
    char		*filterrc;
#endif /* DO_PERL || DO_PYTHON */

    cp->Check++;
    /* Snip off the Message-ID. */
    for (p = cp->In.Data; *p && !ISWHITE(*p); p++)
	continue;
    for ( ; ISWHITE(*p); p++)
	continue;
    idlen = strlen(p);
    msglen = idlen + 5; /* 3 digits + space + id + null */
    if (cp->Sendid.Size < msglen) {
	if (cp->Sendid.Size > 0) DISPOSE(cp->Sendid.Data);
	if (msglen > MAXHEADERSIZE) cp->Sendid.Size = msglen;
	else cp->Sendid.Size = MAXHEADERSIZE;
	cp->Sendid.Data = NEW(char, cp->Sendid.Size);
    }
    if (!ARTidok(p)) {
	(void)sprintf(cp->Sendid.Data, "%d %s", NNTP_ERR_GOTID_VAL, p);
	NCwritereply(cp, cp->Sendid.Data);
	syslog(L_NOTICE, "%s bad_messageid %s", CHANname(cp), MaxLength(p, p));
	return;
    }

    if ((innconf->refusecybercancels) && (strncmp(p, "<cancel.", 8) == 0)) {
	cp->Refused++;
	cp->Check_cybercan++;
	(void)sprintf(cp->Sendid.Data, "%d %s", NNTP_ERR_GOTID_VAL, p);
	NCwritereply(cp, cp->Sendid.Data);
	return;
    }

#if defined(DO_PERL)
    /*  Invoke a perl message filter on the message ID. */
    filterrc = PLmidfilter(p);
    if (filterrc) {
	cp->Refused++;
	sprintf(cp->Sendid.Data, "%d %s", NNTP_ERR_GOTID_VAL, p);
	NCwritereply(cp, cp->Sendid.Data);
	return;
    }
#endif /* defined(DO_PERL) */

#if defined(DO_PYTHON)
    /*  invoke a python message filter on the message id */
    filterrc = PYmidfilter(p, idlen);
    if (filterrc) {
	cp->Refused++;
	sprintf(cp->Sendid.Data, "%d %s", NNTP_ERR_GOTID_VAL, p);
	NCwritereply(cp, cp->Sendid.Data);
	return;
    }
#endif /* defined(DO_PYTHON) */

    if (HIShavearticle(HashMessageID(p))) {
	cp->Refused++;
	cp->Check_got++;
	(void)sprintf(cp->Sendid.Data, "%d %s", NNTP_ERR_GOTID_VAL, p);
	NCwritereply(cp, cp->Sendid.Data);
    } else if (WIPinprogress(p, cp, TRUE)) {
	cp->Check_deferred++;
	if (cp->NoResendId) {
	    cp->Refused++;
	    (void)sprintf(cp->Sendid.Data, "%d %s", NNTP_ERR_GOTID_VAL, p);
	} else {
	    (void)sprintf(cp->Sendid.Data, "%d %s", NNTP_RESENDID_VAL, p);
	}
	NCwritereply(cp, cp->Sendid.Data);
    } else {
	cp->Check_send++;
	(void)sprintf(cp->Sendid.Data, "%d %s", NNTP_OK_SENDID_VAL, p);
	NCwritereply(cp, cp->Sendid.Data);
    }
    /* stay in command mode */
}

/*
**  The "takethis" command.  Article follows.
**  Remember <id> for later ack.
*/
STATIC FUNCTYPE NCtakethis(CHANNEL *cp)
{
    char	        *p;
    int			msglen;
    WIP                 *wp;

    cp->Takethis++;
    /* Snip off the Message-ID. */
    for (p = cp->In.Data + STRLEN("takethis"); ISWHITE(*p); p++)
	continue;
    for ( ; ISWHITE(*p); p++)
	continue;
    if (!ARTidok(p)) {
	syslog(L_NOTICE, "%s bad_messageid %s", CHANname(cp), MaxLength(p, p));
    }
    msglen = strlen(p) + 5; /* 3 digits + space + id + null */
    if (cp->Sendid.Size < msglen) {
	if (cp->Sendid.Size > 0) DISPOSE(cp->Sendid.Data);
	if (msglen > MAXHEADERSIZE) cp->Sendid.Size = msglen;
	else cp->Sendid.Size = MAXHEADERSIZE;
	cp->Sendid.Data = NEW(char, cp->Sendid.Size);
    }
    /* save ID for later NACK or ACK */
    (void)sprintf(cp->Sendid.Data, "%d %s", NNTP_ERR_FAILID_VAL, p);

    cp->ArtBeg = Now.time;
    cp->State = CSgetarticle;
    /* set WIP for benefit of later code in NCreader */
    if ((wp = WIPbyid(p)) == (WIP *)NULL)
	wp = WIPnew(p, cp);
    WIPfree(WIPbyhash(cp->CurrentMessageIDHash));
    cp->CurrentMessageIDHash = wp->MessageID;
}

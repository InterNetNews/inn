/*  $Id$
**
**  Routines to implement site-feeding.  Mainly working with channels to
**  do buffering and determine what to send.
*/
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "configdata.h"
#include "clibrary.h"
#include "innd.h"


STATIC int	SITEcount;
STATIC int	SITEhead = NOSITE;
STATIC int	SITEtail = NOSITE;
STATIC char	SITEshell[] = _PATH_SH;


/*
**  Called when input is ready to read.  Shouldn't happen.
*/
/* ARGSUSED0 */
STATIC FUNCTYPE SITEreader(CHANNEL *cp)
{
    syslog(L_ERROR, "%s internal SITEreader", LogName);
}


/*
**  Called when write is done.  No-op.
*/
/* ARGSUSED0 */
STATIC FUNCTYPE SITEwritedone(CHANNEL *cp)
{
}


/*
**  Make a site start spooling.
*/
BOOL SITEspool(SITE *sp, CHANNEL *cp)
{
    int			i;
    char		buff[SPOOLNAMEBUFF];
    char		*name;

    name = sp->SpoolName;
    i = open(name, O_APPEND | O_CREAT | O_WRONLY, BATCHFILE_MODE);
    if (i < 0 && errno == EISDIR) {
	FileGlue(buff, sp->SpoolName, '/', "togo");
	name = buff;
	i = open(buff, O_APPEND | O_CREAT | O_WRONLY, BATCHFILE_MODE);
    }
    if (i < 0) {
	i = errno;
	syslog(L_ERROR, "%s cant open %s %m", sp->Name, name);
	IOError("site batch file", i);
	sp->Channel = NULL;
	return FALSE;
    }
    if (AmRoot)
	xchown(name);
    if (cp) {
      if (cp->fd >= 0)
        /* syslog(L_ERROR, "DEBUG ERROR SITEspool trashed:%d %s:%d", cp->fd, sp->Name, i);
	   CPU-eating bug, killed by kre. */
	WCHANremove(cp);
	RCHANremove(cp);
	SCHANremove(cp);
	(void)close(cp->fd);
	cp->fd = i;
	return TRUE;
    }
    sp->Channel = CHANcreate(i, CTfile, CSwriting, SITEreader, SITEwritedone);
    if (sp->Channel == NULL) {
	syslog(L_ERROR, "%s cant channel %m", sp->Name);
	(void)close(i);
	return FALSE;
    }
    WCHANset(sp->Channel, "", 0);
    sp->Spooling = TRUE;
    return TRUE;
}


/*
**  Delete a site from the file writing list.  Can be called even if
**  site is not on the list.
*/
static void SITEunlink(SITE *sp)
{
    if (sp->Next != NOSITE || sp->Prev != NOSITE
     || (SITEhead != NOSITE && sp == &Sites[SITEhead]))
	SITEcount--;

    if (sp->Next != NOSITE)
	Sites[sp->Next].Prev = sp->Prev;
    else if (SITEtail != NOSITE && sp == &Sites[SITEtail])
	SITEtail = sp->Prev;

    if (sp->Prev != NOSITE)
	Sites[sp->Prev].Next = sp->Next;
    else if (SITEhead != NOSITE && sp == &Sites[SITEhead])
	SITEhead = sp->Next;

    sp->Next = sp->Prev = NOSITE;
}


/*
**  Find the oldest "file feed" site and buffer it.
*/
STATIC void SITEbufferoldest(void)
{
    SITE	        *sp;
    BUFFER	        *bp;
    BUFFER	        *out;

    /* Get the oldest user of a file. */
    if (SITEtail == NOSITE) {
	syslog(L_ERROR, "%s internal no oldest site found", LogName);
	SITEcount = 0;
	return;
    }

    sp = &Sites[SITEtail];
    SITEunlink(sp);

    if (sp->Buffered) {
	syslog(L_ERROR, "%s internal oldest (%s) was buffered", LogName,
	    sp->Name);
	return;
    }

    if (sp->Type != FTfile) {
	syslog(L_ERROR, "%s internal oldest (%s) not FTfile", LogName,
	    sp->Name);
	return;
    }

    /* Write out what we can. */
    (void)WCHANflush(sp->Channel);

    /* Get a buffer for the site. */
    sp->Buffered = TRUE;
    bp = &sp->Buffer;
    bp->Used = 0;
    bp->Left = 0;
    if (bp->Size == 0) {
	bp->Size = sp->Flushpoint;
	bp->Data = NEW(char, bp->Size);
    }
    else {
	bp->Size = sp->Flushpoint;
	RENEW(bp->Data, char, bp->Size);
    }

    /* If there's any unwritten data, copy it. */
    out = &sp->Channel->Out;
    if (out->Left) {
	BUFFset(bp, &out->Data[out->Used], out->Left);
	out->Left = 0;
    }

    /* Now close the original channel. */
    CHANclose(sp->Channel, sp->Name);
    sp->Channel = NULL;
}

/*
 * *  Bilge Site's Channel out buffer.
 */
STATIC BOOL SITECHANbilge(SITE *sp)
{
    int             fd;
    int             i;
    char            buff[SPOOLNAMEBUFF];
    char           *name;

    name = sp->SpoolName;
    fd = open(name, O_APPEND | O_CREAT | O_WRONLY, BATCHFILE_MODE);
    if (fd < 0 && errno == EISDIR) {
        FileGlue(buff, sp->SpoolName, '/', "togo");
        name = buff;
        fd = open(buff, O_APPEND | O_CREAT | O_WRONLY, BATCHFILE_MODE);
    }
    if (fd < 0) {
	int oerrno = errno ;
        syslog(L_ERROR, "%s cant open %s %m", sp->Name, name);
        IOError("site batch file",oerrno);
        sp->Channel = NULL;
        return FALSE;
    }
    while(sp->Channel->Out.Left > 0) {
        i = write(fd, (POINTER) &sp->Channel->Out.Data[sp->Channel->Out.Used],
            (SIZE_T) sp->Channel->Out.Left);
        if(i <= 0) {
            syslog(L_ERROR,"%s cant spool count %ld",CHANname(sp->Channel),
                sp->Channel->Out.Left);
            close(fd);
            return FALSE;
        }
        sp->Channel->Out.Left -= i;
        sp->Channel->Out.Used += i;
    }
    close(fd);
    DISPOSE(sp->Channel->Out.Data);
    sp->Channel->Out.Data = NEW(char, SMBUF);
    sp->Channel->Out.Size = SMBUF;
    sp->Channel->Out.Left = 0;
    sp->Channel->Out.Used = 0;
    return TRUE;
}

/*
**  Check if we need to write out the site's buffer.  If we're buffered
**  or the feed is backed up, this gets a bit complicated.
*/
STATIC void SITEflushcheck(SITE *sp, BUFFER *bp)
{
    int	                i;
    CHANNEL	        *cp;

    /* If we're buffered, and we hit the flushpoint, do an LRU. */
    if (sp->Buffered) {
	if (bp->Left < sp->Flushpoint)
	    return;
	while (SITEcount >= MaxOutgoing)
	    SITEbufferoldest();
	if (!SITEsetup(sp) || sp->Buffered) {
	    syslog(L_ERROR, "%s cant unbuffer %m", sp->Name);
	    return;
	}
	WCHANsetfrombuffer(sp->Channel, bp);
	WCHANadd(sp->Channel);
	/* Reset buffer; data has been moved. */
	BUFFset(bp, "", 0);
    }

    if (PROCneedscan)
	PROCscan();

    /* Handle buffering. */
    cp = sp->Channel;
    i = cp->Out.Left;
    if (i < sp->StopWriting)
	WCHANremove(cp);
    if ((sp->StartWriting == 0 || i > sp->StartWriting)
     && !CHANsleeping(cp)) {
	if (sp->Type == FTchannel) {	/* channel feed, try the write */
	    int j;
	    if (bp->Left == 0)
		return;
	    j = write(cp->fd, (POINTER)&bp->Data[bp->Used], (SIZE_T)bp->Left);
	    if (j > 0) {
		bp->Left -= j;
		bp->Used += j;
		i = cp->Out.Left;
		/* Since we just had a successful write, we need to clear the 
		 * channel's error counts. - dave@jetcafe.org */
		cp->BadWrites = 0;
		cp->BlockedWrites = 0;
	    }
	    if (bp->Left <= 0) {
                /* reset Used, Left on bp, keep channel buffer size from
                   exploding. */
                bp->Used = bp->Left = 0;
		WCHANremove(cp);
            } else
		WCHANadd(cp);
	}
	else
	    WCHANadd(cp);
    }

    cp->LastActive = Now.time;

    /* If we're a channel that's getting big, see if we need to spool. */
    if (sp->Type == FTfile || sp->StartSpooling == 0 || i < sp->StartSpooling)
	return;

    syslog(L_ERROR, "%s spooling %d bytes", sp->Name, i);
    if(!SITECHANbilge(sp)) {
	syslog(L_ERROR, "%s overflow %d bytes", sp->Name, i);
	return;
    }
}


/*
**  Send a control line to an exploder.
*/
void SITEwrite(SITE *sp, STRING text)
{
    static char		PREFIX[] = { EXP_CONTROL, '\0' };
    BUFFER	        *bp;

    if (sp->Buffered)
	bp = &sp->Buffer;
    else {
	if (sp->Channel == NULL)
	    return;
	sp->Channel->LastActive = Now.time;
	bp = &sp->Channel->Out;
    }
    BUFFappend(bp, PREFIX, STRLEN(PREFIX));
    BUFFappend(bp, text, (int)strlen(text));
    BUFFappend(bp, "\n", 1);
    if (sp->Channel != NULL)
	WCHANadd(sp->Channel);
}


/*
**  Send the desired data about an article down a channel.
*/
STATIC void SITEwritefromflags(SITE *sp, ARTDATA *Data)
{
    static char		ITEMSEP[] = " ";
    static char		NL[] = "\n";
    char		pbuff[12];
    char	        *p;
    BOOL	        Dirty;
    BUFFER	        *bp;
    SITE	        *spx;
    int	                i;

    if (sp->Buffered)
	bp = &sp->Buffer;
    else {
	/* This should not happen, but if we tried to spool and failed,
	 * e.g., because of a bad F param for this site, we can get
	 * into this state.  We already logged a message so give up. */
	if (sp->Channel == NULL)
	    return;
	bp = &sp->Channel->Out;
    }
    for (Dirty = FALSE, p = sp->FileFlags; *p; p++) {
	switch (*p) {
	default:
	    syslog(L_ERROR, "%s internal SITEwritefromflags %c", sp->Name, *p);
	    continue;
	case FEED_BYTESIZE:
	    if (Dirty)
		BUFFappend(bp, ITEMSEP, STRLEN(ITEMSEP));
	    BUFFappend(bp, Data->Size, Data->SizeLength);
	    break;
	case FEED_FULLNAME:
	    if (Dirty)
		BUFFappend(bp, ITEMSEP, STRLEN(ITEMSEP));
	    BUFFappend(bp, Data->Name, Data->NameLength);
	    break;
	case FEED_HASH:
	    if (Dirty)
		BUFFappend(bp, ITEMSEP, STRLEN(ITEMSEP));
	    BUFFappend(bp, "[", 1);
	    BUFFappend(bp, HashToText(*(Data->Hash)), sizeof(HASH)*2);
	    BUFFappend(bp, "]", 1);
	    break;
	case FEED_HDR_DISTRIB:
	    if (Dirty)
		BUFFappend(bp, ITEMSEP, STRLEN(ITEMSEP));
	    BUFFappend(bp, Data->Distribution, Data->DistributionLength);
	    break;
	case FEED_HDR_NEWSGROUP:
	    if (Dirty)
		BUFFappend(bp, ITEMSEP, STRLEN(ITEMSEP));
	    BUFFappend(bp, Data->Newsgroups, Data->NewsgroupsLength);
	    break;
	case FEED_HEADERS:
	    if (Dirty)
		BUFFappend(bp, NL, STRLEN(NL));
	    BUFFappend(bp, Data->Headers->Data, Data->Headers->Left);
	    break;
	case FEED_OVERVIEW:
	    if (Dirty)
		BUFFappend(bp, ITEMSEP, STRLEN(ITEMSEP));
	    BUFFappend(bp, Data->Overview->Data, Data->Overview->Left);
	    break;
	case FEED_PATH:
	    if (Dirty)
		BUFFappend(bp, ITEMSEP, STRLEN(ITEMSEP));
	    if (!Hassamepath)
		BUFFappend(bp, Path.Data, Path.Used);
	    if (AddAlias)
		BUFFappend(bp, Pathalias.Data, Pathalias.Used);
	    BUFFappend(bp, Data->Path, Data->PathLength);
	    break;
	case FEED_REPLIC:
	    if (Dirty)
		BUFFappend(bp, ITEMSEP, STRLEN(ITEMSEP));
	    BUFFappend(bp, Data->Replic, Data->ReplicLength);
	    break;
	case FEED_STOREDGROUP:
	    if (Dirty)
		BUFFappend(bp, ITEMSEP, STRLEN(ITEMSEP));
	    BUFFappend(bp, Data->StoredGroup, Data->StoredGroupLength);
	    break;
	case FEED_TIMERECEIVED:
	    if (Dirty)
		BUFFappend(bp, ITEMSEP, STRLEN(ITEMSEP));
	    BUFFappend(bp, Data->TimeReceived, Data->TimeReceivedLength);
	    break;
	case FEED_TIMEPOSTED:
	    if (Dirty)
		BUFFappend(bp, ITEMSEP, STRLEN(ITEMSEP));
	    sprintf(pbuff, "%ld", Data->Posted);
	    BUFFappend(bp, pbuff, strlen(pbuff));
	    break;
	case FEED_TIMEEXPIRED:
	    if (Dirty)
		BUFFappend(bp, ITEMSEP, STRLEN(ITEMSEP));
	    sprintf(pbuff, "%ld", Data->Expires);
	    BUFFappend(bp, pbuff, strlen(pbuff));
	    break;
	case FEED_MESSAGEID:
	    if (Dirty)
		BUFFappend(bp, ITEMSEP, STRLEN(ITEMSEP));
	    BUFFappend(bp, Data->MessageID, Data->MessageIDLength);
	    break;
	case FEED_FNLNAMES:
	    if (sp->FNLnames.Data) {
		/* Funnel; write names of our sites that got it. */
		if (Dirty)
		    BUFFappend(bp, ITEMSEP, STRLEN(ITEMSEP));
		BUFFappend(bp, sp->FNLnames.Data, sp->FNLnames.Used);
	    }
	    else {
		/* Not funnel; write names of all sites that got it. */
		for (spx = Sites, i = nSites; --i >= 0; spx++)
		    if (spx->Sendit) {
			if (Dirty)
			    BUFFappend(bp, ITEMSEP, STRLEN(ITEMSEP));
			BUFFappend(bp, spx->Name, spx->NameLength);
			Dirty = TRUE;
		    }
	    }
	    break;
	case FEED_NAME:
	    if (Dirty)
		BUFFappend(bp, ITEMSEP, STRLEN(ITEMSEP));
	    BUFFappend(bp, Data->Name, Data->NameLength);
	    break;
	case FEED_NEWSGROUP:
	    if (Dirty)
		BUFFappend(bp, ITEMSEP, STRLEN(ITEMSEP));
	    if (sp->ng)
		BUFFappend(bp, sp->ng->Name, sp->ng->NameLength);
	    else
		BUFFappend(bp, "?", 1);
	    break;
	case FEED_SITE:
	    if (Dirty)
		BUFFappend(bp, ITEMSEP, STRLEN(ITEMSEP));
	    BUFFappend(bp, Data->Feedsite, Data->FeedsiteLength);
	    break;
	}
	Dirty = TRUE;
    }
    if (Dirty) {
	BUFFappend(bp, "\n", 1);
	SITEflushcheck(sp, bp);
    }
}


/*
**  Send one article to a site.
*/
void SITEsend(SITE *sp, ARTDATA *Data)
{
    int	                i;
    char	        *p;
    char		*temp;
    char		buff[BUFSIZ];
    STRING		argv[MAX_BUILTIN_ARGV];
    int			fd;

    switch (sp->Type) {
    default:
	syslog(L_ERROR, "%s internal SITEsend type %d", sp->Name, sp->Type);
	break;
    case FTlogonly:
	break;
    case FTfunnel:
	syslog(L_ERROR, "%s funnel_send", sp->Name);
	break;
    case FTfile:
    case FTchannel:
    case FTexploder:
	SITEwritefromflags(sp, Data);
	break;
    case FTprogram:
	/* Set up the argument vector. */
	if (sp->FNLwantsnames) {
	    i = strlen(sp->Param) + sp->FNLnames.Used;
	    if (i + strlen(Data->Name) >= sizeof buff) {
		syslog(L_ERROR, "%s toolong need %d for %s",
		    sp->Name, i + strlen(Data->Name), Data->Name);
		break;
	    }
	    temp = NEW(char, i + 1);
	    p = strchr(sp->Param, '*');
	    *p = '\0';
	    (void)strcpy(temp, sp->Param);
	    (void)strcat(temp, sp->FNLnames.Data);
	    (void)strcat(temp, &p[1]);
	    *p = '*';
	    (void)sprintf(buff, temp, Data->Name);
	    DISPOSE(temp);
	}
	else
	    (void)sprintf(buff, sp->Param, Data->Name);

	if (NeedShell(buff, argv, ENDOF(argv))) {
	    argv[0] = SITEshell;
	    argv[1] = "-c";
	    argv[2] = buff;
	    argv[3] = NULL;
	}

	/* Feed the article on standard input. */
	if (IsToken(Data->Name))
	    fd = 0;
	else
	    fd = open(Data->Name, O_RDONLY);
	if (fd < 0) {
	    /* Unlikely, but we could check if the article is cross-posted
	     * and try it under other names... */
	    syslog(L_ERROR, "%s cant open %s %m", sp->Name, Data->Name);
	    fd = 0;
	}

	/* Start the process. */
	i = Spawn(sp->Nice, fd, (int)fileno(Errlog), (int)fileno(Errlog), argv);
	if (i >= 0)
	    (void)PROCwatch(i, -1);
	if (fd != 0)
	    (void)close(fd);
	break;
    }
}


/*
**  The channel was sleeping because we had to spool our output to
**  a file.  Flush and restart.
*/
STATIC FUNCTYPE SITEspoolwake(CHANNEL *cp)
{
    SITE	*sp;
    int		*ip;

    ip = (int *) cp->Argument;
    sp = &Sites[*ip];
    DISPOSE(cp->Argument);
    cp->Argument = NULL;
    if (sp->Channel != cp) {
	syslog(L_ERROR, "%s internal SITEspoolwake %s got %d, not %d",
	    LogName, sp->Name, cp->fd, sp->Channel->fd);
        return;
    }
    syslog(L_NOTICE, "%s spoolwake", sp->Name);
    SITEflush(sp, TRUE);
}


/*
**  Start up a process for a channel, or a spool to a file if we can't.
**  Create a channel for the site to talk to.
*/
STATIC BOOL SITEstartprocess(SITE *sp)
{
    PID_T	        i;
    STRING		argv[MAX_BUILTIN_ARGV];
    char		*process;
    int			*ip;
    int			pan[2];

#if HAVE_SOCKETPAIR
    /* Create a socketpair. */
    if (socketpair(PF_UNIX, SOCK_STREAM, 0, pan) < 0) {
	syslog(L_ERROR, "%s cant socketpair %m", sp->Name);
	return FALSE;
    }
#else
    /* Create a pipe. */
    if (pipe(pan) < 0) {
	syslog(L_ERROR, "%s cant pipe %m", sp->Name);
	return FALSE;
    }
#endif
    CloseOnExec(pan[PIPE_WRITE], TRUE);

    /* Set up the argument vector. */
    process = COPY(sp->Param);
    if (NeedShell(process, argv, ENDOF(argv))) {
	argv[0] = SITEshell;
	argv[1] = "-c";
	argv[2] = process;
	argv[3] = NULL;
    }

    /* Fork a child. */
    i = Spawn(sp->Nice, pan[PIPE_READ], (int)fileno(Errlog), (int)fileno(Errlog), argv);
    if (i > 0) {
	sp->pid = i;
	sp->Spooling = FALSE;
	sp->Process = PROCwatch(i, sp - Sites);
	(void)close(pan[PIPE_READ]);
	sp->Channel = CHANcreate(pan[PIPE_WRITE],
			sp->Type == FTchannel ? CTprocess : CTexploder,
			CSwriting, SITEreader, SITEwritedone);
	DISPOSE(process);
	return TRUE;
    }

    /* Error.  Switch to spooling. */
    syslog(L_ERROR, "%s cant spawn spooling %m", sp->Name);
    (void)close(pan[PIPE_WRITE]);
    (void)close(pan[PIPE_READ]);
    DISPOSE(process);
    if (!SITEspool(sp, (CHANNEL *)NULL))
	return FALSE;

    /* We'll try to restart the channel later. */
    syslog(L_ERROR, "%s cant spawn spooling %m", sp->Name);
    ip = NEW(int, 1);
    *ip = sp - Sites;
    SCHANadd(sp->Channel, (time_t)(Now.time + innconf->chanretrytime),
	(POINTER)NULL, SITEspoolwake, (POINTER)ip);
    return TRUE;
}


/*
**  Set up a site for internal buffering.
*/
STATIC void SITEbuffer(SITE *sp)
{
    BUFFER	        *bp;

    SITEunlink(sp);
    sp->Buffered = TRUE;
    sp->Channel = NULL;
    bp = &sp->Buffer;
    if (bp->Size == 0) {
	bp->Size = sp->Flushpoint;
	bp->Data = NEW(char, bp->Size);
	BUFFset(bp, "", 0);
    }
    else if (bp->Size < sp->Flushpoint) {
	bp->Size = sp->Flushpoint;
	RENEW(bp->Data, char, bp->Size);
    }
    BUFFset(bp, "", 0);
    syslog(L_NOTICE, "%s buffered", sp->Name);
}


/*
**  Link a site at the head of the "currently writing to a file" list
*/
static void SITEmovetohead(SITE *sp)
{
    if ((SITEhead == NOSITE) && ((sp->Next != NOSITE) || (sp->Prev != NOSITE)))
	SITEunlink(sp);

    if ((sp->Next = SITEhead) != NOSITE)
	Sites[SITEhead].Prev = sp - Sites;
    sp->Prev = NOSITE;

    SITEhead = sp - Sites;
    if (SITEtail == NOSITE)
	SITEtail = sp - Sites;

    SITEcount++;
}


/*
**  Set up a site's feed.  This means opening a file or channel if needed.
*/
BOOL SITEsetup(SITE *sp)
{
    int			fd;
    int			oerrno;

    switch (sp->Type) {
    default:
	syslog(L_ERROR, "%s internal SITEsetup %d",
	    sp->Name, sp->Type);
	return FALSE;
    case FTfunnel:
    case FTlogonly:
    case FTprogram:
	/* Nothing to do here. */
	break;
    case FTfile:
	if (SITEcount >= MaxOutgoing)
	    SITEbuffer(sp);
	else {
	    sp->Buffered = FALSE;
	    fd = open(sp->Param, O_APPEND | O_CREAT | O_WRONLY, BATCHFILE_MODE);
	    if (fd < 0) {
		if (errno == EMFILE) {
		    syslog(L_ERROR, "%s cant open %s %m", sp->Name, sp->Param);
		    SITEbuffer(sp);
		    break;
		}
		oerrno = errno;
		syslog(L_NOTICE, "%s cant open %s %m", sp->Name, sp->Param);
		IOError("site file", oerrno);
		return FALSE;
	    }
	    SITEmovetohead(sp);
	    if (AmRoot)
		xchown(sp->Param);
	    sp->Channel = CHANcreate(fd, CTfile, CSwriting,
			    SITEreader, SITEwritedone);
	    syslog(L_NOTICE, "%s opened %s", sp->Name, CHANname(sp->Channel));
	    WCHANset(sp->Channel, "", 0);
	}
	break;
    case FTchannel:
    case FTexploder:
	if (!SITEstartprocess(sp))
	    return FALSE;
	syslog(L_NOTICE, "%s spawned %s", sp->Name, CHANname(sp->Channel));
	WCHANset(sp->Channel, "", 0);
	WCHANadd(sp->Channel);
	break;
    }
    return TRUE;
}


/*
**  A site's channel process died; restart it.
*/
void SITEprocdied(SITE *sp, int process, PROCESS *pp)
{
    syslog(pp->Status ? L_ERROR : L_NOTICE, "%s exit %d elapsed %ld pid %ld",
	sp->Name ? sp->Name : "?", pp->Status,
	pp->Collected - pp->Started, (long)pp->Pid);
    if (sp->Process != process || sp->Name == NULL)
	/* We already started a new process for this channel
	 * or this site has been dropped. */
	return;
    if (sp->Channel != NULL)
	CHANclose(sp->Channel, CHANname(sp->Channel));
    sp->Working = SITEsetup(sp);
    if (!sp->Working) {
	syslog(L_ERROR, "%s cant restart %m", sp->Name);
	return;
    }
    syslog(L_NOTICE, "%s restarted", sp->Name);
}

/*
**  A channel is about to be closed; see if any site cares.
*/
void SITEchanclose(CHANNEL *cp)
{
    int	                i;
    SITE	        *sp;
    int			*ip;

    for (i = nSites, sp = Sites; --i >= 0; sp++)
	if (sp->Channel == cp) {
	    /* Found the site that has this channel.  Start that
	     * site spooling, copy any data that might be pending,
	     * and arrange to retry later. */
	    if (!SITEspool(sp, (CHANNEL *)NULL)) {
		syslog(L_ERROR, "%s loss %ld bytes", sp->Name, cp->Out.Left);
		return;
	    }
	    WCHANsetfrombuffer(sp->Channel, &cp->Out);
	    WCHANadd(sp->Channel);
	    ip = NEW(int, 1);
	    *ip = sp - Sites;
	    SCHANadd(sp->Channel, (time_t)(Now.time + innconf->chanretrytime),
		(POINTER)NULL, SITEspoolwake, (POINTER)ip);
	    break;
	}
}


/*
**  Flush any pending data waiting to be sent.
*/
void SITEflush(SITE *sp, BOOL Restart)
{
    CHANNEL	        *cp;
    BUFFER	        *out;
    int			count;

    if (sp->Name == NULL)
	return;

    if (Restart)
	SITEforward(sp, "flush");

    switch (sp->Type) {
    default:
	syslog(L_ERROR, "%s internal SITEflush %d", sp->Name, sp->Type);
	return;

    case FTlogonly:
    case FTprogram:
    case FTfunnel:
	/* Nothing to do here. */
	return;

    case FTchannel:
    case FTexploder:
	/* If spooling, close the file right now -- documented behavior. */
	if (sp->Spooling && (cp = sp->Channel) != NULL) {
	    (void)WCHANflush(cp);
	    CHANclose(cp, CHANname(cp));
	    sp->Channel = NULL;
	}
	break;

    case FTfile:
	/* We must ensure we have a file open for this site, so if
	 * we're buffered we HACK and pretend we have no sites
	 * for a moment. */
	if (sp->Buffered) {
	    count = SITEcount;
	    SITEcount = 0;
	    if (!SITEsetup(sp) || sp->Buffered)
		syslog(L_ERROR, "%s cant unbuffer to flush", sp->Name);
	    else
		BUFFswap(&sp->Buffer, &sp->Channel->Out);
	    SITEcount += count;
	}
	break;
    }

    /* We're only dealing with files and channels now. */
    if ((cp = sp->Channel) != NULL)
	(void)WCHANflush(cp);

    /* Restart the site, copy any pending data. */
    if (Restart) {
	if (!SITEsetup(sp))
	    syslog(L_ERROR, "%s cant restart %m", sp->Name);
	else if (cp != NULL) {
	    if (sp->Buffered) {
		/* SITEsetup had to buffer us; save any residue. */
		out = &cp->Out;
	        if (out->Left)
		    BUFFset(&sp->Buffer, &out->Data[out->Used], out->Left);
	    }
	    else
		WCHANsetfrombuffer(sp->Channel, &cp->Out);
	}
    }
    else if (cp != NULL && cp->Out.Left) {
	if (sp->Type == FTfile || sp->Spooling) {
	    /* Can't flush a file?  Hopeless. */
	    syslog(L_ERROR, "%s dataloss %ld", sp->Name, cp->Out.Left);
	    return;
	}
	/* Must be a working channel; spool and retry. */
	syslog(L_ERROR, "%s spooling %ld bytes", sp->Name, cp->Out.Left);
	if (SITEspool(sp, cp))
	    SITEflush(sp, FALSE);
	return;
    }

    /* Close the old channel if it was open. */
    if (cp != NULL) {
        /* Make sure we have no dangling pointers to it. */
	if (!Restart)
	    sp->Channel = NULL;
	CHANclose(cp, sp->Name);
	if (sp->Type == FTfile)
	    SITEunlink(sp);
    }
}


/*
**  Flush all sites.
*/
void SITEflushall(BOOL Restart)
{
    int	                i;
    SITE	        *sp;

    for (i = nSites, sp = Sites; --i >= 0; sp++)
	if (sp->Name)
	    SITEflush(sp, Restart);
}


/*
**  Run down the site's pattern list and see if it wants the specified
**  newsgroup.
*/
BOOL SITEwantsgroup(SITE *sp, char *name)
{
    BOOL	        match;
    BOOL	        subvalue;
    char	        *pat;
    char	        **argv;

    match = SUB_DEFAULT;
    if (ME.Patterns) {
	for (argv = ME.Patterns; (pat = *argv++) != NULL; ) {
	    subvalue = *pat != SUB_NEGATE && *pat != SUB_POISON;
	    if (!subvalue)
		pat++;
	    if ((match != subvalue) && wildmat(name, pat))
		match = subvalue;
	}
    }
    for (argv = sp->Patterns; (pat = *argv++) != NULL; ) {
	subvalue = *pat != SUB_NEGATE && *pat != SUB_POISON;
	if (!subvalue)
	    pat++;
	if ((match != subvalue) && wildmat(name, pat))
	    match = subvalue;
    }
    return match;
}


/*
**  Run down the site's pattern list and see if specified newsgroup is
**  considered poison.
*/
BOOL SITEpoisongroup(SITE *sp, char *name)
{
    BOOL	        match;
    BOOL	        poisonvalue;
    char	        *pat;
    char	        **argv;

    match = SUB_DEFAULT;
    if (ME.Patterns) {
	for (argv = ME.Patterns; (pat = *argv++) != NULL; ) {
	    poisonvalue = *pat == SUB_POISON;
	    if (*pat == SUB_NEGATE || *pat == SUB_POISON)
		pat++;
	    if (wildmat(name, pat))
		match = poisonvalue;
	}
    }
    for (argv = sp->Patterns; (pat = *argv++) != NULL; ) {
	poisonvalue = *pat == SUB_POISON;
	if (*pat == SUB_NEGATE || *pat == SUB_POISON)
	    pat++;
	if (wildmat(name, pat))
	    match = poisonvalue;
    }
    return match;
}


/*
**  Find a site.
*/
SITE *SITEfind(char *p)
{
    int	                i;
    SITE	        *sp;

    for (i = nSites, sp = Sites; --i >= 0; sp++)
	if (sp->Name && caseEQ(p, sp->Name))
	    return sp;
    return NULL;
}


/*
**  Find the next site that matches this site.
*/
SITE *SITEfindnext(char *p, SITE *sp)
{
    SITE	        *end;

    for (sp++, end = &Sites[nSites]; sp < end; sp++)
	if (sp->Name && caseEQ(p, sp->Name))
	    return sp;
    return NULL;
}


/*
**  Close a site down.
*/
void SITEfree(SITE *sp)
{
    SITE                *s;
    int                 new;
    int                 i;
    
    if (sp->Channel) {
	CHANclose(sp->Channel, CHANname(sp->Channel));
	sp->Channel = NULL;
    }

    SITEunlink(sp);

    sp->Name = NULL;
    if (sp->Process > 0) {
	/* Kill the backpointer so PROCdied won't call us. */
	PROCunwatch(sp->Process);
	sp->Process = -1;
    }
    if (sp->Entry) {
	DISPOSE(sp->Entry);
	sp->Entry = NULL;
    }
    if (sp->Originator) {
    	DISPOSE(sp->Originator);
    	sp->Originator = NULL;
    }
    if (sp->Param) {
	DISPOSE(sp->Param);
	sp->Param = NULL;
    }
    if (sp->SpoolName) {
	DISPOSE(sp->SpoolName);
	sp->SpoolName = NULL;
    }
    if (sp->Patterns) {
	DISPOSE(sp->Patterns);
	sp->Patterns = NULL;
    }
    if (sp->Exclusions) {
	DISPOSE(sp->Exclusions);
	sp->Exclusions = NULL;
    }
    if (sp->Distributions) {
	DISPOSE(sp->Distributions);
	sp->Distributions = NULL;
    }
    if (sp->Buffer.Data) {
	DISPOSE(sp->Buffer.Data);
	sp->Buffer.Data = NULL;
	sp->Buffer.Size = 0;
    }
    if (sp->FNLnames.Data) {
	DISPOSE(sp->FNLnames.Data);
	sp->FNLnames.Data = NULL;
	sp->FNLnames.Size = 0;
    }

    /* If this site was a master, find a new one. */
    if (sp->IsMaster) {
	for (new = NOSITE, s = Sites, i = nSites; --i >= 0; s++)
	    if (&Sites[s->Master] == sp)
		if (new == NOSITE) {
		    s->Master = NOSITE;
		    s->IsMaster = TRUE;
		    new = s - Sites;
		}
		else
		    s->Master = new;
	sp->IsMaster = FALSE;
    }
}


/*
**  If a site is an exploder or funnels into one, forward a command
**  to it.
*/
void SITEforward(SITE *sp, char *text)
{
    SITE	        *fsp;
    char	        *p;
    char		buff[SMBUF];

    fsp = sp;
    if (fsp->Funnel != NOSITE)
	fsp = &Sites[fsp->Funnel];
    if (sp->Name == NULL || fsp->Name == NULL)
	return;
    if (fsp->Type == FTexploder) {
	(void)strcpy(buff, text);
	if (fsp != sp && fsp->FNLwantsnames) {
	    p = buff + strlen(buff);
	    *p++ = ' ';
	    (void)strcpy(p, sp->Name);
	}
	SITEwrite(fsp, buff);
    }
}


/*
**  Drop a site.
*/
void SITEdrop(SITE *sp)
{
    SITEforward(sp, "drop");
    SITEflush(sp, FALSE);
    SITEfree(sp);
}


/*
**  Append info about the current state of the site to the buffer
*/
void SITEinfo(BUFFER *bp, SITE *sp, BOOL Verbose)
{
    static char		FREESITE[] = "<<No name>>\n\n";
    char	        *p;
    CHANNEL	        *cp;
    char		*sep;
    char		buff[BUFSIZ];

    if (sp->Name == NULL) {
	BUFFappend(bp, FREESITE, STRLEN(FREESITE));
	return;
    }

    p = buff;
    (void)sprintf(buff, "%s%s:\t", sp->Name, sp->IsMaster ? "(*)" : "");
    p += strlen(p);

    if (sp->Type == FTfunnel) {
	sp = &Sites[sp->Funnel];
	(void)sprintf(p, "funnel -> %s: ", sp->Name);
	p += strlen(p);
    }

    switch (sp->Type) {
    default:
	(void)sprintf(p, "unknown feed type %d", sp->Type);
	p += strlen(p);
	break;
    case FTerror:
    case FTfile:
	p += strlen(strcpy(p, "file"));
	if (sp->Buffered) {
	    (void)sprintf(p, " buffered(%ld)", sp->Buffer.Left);
	    p += strlen(p);
	}
	else if ((cp = sp->Channel) == NULL)
	    p += strlen(strcpy(p, " no channel?"));
	else {
	    (void)sprintf(p, " open fd=%d, in mem %ld", cp->fd, cp->Out.Left);
	    p += strlen(p);
	}
	break;
    case FTchannel:
	p += strlen(strcpy(p, "channel"));
	goto Common;
    case FTexploder:
	p += strlen(strcpy(p, "exploder"));
Common:
	if (sp->Process >= 0) {
	    (void)sprintf(p, " pid=%ld", (long) sp->pid);
	    p += strlen(p);
	}
	if (sp->Spooling)
	    p += strlen(strcpy(p, " spooling"));
	if ((cp = sp->Channel) == NULL)
	    p += strlen(strcpy(p, " no channel?"));
	else {
	    (void)sprintf(p, " fd=%d, in mem %ld", cp->fd, cp->Out.Left);
	    p += strlen(p);
	}
	break;
    case FTfunnel:
	p += strlen(strcpy(p, "recursive funnel"));
	break;
    case FTlogonly:
	p += strlen(strcpy(p, "log only"));
	break;
    case FTprogram:
	p += strlen(strcpy(p, "program"));
	if (sp->FNLwantsnames)
	    p += strlen(strcpy(p, " with names"));
	break;
    }
    *p++ = '\n';
    if (Verbose) {
	sep = "\t";
	if (sp->Buffered && sp->Flushpoint) {
	    (void)sprintf(p, "%sFlush @ %ld", sep, sp->Flushpoint);
	    p += strlen(p);
	    sep = "; ";
	}
	if (sp->StartWriting || sp->StopWriting) {
	    (void)sprintf(p, "%sWrite [%ld..%ld]", sep,
		sp->StopWriting, sp->StartWriting);
	    p += strlen(p);
	    sep = "; ";
	}
	if (sp->StartSpooling) {
	    (void)sprintf(p, "%sSpool @ %ld", sep, sp->StartSpooling);
	    p += strlen(p);
	    sep = "; ";
	}
	if (sep[0] != '\t')
	    *p++ = '\n';
	if (sp->Spooling && sp->SpoolName) {
	    (void)sprintf(p, "\tSpooling to \"%s\"\n", sp->SpoolName);
	    p += strlen(p);
	}
	if ((cp = sp->Channel) != NULL) {
	    (void)sprintf(p, "\tChannel created %.12s",
		ctime(&cp->Started) + 4);
	    p += strlen(p);
	    (void)sprintf(p, ", last active %.12s\n",
		ctime(&cp->LastActive) + 4);
	    p += strlen(p);
	    if (cp->Waketime > Now.time) {
		(void)sprintf(p, "\tSleeping until %.12s\n",
		    ctime(&cp->Waketime) + 4);
		p += strlen(p);
	    }
	}

    }
    BUFFappend(bp, buff, p - buff);
}

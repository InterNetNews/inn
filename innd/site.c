/*  $Id$
**
**  Routines to implement site-feeding.  Mainly working with channels to
**  do buffering and determine what to send.
*/

#include "config.h"
#include "clibrary.h"

#include "inn/innconf.h"
#include "innd.h"


static int	SITEcount;
static int	SITEhead = NOSITE;
static int	SITEtail = NOSITE;
static char	SITEshell[] = "/bin/sh";


/*
**  Called when input is ready to read.  Shouldn't happen.
*/
static void
SITEreader(CHANNEL *cp)
{
    syslog(L_ERROR, "%s internal SITEreader: %s", LogName, CHANname(cp));
}


/*
**  Called when write is done.  No-op.
*/
static void
SITEwritedone(CHANNEL *cp UNUSED)
{
}


/*
**  Make a site start spooling.
*/
static bool
SITEspool(SITE *sp, CHANNEL *cp)
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
	return false;
    }
    if (cp) {
      if (cp->fd >= 0)
        /* syslog(L_ERROR, "DEBUG ERROR SITEspool trashed:%d %s:%d", cp->fd, sp->Name, i);
	   CPU-eating bug, killed by kre. */
	WCHANremove(cp);
	RCHANremove(cp);
	SCHANremove(cp);
	close(cp->fd);
	cp->fd = i;
	return true;
    }
    sp->Channel = CHANcreate(i, CTfile, CSwriting, SITEreader, SITEwritedone);
    if (sp->Channel == NULL) {
	syslog(L_ERROR, "%s cant channel %m", sp->Name);
	close(i);
	return false;
    }
    WCHANset(sp->Channel, "", 0);
    sp->Spooling = true;
    return true;
}


/*
**  Delete a site from the file writing list.  Can be called even if
**  site is not on the list.
*/
static void
SITEunlink(SITE *sp)
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
static void
SITEbufferoldest(void)
{
    SITE	        *sp;
    struct buffer       *bp;
    struct buffer       *out;

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
    WCHANflush(sp->Channel);

    /* Get a buffer for the site. */
    sp->Buffered = true;
    bp = &sp->Buffer;
    bp->used = 0;
    bp->left = 0;
    if (bp->size == 0) {
	bp->size = sp->Flushpoint;
	bp->data = xmalloc(bp->size);
    }
    else {
	bp->size = sp->Flushpoint;
        bp->data = xrealloc(bp->data, bp->size);
    }

    /* If there's any unwritten data, copy it. */
    out = &sp->Channel->Out;
    if (out->left) {
        buffer_set(bp, &out->data[out->used], out->left);
	out->left = 0;
    }

    /* Now close the original channel. */
    CHANclose(sp->Channel, sp->Name);
    sp->Channel = NULL;
}

/*
 * *  Bilge Site's Channel out buffer.
 */
static bool
SITECHANbilge(SITE *sp)
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
        return false;
    }
    while (sp->Channel->Out.left > 0) {
        i = write(fd, &sp->Channel->Out.data[sp->Channel->Out.used],
                  sp->Channel->Out.left);
        if(i <= 0) {
            syslog(L_ERROR,"%s cant spool count %d", CHANname(sp->Channel),
                sp->Channel->Out.left);
            close(fd);
            return false;
        }
        sp->Channel->Out.left -= i;
        sp->Channel->Out.used += i;
    }
    close(fd);
    free(sp->Channel->Out.data);
    sp->Channel->Out.data = xmalloc(SMBUF);
    sp->Channel->Out.size = SMBUF;
    sp->Channel->Out.left = 0;
    sp->Channel->Out.used = 0;
    return true;
}

/*
**  Check if we need to write out the site's buffer.  If we're buffered
**  or the feed is backed up, this gets a bit complicated.
*/
static void
SITEflushcheck(SITE *sp, struct buffer *bp)
{
    int	                i;
    CHANNEL	        *cp;

    /* If we're buffered, and we hit the flushpoint, do an LRU. */
    if (sp->Buffered) {
	if (bp->left < sp->Flushpoint)
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
	buffer_set(bp, "", 0);
    }

    if (PROCneedscan)
	PROCscan();

    /* Handle buffering. */
    cp = sp->Channel;
    i = cp->Out.left;
    if (i < sp->StopWriting)
	WCHANremove(cp);
    if ((sp->StartWriting == 0 || i > sp->StartWriting)
     && !CHANsleeping(cp)) {
	if (sp->Type == FTchannel) {	/* channel feed, try the write */
	    int j;
	    if (bp->left == 0)
		return;
	    j = write(cp->fd, &bp->data[bp->used], bp->left);
	    if (j > 0) {
		bp->left -= j;
		bp->used += j;
		i = cp->Out.left;
		/* Since we just had a successful write, we need to clear the 
		 * channel's error counts. - dave@jetcafe.org */
		cp->BadWrites = 0;
		cp->BlockedWrites = 0;
	    }
	    if (bp->left <= 0) {
                /* reset Used, Left on bp, keep channel buffer size from
                   exploding. */
                bp->used = bp->left = 0;
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
void
SITEwrite(SITE *sp, const char *text)
{
    static char		PREFIX[] = { EXP_CONTROL, '\0' };
    struct buffer       *bp;

    if (sp->Buffered)
	bp = &sp->Buffer;
    else {
	if (sp->Channel == NULL)
	    return;
	sp->Channel->LastActive = Now.time;
	bp = &sp->Channel->Out;
    }
    buffer_append(bp, PREFIX, strlen(PREFIX));
    buffer_append(bp, text, strlen(text));
    buffer_append(bp, "\n", 1);
    if (sp->Channel != NULL)
	WCHANadd(sp->Channel);
}


/*
**  Send the desired data about an article down a channel.
*/
static void
SITEwritefromflags(SITE *sp, ARTDATA *Data)
{
    HDRCONTENT		*hc = Data->HdrContent;
    static char		ITEMSEP[] = " ";
    static char		NL[] = "\n";
    char		pbuff[12];
    char	        *p;
    bool	        Dirty;
    struct buffer       *bp;
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
    for (Dirty = false, p = sp->FileFlags; *p; p++) {
	switch (*p) {
	default:
	    syslog(L_ERROR, "%s internal SITEwritefromflags %c", sp->Name, *p);
	    continue;
	case FEED_BYTESIZE:
	    if (Dirty)
		buffer_append(bp, ITEMSEP, strlen(ITEMSEP));
	    buffer_append(bp, Data->Bytes + sizeof("Bytes: ") - 1,
                          Data->BytesLength);
	    break;
	case FEED_FULLNAME:
	case FEED_NAME:
	    if (Dirty)
		buffer_append(bp, ITEMSEP, strlen(ITEMSEP));
	    buffer_append(bp, Data->TokenText, sizeof(TOKEN) * 2 + 2);
	    break;
	case FEED_HASH:
	    if (Dirty)
		buffer_append(bp, ITEMSEP, strlen(ITEMSEP));
	    buffer_append(bp, "[", 1);
	    buffer_append(bp, HashToText(*(Data->Hash)), sizeof(HASH)*2);
	    buffer_append(bp, "]", 1);
	    break;
	case FEED_HDR_DISTRIB:
	    if (Dirty)
		buffer_append(bp, ITEMSEP, strlen(ITEMSEP));
	    buffer_append(bp, HDR(HDR__DISTRIBUTION),
                          HDR_LEN(HDR__DISTRIBUTION));
	    break;
	case FEED_HDR_NEWSGROUP:
	    if (Dirty)
		buffer_append(bp, ITEMSEP, strlen(ITEMSEP));
	    buffer_append(bp, HDR(HDR__NEWSGROUPS), HDR_LEN(HDR__NEWSGROUPS));
	    break;
	case FEED_HEADERS:
	    if (Dirty)
		buffer_append(bp, NL, strlen(NL));
	    buffer_append(bp, Data->Headers.data, Data->Headers.left);
	    break;
	case FEED_OVERVIEW:
	    if (Dirty)
		buffer_append(bp, ITEMSEP, strlen(ITEMSEP));
	    buffer_append(bp, Data->Overview.data, Data->Overview.left);
	    break;
	case FEED_PATH:
	    if (Dirty)
		buffer_append(bp, ITEMSEP, strlen(ITEMSEP));
	    if (!Data->Hassamepath)
		buffer_append(bp, Path.data, Path.used);
	    if (Data->AddAlias)
		buffer_append(bp, Pathalias.data, Pathalias.used);
	    buffer_append(bp, HDR(HDR__PATH), HDR_LEN(HDR__PATH));
	    break;
	case FEED_REPLIC:
	    if (Dirty)
		buffer_append(bp, ITEMSEP, strlen(ITEMSEP));
	    buffer_append(bp, Data->Replic, Data->ReplicLength);
	    break;
	case FEED_STOREDGROUP:
	    if (Dirty)
		buffer_append(bp, ITEMSEP, strlen(ITEMSEP));
	    buffer_append(bp, Data->Newsgroups.List[0],
                          Data->StoredGroupLength);
	    break;
	case FEED_TIMERECEIVED:
	    if (Dirty)
		buffer_append(bp, ITEMSEP, strlen(ITEMSEP));
	    snprintf(pbuff, sizeof(pbuff), "%ld", (long) Data->Arrived);
	    buffer_append(bp, pbuff, strlen(pbuff));
	    break;
	case FEED_TIMEPOSTED:
	    if (Dirty)
		buffer_append(bp, ITEMSEP, strlen(ITEMSEP));
	    snprintf(pbuff, sizeof(pbuff), "%ld", (long) Data->Posted);
	    buffer_append(bp, pbuff, strlen(pbuff));
	    break;
	case FEED_TIMEEXPIRED:
	    if (Dirty)
		buffer_append(bp, ITEMSEP, strlen(ITEMSEP));
	    snprintf(pbuff, sizeof(pbuff), "%ld", (long) Data->Expires);
	    buffer_append(bp, pbuff, strlen(pbuff));
	    break;
	case FEED_MESSAGEID:
	    if (Dirty)
		buffer_append(bp, ITEMSEP, strlen(ITEMSEP));
	    buffer_append(bp, HDR(HDR__MESSAGE_ID), HDR_LEN(HDR__MESSAGE_ID));
	    break;
	case FEED_FNLNAMES:
	    if (sp->FNLnames.data) {
		/* Funnel; write names of our sites that got it. */
		if (Dirty)
		    buffer_append(bp, ITEMSEP, strlen(ITEMSEP));
		buffer_append(bp, sp->FNLnames.data, sp->FNLnames.used);
	    }
	    else {
		/* Not funnel; write names of all sites that got it. */
		for (spx = Sites, i = nSites; --i >= 0; spx++)
		    if (spx->Sendit) {
			if (Dirty)
			    buffer_append(bp, ITEMSEP, strlen(ITEMSEP));
			buffer_append(bp, spx->Name, spx->NameLength);
			Dirty = true;
		    }
	    }
	    break;
	case FEED_NEWSGROUP:
	    if (Dirty)
		buffer_append(bp, ITEMSEP, strlen(ITEMSEP));
	    if (sp->ng)
		buffer_append(bp, sp->ng->Name, sp->ng->NameLength);
	    else
		buffer_append(bp, "?", 1);
	    break;
	case FEED_SITE:
	    if (Dirty)
		buffer_append(bp, ITEMSEP, strlen(ITEMSEP));
	    buffer_append(bp, Data->Feedsite, Data->FeedsiteLength);
	    break;
	}
	Dirty = true;
    }
    if (Dirty) {
	buffer_append(bp, "\n", 1);
	SITEflushcheck(sp, bp);
    }
}


/*
**  Send one article to a site.
*/
void
SITEsend(SITE *sp, ARTDATA *Data)
{
    int	                i;
    char	        *p;
    char		*temp;
    char		buff[BUFSIZ];
    char *		argv[MAX_BUILTIN_ARGV];

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
	    i = strlen(sp->Param) + sp->FNLnames.used;
	    if (i + (sizeof(TOKEN) * 2) + 3 >= sizeof buff) {
		syslog(L_ERROR, "%s toolong need %d for %s",
		    sp->Name, i + (sizeof(TOKEN) * 2) + 3, sp->Name);
		break;
	    }
	    temp = xmalloc(i + 1);
	    p = strchr(sp->Param, '*');
	    *p = '\0';
	    strlcpy(temp, sp->Param, i + 1);
	    strlcat(temp, sp->FNLnames.data, i + 1);
	    strlcat(temp, &p[1], i + 1);
	    *p = '*';
	    snprintf(buff, sizeof(buff), temp, Data->TokenText);
	    free(temp);
	}
	else
	    snprintf(buff, sizeof(buff), sp->Param, Data->TokenText);

	if (NeedShell(buff, (const char **)argv, (const char **)ARRAY_END(argv))) {
	    argv[0] = SITEshell;
	    argv[1] = (char *) "-c";
	    argv[2] = buff;
	    argv[3] = NULL;
	}

	/* Start the process. */
	i = Spawn(sp->Nice, 0, fileno(Errlog), fileno(Errlog), argv);
	if (i >= 0)
	    PROCwatch(i, -1);
	break;
    }
}


/*
**  The channel was sleeping because we had to spool our output to
**  a file.  Flush and restart.
*/
static void
SITEspoolwake(CHANNEL *cp)
{
    SITE	*sp;
    int		*ip;

    ip = (int *) cp->Argument;
    sp = &Sites[*ip];
    free(cp->Argument);
    cp->Argument = NULL;
    if (sp->Channel != cp) {
	syslog(L_ERROR, "%s internal SITEspoolwake %s got %d, not %d",
	    LogName, sp->Name, cp->fd, sp->Channel->fd);
        return;
    }
    syslog(L_NOTICE, "%s spoolwake", sp->Name);
    SITEflush(sp, true);
}


/*
**  Start up a process for a channel, or a spool to a file if we can't.
**  Create a channel for the site to talk to.
*/
static bool
SITEstartprocess(SITE *sp)
{
    pid_t	        i;
    char		*argv[MAX_BUILTIN_ARGV];
    char		*process;
    int			*ip;
    int			pan[2];

#if HAVE_SOCKETPAIR
    /* Create a socketpair. */
    if (socketpair(PF_UNIX, SOCK_STREAM, 0, pan) < 0) {
	syslog(L_ERROR, "%s cant socketpair %m", sp->Name);
	return false;
    }
#else
    /* Create a pipe. */
    if (pipe(pan) < 0) {
	syslog(L_ERROR, "%s cant pipe %m", sp->Name);
	return false;
    }
#endif
    close_on_exec(pan[PIPE_WRITE], true);

    /* Set up the argument vector. */
    process = xstrdup(sp->Param);
    if (NeedShell(process, (const char **)argv, (const char **)ARRAY_END(argv))) {
	argv[0] = SITEshell;
	argv[1] = (char *) "-c";
	argv[2] = process;
	argv[3] = NULL;
    }

    /* Fork a child. */
    i = Spawn(sp->Nice, pan[PIPE_READ], (int)fileno(Errlog),
	      (int)fileno(Errlog), argv);
    if (i > 0) {
	sp->pid = i;
	sp->Spooling = false;
	sp->Process = PROCwatch(i, sp - Sites);
	close(pan[PIPE_READ]);
	sp->Channel = CHANcreate(pan[PIPE_WRITE],
			sp->Type == FTchannel ? CTprocess : CTexploder,
			CSwriting, SITEreader, SITEwritedone);
	free(process);
	return true;
    }

    /* Error.  Switch to spooling. */
    syslog(L_ERROR, "%s cant spawn spooling %m", sp->Name);
    close(pan[PIPE_WRITE]);
    close(pan[PIPE_READ]);
    free(process);
    if (!SITEspool(sp, (CHANNEL *)NULL))
	return false;

    /* We'll try to restart the channel later. */
    syslog(L_ERROR, "%s cant spawn spooling %m", sp->Name);
    ip = xmalloc(sizeof(int));
    *ip = sp - Sites;
    SCHANadd(sp->Channel, Now.time + innconf->chanretrytime, NULL,
             SITEspoolwake, ip);
    return true;
}


/*
**  Set up a site for internal buffering.
*/
static void
SITEbuffer(SITE *sp)
{
    struct buffer *bp;

    SITEunlink(sp);
    sp->Buffered = true;
    sp->Channel = NULL;
    bp = &sp->Buffer;
    buffer_resize(bp, sp->Flushpoint);
    buffer_set(bp, "", 0);
    syslog(L_NOTICE, "%s buffered", sp->Name);
}


/*
**  Link a site at the head of the "currently writing to a file" list
*/
static void
SITEmovetohead(SITE *sp)
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
bool
SITEsetup(SITE *sp)
{
    int			fd;
    int			oerrno;

    switch (sp->Type) {
    default:
	syslog(L_ERROR, "%s internal SITEsetup %d",
	    sp->Name, sp->Type);
	return false;
    case FTfunnel:
    case FTlogonly:
    case FTprogram:
	/* Nothing to do here. */
	break;
    case FTfile:
	if (SITEcount >= MaxOutgoing)
	    SITEbuffer(sp);
	else {
	    sp->Buffered = false;
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
		return false;
	    }
	    SITEmovetohead(sp);
	    sp->Channel = CHANcreate(fd, CTfile, CSwriting,
			    SITEreader, SITEwritedone);
	    syslog(L_NOTICE, "%s opened %s", sp->Name, CHANname(sp->Channel));
	    WCHANset(sp->Channel, "", 0);
	}
	break;
    case FTchannel:
    case FTexploder:
	if (!SITEstartprocess(sp))
	    return false;
	syslog(L_NOTICE, "%s spawned %s", sp->Name, CHANname(sp->Channel));
	WCHANset(sp->Channel, "", 0);
	WCHANadd(sp->Channel);
	break;
    }
    return true;
}


/*
**  A site's channel process died; restart it.
*/
void
SITEprocdied(SITE *sp, int process, PROCESS *pp)
{
    syslog(pp->Status ? L_ERROR : L_NOTICE, "%s exit %d elapsed %ld pid %ld",
	sp->Name ? sp->Name : "?", pp->Status,
	(long) (pp->Collected - pp->Started), (long) pp->Pid);
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
void
SITEchanclose(CHANNEL *cp)
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
		syslog(L_ERROR, "%s loss %d bytes", sp->Name, cp->Out.left);
		return;
	    }
	    WCHANsetfrombuffer(sp->Channel, &cp->Out);
	    WCHANadd(sp->Channel);
	    ip = xmalloc(sizeof(int));
	    *ip = sp - Sites;
	    SCHANadd(sp->Channel, Now.time + innconf->chanretrytime, NULL,
                     SITEspoolwake, ip);
	    break;
	}
}


/*
**  Flush any pending data waiting to be sent.
*/
void
SITEflush(SITE *sp, const bool Restart)
{
    CHANNEL	        *cp;
    struct buffer       *out;
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
	    WCHANflush(cp);
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
		buffer_swap(&sp->Buffer, &sp->Channel->Out);
	    SITEcount += count;
	}
	break;
    }

    /* We're only dealing with files and channels now. */
    if ((cp = sp->Channel) != NULL)
	WCHANflush(cp);

    /* Restart the site, copy any pending data. */
    if (Restart) {
	if (!SITEsetup(sp))
	    syslog(L_ERROR, "%s cant restart %m", sp->Name);
	else if (cp != NULL) {
	    if (sp->Buffered) {
		/* SITEsetup had to buffer us; save any residue. */
		out = &cp->Out;
	        if (out->left)
		    buffer_set(&sp->Buffer, &out->data[out->used], out->left);
	    }
	    else
		WCHANsetfrombuffer(sp->Channel, &cp->Out);
	}
    }
    else if (cp != NULL && cp->Out.left) {
	if (sp->Type == FTfile || sp->Spooling) {
	    /* Can't flush a file?  Hopeless. */
	    syslog(L_ERROR, "%s dataloss %d", sp->Name, cp->Out.left);
	    return;
	}
	/* Must be a working channel; spool and retry. */
	syslog(L_ERROR, "%s spooling %d bytes", sp->Name, cp->Out.left);
	if (SITEspool(sp, cp))
	    SITEflush(sp, false);
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
void
SITEflushall(const bool Restart)
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
bool
SITEwantsgroup(SITE *sp, char *name)
{
    bool	        match;
    bool	        subvalue;
    char	        *pat;
    char	        **argv;

    match = SUB_DEFAULT;
    if (ME.Patterns) {
	for (argv = ME.Patterns; (pat = *argv++) != NULL; ) {
	    subvalue = *pat != SUB_NEGATE && *pat != SUB_POISON;
	    if (!subvalue)
		pat++;
	    if ((match != subvalue) && uwildmat(name, pat))
		match = subvalue;
	}
    }
    for (argv = sp->Patterns; (pat = *argv++) != NULL; ) {
	subvalue = *pat != SUB_NEGATE && *pat != SUB_POISON;
	if (!subvalue)
	    pat++;
	if ((match != subvalue) && uwildmat(name, pat))
	    match = subvalue;
    }
    return match;
}


/*
**  Run down the site's pattern list and see if specified newsgroup is
**  considered poison.
*/
bool
SITEpoisongroup(SITE *sp, char *name)
{
    bool	        match;
    bool	        poisonvalue;
    char	        *pat;
    char	        **argv;

    match = SUB_DEFAULT;
    if (ME.Patterns) {
	for (argv = ME.Patterns; (pat = *argv++) != NULL; ) {
	    poisonvalue = *pat == SUB_POISON;
	    if (*pat == SUB_NEGATE || *pat == SUB_POISON)
		pat++;
	    if (uwildmat(name, pat))
		match = poisonvalue;
	}
    }
    for (argv = sp->Patterns; (pat = *argv++) != NULL; ) {
	poisonvalue = *pat == SUB_POISON;
	if (*pat == SUB_NEGATE || *pat == SUB_POISON)
	    pat++;
	if (uwildmat(name, pat))
	    match = poisonvalue;
    }
    return match;
}


/*
**  Find a site.
*/
SITE *
SITEfind(const char *p)
{
    int	                i;
    SITE	        *sp;

    for (i = nSites, sp = Sites; --i >= 0; sp++)
	if (sp->Name && strcasecmp(p, sp->Name) == 0)
	    return sp;
    return NULL;
}


/*
**  Find the next site that matches this site.
*/
SITE *
SITEfindnext(const char *p, SITE *sp)
{
    SITE	        *end;

    for (sp++, end = &Sites[nSites]; sp < end; sp++)
	if (sp->Name && strcasecmp(p, sp->Name) == 0)
	    return sp;
    return NULL;
}


/*
**  Close a site down.
*/
void
SITEfree(SITE *sp)
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
	free(sp->Entry);
	sp->Entry = NULL;
    }
    if (sp->Originator) {
    	free(sp->Originator);
    	sp->Originator = NULL;
    }
    if (sp->Param) {
	free(sp->Param);
	sp->Param = NULL;
    }
    if (sp->SpoolName) {
	free(sp->SpoolName);
	sp->SpoolName = NULL;
    }
    if (sp->Patterns) {
	free(sp->Patterns);
	sp->Patterns = NULL;
    }
    if (sp->Exclusions) {
	free(sp->Exclusions);
	sp->Exclusions = NULL;
    }
    if (sp->Distributions) {
	free(sp->Distributions);
	sp->Distributions = NULL;
    }
    if (sp->Buffer.data) {
	free(sp->Buffer.data);
	sp->Buffer.data = NULL;
	sp->Buffer.size = 0;
    }
    if (sp->FNLnames.data) {
	free(sp->FNLnames.data);
	sp->FNLnames.data = NULL;
	sp->FNLnames.size = 0;
    }

    /* If this site was a master, find a new one. */
    if (sp->IsMaster) {
	for (new = NOSITE, s = Sites, i = nSites; --i >= 0; s++)
	    if (&Sites[s->Master] == sp) {
		if (new == NOSITE) {
		    s->Master = NOSITE;
		    s->IsMaster = true;
		    new = s - Sites;
		}
		else
		    s->Master = new;
            }
	sp->IsMaster = false;
    }
}


/*
**  If a site is an exploder or funnels into one, forward a command
**  to it.
*/
void
SITEforward(SITE *sp, const char *text)
{
    SITE	        *fsp;
    char		buff[SMBUF];

    fsp = sp;
    if (fsp->Funnel != NOSITE)
	fsp = &Sites[fsp->Funnel];
    if (sp->Name == NULL || fsp->Name == NULL)
	return;
    if (fsp->Type == FTexploder) {
	strlcpy(buff, text, sizeof(buff));
	if (fsp != sp && fsp->FNLwantsnames) {
            strlcat(buff, " ", sizeof(buff));
            strlcat(buff, sp->Name, sizeof(buff));
	}
	SITEwrite(fsp, buff);
    }
}


/*
**  Drop a site.
*/
void
SITEdrop(SITE *sp)
{
    SITEforward(sp, "drop");
    SITEflush(sp, false);
    SITEfree(sp);
}


/*
**  Append info about the current state of the site to the buffer
*/
void
SITEinfo(struct buffer *bp, SITE *sp, const bool Verbose)
{
    static char		FREESITE[] = "<<No name>>\n\n";
    char	        *p;
    CHANNEL	        *cp;
    const char		*sep;
    char		buff[BUFSIZ];

    if (sp->Name == NULL) {
	buffer_append(bp, FREESITE, strlen(FREESITE));
	return;
    }

    p = buff;
    snprintf(buff, sizeof(buff), "%s%s:\t", sp->Name,
             sp->IsMaster ? "(*)" : "");
    p += strlen(p);

    if (sp->Type == FTfunnel) {
	sp = &Sites[sp->Funnel];
	sprintf(p, "funnel -> %s: ", sp->Name);
	p += strlen(p);
    }

    switch (sp->Type) {
    default:
	sprintf(p, "unknown feed type %d", sp->Type);
	p += strlen(p);
	break;
    case FTerror:
    case FTfile:
	p += strlen(strcpy(p, "file"));
	if (sp->Buffered) {
	    sprintf(p, " buffered(%d)", sp->Buffer.left);
	    p += strlen(p);
	}
	else if ((cp = sp->Channel) == NULL)
	    p += strlen(strcpy(p, " no channel?"));
	else {
	    sprintf(p, " open fd=%d, in mem %d", cp->fd, cp->Out.left);
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
	    sprintf(p, " pid=%ld", (long) sp->pid);
	    p += strlen(p);
	}
	if (sp->Spooling)
	    p += strlen(strcpy(p, " spooling"));
	if ((cp = sp->Channel) == NULL)
	    p += strlen(strcpy(p, " no channel?"));
	else {
	    sprintf(p, " fd=%d, in mem %d", cp->fd, cp->Out.left);
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
	    sprintf(p, "%sFlush @ %lu", sep, (unsigned long) sp->Flushpoint);
	    p += strlen(p);
	    sep = "; ";
	}
	if (sp->StartWriting || sp->StopWriting) {
	    sprintf(p, "%sWrite [%ld..%ld]", sep,
		sp->StopWriting, sp->StartWriting);
	    p += strlen(p);
	    sep = "; ";
	}
	if (sp->StartSpooling) {
	    sprintf(p, "%sSpool @ %ld", sep, sp->StartSpooling);
	    p += strlen(p);
	    sep = "; ";
	}
	if (sep[0] != '\t')
	    *p++ = '\n';
	if (sp->Spooling && sp->SpoolName) {
	    sprintf(p, "\tSpooling to \"%s\"\n", sp->SpoolName);
	    p += strlen(p);
	}
	if ((cp = sp->Channel) != NULL) {
	    sprintf(p, "\tChannel created %.12s",
		ctime(&cp->Started) + 4);
	    p += strlen(p);
	    sprintf(p, ", last active %.12s\n",
		ctime(&cp->LastActive) + 4);
	    p += strlen(p);
	    if (cp->Waketime > Now.time) {
		sprintf(p, "\tSleeping until %.12s\n",
		    ctime(&cp->Waketime) + 4);
		p += strlen(p);
	    }
	}

    }
    buffer_append(bp, buff, p - buff);
}

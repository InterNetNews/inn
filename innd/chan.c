/*  $Id$
**
**  I/O channel (and buffer) processing.
*/

#include "config.h"
#include "clibrary.h"

/* Needed on AIX 4.1 to get fd_set and friends. */
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif

#include "innd.h"

/* These errno values don't exist on all systems, but may be returned as an
   (ignorable) error to setting the accept socket nonblocking.  Define them
   to 0 if they don't exist so that we can unconditionally compare errno to
   them in the code. */
#ifndef ENOTSOCK
# define ENOTSOCK 0
#endif
#ifndef ENOTTY
# define ENOTTY 0
#endif

static const char * const timer_name[] = {
    "idle", "artclean", "artwrite", "artcncl", "sitesend", "overv",
    "perl", "python", "nntpread", "artparse", "artlog", "datamove"
};

/* Minutes - basically, keep the connection open but idle */
#define PAUSE_BEFORE_DROP               5

/* Divisor of the BUFFER size. If the amount free at the beginning of the
   buffer is bigger than the quotient, then it is compacted in the
   readloop */
#define COMP_THRESHOLD 10

static fd_set	RCHANmask;
static fd_set	SCHANmask;
static fd_set	WCHANmask;
static int	SCHANcount;
static int	CHANlastfd;
static int	CHANlastsleepfd;
static int	CHANccfd;
static int	CHANtablesize;
static CHANNEL	*CHANtable;
static CHANNEL	*CHANcc;
static CHANNEL	CHANnull = { CTfree, CSerror, -1 };

#define PRIORITISE_REMCONN
#ifdef PRIORITISE_REMCONN
static int	*CHANrcfd;
static CHANNEL	**CHANrc;
static int	chanlimit;
#endif /* PRIORITISE_REMCONN */

/*
**  Append data to a buffer.
*/
void
BUFFappend(BUFFER *bp, const char *p, const int len) {
    int i;
    
    if (len == 0)
	return;
    /* Note end of buffer, grow it if we need more room */
    i = bp->Used + bp->Left;
    if (i + len > bp->Size) {
	/* Round size up to next 1K */
	bp-> Size += (len + 0x3FF) & ~0x3FF;
	RENEW(bp->Data, char, bp->Size);
    }
    bp->Left += len;
    memcpy(&bp->Data[i], p, len);
}

/*
**  Set a buffer's contents, ignoring anything that might have
**  been there.
*/
void
BUFFset(BUFFER *bp, const char *p, const int length)
{
    if ((bp->Left = length) != 0) {
	/* Need more space? */
	if (bp->Size < length) {
	    bp->Size = length;
	    RENEW(bp->Data, char, bp->Size);
	}
	
	/* Try to test for non-overlapping copies. */
	memmove(bp->Data, p, length);
    }
    bp->Used = 0;
}

/*
**  Swap the contents of two buffers.
*/
void
BUFFswap(BUFFER *b1, BUFFER *b2)
{
    BUFFER		b;

    b = *b1;
    *b1 = *b2;
    *b2 = b;
}

/*
**  Trim '\r' from buffer.
*/
void
BUFFtrimcr(BUFFER *bp)
{
    char *p, *q;
    int trimmed = 0;

    for (p = q = bp->Data ; p < bp->Data + bp->Left ; p++) {
	if (*p == '\r' && p+1 < bp->Data + bp->Left && p[1] == '\n') {
	    trimmed++;
	    continue;
	}
	*q++ = *p;
    }
    bp->Left -= trimmed;
}

/*
** Tear down our world
*/
void
CHANshutdown(void)
{
  CHANNEL	        *cp;
  int			i;

  if (CHANtable) {
    for (i = CHANtablesize, cp = &CHANtable[0]; --i >= 0; cp++) {
      if (cp->In.Data) {
	DISPOSE(cp->In.Data);
      }
      if (cp->Out.Data) {
	DISPOSE(cp->Out.Data);
      }
    }
    DISPOSE(CHANtable);
    CHANtable = NULL;
  }
}

/*
**  Initialize all the I/O channels.
*/
void
CHANsetup(int i)
{
    CHANNEL	        *cp;

    FD_ZERO(&RCHANmask);
    FD_ZERO(&SCHANmask);
    FD_ZERO(&WCHANmask);
    CHANshutdown();
    CHANtablesize = i;
    CHANtable = NEW(CHANNEL, CHANtablesize);
    memset(CHANtable, 0, CHANtablesize * sizeof *CHANtable);
    CHANnull.NextLog = innconf->chaninacttime;
    memset( &CHANnull.Address, 0, sizeof( CHANnull.Address ) );
    for (cp = CHANtable; --i >= 0; cp++)
	*cp = CHANnull;
}


/*
**  Create a channel from a descriptor.
*/
CHANNEL *
CHANcreate(int fd, CHANNELTYPE Type, CHANNELSTATE State,
           innd_callback_t Reader, innd_callback_t WriteDone)
{
    CHANNEL	        *cp;
    BUFFER		in;
    BUFFER		out;

    cp = &CHANtable[fd];

    /* Don't overwrite the buffers with CHANnull. */
    in = cp->In;
    if (in.Size == 0) {
	in.Size = START_BUFF_SIZE;
	in.Data = NEW(char, in.Size);
    }
    in.Used = 0;
    in.Left = in.Size;
    out = cp->Out;
    if (out.Size == 0) {
	out.Size = SMBUF;
	out.Data = NEW(char, out.Size);
    }
    out.Used = 0;
    out.Left = 0;

    /* Set up the channel's info. */
    *cp = CHANnull;
    cp->fd = fd;
    cp->Type = Type;
    cp->State = State;
    cp->Streaming = FALSE;
    cp->Skip = FALSE;
    cp->NoResendId = FALSE;
    cp->privileged = FALSE;
    cp->Ihave = cp->Ihave_Duplicate = cp->Ihave_Deferred = cp->Ihave_SendIt = cp->Ihave_Cybercan = 0;
    cp->Check = cp->Check_send = cp->Check_deferred = cp->Check_got = cp->Check_cybercan = 0;
    cp->Takethis = cp->Takethis_Ok = cp->Takethis_Err = 0;
    cp->Size = cp->Duplicate = 0;
    cp->Unwanted_s = cp->Unwanted_f = cp->Unwanted_d = 0;
    cp->Unwanted_g = cp->Unwanted_u = cp->Unwanted_o = 0;
    cp->Reader = Reader;
    cp->WriteDone = WriteDone;
    cp->Started = cp->LastActive = Now.time;
    cp->In = in;
    cp->Out = out;
    cp->Tracing = Tracing;
    cp->Sendid.Size = 0;
    cp->Next=0;
    cp->MaxCnx=0;
    cp->ActiveCnx=0;
    cp->ArtBeg = 0;
    cp->ArtMax = 0;
    cp->Start = 0;
    HashClear(&cp->CurrentMessageIDHash);
    memset(cp->PrecommitWIP, '\0', sizeof(cp->PrecommitWIP));
    cp->PrecommitiCachenext=0;
    ARTprepare(cp);

    close_on_exec(fd, true);

#ifndef _HPUX_SOURCE
    /* Stupid HPUX 11.00 has a broken listen/accept where setting the listen
       socket to nonblocking prevents you from successfully setting the
       socket returned by accept(2) back to blocking mode, no matter what,
       resulting in all kinds of funny behaviour, data loss, etc. etc.  */
    if (nonblocking(fd, true) < 0 && errno != ENOTSOCK && errno != ENOTTY)
	syslog(L_ERROR, "%s cant nonblock %d %m", LogName, fd);
#endif

    /* Note control channel, for efficiency. */
    if (Type == CTcontrol) {
	CHANcc = cp;
	CHANccfd = fd;
    }
#ifdef PRIORITISE_REMCONN
    /* Note remconn channel, for efficiency */
    if (Type == CTremconn) {
	int j;
	for (j = 0 ; j < chanlimit ; j++ ) {
	    if (CHANrcfd[j] == -1) {
		break;
	    }
	}
	if (j < chanlimit) {
	    CHANrc[j] = cp;
	    CHANrcfd[j] = fd;
	} else if (chanlimit == 0) {
	    /* assuming two file descriptors(AF_INET and AF_INET6) */
	    chanlimit = 2;
	    CHANrc = NEW(CHANNEL **, chanlimit);
	    CHANrcfd = NEW(int *, chanlimit);
	    for (j = 0 ; j < chanlimit ; j++ ) {
		CHANrc[j] = NULL;
		CHANrcfd[j] = -1;
	    }
	    CHANrc[0] = cp;
	    CHANrcfd[0] = fd;
	} else {
	    /* extend to double size */
	    RENEW(CHANrc, CHANNEL **, chanlimit * 2);
	    RENEW(CHANrcfd, int *, chanlimit * 2);
	    for (j = chanlimit ; j < chanlimit * 2 ; j++ ) {
		CHANrc[j] = NULL;
		CHANrcfd[j] = -1;
	    }
	    CHANrc[chanlimit] = cp;
	    CHANrcfd[chanlimit] = fd;
	    chanlimit *= 2;
	}
    }
#endif /* PRIORITISE_REMCONN */
    return cp;
}


/*
**  Start tracing a channel.
*/
void
CHANtracing(CHANNEL *cp, bool Flag)
{
    char		*p;

    p = CHANname(cp);
    syslog(L_NOTICE, "%s trace %s", p, Flag ? "on" : "off");
    cp->Tracing = Flag;
    if (Flag) {
	syslog(L_NOTICE, "%s trace badwrites %d blockwrites %d badreads %d",
	    p, cp->BadWrites, cp->BlockedWrites, cp->BadReads);
	syslog(L_NOTICE, "%s trace address %s lastactive %ld nextlog %ld",
	    p, sprint_sockaddr((struct sockaddr *)&cp->Address),
	    cp->LastActive, cp->NextLog);
	if (FD_ISSET(cp->fd, &SCHANmask))
	    syslog(L_NOTICE, "%s trace sleeping %ld 0x%p",
		p, (long)cp->Waketime, cp->Waker);
	if (FD_ISSET(cp->fd, &RCHANmask))
	    syslog(L_NOTICE, "%s trace reading %d %s",
		p, cp->In.Used, MaxLength(cp->In.Data, cp->In.Data));
	if (FD_ISSET(cp->fd, &WCHANmask))
	    syslog(L_NOTICE, "%s trace writing %d %s",
		p, cp->Out.Left, MaxLength(cp->Out.Data, cp->Out.Data));
    }
}


/*
**  Close a channel.
*/
void
CHANclose(CHANNEL *cp, const char *name)
{
    char	*label, *tmplabel, buff[SMBUF];

    if (cp->Type == CTfree)
	syslog(L_ERROR, "%s internal closing free channel %d", name, cp->fd);
    else {
	if (cp->Type == CTnntp) {
	    WIPprecomfree(cp);
	    NCclearwip(cp);
            if (cp->State == CScancel)
                syslog(L_NOTICE,
               "%s closed seconds %ld cancels %ld",
               name, (long)(Now.time - cp->Started),
               cp->Received);
            else {
	    sprintf(buff, "accepted size %.0f duplicate size %.0f", cp->Size, cp->DuplicateSize);
	    syslog(L_NOTICE,
		"%s closed seconds %ld accepted %ld refused %ld rejected %ld duplicate %ld %s",
		name, (long)(Now.time - cp->Started),
		cp->Received, cp->Refused, cp->Rejected,
		cp->Duplicate, buff);
	    }
	    if (cp->Data.Newsgroups.Data != NULL) {
		DISPOSE(cp->Data.Newsgroups.Data);
		cp->Data.Newsgroups.Data = NULL;
	    }
	    if (cp->Data.Newsgroups.List != NULL) {
		DISPOSE(cp->Data.Newsgroups.List);
		cp->Data.Newsgroups.List = NULL;
	    }
	    if (cp->Data.Distribution.Data != NULL) {
		DISPOSE(cp->Data.Distribution.Data);
		cp->Data.Distribution.Data = NULL;
	    }
	    if (cp->Data.Distribution.List != NULL) {
		DISPOSE(cp->Data.Distribution.List);
		cp->Data.Distribution.List = NULL;
	    }
	    if (cp->Data.Path.Data != NULL) {
		DISPOSE(cp->Data.Path.Data);
		cp->Data.Path.Data = NULL;
	    }
	    if (cp->Data.Path.List != NULL) {
		DISPOSE(cp->Data.Path.List);
		cp->Data.Path.List = NULL;
	    }
	    if (cp->Data.Overview.Size != 0) {
		DISPOSE(cp->Data.Overview.Data);
		cp->Data.Overview.Data = NULL;
		cp->Data.Overview.Size = 0;
	    }
	    if (cp->Data.XrefBufLength != 0) {
		DISPOSE(cp->Data.Xref);
		cp->Data.Xref = NULL;
		cp->Data.XrefBufLength = 0;
	    }
	} else if (cp->Type == CTreject)
	    syslog(L_NOTICE, "%s %ld", name, cp->Rejected);
	else if (cp->Out.Left)
	    syslog(L_NOTICE, "%s closed lost %d", name, cp->Out.Left);
	else
	    syslog(L_NOTICE, "%s closed", name);
	WCHANremove(cp);
	RCHANremove(cp);
	SCHANremove(cp);
	if (cp->Argument != NULL)
	    /* Set to NULL below. */
	    DISPOSE(cp->Argument);
	if (cp->fd >= 0 && close(cp->fd) < 0)
	    syslog(L_ERROR, "%s cant close %s %m", LogName, name);
 
	if (cp->MaxCnx > 0 && cp->Type == CTnntp) {
	    int tfd;
	    CHANNEL *tempchan;

	    cp->fd = -1;
	    if ((label = RClabelname(cp)) != NULL) {
		for(tfd = 0; tfd <= CHANlastfd; tfd++) {
		    tempchan = &CHANtable[tfd];
		    if(tempchan->fd > 0 &&
			((tmplabel = RClabelname(tempchan)) != NULL) &&
			strcmp(label, tmplabel) == 0 &&
			tempchan->ActiveCnx == 0) {
			    tempchan->ActiveCnx = cp->ActiveCnx;
			    RCHANadd(tempchan);
			    break;
		    }
		}
	    }
	}
    }

    /* Mark it unused. */
    cp->Type = CTfree;
    cp->State = CSerror;
    cp->fd = -1;
    cp->Argument = NULL;
    cp->ActiveCnx = 0;

    /* Free the buffers if they got big. */
    if (cp->In.Size > BIG_BUFFER) {
	cp->In.Size = 0;
	DISPOSE(cp->In.Data);
	cp->In.Data = NULL; /* avoid spurious free()s of already freed data in CHANshutdown */
    }
    if (cp->Out.Size > BIG_BUFFER) {
	cp->Out.Size = 0;
	DISPOSE(cp->Out.Data);
	cp->Out.Data = NULL; /* ditto */
    }
    if (cp->Sendid.Size) {
	cp->Sendid.Size = 0;
	DISPOSE(cp->Sendid.Data);
    }
}


/*
**  Return a printable name for the channel.
*/
char *
CHANname(const CHANNEL *cp)
{
    static char		buff[SMBUF];
    int	                i;
    SITE *	        sp;
    const char *	p;
    pid_t		pid;

    switch (cp->Type) {
    default:
	(void)sprintf(buff, "?%d(#%d@%d)?", cp->Type, cp->fd, cp - CHANtable);
	break;
    case CTany:
	(void)sprintf(buff, "any:%d", cp->fd);
	break;
    case CTfree:
	(void)sprintf(buff, "free:%d", cp->fd);
	break;
    case CTremconn:
	(void)sprintf(buff, "remconn:%d", cp->fd);
	break;
    case CTreject:
	(void)sprintf(buff, "%s rejected", RChostname(cp));
	break;
    case CTnntp:
	(void)sprintf(buff, "%s:%d",
		cp->Address.ss_family == 0 ? "localhost" : RChostname(cp),
		cp->fd);
	break;
    case CTlocalconn:
	(void)sprintf(buff, "localconn:%d", cp->fd);
	break;
    case CTcontrol:
	(void)sprintf(buff, "control:%d", cp->fd);
	break;
    case CTexploder:
    case CTfile:
    case CTprocess:
	/* Find the site that has this channel. */
	for (p = "?", i = nSites, sp = Sites, pid = 0; --i >= 0; sp++)
	    if (sp->Channel == cp) {
		p = sp->Name;
		if (cp->Type != CTfile)
		    pid = sp->pid;
		break;
	    }
	if (pid == 0)
	    (void)sprintf(buff, "%s:%d:%s",
		    MaxLength(p, p), cp->fd,
		    cp->Type == CTfile ? "file" : "proc");
	else
	    (void)sprintf(buff, "%s:%d:%s:%ld",
		    MaxLength(p, p), cp->fd,
		    cp->Type == CTfile ? "file" : "proc", (long)pid);
	break;
    }
    return buff;
}


/*
**  Return the channel for a specified descriptor.
*/
CHANNEL *
CHANfromdescriptor(int fd)
{
    if (fd <0 || fd > CHANtablesize)
	return NULL;
    return &CHANtable[fd];
}


/*
**  Iterate over all channels of a specified type.
*/
CHANNEL *
CHANiter(int *ip, CHANNELTYPE Type)
{
    CHANNEL	        *cp;
    int	                i;

    if ((i = *ip) >= 0 && i < CHANtablesize) {
	do {
	    cp = &CHANtable[i];
	    if (cp->Type == CTfree && cp->fd == -1)
		continue;
	    if (Type == CTany || cp->Type == Type) {
		*ip = ++i;
		return cp;
	    }
	} while (++i < CHANtablesize);
    }
    return NULL;
}


/*
**  Mark a channel as an active reader.
*/
void
RCHANadd(CHANNEL *cp)
{
    FD_SET(cp->fd, &RCHANmask);
    if (cp->fd > CHANlastfd)
	CHANlastfd = cp->fd;

    if (cp->Type != CTnntp)
	/* Start reading at the beginning of the buffer. */
	cp->In.Used = 0;
}


/*
**  Remove a channel from the set of readers.
*/
void
RCHANremove(CHANNEL *cp)
{
    if (FD_ISSET(cp->fd, &RCHANmask)) {
	FD_CLR(cp->fd, &RCHANmask);
	if (cp->fd == CHANlastfd) {
	    /* This was the highest descriptor, get a new highest. */
	    while (!FD_ISSET(CHANlastfd, &RCHANmask)
	      && !FD_ISSET(CHANlastfd, &WCHANmask)
	      && CHANlastfd > 1)
		CHANlastfd--;
	}
    }
}


/*
**  Put a channel to sleep, call a function when it wakes.
**  Note that the Argument must be NULL or allocated memory!
*/
void
SCHANadd(CHANNEL *cp, time_t Waketime, void *Event, innd_callback_t Waker,
         void *Argument)
{
    if (!FD_ISSET(cp->fd, &SCHANmask)) {
	SCHANcount++;
	FD_SET(cp->fd, &SCHANmask);
    }
    if (cp->fd > CHANlastsleepfd)
	CHANlastsleepfd = cp->fd;
    cp->Waketime = Waketime;
    cp->Waker = Waker;
    if (cp->Argument != Argument) {
	DISPOSE(cp->Argument);
	cp->Argument = Argument;
    }
    cp->Event = Event;
}


/*
**  Take a channel off the sleep list.
*/
void
SCHANremove(CHANNEL *cp)
{
    if (FD_ISSET(cp->fd, &SCHANmask)) {
	FD_CLR(cp->fd, &SCHANmask);
	SCHANcount--;
	cp->Waketime = 0;
	if (cp->fd == CHANlastsleepfd) {
	    /* This was the highest descriptor, get a new highest. */
	    while (!FD_ISSET(CHANlastsleepfd, &SCHANmask)
	      && CHANlastsleepfd > 1)
		CHANlastsleepfd--;
	}
    }
}


/*
**  Is a channel on the sleep list?
*/
bool
CHANsleeping(CHANNEL *cp)
{
    return FD_ISSET(cp->fd, &SCHANmask);
}


/*
**  Wake up channels waiting for a specific event.
*/
void
SCHANwakeup(void *Event)
{
    CHANNEL	        *cp;
    int	                i;

    for (cp = CHANtable, i = CHANtablesize; --i >= 0; cp++)
	if (cp->Type != CTfree && cp->Event == Event && CHANsleeping(cp))
	    cp->Waketime = 0;
}


/*
**  Mark a channel as an active writer.  Don't reset the Out->Left field
**  since we could have buffered I/O already in there.
*/
void
WCHANadd(CHANNEL *cp)
{
    if (cp->Out.Left > 0) {
	FD_SET(cp->fd, &WCHANmask);
	if (cp->fd > CHANlastfd)
	    CHANlastfd = cp->fd;
    }
}


/*
**  Remove a channel from the set of writers.
*/
void
WCHANremove(CHANNEL *cp)
{
    if (FD_ISSET(cp->fd, &WCHANmask)) {
	FD_CLR(cp->fd, &WCHANmask);
	if (cp->Out.Left <= 0) {
	    /* No data left -- reset used so we don't grow the buffer. */
	    cp->Out.Used = 0;
	    cp->Out.Left = 0;
	}
	if (cp->fd == CHANlastfd) {
	    /* This was the highest descriptor, get a new highest. */
	    while (!FD_ISSET(CHANlastfd, &RCHANmask)
	      && !FD_ISSET(CHANlastfd, &WCHANmask)
	      && CHANlastfd > 1)
		CHANlastfd--;
	}
    }
}


/*
**  Set a channel to start off with the contents of an existing channel.
*/
void
WCHANsetfrombuffer(CHANNEL *cp, BUFFER *bp)
{
    WCHANset(cp, &bp->Data[bp->Used], bp->Left);
}



/*
**  Read in text data, return the amount we read.
*/
int
CHANreadtext(CHANNEL *cp)
{
    int	                i, j;
    BUFFER	        *bp;
    char		*p;
    int			oerrno;
    int			maxbyte;
    HDRCONTENT		*hc = cp->Data.HdrContent;

    /* Grow buffer if we're getting close to current limit. */
    bp = &cp->In;
    bp->Left = bp->Size - bp->Used;
    if (bp->Left <= LOW_WATER) {
	i = GROW_AMOUNT(bp->Size);
	bp->Size += i;
	bp->Left += i;
	p = bp->Data;
	TMRstart(TMR_DATAMOVE);
	RENEW(bp->Data, char, bp->Size);
	/* do not move data, since RENEW did it already */
	if ((i = p - bp->Data) != 0) {
	    if (cp->State == CSgetheader || cp->State == CSgetbody ||
		cp->State == CSeatarticle) {
		/* adjust offset only in CSgetheader, CSgetbody or
		   CSeatarticle */
		if (cp->Data.BytesHeader != NULL)
		  cp->Data.BytesHeader -= i;
		for (j = 0 ; j < MAX_ARTHEADER ; j++, hc++) {
		    if (hc->Value != NULL)
			hc->Value -= i;
		}
	    }
	}
	TMRstop(TMR_DATAMOVE);
    }

    /* Read in whatever is there, up to some reasonable limit. */
    /*
     * XXX We really want to limit the amount of time it takes to
     * process the incoming data for this channel.  But there's
     * no easy way of doing that, so we restrict the data size instead.
     * If the data is part of a single large article, then reading
     * and processing many kilobytes at a time costs very little.
     * If the data is a long list of CHECK commands from a streaming
     * feed, then every line of data will require a history lookup, and
     * we probably don't want to do more than about 10 of those per
     * channel on each cycle of the main select() loop (otherwise we
     * might take too long before giving other channels a turn).  10
     * lines of CHECK commands suggests a limit of about 1 kilobyte of
     * data, or less.  innconf->maxcmdreadsize(BUFSIZ by default) is often
     * about 1 kilobyte, and is attractive for other reasons, so let's
     * use that as our size limit.
     *
     * there is no read size limit if innconf->maxcmdreadsize is 0
     */
    /*
     * Reduce the read size only if we are reading commands.
     */
    if (innconf->maxcmdreadsize > 0)
	maxbyte = (cp->State != CSgetcmd || bp->Left < innconf->maxcmdreadsize) ? bp->Left : innconf->maxcmdreadsize;
    else
	maxbyte = bp->Left;
    TMRstart(TMR_NNTPREAD);
    i = read(cp->fd, &bp->Data[bp->Used], maxbyte);
    TMRstop(TMR_NNTPREAD);
    if (i < 0) {
        /* Solaris (at least 2.4 through 2.6) will occasionally return
           EAGAIN in response to a read even if the file descriptor already
           selected true for reading, apparently due to some internal
           resource exhaustion.  In that case, return -2, which will drop
           back out to the main loop and go on to the next file descriptor,
           as if the descriptor never selected true.  This check will
           probably never trigger on platforms other than Solaris. */
        if (errno == EAGAIN) {
            return -2;
        }
	oerrno = errno;
	p = CHANname(cp);
	errno = oerrno;
	syslog(L_ERROR, "%s cant read %m", p);
	return -1;
    }
    if (i == 0) {
	p = CHANname(cp);
	syslog(L_NOTICE, "%s readclose", p);
	return 0;
    }

    bp->Used += i;
    bp->Left -= i;
    return i;
}


/*
**  If I/O backs up a lot, we can get EMSGSIZE on some systems.  If that
**  happens we want to do the I/O in chunks.  We assume stdio's BUFSIZ is
**  a good chunk value.
*/
static int
CHANwrite(int fd, char *p, long length)
{
    int	i;
    char	*save;

    do {
	/* Try the standard case -- write it all. */
	i = write(fd, p, length);
	if (i > 0 || (i < 0 && errno != EMSGSIZE && errno != EINTR))
	    return i;
    } while (i < 0 && errno == EINTR);

    /* Write it in pieces. */
    for (save = p, i = 0; length; p += i, length -= i) {
	i = write(fd, p, (length > BUFSIZ ? BUFSIZ : length));
	if (i <= 0)
	    break;
    }

    /* Return error, or partial results if we got something. */
    return p == save ? i : p - save;
}


/*
**  Try to flush out the buffer.  Use this only on file channels!
*/
bool
WCHANflush(CHANNEL *cp)
{
    BUFFER	        *bp;
    int	                i;

    /* Write it. */
    for (bp = &cp->Out; bp->Left > 0; bp->Left -= i, bp->Used += i) {
	i = CHANwrite(cp->fd, &bp->Data[bp->Used], bp->Left);
	if (i < 0) {
	    syslog(L_ERROR, "%s cant flush count %d %m",
		CHANname(cp), bp->Left);
	    return FALSE;
	}
	if (i == 0) {
	    syslog(L_ERROR, "%s cant flush count %d",
		CHANname(cp), bp->Left);
	    return FALSE;
	}
    }
    WCHANremove(cp);
    return TRUE;
}



/*
**  Wakeup routine called after a write channel was put to sleep.
*/
static void
CHANwakeup(CHANNEL *cp)
{
    syslog(L_NOTICE, "%s wakeup", CHANname(cp));
    WCHANadd(cp);
}


/*
**  Attempting to write would block; stop output or give up.
*/
static void
CHANwritesleep(CHANNEL *cp, char *p)
{
    int			i;

    if ((i = ++(cp->BlockedWrites)) > innconf->badiocount)
	switch (cp->Type) {
	default:
	    break;
	case CTreject:
	case CTnntp:
	case CTfile:
	case CTexploder:
	case CTprocess:
	    syslog(L_ERROR, "%s blocked closing", p);
	    SITEchanclose(cp);
	    CHANclose(cp, p);
	    return;
	}
    i *= innconf->blockbackoff;
    syslog(L_ERROR, "%s blocked sleeping %d", p, i);
    SCHANadd(cp, Now.time + i, NULL, CHANwakeup, NULL);
}


#if	defined(INND_FIND_BAD_FDS)
/*
**  We got an unknown error in select.  Find out the culprit.
**  Not really ready for production use yet, and it's expensive, too.
*/
static void
CHANdiagnose(void)
{
    fd_set		Test;
    int			i;
    struct timeval	t;

    FD_ZERO(&Test);
    for (i = CHANlastfd; i >= 0; i--) {
	if (FD_ISSET(i, &RCHANmask)) {
	    FD_SET(i, &Test);
	    t.tv_sec = 0;
	    t.tv_usec = 0;
	    if (select(i + 1, &Test, NULL, NULL, &t) < 0
	      && errno != EINTR) {
		syslog(L_ERROR, "%s Bad Read File %d", LogName, i);
		FD_CLR(i, &RCHANmask);
		/* Probably do something about the file descriptor here; call
		 * CHANclose on it? */
	    }
	    FD_CLR(i, &Test);
	}
	if (FD_ISSET(i, &WCHANmask)) {
	    FD_SET(i, &Test);
	    t.tv_sec = 0;
	    t.tv_usec = 0;
	    if (select(i + 1, NULL, &Test, NULL, &t) < 0
	     && errno != EINTR) {
		syslog(L_ERROR, "%s Bad Write File %d", LogName, i);
		FD_CLR(i, &WCHANmask);
		/* Probably do something about the file descriptor here; call
		 * CHANclose on it? */
	    }
	    FD_CLR(i, &Test);
	}
    }
}
#endif	/* defined(INND_FIND_BAD_FDS) */

void
CHANsetActiveCnx(CHANNEL *cp) {
    int		found;  
    CHANNEL	*tempchan;
    char	*label, *tmplabel;
    int		tfd;
    
    if((cp->fd > 0) && (cp->Type == CTnntp) && (cp->ActiveCnx == 0)) {
	found = 1;      
	if ((label = RClabelname(cp)) != NULL) {
	    for(tfd = 0; tfd <= CHANlastfd; tfd++) {
		tempchan = &CHANtable[tfd];
		if ((tmplabel = RClabelname(tempchan)) == NULL) {
		    continue;
		}
		if(strcmp(label, tmplabel) == 0) {
		    if(tempchan->ActiveCnx != 0)
			found++;
		}
	    }
	} 
	cp->ActiveCnx = found;
    }   
}

/*
**  Main I/O loop.  Wait for data, call the channel's handler when there is
**  something to read or when the queued write is finished.  In order to
**  be fair (i.e., don't always give descriptor n priority over n+1), we
**  remember where we last had something and pick up from there.
**
**  Yes, the main code has really wandered over to the side a lot.
*/
void
CHANreadloop(void)
{
    static char		EXITING[] = "INND exiting because of signal\n";
    static int		fd;
    int			i, j;
    int			startpoint;
    int			count;
    int			lastfd;
    int			oerrno;
    CHANNEL		*cp;
    BUFFER		*bp;
    fd_set		MyRead;
    fd_set		MyWrite;
    struct timeval	MyTime;
    long		silence;
    char		*p;
    time_t		LastUpdate;
    HDRCONTENT		*hc;

    STATUSinit();
    
    LastUpdate = GetTimeInfo(&Now) < 0 ? 0 : Now.time;
    for ( ; ; ) {
	/* See if any processes died. */
	PROCscan();

	/* Wait for data, note the time. */
	MyRead = RCHANmask;
	MyWrite = WCHANmask;
	MyTime = TimeOut;
	if (innconf->timer) {
	    unsigned long now = TMRnow();

	    if (now >= innconf->timer * 1000) {
		TMRsummary("ME", timer_name);
		InndHisLogStats();
		MyTime.tv_sec = innconf->timer;
	    }
	    else {
		MyTime.tv_sec = innconf->timer - now / 1000;
	    }
	}
	TMRstart(TMR_IDLE);
	count = select(CHANlastfd + 1, &MyRead, &MyWrite, NULL, &MyTime);
	TMRstop(TMR_IDLE);

	STATUSmainloophook();
	if (GotTerminate) {
	    (void)write(2, EXITING, STRLEN(EXITING));
	    CleanupAndExit(0, (char *)NULL);
	}
	if (count < 0) {
	    if (errno != EINTR) {
		syslog(L_ERROR, "%s cant select %m", LogName);
#if	defined(INND_FIND_BAD_FDS)
		CHANdiagnose();
#endif	/* defined(INND_FIND_BAD_FDS) */
	    }
	    continue;
	}

	/* Update the "reasonably accurate" time. */
	if (GetTimeInfo(&Now) < 0)
	    syslog(L_ERROR, "%s cant gettimeinfo %m", LogName);
	if (Now.time > LastUpdate + TimeOut.tv_sec) {
	    HISsync(History);
	    if (ICDactivedirty) {
		ICDwriteactive();
		ICDactivedirty = 0;
	    }
            LastUpdate = Now.time;
	}

	if (count == 0) {
	    /* No channels active, so flush and skip if nobody's
	     * sleeping. */
	    if (Mode == OMrunning)
		ICDwrite();
	    if (SCHANcount == 0)
		continue;
	}

	/* Try the control channel first. */
	if (FD_ISSET(CHANccfd, &RCHANmask) && FD_ISSET(CHANccfd, &MyRead)) {
	    count--;
 	    if (count > 3)
		count = 3; /* might be more requests */
	    (*CHANcc->Reader)(CHANcc);
	    FD_CLR(CHANccfd, &MyRead);
	}

#ifdef PRIORITISE_REMCONN
	/* Try the remconn channel next. */
	for (j = 0 ; (j < chanlimit) && (CHANrcfd[j] >= 0) ; j++) {
	    if (FD_ISSET(CHANrcfd[j], &RCHANmask) && FD_ISSET(CHANrcfd[j], &MyRead)) {
		count--;
		if (count > 3)
		    count = 3; /* might be more requests */
		(*CHANrc[j]->Reader)(CHANrc[j]);
		FD_CLR(CHANrcfd[j], &MyRead);
	    }
	}
#endif /* PRIORITISE_REMCONN */

	/* Loop through all active channels.  Somebody could have closed
	 * closed a channel so we double-check the global mask before
	 * looking at what select returned.  The code here is written so
	 * that a channel could be reading and writing and sleeping at the
	 * same time, even though that's not possible.  (Just as well,
	 * since in SysVr4 the count would be wrong.) */
	lastfd = CHANlastfd;
	if (lastfd < CHANlastsleepfd)
	    lastfd = CHANlastsleepfd;
	if (fd > lastfd)
	    fd = 0;
	startpoint = fd;
	do {
	    cp = &CHANtable[fd];

            if (cp->MaxCnx > 0 && cp->HoldTime > 0) {
		CHANsetActiveCnx(cp);
                if((cp->ActiveCnx > cp->MaxCnx) && (cp->fd > 0)) {
		    if(cp->Started + cp->HoldTime < Now.time) {
                        CHANclose(cp, CHANname(cp));
                    } else {
                        if (fd >= lastfd)
                            fd = 0;
                        else
                            fd++;
			cp->ActiveCnx = 0;
			RCHANremove(cp);
                    }
                    continue;
                }
            }
	    
	    /* Anything to read? */
	    if (FD_ISSET(fd, &RCHANmask) && FD_ISSET(fd, &MyRead)) {
		count--;
		if (cp->Type == CTfree) {
		    syslog(L_ERROR, "%s %d free but was in RMASK",
			CHANname(cp), fd);
		    /* Don't call RCHANremove since cp->fd will be -1. */
		    FD_CLR(fd, &RCHANmask);
		    (void)close(fd);
		}
		else {
		    cp->LastActive = Now.time;
		    (*cp->Reader)(cp);
		}
	    }

	    /* Check and see if the buffer is grossly overallocated and shrink
	       if needed */
	    if (cp->In.Size > (BIG_BUFFER)) {
		if (cp->In.Used) {
		    if ((cp->In.Size / cp->In.Used) > 10) {
			cp->In.Size = (cp->In.Used * 2) > START_BUFF_SIZE ? (cp->In.Used * 2) :  START_BUFF_SIZE;
			p = cp->In.Data;
			TMRstart(TMR_DATAMOVE);
			RENEW(cp->In.Data, char, cp->In.Size);
			cp->In.Left = cp->In.Size - cp->In.Used;
			/* do not move data, since RENEW did it already */
			if ((i = p - cp->In.Data) != 0) {
			    if (cp->State == CSgetheader ||
				cp->State == CSgetbody ||
				cp->State == CSeatarticle) {
				/* adjust offset only in CSgetheader, CSgetbody
				   or CSeatarticle */
				if (cp->Data.BytesHeader != NULL)
				  cp->Data.BytesHeader -= i;
				hc = cp->Data.HdrContent;
				for (j = 0 ; j < MAX_ARTHEADER ; j++, hc++) {
				    if (hc->Value != NULL)
					hc->Value -= i;
				}
			    }
			}
			TMRstop(TMR_DATAMOVE);
		    }
		} else {
		    p = cp->In.Data;
		    TMRstart(TMR_DATAMOVE);
		    RENEW(cp->In.Data, char, START_BUFF_SIZE);
		    cp->In.Size = cp->In.Left = START_BUFF_SIZE;
		    if ((i = p - cp->In.Data) != 0) {
			if (cp->State == CSgetheader ||
			    cp->State == CSgetbody ||
			    cp->State == CSeatarticle) {
			    /* adjust offset only in CSgetheader, CSgetbody
			       or CSeatarticle */
			    if (cp->Data.BytesHeader != NULL)
			      cp->Data.BytesHeader -= i;
			    hc = cp->Data.HdrContent;
			    for (j = 0 ; j < MAX_ARTHEADER ; j++, hc++) {
				if (hc->Value != NULL)
				    hc->Value -= i;
			    }
			}
		    }
		    TMRstop(TMR_DATAMOVE);
		}
	    }
	    /* Possibly recheck for dead children so we don't get SIGPIPE
	     * on readerless channels. */
	    if (PROCneedscan)
		PROCscan();

	    /* Ready to write? */
	    if (FD_ISSET(fd, &WCHANmask) && FD_ISSET(fd, &MyWrite)) {
		count--;
		if (cp->Type == CTfree) {
		    syslog(L_ERROR, "%s %d free but was in WMASK",
			CHANname(cp), fd);
		    /* Don't call WCHANremove since cp->fd will be -1. */
		    FD_CLR(fd, &WCHANmask);
		    (void)close(fd);
		}
		else {
		    bp = &cp->Out;
		    if (bp->Left) {
			cp->LastActive = Now.time;
			i = CHANwrite(fd, &bp->Data[bp->Used], bp->Left);
			if (i <= 0) {
			    oerrno = errno;
			    p = CHANname(cp);
			    errno = oerrno;
			    if (i < 0)
				syslog(L_ERROR, "%s cant write %m", p);
			    else
				syslog(L_ERROR, "%s cant write", p);
			    cp->BadWrites++;
			    if (i < 0 && oerrno == EPIPE) {
				SITEchanclose(cp);
				CHANclose(cp, p);
			    }
			    else if (i < 0 &&
                                     (oerrno == EWOULDBLOCK
                                      || oerrno == EAGAIN)) {
				WCHANremove(cp);
				CHANwritesleep(cp, p);
			    }
			    else if (cp->BadWrites >= innconf->badiocount) {
				syslog(L_ERROR, "%s sleeping", p);
				WCHANremove(cp);
				SCHANadd(cp,
                                         Now.time + innconf->pauseretrytime,
                                         NULL, CHANwakeup, NULL);
			    }
			}
			else {
			    cp->BadWrites = 0;
			    cp->BlockedWrites = 0;
			    bp->Left -= i;
			    bp->Used += i;
			    if (bp->Left <= 0) {
				WCHANremove(cp);
				(*cp->WriteDone)(cp);
			    } else if (bp->Used > (bp->Size/COMP_THRESHOLD)) {
                                /* compact the buffer, shoving the
                                   data back to the beginning.
                                   <rmtodd@mailhost.ecn.ou.edu> */
                                BUFFset(bp, &bp->Data[bp->Used], bp->Left);
 			    }
			}
		    }
		    else
			/* Should not be possible. */
			WCHANremove(cp);
		}
	    }

	    /* Coming off a sleep? */
	    if (FD_ISSET(fd, &SCHANmask) && cp->Waketime <= Now.time) {
		if (cp->Type == CTfree) {
		    syslog(L_ERROR,"%s ERROR s-select free %d",CHANname(cp),fd);
		    FD_CLR(fd, &SCHANmask);
		    (void) close(fd);
		} else {
		    cp->LastActive = Now.time;
		    SCHANremove(cp);
		    (*cp->Waker)(cp);
		}
	    }

	    /* Toss CTreject channel early if it's inactive. */
	    if (cp->Type == CTreject
	     && cp->LastActive + REJECT_TIMEOUT < Now.time) {
		p = CHANname(cp);
		syslog(L_NOTICE, "%s timeout reject", p);
		CHANclose(cp, p);
	    }

	    /* Has this channel been inactive very long? */
	    if (cp->Type == CTnntp
	     && cp->LastActive + cp->NextLog < Now.time) {
		p = CHANname(cp);
		silence = Now.time - cp->LastActive;
		cp->NextLog += innconf->chaninacttime;
		syslog(L_NOTICE, "%s inactive %ld", p, silence / 60L);
		if (silence > innconf->peertimeout) {
		    syslog(L_NOTICE, "%s timeout", p);
		    CHANclose(cp, p);
		}
	    }

	    /* Bump pointer, modulo the table size. */
	    if (fd >= lastfd)
		fd = 0;
	    else
		fd++;

	    /* If there is nothing to do, break out. */
	    if (count == 0 && SCHANcount == 0)
		break;

	} while (fd != startpoint);
    }
}

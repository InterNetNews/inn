/*  $Id$
**
**  I/O channel (and buffer) processing.
**
**  This file is the heart of innd.  Everything innd does is represented by
**  channels; waiting for connections, reading from network connections,
**  writing to network connections, writing to processes, and writing to files
**  are all channel operations.
**
**  Channels can be in one of three states: reading, writing, or sleeping.
**  The first two are represented in the select file descriptor sets.  The
**  last sits there until something else wakes the channel up.  CHANreadloop
**  is the main I/O loop for innd, calling select and then dispatching control
**  to whatever channels have work to do.
*/

#include "config.h"
#include "clibrary.h"

/* Needed on AIX 4.1 to get fd_set and friends. */
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif

#include "inn/innconf.h"
#include "inn/network.h"
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

/* Compaction threshold as a divisor of the buffer size.  If the amount free
   at the beginning of the buffer is bigger than the quotient, it is compacted
   in the read loop. */
#define COMP_THRESHOLD 10

/* Global data about the channels. */
struct channels {
    fd_set read_set;
    fd_set sleep_set;
    fd_set write_set;
    int sleep_count;            /* Number of sleeping channels. */
    int max_fd;                 /* Max fd in read_set or write_set. */
    int max_sleep_fd;           /* Max fd in sleep_set. */
    int table_size;             /* Total number of channels. */
    CHANNEL *table;             /* Table of channel structs. */

    /* Special prioritized channels, for the control and remconn channels.  We
       check these first each time. */
    CHANNEL **prioritized;
    int prioritized_size;
};

/* Eventually this will move into some sort of global INN state structure. */
static struct channels channels;

/* We want to initialize four elements of CHANnull but let all of the other
   elements be initialized according to the standard rules for a static
   initializer.  However, GCC warns about incomplete initializer lists (since
   it may be a mistake), so we don't initialize anything at all here and
   instead explicitly set the first three values in CHANsetup.  This is the
   only reason why this isn't const. */
static CHANNEL CHANnull;


/*
**  Tear down our world.  Free all of the allocated channels and clear all
**  global state data.  This function can also be used to initialize the
**  channels structure to a known empty state.
*/
void
CHANshutdown(void)
{
    CHANNEL *cp;
    int i;

    FD_ZERO(&channels.read_set);
    FD_ZERO(&channels.sleep_set);
    FD_ZERO(&channels.write_set);
    channels.sleep_count = 0;
    channels.max_fd = -1;
    channels.max_sleep_fd = -1;

    if (channels.table != NULL) {
        cp = channels.table;
        for (i = channels.table_size; --i >= 0; cp++) {
            if (cp->Type != CTfree)
                CHANclose(cp, CHANname(cp));
            if (cp->In.data)
                free(cp->In.data);
            if (cp->Out.data)
                free(cp->Out.data);
        }
    }
    free(channels.table);
    channels.table = NULL;
    channels.table_size = 0;
    if (channels.prioritized_size > 0) {
        free(channels.prioritized);
        channels.prioritized_size = 0;
    }
}


/*
**  Initialize the global I/O channel state and prepare for creation of new
**  channels.  Takes the number of channels to pre-create, which currently
**  must be as large as the largest file descriptor that will be used for a
**  channel.
*/
void
CHANsetup(int count)
{
    CHANNEL *cp;
    int i;

    CHANshutdown();
    channels.table_size = count;
    channels.table = xmalloc(count * sizeof(CHANNEL));

    /* Finish initializing CHANnull, since we can't do this entirely with a
       static initializer without having to list every element in the
       incredibly long channel struct and update it whenever the channel
       struct changes. */
    CHANnull.Type = CTfree;
    CHANnull.State = CSerror;
    CHANnull.fd = -1;
    CHANnull.NextLog = innconf->chaninacttime;

    /* Now, we can use CHANnull to initialize all of the other channels. */
    for (cp = channels.table; --count >= 0; cp++)
        *cp = CHANnull;

    /* Reserve three slots for prioritized channels (control and AF_INET and
       AF_INET6 remconn).  If we end up with more somehow, we'll resize in
       CHANcreate. */
    channels.prioritized_size = 3;
    channels.prioritized = xmalloc(3 * sizeof(CHANNEL *));
    for (i = 0; i < 3; i++)
        channels.prioritized[i] = NULL;
}


/*
**  Create a channel from a file descriptor.  Takes the channel type, the
**  initial state, the reader callback, and the writer callback.
*/
CHANNEL *
CHANcreate(int fd, enum channel_type type, enum channel_state state,
           innd_callback_func reader, innd_callback_func write_done)
{
    CHANNEL *cp;
    int i, j, size;
    struct buffer in  = { 0, 0, 0, NULL };
    struct buffer out = { 0, 0, 0, NULL };

    /* Currently, we do no dynamic allocation, but instead assume that the
       channel table is sized large enough to hold all possible file
       descriptors.  The difficulty with dynamically resizing is that it may
       invalidate any pointers into the channels, which hold all sorts of
       random things, including article data.

       FIXME: Design a better data structure that can be resized easily. */
    cp = &channels.table[fd];

    /* Don't lose the existing buffers when overwriting with CHANnull. */
    in = cp->In;
    buffer_resize(&in, START_BUFF_SIZE);
    in.used = 0;
    in.left = in.size;
    out = cp->Out;
    buffer_resize(&out, SMBUF);
    buffer_set(&out, "", 0);

    /* Set up the channel's info.  Note that we don't have to initialize
       anything that's already set properly to zero in CHANnull. */
    *cp = CHANnull;
    cp->av = NULL;
    cp->fd = fd;
    cp->Type = type;
    cp->State = state;
    cp->Reader = reader;
    cp->WriteDone = write_done;
    cp->Started = Now.tv_sec;
    cp->Started_checkpoint = Now.tv_sec;
    cp->LastActive = Now.tv_sec;
    cp->In = in;
    cp->Out = out;
    cp->Tracing = Tracing;
    HashClear(&cp->CurrentMessageIDHash);
    ARTprepare(cp);

    close_on_exec(fd, true);

#ifndef _HPUX_SOURCE
    /* Stupid HPUX 11.00 has a broken listen/accept where setting the listen
       socket to nonblocking prevents you from successfully setting the
       socket returned by accept(2) back to blocking mode, no matter what,
       resulting in all kinds of funny behaviour, data loss, etc. etc.  */
    if (nonblocking(fd, true) < 0 && errno != ENOTSOCK && errno != ENOTTY)
        syswarn("%s cant nonblock %d", LogName, fd);
#endif

    /* Note the control and remconn channels, for efficiency. */
    if (type == CTcontrol || type == CTremconn) {
        for (i = 0; i < channels.prioritized_size; i++)
            if (channels.prioritized[i] == NULL)
                break;
        if (i >= channels.prioritized_size) {
            size = channels.prioritized_size + 1;
            channels.prioritized
                = xrealloc(channels.prioritized, size * sizeof(CHANNEL *));
            for (j = channels.prioritized_size; j < size; j++)
                channels.prioritized[j] = NULL;
            channels.prioritized_size = size;
        }
        channels.prioritized[i] = cp;
    }

    /* Return a pointer to the new channel. */
    return cp;
}


/*
**  Start or stop tracing a channel.  If we're turning on tracing, dump a
**  bunch of information about that particular channel.
*/
void
CHANtracing(CHANNEL *cp, bool flag)
{
    char *name;
    char addr[INET6_ADDRSTRLEN] = "?";

    name = CHANname(cp);
    notice("%s trace %s", name, flag ? "on" : "off");
    cp->Tracing = flag;
    if (flag) {
        notice("%s trace badwrites %lu blockwrites %lu badreads %lu", name,
               cp->BadWrites, cp->BlockedWrites, cp->BadReads);
        network_sockaddr_sprint(addr, sizeof(addr),
                                (struct sockaddr *) &cp->Address);
        notice("%s trace address %s lastactive %ld nextlog %ld", name,
               addr, (long) cp->LastActive, (long) cp->NextLog);
        if (FD_ISSET(cp->fd, &channels.sleep_set))
            notice("%s trace sleeping %ld 0x%p", name, (long) cp->Waketime,
                   (void *) cp->Waker);
        if (FD_ISSET(cp->fd, &channels.read_set))
            notice("%s trace reading %lu %s", name,
                   (unsigned long) cp->In.used,
                   MaxLength(cp->In.data, cp->In.data));
        if (FD_ISSET(cp->fd, &channels.write_set))
            notice("%s trace writing %lu %s", name,
                   (unsigned long) cp->Out.left,
                   MaxLength(cp->Out.data, cp->Out.data));
    }
}


/*
**  Close an NNTP channel.  Called by CHANclose to do the special handling for
**  NNTP channels, which carry quite a bit more data than other channels.
**
**  FIXME: There really should be a type-specific channel closing callback
**  that can take care of this, and this will be mandatory when all the
**  NNTP-specific stuff is moved out of the core channel struct.
*/
static void
CHANclose_nntp(CHANNEL *cp, const char *name)
{
    WIPprecomfree(cp);
    NCclearwip(cp);
    if (cp->State == CScancel)
        notice("%s closed seconds %ld cancels %ld", name,
               (long)(Now.tv_sec - cp->Started), cp->Received);
    else {
        notice("%s checkpoint seconds %ld accepted %ld refused %ld rejected %ld"
               " duplicate %ld accepted size %.0f duplicate size %.0f"
               " rejected size %.0f", name,
               (long)(Now.tv_sec - cp->Started_checkpoint),
               cp->Received - cp->Received_checkpoint,
               cp->Refused - cp->Refused_checkpoint,
               cp->Rejected - cp->Rejected_checkpoint,
               cp->Duplicate - cp->Duplicate_checkpoint,
               cp->Size - cp->Size_checkpoint,
               cp->DuplicateSize - cp->DuplicateSize_checkpoint,
               cp->RejectSize - cp->RejectSize_checkpoint);
        notice("%s closed seconds %ld accepted %ld refused %ld rejected %ld"
               " duplicate %ld accepted size %.0f duplicate size %.0f"
               " rejected size %.0f", name, (long)(Now.tv_sec - cp->Started),
               cp->Received, cp->Refused, cp->Rejected, cp->Duplicate,
               cp->Size, cp->DuplicateSize, cp->RejectSize);
    }
    if (cp->Data.Newsgroups.Data != NULL) {
        free(cp->Data.Newsgroups.Data);
        cp->Data.Newsgroups.Data = NULL;
    }
    if (cp->Data.Newsgroups.List != NULL) {
        free(cp->Data.Newsgroups.List);
        cp->Data.Newsgroups.List = NULL;
    }
    if (cp->Data.Distribution.Data != NULL) {
        free(cp->Data.Distribution.Data);
        cp->Data.Distribution.Data = NULL;
    }
    if (cp->Data.Distribution.List != NULL) {
        free(cp->Data.Distribution.List);
        cp->Data.Distribution.List = NULL;
    }
    if (cp->Data.Path.Data != NULL) {
        free(cp->Data.Path.Data);
        cp->Data.Path.Data = NULL;
    }
    if (cp->Data.Path.List != NULL) {
        free(cp->Data.Path.List);
        cp->Data.Path.List = NULL;
    }
    if (cp->Data.Overview.size != 0) {
        free(cp->Data.Overview.data);
        cp->Data.Overview.data = NULL;
        cp->Data.Overview.size = 0;
        cp->Data.Overview.left = 0;
        cp->Data.Overview.used = 0;
    }
    if (cp->Data.XrefBufLength != 0) {
        free(cp->Data.Xref);
        cp->Data.Xref = NULL;
        cp->Data.XrefBufLength = 0;
    }

    /* If this was a peer who was connection-limited, take note that one of
       their connections was just closed and possibly wake up another one.

       FIXME: This is a butt-ugly way of handling this and a layering
       violation to boot.  This needs to happen in the NC code, possibly via a
       channel closing callback. */
    if (cp->MaxCnx > 0) {
        char *label, *tmplabel;
        int tfd;
        CHANNEL *tempchan;

        label = RClabelname(cp);
        if (label != NULL) {
            for (tfd = 0; tfd <= channels.max_fd; tfd++) {
                if (tfd == cp->fd)
                    continue;
                tempchan = &channels.table[tfd];
                if (tempchan->fd < 0 || tempchan->Type != CTnntp)
                    continue;
                tmplabel = RClabelname(tempchan);
                if (tmplabel == NULL)
                    continue;
                if (strcmp(label, tmplabel) == 0 && tempchan->ActiveCnx != 0) {
                    tempchan->ActiveCnx = cp->ActiveCnx;
                    RCHANadd(tempchan);
                    break;
                }
            }
        }
    }
}


/*
**  Close a channel.
*/
void
CHANclose(CHANNEL *cp, const char *name)
{
    int i;

    if (cp->Type == CTfree)
        warn("%s internal closing free channel %d", name, cp->fd);
    else {
        if (cp->Type == CTnntp)
            CHANclose_nntp(cp, name);
        else if (cp->Type == CTreject)
            notice("%s %ld", name, cp->Rejected); /* Use cp->Rejected for the response code. */
        else if (cp->Out.left)
            warn("%s closed lost %lu", name, (unsigned long) cp->Out.left);
        else
            notice("%s closed", name);
        WCHANremove(cp);
        RCHANremove(cp);
        SCHANremove(cp);
        if (cp->fd >= 0 && close(cp->fd) < 0)
            syswarn("%s cant close %s", LogName, name);
        for (i = 0; i < channels.prioritized_size; i++)
            if (channels.prioritized[i] == cp)
                channels.prioritized[i] = NULL;
    }

    /* Mark it unused. */
    cp->Type = CTfree;
    cp->State = CSerror;
    cp->fd = -1;
    if (cp->Argument != NULL)
        free(cp->Argument);
    cp->Argument = NULL;
    cp->ActiveCnx = 0;

    /* Free the buffers if they got big. */
    if (cp->In.size > BIG_BUFFER) {
        cp->In.size = 0;
        cp->In.used = 0;
        cp->In.left = 0;
        free(cp->In.data);
        cp->In.data = NULL;
    }
    if (cp->Out.size > BIG_BUFFER) {
        cp->Out.size = 0;
        cp->Out.used = 0;
        cp->Out.left = 0;
        free(cp->Out.data);
        cp->Out.data = NULL;
    }

    /* Always free the Sendid buffer. */
    if (cp->Sendid.size > 0) {
        cp->Sendid.size = 0;
        cp->Sendid.used = 0;
        cp->Sendid.left = 0;
        free(cp->Sendid.data);
        cp->Sendid.data = NULL;
    }

    /* Free the space allocated for NNTP commands. */
    if (cp->av != NULL) {
        free(cp->av[0]);
        free(cp->av);
        cp->av = NULL;
    }
}


/*
**  Return a printable name for the channel.
**
**  FIXME: Another layering violation.  Each type of channel should register a
**  callback or a name rather than making the general code know how to name
**  channels.
*/
char *
CHANname(CHANNEL *cp)
{
    int i;
    SITE *sp;
    const char *site;
    pid_t pid;

    switch (cp->Type) {
    default:
        snprintf(cp->Name, sizeof(cp->Name), "?%d(#%d@%ld)?", cp->Type,
                 cp->fd, (long) (cp - channels.table));
        break;
    case CTany:
        snprintf(cp->Name, sizeof(cp->Name), "any:%d", cp->fd);
        break;
    case CTfree:
        snprintf(cp->Name, sizeof(cp->Name), "free:%d", cp->fd);
        break;
    case CTremconn:
        snprintf(cp->Name, sizeof(cp->Name), "remconn:%d", cp->fd);
        break;
    case CTreject:
        snprintf(cp->Name, sizeof(cp->Name), "%s rejected",
                 cp->Address.ss_family == 0 ? "localhost" : RChostname(cp));
        break;
    case CTnntp:
        snprintf(cp->Name, sizeof(cp->Name), "%s:%d",
                 cp->Address.ss_family == 0 ? "localhost" : RChostname(cp),
                 cp->fd);
        break;
    case CTlocalconn:
        snprintf(cp->Name, sizeof(cp->Name), "localconn:%d", cp->fd);
        break;
    case CTcontrol:
        snprintf(cp->Name, sizeof(cp->Name), "control:%d", cp->fd);
        break;
    case CTexploder:
    case CTfile:
    case CTprocess:
        /* Find the site that has this channel. */
        site = "?";
        pid = 0;
        for (i = nSites, sp = Sites; --i >= 0; sp++)
            if (sp->Channel == cp) {
                site = sp->Name;
                if (cp->Type != CTfile)
                    pid = sp->pid;
                break;
            }
        if (pid == 0)
            snprintf(cp->Name, sizeof(cp->Name), "%s:%d:%s",
                     MaxLength(site, site), cp->fd,
                     cp->Type == CTfile ? "file" : "proc");
        else
            snprintf(cp->Name, sizeof(cp->Name), "%s:%d:%s:%ld",
                     MaxLength(site, site), cp->fd,
                     cp->Type == CTfile ? "file" : "proc", (long) pid);
        break;
    }
    return cp->Name;
}


/*
**  Return the channel for a specified descriptor.
*/
CHANNEL *
CHANfromdescriptor(int fd)
{
    if (fd < 0 || fd > channels.table_size)
        return NULL;
    return &channels.table[fd];
}


/*
**  Iterate over all channels of a specified type.  The next channel is
**  returned and its index is put into ip, which serves as the cursor.
**
**  FIXME: Make ip an opaque cursor.
*/
CHANNEL *
CHANiter(int *ip, enum channel_type type)
{
    CHANNEL *cp;
    int i = *ip;

    for (i = *ip; i >= 0 && i < channels.table_size; i++) {
        cp = &channels.table[i];
        if (cp->Type == CTfree && cp->fd == -1)
            continue;
        if (type == CTany || cp->Type == type) {
            *ip = ++i;
            return cp;
        }
    }
    return NULL;
}


/*
**  When removing a channel from the read and write masks, we want to lower
**  the last file descriptor if we removed the highest one.  Called from both
**  RCHANremove and WCHANremove.
*/
static void
CHANresetlast(int fd)
{
    if (fd == channels.max_fd)
        while (   !FD_ISSET(channels.max_fd, &channels.read_set)
               && !FD_ISSET(channels.max_fd, &channels.write_set)
               && channels.max_fd > 1)
            channels.max_fd--;
}


/*
**  When removing a channel from the sleep mask, we want to lower the last
**  file descriptor if we removed the highest one.  Called from SCHANremove.
*/
static void
CHANresetlastsleeping(int fd)
{
    if (fd == channels.max_sleep_fd) {
        while (   !FD_ISSET(channels.max_sleep_fd, &channels.sleep_set)
               && channels.max_sleep_fd > 1)
            channels.max_sleep_fd--;
    }
}


/*
**  Mark a channel as an active reader.
*/
void
RCHANadd(CHANNEL *cp)
{
    FD_SET(cp->fd, &channels.read_set);
    if (cp->fd > channels.max_fd)
        channels.max_fd = cp->fd;

    /* For non-NNTP channels, start reading at the beginning of the buffer. */
    if (cp->Type != CTnntp)
        cp->In.used = 0;
}


/*
**  Remove a channel from the set of readers.
*/
void
RCHANremove(CHANNEL *cp)
{
    if (FD_ISSET(cp->fd, &channels.read_set)) {
        FD_CLR(cp->fd, &channels.read_set);
        CHANresetlast(cp->fd);
    }
}


/*
**  Put a channel to sleep and call a function when it wakes.  Note that arg
**  must be NULL or allocated memory, since it will be freed later.
*/
void
SCHANadd(CHANNEL *cp, time_t wake, void *event, innd_callback_func waker,
         void *arg)
{
    if (!CHANsleeping(cp)) {
        channels.sleep_count++;
        FD_SET(cp->fd, &channels.sleep_set);
    }
    if (cp->fd > channels.max_sleep_fd)
        channels.max_sleep_fd = cp->fd;
    cp->Waketime = wake;
    cp->Waker = waker;
    if (cp->Argument != arg) {
        free(cp->Argument);
        cp->Argument = arg;
    }
    cp->Event = event;
}


/*
**  Take a channel off the sleep list.
*/
void
SCHANremove(CHANNEL *cp)
{
    if (!CHANsleeping(cp))
        return;
    FD_CLR(cp->fd, &channels.sleep_set);
    channels.sleep_count--;
    cp->Waketime = 0;

    /* If this was the highest descriptor, get a new highest. */
    CHANresetlastsleeping(cp->fd);
}


/*
**  Is a channel on the sleep list?
*/
bool
CHANsleeping(CHANNEL *cp)
{
    return FD_ISSET(cp->fd, &channels.sleep_set);
}


/*
**  Wake up channels waiting for a specific event.  (Or rather, mark them
**  ready to wake up; they'll actually be woken up the next time through the
**  main loop.)
*/
void
SCHANwakeup(void *event)
{
    CHANNEL *cp;
    int i;

    for (cp = channels.table, i = channels.table_size; --i >= 0; cp++)
        if (cp->Type != CTfree && cp->Event == event && CHANsleeping(cp))
            cp->Waketime = 0;
}


/*
**  Mark a channel as an active writer.  Don't reset the Out->left field
**  since we could have buffered I/O already in there.
*/
void
WCHANadd(CHANNEL *cp)
{
    if (cp->Out.left > 0) {
        FD_SET(cp->fd, &channels.write_set);
        if (cp->fd > channels.max_fd)
            channels.max_fd = cp->fd;
    }
}


/*
**  Remove a channel from the set of writers.
*/
void
WCHANremove(CHANNEL *cp)
{
    if (FD_ISSET(cp->fd, &channels.write_set)) {
        FD_CLR(cp->fd, &channels.write_set);
        CHANresetlast(cp->fd);

        /* No data left -- reset used so we don't grow the buffer. */
        if (cp->Out.left <= 0) {
            cp->Out.used = 0;
            cp->Out.left = 0;
        }
    }
}


/*
**  Set a channel to start off with the contents of an existing channel.
*/
void
WCHANsetfrombuffer(CHANNEL *cp, struct buffer *bp)
{
    WCHANset(cp, &bp->data[bp->used], bp->left);
}


/*
**  Internal function to resize the in buffer for a given channel to the new
**  size given, adjusting pointers as required.
*/
static void
CHANresize(CHANNEL *cp, size_t size)
{
    struct buffer *bp;
    char *p;
    size_t change;
    ptrdiff_t offset;
    int i;
    HDRCONTENT *hc = cp->Data.HdrContent;

    bp = &cp->In;
    change = size - bp->size;
    bp->size = size;
    bp->left += change;
    p = bp->data;

    /* Reallocate the buffer and adjust offets if realloc moved the location
       of the memory region.  Only adjust offets if we're in a state where we
       care about the header contents.

       FIXME: This is invalid C, although it will work on most (all?)  common
       systems.  The pointers need to be reduced to offets and then turned
       back into relative pointers rather than adjusting the pointers
       directly, since as soon as realloc is called, pointers into the old
       space become invalid and may not be used further, even for arithmetic.
       (Not to mention that two pointers to different objects may not be
       compared and arithmetic may not be performed on them. */
    TMRstart(TMR_DATAMOVE);
    bp->data = xrealloc(bp->data, bp->size);
    offset = p - bp->data;
    if (offset != 0) {
        if (cp->State == CSgetheader || cp->State == CSgetbody ||
            cp->State == CSeatarticle) {
            if (cp->Data.BytesHeader != NULL)
                cp->Data.BytesHeader -= offset;
            for (i = 0; i < MAX_ARTHEADER; i++, hc++) {
                if (hc->Value != NULL)
                    hc->Value -= offset;
            }
        }
    }
    TMRstop(TMR_DATAMOVE);
}


/*
**  Read in text data, return the amount we read.
*/
int
CHANreadtext(CHANNEL *cp)
{
    struct buffer *bp;
    char *name;
    int oerrno, maxbyte;
    ssize_t count;

    /* Grow buffer if we're getting close to current limit.

       FIXME: The In buffer doesn't use the normal meanings of .used and
       .left.  */
    bp = &cp->In;
    bp->left = bp->size - bp->used;
    if (bp->left <= LOW_WATER)
        CHANresize(cp, bp->size + GROW_AMOUNT(bp->size));

    /* Read in whatever is there, up to some reasonable limit.

       We want to limit the amount of time devoted to processing the incoming
       data for any given channel.  There's no easy way of doing that, though,
       so we restrict the data size instead.

       If the data is part of a single large article, then reading and
       processing many kilobytes at a time costs very little.  If the data is
       a long list of CHECK commands from a streaming feed, then every line of
       data will require a history lookup, and we probably don't want to do
       more than about 10 of those per channel on each cycle of the main
       select() loop (otherwise we might take too long before giving other
       channels a turn).  10 lines of CHECK commands suggests a limit of about
       1KB of data, or less, which is the innconf->maxcmdreadsize default.  If
       innconf->maxcmdreadsize is 0, there is no limit.

       Reduce the read size only if we're reading commands.

       FIXME: A better approach would be to limit the number of commands we
       process for each channel. */
    if (innconf->maxcmdreadsize == 0 || cp->State != CSgetcmd
        || bp->left < innconf->maxcmdreadsize)
        maxbyte = bp->left;
    else
        maxbyte = innconf->maxcmdreadsize;
    TMRstart(TMR_NNTPREAD);
    count = read(cp->fd, &bp->data[bp->used], maxbyte);
    TMRstop(TMR_NNTPREAD);

    /* Solaris (at least 2.4 through 2.6) will occasionally return EAGAIN in
       response to a read even if the file descriptor already selected true
       for reading, apparently due to some internal resource exhaustion.  In
       that case, return -2, which will drop back out to the main loop and go
       on to the next file descriptor, as if the descriptor never selected
       true.  This check will probably never trigger on platforms other than
       Solaris. */
    if (count < 0) {
        if (errno == EAGAIN)
            return -2;
        oerrno = errno;
        name = CHANname(cp);
        errno = oerrno;
        sysnotice("%s cant read", name);
        return -1;
    }
    if (count == 0) {
        name = CHANname(cp);
        notice("%s readclose", name);
        return 0;
    }
    bp->used += count;
    bp->left -= count;
    return count;
}


/*
**  Internal function to write out channel data.
**
**  If I/O backs up a lot, we can get EMSGSIZE on some systems.  If that
**  happens we want to do the I/O in chunks.  We assume stdio's BUFSIZ is a
**  good chunk value.
**
**  FIXME: Roll this code into xwrite.
*/
static int
CHANwrite(int fd, const char *data, long length)
{
    ssize_t count;
    const char *p;

    /* Try the standard case -- write it all. */
    do {
        count = write(fd, data, length);
    } while (count < 0 && errno == EINTR);
    if (count > 0 || (count < 0 && errno != EMSGSIZE))
        return count;

    /* We got EMSGSIZE, so write it in pieces. */
    for (p = data, count = 0; length > 0; p += count, length -= count) {
        count = write(fd, p, (length > BUFSIZ ? BUFSIZ : length));
        if (count < 0 && errno == EINTR) {
            count = 0;
            continue;
        }
        if (count <= 0)
            break;
    }

    /* Return error, or partial results if we got something. */
    return (p == data) ? count : (p - data);
}


/*
**  Try to flush out the buffer.  Use this only on file channels!
*/
bool
WCHANflush(CHANNEL *cp)
{
    struct buffer *bp;
    ssize_t count;

    /* Write it. */
    for (bp = &cp->Out; bp->left > 0; bp->left -= count, bp->used += count) {
        count = CHANwrite(cp->fd, &bp->data[bp->used], bp->left);
        if (count <= 0) {
            syswarn("%s cant flush count %lu", CHANname(cp),
                    (unsigned long) bp->left);
            return false;
        }
        if (count == 0) {
            warn("%s cant flush count %lu", CHANname(cp),
                 (unsigned long) bp->left);
            return false;
        }
    }
    WCHANremove(cp);
    return true;
}


/*
**  Standard wakeup routine called after a write channel was put to sleep.
*/
static void
CHANwakeup(CHANNEL *cp)
{
    notice("%s wakeup", CHANname(cp));
    WCHANadd(cp);
}


/*
**  Attempting to write would block; stop output or give up.
*/
static void
CHANwritesleep(CHANNEL *cp, const char *name)
{
    unsigned long bad, delay;

    bad = ++(cp->BlockedWrites);
    if (bad > innconf->badiocount)
        switch (cp->Type) {
        default:
            break;
        case CTreject:
        case CTnntp:
        case CTfile:
        case CTexploder:
        case CTprocess:
            warn("%s blocked closing", name);
            SITEchanclose(cp);
            CHANclose(cp, name);
            return;
        }
    delay = bad * innconf->blockbackoff;
    warn("%s blocked sleeping %lu", name, delay);
    SCHANadd(cp, Now.tv_sec + delay, NULL, CHANwakeup, NULL);
}


/*
**  We got an unknown error in select.  Find out the culprit.  This is not
**  enabled yet; it's been disabled for years, and is expensive.
*/
static void UNUSED
CHANdiagnose(void)
{
    fd_set test;
    int fd;
    struct timeval tv;

    FD_ZERO(&test);
    for (fd = channels.max_fd; fd >= 0; fd--) {
        if (FD_ISSET(fd, &channels.read_set)) {
            FD_SET(fd, &test);
            tv.tv_sec = 0;
            tv.tv_usec = 0;
            if (select(fd + 1, &test, NULL, NULL, &tv) < 0 && errno != EINTR) {
                warn("%s bad read file %d", LogName, fd);
                FD_CLR(fd, &channels.read_set);
                /* Probably do something about the file descriptor here; call
                   CHANclose on it? */
            }
            FD_CLR(fd, &test);
        }
        if (FD_ISSET(fd, &channels.write_set)) {
            FD_SET(fd, &test);
            tv.tv_sec = 0;
            tv.tv_usec = 0;
            if (select(fd + 1, NULL, &test, NULL, &tv) < 0 && errno != EINTR) {
                warn("%s bad write file %d", LogName, fd);
                FD_CLR(fd, &channels.write_set);
                /* Probably do something about the file descriptor here; call
                   CHANclose on it? */
            }
            FD_CLR(fd, &test);
        }
    }
}


/*
**  Count the number of active connections for a given peer.
**
**  FIXME: This seems like an overly cumbersome way to do this, and also seems
**  like a layering violation.  Do we really have to do this here?
*/
void
CHANcount_active(CHANNEL *cp)
{
    int found;  
    CHANNEL *tempchan;
    char *label, *tmplabel;
    int fd;

    if (cp->fd < 0 || cp->Type != CTnntp || cp->ActiveCnx == 0)
        return;
    found = 1;
    label = RClabelname(cp);
    if (label == NULL)
        return;
    for (fd = 0; fd <= channels.max_fd; fd++) {
        tempchan = &channels.table[fd];
        tmplabel = RClabelname(tempchan);
        if (tmplabel == NULL)
            continue;
        if (strcmp(label, tmplabel) == 0 && tempchan->ActiveCnx != 0)
            found++;
    }
    cp->ActiveCnx = found;
}


/*
**  Handle a file descriptor that selects ready to read.  Dispatch to the
**  reader function, and then resize its input buffer if needed.
*/
static void
CHANhandle_read(CHANNEL *cp)
{
    size_t size;

    if (cp->Type == CTfree) {
        warn("%s %d free but was in RMASK", CHANname(cp), cp->fd);
        RCHANremove(cp);
        close(cp->fd);
        cp->fd = -1;
        return;
    }
    cp->LastActive = Now.tv_sec;
    (*cp->Reader)(cp);

    /* Check and see if the buffer is grossly overallocated and shrink if
       needed. */
    if (cp->In.size <= BIG_BUFFER)
        return;
    if (cp->In.used == 0)
        CHANresize(cp, START_BUFF_SIZE);
    else if ((cp->In.size / cp->In.used) > 10) {
        size = cp->In.used * 2;
        if (size < START_BUFF_SIZE)
            size = START_BUFF_SIZE;
        CHANresize(cp, size);
    }
}


/*
**  Handle a file descriptor that selects ready to write.  Write out the
**  pending data.
*/
static void
CHANhandle_write(CHANNEL *cp)
{
    struct buffer *bp;
    ssize_t count;
    int oerrno;
    const char *name;

    if (cp->Type == CTfree) {
        warn("%s %d free but was in WMASK", CHANname(cp), cp->fd);
        WCHANremove(cp);
        close(cp->fd);
        cp->fd = -1;
        return;
    }
    bp = &cp->Out;
    if (bp->left == 0) {
        /* Should not be possible. */
        WCHANremove(cp);
        return;
    }
    cp->LastActive = Now.tv_sec;
    count = CHANwrite(cp->fd, &bp->data[bp->used], bp->left);
    if (count <= 0) {
        oerrno = errno;
        name = CHANname(cp);
        errno = oerrno;
        if (oerrno == EWOULDBLOCK)
            oerrno = EAGAIN;
        if (count < 0)
            sysnotice("%s cant write", name);
        else
            notice("%s cant write", name);
        cp->BadWrites++;
        if (count < 0 && oerrno == EPIPE) {
            SITEchanclose(cp);
            CHANclose(cp, name);
        } else if (count < 0 && oerrno == EAGAIN) {
            WCHANremove(cp);
            CHANwritesleep(cp, name);
        } else if (cp->BadWrites >= innconf->badiocount) {
            warn("%s sleeping", name);
            WCHANremove(cp);
            SCHANadd(cp, Now.tv_sec + innconf->pauseretrytime, NULL,
                     CHANwakeup, NULL);
        }
    } else {
        cp->BadWrites = 0;
        cp->BlockedWrites = 0;
        bp->left -= count;
        bp->used += count;
        if (bp->left > 0)
            buffer_compact(bp);
        else {
            WCHANremove(cp);
            (*cp->WriteDone)(cp);
        }
    }
}


/*
**  Main I/O loop.  Wait for data, call the channel's handler when there is
**  something to read or when the queued write is finished.  In order to be
**  fair (i.e., don't always give descriptor n priority over n+1), we remember
**  where we last had something and pick up from there.
**
**  Yes, the main code has really wandered over to the side a lot.
*/
void
CHANreadloop(void)
{
    int i, startpoint, count, lastfd;
    CHANNEL *cp;
    fd_set rdfds, wrfds;
    struct timeval tv;
    unsigned long silence;
    const char *name;
    time_t last_sync;
    int fd = 0;

    STATUSinit();
    gettimeofday(&Now, NULL);
    last_sync = Now.tv_sec;

    while (1) {
        /* See if any processes died. */
        PROCscan();

        /* Wait for data, note the time. */
        rdfds = channels.read_set;
        wrfds = channels.write_set;
        tv = TimeOut;
        if (innconf->timer != 0) {
            unsigned long now = TMRnow();

            if (now < 1000 * innconf->timer)
                tv.tv_sec = innconf->timer - now / 1000;
            else {
                TMRsummary("ME", timer_name);
                InndHisLogStats();
                tv.tv_sec = innconf->timer;
            }
        }
        TMRstart(TMR_IDLE);
        count = select(channels.max_fd + 1, &rdfds, &wrfds, NULL, &tv);
        TMRstop(TMR_IDLE);

        if (count < 0) {
            if (errno != EINTR) {
                syswarn("%s cant select", LogName);
#ifdef INND_FIND_BAD_FDS
                CHANdiagnose();
#endif      
            }
            continue;
        }

        STATUSmainloophook();
        if (GotTerminate) {
            warn("%s exiting due to signal", LogName);
            CleanupAndExit(0, NULL);
        }

        /* Update the "reasonably accurate" time. */
        gettimeofday(&Now, NULL);
        if (Now.tv_sec > last_sync + TimeOut.tv_sec) {
            HISsync(History);
            if (ICDactivedirty != 0) {
                ICDwriteactive();
                ICDactivedirty = 0;
            }
            last_sync = Now.tv_sec;
        }

        /* If no channels are active, flush and skip if nobody's sleeping. */
        if (count == 0) {
            if (Mode == OMrunning)
                ICDwrite();
            if (channels.sleep_count == 0)
                continue;
        }

        /* Try the prioritized channels first. */
        for (i = 0; i < channels.prioritized_size; i++) {
            int pfd;

            if (channels.prioritized[i] == NULL)
                continue;
            pfd = channels.prioritized[i]->fd;
            if (FD_ISSET(pfd, &channels.read_set) && FD_ISSET(pfd, &rdfds)) {
                count--;
                if (count > 4)
                    count = 4; /* might be more requests */
                (*channels.prioritized[i]->Reader)(channels.prioritized[i]);
                FD_CLR(pfd, &rdfds);
            }
        }

        /* Loop through all active channels.  Somebody could have closed a
           channel so we double-check the global mask before looking at what
           select returned.  The code here is written so that a channel could
           be reading and writing and sleeping at the same time, even though
           that's not possible.  (Just as well, since in SysVr4 the count
           would be wrong.) */
        lastfd = channels.max_fd;
        if (lastfd < channels.max_sleep_fd)
            lastfd = channels.max_sleep_fd;
        if (fd > lastfd)
            fd = 0;
        startpoint = fd;
        do {
            cp = &channels.table[fd];

            /* Check to see if this peer has too many open connections, and if
               so, either close or make inactive this connection. */
            if (cp->Type == CTnntp && cp->MaxCnx > 0 && cp->HoldTime > 0) {
                CHANcount_active(cp);
                if (cp->ActiveCnx > cp->MaxCnx && cp->fd > 0) {
                    if (cp->Started + cp->HoldTime < Now.tv_sec)
                        CHANclose(cp, CHANname(cp));
                    else {
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
            if (FD_ISSET(fd, &channels.read_set) && FD_ISSET(fd, &rdfds)) {
                count--;
                CHANhandle_read(cp);
            }

            /* Possibly recheck for dead children so we don't get SIGPIPE on
               readerless channels. */
            if (PROCneedscan)
                PROCscan();

            /* Ready to write? */
            if (FD_ISSET(fd, &channels.write_set) && FD_ISSET(fd, &wrfds)) {
                count--;
                CHANhandle_write(cp);
            }

            /* Coming off a sleep? */
            if (FD_ISSET(fd, &channels.sleep_set)
                && cp->Waketime <= Now.tv_sec) {
                if (cp->Type == CTfree) {
                    warn("%s %d free but was in SMASK", CHANname(cp), fd);
                    FD_CLR(fd, &channels.sleep_set);
                    channels.sleep_count--;
                    CHANresetlastsleeping(fd);
                    close(fd);
                    cp->fd = -1;
                } else {
                    cp->LastActive = Now.tv_sec;
                    SCHANremove(cp);
                    if (cp->Waker != NULL) {
                        (*cp->Waker)(cp);
                    } else {
                        name = CHANname(cp);
                        warn("%s %d sleeping without Waker", name, fd);
                        SITEchanclose(cp);
                        CHANclose(cp, name);
                    }
                }
            }

            /* Toss CTreject channel early if it's inactive. */
            if (cp->Type == CTreject
                && cp->LastActive + REJECT_TIMEOUT < Now.tv_sec) {
                name = CHANname(cp);
                notice("%s timeout reject", name);
                CHANclose(cp, name);
            }

            /* Has this channel been inactive very long? */
            if (cp->Type == CTnntp
                && cp->LastActive + cp->NextLog < Now.tv_sec) {
                name = CHANname(cp);
                silence = Now.tv_sec - cp->LastActive;
                cp->NextLog += innconf->chaninacttime;
                notice("%s inactive %ld", name, silence / 60L);
                if (silence > innconf->peertimeout) {
                    notice("%s timeout", name);
                    CHANclose(cp, name);
                }
            }

            /* Bump pointer, modulo the table size. */
            if (fd >= lastfd)
                fd = 0;
            else
                fd++;

            /* If there is nothing to do, break out.

               FIXME: Do we really want to keep going until sleep_count is
               zero?  That means that when there are sleeping channels, we do
               a full traversal every time through the select loop. */
            if (count == 0 && channels.sleep_count == 0)
                break;
        } while (fd != startpoint);
    }
}

/*  $Id$
**
**  Routines for the remote connect channel.  Create an Internet stream
**  socket that processes connect to.  If the incoming site is not one of
**  our feeds, then we optionally pass the connection off to the standard
**  NNTP daemon.
*/
#include "config.h"
#include "clibrary.h"
#include "innd.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>

#define TEST_CONFIG(a, b) \
    { \
	b = ((peer_params.Keysetbit & (1 << a)) != 0) ? TRUE : FALSE; \
    }

#define SET_CONFIG(a) \
    { \
	peer_params.Keysetbit |= (1 << a); \
    }

/*
**  A remote host has an address and a password.
*/
typedef struct _REMOTEHOST {
    char	*Label;         /* Peer label */
    char	*Name;          /* Hostname */
    struct sockaddr_storage Address;     /* List of ip adresses */
    char	*Password;      /* Optional password */
    char 	*Identd;	/* Optional identd */
    bool	Streaming;      /* Streaming allowed ? */
    bool	Skip;	        /* Skip this peer ? */
    bool	NoResendId;	/* Don't send RESEND responses ? */
    bool	Nolist;		/* no list command allowed */
    int		MaxCnx;		/* Max connections (per peer) */
    char	**Patterns;	/* List of groups allowed */
    char	*Pattern;       /* List of groups allowed (string) */
    char        *Email;         /* Email(s) of contact */
    char	*Comment;	/* Commentary [max size = MAXBUFF] */
    int		HoldTime;	/* Hold time before disconnect over MaxCnx */
    int		Keysetbit;	/* Bit to check duplicated key */
} REMOTEHOST;

typedef struct _REMOTEHOST_DATA {
    int         key;            /* Key (as defined in the _Keywords enum) */
    int         type;           /* Type of the value (see _Type enum) */
    char        *value;         /* Value */
} REMOTEHOST_DATA;

typedef struct _REMOTETABLE {
    struct sockaddr_storage Address;
    time_t         Expires;
} REMOTETABLE;

static char		*RCslaveflag;
static char		*RCnnrpd = NULL;
static char		*RCnntpd = NULL;
static CHANNEL		**RCchan;
static int		chanlimit;
static REMOTEHOST_DATA	*RCpeerlistfile;
static REMOTEHOST	*RCpeerlist;
static int		RCnpeerlist;
static char		RCbuff[BIG_BUFFER];

#define PEER	        "peer"
#define GROUP	        "group"
#define HOSTNAME        "hostname:"
#define STREAMING       "streaming:"
#define MAX_CONN        "max-connections:"
#define PASSWORD        "password:"
#define IDENTD	        "identd:"
#define PATTERNS        "patterns:"
#define EMAIL	        "email:"
#define COMMENT	        "comment:"
#define SKIP		"skip:"
#define NORESENDID	"noresendid:"
#define HOLD_TIME	"hold-time:"
#define NOLIST		"nolist:"

typedef enum {K_END, K_BEGIN_PEER, K_BEGIN_GROUP, K_END_PEER, K_END_GROUP,
	      K_STREAM, K_HOSTNAME, K_MAX_CONN, K_PASSWORD, K_IDENTD,
	      K_EMAIL, K_PATTERNS, K_COMMENT, K_SKIP, K_NORESENDID,
	      K_HOLD_TIME, K_NOLIST
	     } _Keywords;

typedef enum {T_STRING, T_BOOLEAN, T_INTEGER} _Types;

#define GROUP_NAME	"%s can't get group name in %s line %d"
#define PEER_IN_PEER	"%s peer can't contain peer in %s line %d"
#define PEER_NAME	"%s can't get peer name in %s line %d"
#define LEFT_BRACE	"%s '{' expected in %s line %d"
#define RIGHT_BRACE	"%s '}' unexpected line %d in %s"
#define INCOMPLETE_PEER "%s incomplete peer (%s) in %s line %d"
#define INCOMPLETE_GROUP "%s incomplete group (%s) in %s line %d"
#define MUST_BE_BOOL    "%s Must be 'true' or 'false' in %s line %d"
#define MUST_BE_INT	"%s Must be an integer value in %s line %d"
#define HOST_NEEDED     "%s 'hostname' needed in %s line %d"
#define DUPLICATE_KEY   "%s duplicate key in %s line %d"

/*
** Stuff needed for limiting incoming connects.
*/
static char		RCterm[] = "\r\n";
static REMOTETABLE	remotetable[REMOTETABLESIZE];
static int		remotecount;
static int		remotefirst;

/*
 * Check that the client has the right identd. Return TRUE if is the
 * case, false, if not.
 */
static bool
GoodIdent(int fd, char *identd)
{
#define PORT_IDENTD 113
    char IDENTuser[80];
    struct sockaddr_storage ss_local;
    struct sockaddr_storage ss_distant;
    struct sockaddr *s_local = (struct sockaddr *)&ss_local;
    struct sockaddr *s_distant = (struct sockaddr *)&ss_distant;
    int ident_fd;
    socklen_t len;
    int port1,port2;
    ssize_t lu;
    char buf[80], *buf2;

    if(identd[0] == '\0') {
         return TRUE;
    }
    
    len = sizeof( ss_local );
    if ((getsockname(fd,s_local,&len)) < 0) {
	syslog(L_ERROR, "can't do getsockname for identd");
	return FALSE;
    }
    len = sizeof( ss_distant );
    if ((getpeername(fd,s_distant,&len)) < 0) {
	syslog(L_ERROR, "can't do getsockname for identd");
	return FALSE;
    }
#ifdef HAVE_INET6
    if( s_local->sa_family == AF_INET6 )
    {
	struct sockaddr_in6 *s_l6 = (struct sockaddr_in6 *)s_local;
	struct sockaddr_in6 *s_d6 = (struct sockaddr_in6 *)s_distant;

	port1=ntohs(s_l6->sin6_port);
	port2=ntohs(s_d6->sin6_port);
	s_l6->sin6_port = 0;
	s_d6->sin6_port = htons( PORT_IDENTD );
	ident_fd=socket(PF_INET6, SOCK_STREAM, 0);
    } else
#endif
    if( s_local->sa_family == AF_INET )
    {
	struct sockaddr_in *s_l = (struct sockaddr_in *)s_local;
	struct sockaddr_in *s_d = (struct sockaddr_in *)s_distant;

	port1=ntohs(s_l->sin_port);
	port2=ntohs(s_d->sin_port);
	s_l->sin_port = 0;
	s_d->sin_port = htons( PORT_IDENTD );
	ident_fd=socket(PF_INET, SOCK_STREAM, 0);
    } else
    {
	syslog(L_ERROR, "Bad address family: %d\n", s_local->sa_family );
	return FALSE;
    }
    if (ident_fd < 0) {
	syslog(L_ERROR, "can't open socket for identd (%m)");
	return FALSE;
    }
    if (bind(ident_fd,s_local,SA_LEN(s_local)) < 0) {
	syslog(L_ERROR, "can't bind socket for identd (%m)");
	return FALSE;
    }
    if (connect(ident_fd,s_distant,SA_LEN(s_distant)) < 0) {
	syslog(L_ERROR, "can't connect to identd (%m)");
	return FALSE;
    }

    snprintf(buf,sizeof(buf),"%d,%d\r\n",port2, port1);
    write(ident_fd,buf, strlen(buf));
    memset( buf, 0, 80 );
    lu=read(ident_fd, buf, 79); /* pas encore parfait ("not yet perfect"?) */
    if (lu<0)
    {
	syslog(L_ERROR, "error reading from ident server: %m" );
	close( ident_fd );
	return FALSE;
    }
    buf[lu]='\0';
    if ((lu>0) && (strstr(buf,"ERROR")==NULL)
		    && ((buf2=strrchr(buf,':'))!=NULL)) 
    {
	buf2++;
	while(*buf2 == ' ') buf2++;
	strncpy(IDENTuser,buf2,strlen(buf2)+1);
	IDENTuser[79]='\0';
	buf2=strchr(IDENTuser,'\r');
	if (!buf2) buf2=strchr(IDENTuser,'\n');
	if (buf2) *buf2='\0';
    } else { 
	strncpy(IDENTuser,"UNKNOWN",10);
	IDENTuser[10]='\0';
    }
    close(ident_fd);

    return EQ(identd, IDENTuser);
}

/*
 * Split text into comma-separated fields.  Return an allocated
 * NULL-terminated array of the fields within the modified argument that
 * the caller is expected to save or free.  We don't use strchr() since
 * the text is expected to be either relatively short or "comma-dense."
 * (This function is different from CommaSplit because spaces are allowed
 * and removed here)
 */

static char **
RCCommaSplit(char *text)
{
    int		i;
    char	*p;
    char	*q;
    char	*r;
    char	**av;
    char	**save;
 
    /* How much space do we need? */
    for (i = 2, p = text, q = r = COPY(text); *p; p++) {
        if (*p != ' ' && *p != '\t' && *p != '\n')
	    *q++ = *p;
        if (*p == ',')
            i++;
    }
    *q = '\0';
    DISPOSE (text);
    for (text = r, av = save = NEW(char*, i), *av++ = p = text; *p; )
        if (*p == ',') {
            *p++ = '\0';
	    *av++ = p;
        }
        else
            p++;
    *av = NULL;
    return save;
}

 /*
  * Routine to disable IP-level socket options. This code was taken from 4.4BSD
  * rlogind source, but all mistakes in it are my fault.
  *
  * Author: Wietse Venema, Eindhoven University of Technology, The Netherlands.
  *
  * 21-Jan-1997 smd
  *     Code copied again, and modified for INN, all new mistakes are mine.
  * 
  */

/* fix_options - get rid of IP-level socket options */
#ifndef IP_OPTIONS
#define IP_OPTIONS 1
#endif

static int
RCfix_options(int fd, struct sockaddr_storage *remote)
{
#if IP_OPTIONS
    unsigned char optbuf[BUFSIZ / 3], *cp;
    char    lbuf[BUFSIZ], *lp;
    socklen_t optsize = sizeof(optbuf);
    int     ipproto;
    struct protoent *ip;

    switch (remote->ss_family) {
    case AF_INET:
	if ((ip = getprotobyname("ip")) != 0)
	    ipproto = ip->p_proto;
	else
	    ipproto = IPPROTO_IP;
	break;
#ifdef HAVE_INET6
    case AF_INET6:
	if ((ip = getprotobyname("ipv6")) != 0)
	    ipproto = ip->p_proto;
	else
	    ipproto = IPPROTO_IPV6;
	break;
#endif
    default:
	syslog(LOG_ERR, "unknown address family: %d", remote->ss_family);
	return -1;
    }

    if (getsockopt(fd, ipproto, IP_OPTIONS, (char *) optbuf, &optsize) == 0
	&& optsize != 0) {
	lp = lbuf;
	for (cp = optbuf; optsize > 0; cp++, optsize--, lp += 3)
	    sprintf(lp, " %2.2x", *cp);
	syslog(LOG_NOTICE,
	       "connect from %s with IP options (ignored):%s",
	       sprint_sockaddr((struct sockaddr *)remote), lbuf);
	if (setsockopt(fd, ipproto, IP_OPTIONS, (char *) 0, optsize) != 0) {
	    syslog(LOG_ERR, "setsockopt IP_OPTIONS NULL: %m");
	    return -1;
	}
    }
#endif
    return 0;
}

static bool
RCaddressmatch(const struct sockaddr_storage *cp, const struct sockaddr_storage *rp)
{
#ifdef HAVE_INET6
    struct sockaddr_in	*sin_cp, *sin_rp;
    struct sockaddr_in6	*sin6_cp, *sin6_rp;

    if (cp->ss_family == AF_INET6 && rp->ss_family == AF_INET) {
	sin6_cp = (struct sockaddr_in6 *)cp;
	sin_rp = (struct sockaddr_in *)rp;
	if (IN6_IS_ADDR_V4MAPPED(&sin6_cp->sin6_addr) &&
		memcmp(&sin6_cp->sin6_addr.s6_addr[12],
		    &sin_rp->sin_addr.s_addr, sizeof(struct in_addr)) == 0)
	    return TRUE;
    } else if (cp->ss_family == AF_INET && rp->ss_family == AF_INET6) {
	sin_cp = (struct sockaddr_in *)cp;
	sin6_rp = (struct sockaddr_in6 *)rp;
	if (IN6_IS_ADDR_V4MAPPED(&sin6_rp->sin6_addr) &&
		memcmp(&sin6_rp->sin6_addr.s6_addr[12],
		    &sin_cp->sin_addr.s_addr, sizeof(struct in_addr)) == 0)
	    return TRUE;
    } else if (cp->ss_family == AF_INET6 && rp->ss_family == AF_INET6) {
#ifdef HAVE_BROKEN_IN6_ARE_ADDR_EQUAL
	if (!memcmp(&((struct sockaddr_in6 *)cp)->sin6_addr,
		    &((struct sockaddr_in6 *)rp)->sin6_addr,
		    sizeof(struct in6_addr)))
#else
	if (IN6_ARE_ADDR_EQUAL( &((struct sockaddr_in6 *)cp)->sin6_addr,
				&((struct sockaddr_in6 *)rp)->sin6_addr))
#endif
	    return TRUE;
    } else
#endif /* INET6 */
	if (((struct sockaddr_in *)cp)->sin_addr.s_addr ==
	     ((struct sockaddr_in *)rp)->sin_addr.s_addr)
	    return TRUE;

    return FALSE;
}

/*
**  See if the site properly entered the password.
*/
bool
RCauthorized(CHANNEL *cp, char *pass)
{
    REMOTEHOST	*rp;
    int		i;

    for (rp = RCpeerlist, i = RCnpeerlist; --i >= 0; rp++)
	if (RCaddressmatch(&cp->Address, &rp->Address)) {
	    if (rp->Password[0] == '\0' || EQ(pass, rp->Password))
		return TRUE;
	    syslog(L_ERROR, "%s (%s) bad_auth", rp->Label,
		   sprint_sockaddr((struct sockaddr *)&cp->Address));
	    return FALSE;
	}

    if (!AnyIncoming)
	/* Not found in our table; this can't happen. */
	syslog(L_ERROR, "%s not_found", sprint_sockaddr((struct sockaddr *)&cp->Address));

    /* Anonymous hosts should not authenticate. */
    return FALSE;
}

/*
**  See if a host is limited or not.
*/
bool
RCnolimit(CHANNEL *cp)
{
    REMOTEHOST	*rp;
    int		i;

    for (rp = RCpeerlist, i = RCnpeerlist; --i >= 0; rp++)
	if (RCaddressmatch(&cp->Address, &rp->Address))
            return !rp->MaxCnx;

    /* Not found in our table; this can't happen. */
    return FALSE;
}

/*
**  Return the limit (max number of connections) for a host.
*/
int
RClimit(CHANNEL *cp)
{
    REMOTEHOST	*rp;
    int		i;

    for (rp = RCpeerlist, i = RCnpeerlist; --i >= 0; rp++)
	if (RCaddressmatch(&cp->Address, &rp->Address))
	    return rp->MaxCnx;
    /* Not found in our table; this can't happen. */
    return RemoteLimit;
}


/*
**  Called when input is ready to read.  Shouldn't happen.
*/
static void
RCrejectreader(CHANNEL *cp)
{
    syslog(L_ERROR, "%s internal RCrejectreader (%s)", LogName,
	   sprint_sockaddr((struct sockaddr *)&cp->Address));
}


/*
**  Write-done function for rejects.
*/
static void
RCrejectwritedone(CHANNEL *cp)
{
    switch (cp->State) {
    default:
	syslog(L_ERROR, "%s internal RCrejectwritedone state %d",
	    CHANname(cp), cp->State);
	break;
    case CSwritegoodbye:
	CHANclose(cp, CHANname(cp));
	break;
    }
}


/*
**  Hand off a descriptor to NNRPD.
*/
void
RChandoff(int fd, HANDOFF h)
{
    const char *argv[6];
    char buff[SMBUF];
    int i;

    if (RCnnrpd == NULL)
	RCnnrpd = concatpath(innconf->pathbin, "nnrpd");
    if (RCnntpd == NULL)
	RCnntpd = concatpath(innconf->pathbin, "nnrpd");
#if	defined(SOL_SOCKET) && defined(SO_KEEPALIVE)
    /* Set KEEPALIVE to catch broken socket connections. */
    i = 1;
    if (setsockopt(fd, SOL_SOCKET,  SO_KEEPALIVE, (char *)&i, sizeof i) < 0)
        syslog(L_ERROR, "fd %d cant setsockopt(KEEPALIVE) %m", fd);
#endif /* defined(SOL_SOCKET) && defined(SO_KEEPALIVE) */

    if (nonblocking(fd, false) < 0)
	syslog(L_ERROR, "%s cant nonblock %d in RChandoff %m", LogName, fd);
    switch (h) {
    default:
	syslog(L_ERROR, "%s internal RChandoff %d type %d", LogName, fd, h);
	/* FALLTHROUGH */
    case HOnnrpd:	argv[0] = RCnnrpd;	break;
    case HOnntpd:	argv[0] = RCnntpd;	break;
    }
    argv[1] = "-s                                                ";
    i = 2;
    if (NNRPReason) {
	snprintf(buff, sizeof(buff), "-r%s", NNRPReason);
	argv[i++] = buff;
    }
    if (NNRPTracing)
	argv[i++] = "-t";
    if (RCslaveflag)
	argv[i++] = RCslaveflag;
    argv[i] = NULL;

    /* Call NNRP; don't send back a QUIT message if Spawn fails since  
     * that's a major error we want to find out about quickly. */
    (void)Spawn(innconf->nicekids, fd, fd, fd, (char * const *)argv);
}


/*
**  Read function.  Accept the connection and either create an NNTP channel
**  or spawn an nnrpd to handle it.
*/
static void
RCreader(CHANNEL *cp)
{
    int			fd;
    struct sockaddr_storage	remote;
    socklen_t		size;
    int                 i;
    REMOTEHOST          *rp;
    CHANNEL		*new;
    char		*name;
    long		reject_val = 0;
    const char		*reject_message;
    int			count;
    int			found;
    time_t		now;
    CHANNEL		tempchan;
    char		buff[SMBUF];

    for (i = 0 ; i < chanlimit ; i++) {
	if (RCchan[i] == cp) {
	    break;
	}
    }
    if (i == chanlimit) {
	syslog(L_ERROR, "%s internal RCreader wrong channel 0x%p", LogName, cp);
	return;
    }

    /* Get the connection. */
    size = sizeof remote;
    if ((fd = accept(cp->fd, (struct sockaddr *)&remote, &size)) < 0) {
	if (errno != EWOULDBLOCK && errno != EAGAIN)
	    syslog(L_ERROR, "%s cant accept RCreader %m", LogName);
	return;
    }

    /*
    ** Clear any IP_OPTIONS, including source routing, on the socket
    */
    /* FIXME RCfix_options breaks IPv6 sockets, at least on Linux -lutchann */
#ifndef HAVE_INET6
    if (RCfix_options(fd, &remote) != 0) {
	/* shouldn't happen, but we're bit paranoid at this point */
	if (close(fd) < 0)
	    syslog(L_ERROR, "%s cant close %d %m", LogName, fd);
	return;
    }
#endif

    /*
    ** If RemoteTimer is not zero, then check the limits on incoming
    ** connections on a total and per host basis.
    **
    ** The incoming connection table is fixed at 128 entries to make
    ** calculating the index easy (i + 1) & 7, and to be pretty sure
    ** that you won't run out of space.  The table is used as a ring
    ** with new entries being added to the end (wrapping around) and
    ** expired entries being deleted from the front (again wrapping
    ** around).  It is doubtful that you will ever use even half of
    ** the table.
    **
    ** There are three parameters controlling the use of the table not
    ** counting the starting index and count.
    **
    ** H = per host incoming connects per X seconds allowed
    ** T = total incoming connects per X seconds allowed
    ** X = number of seconds to remember a successful connect
    **
    ** First, one pass is made over the live entries deleting any that
    ** are over X seconds old.  If the entry hasn't expired, compare
    ** the incoming connection's host address with the entry's host
    ** address.  If equal, increment the ``found'' counter.
    **
    ** Second, if the number of entries now in the table is equal to
    ** the ``T'' parameter, reject the connection with the ``504''
    ** error code.
    **
    ** 504 Server overloaded, try later
    **
    ** Third, if the number of entries now in the table which match
    ** the incoming connection's host address is equal to the ``H''
    ** parameter, reject the connection with the ``505'' error code.
    **
    ** 505 Connection rejected, you're making too many connects per minute
    **
    ** Finally, if neither rejection happened, add the entry to the
    ** table, and continue on as a normal connect.
    */
    memcpy(&tempchan.Address, &remote, SA_LEN((struct sockaddr *)&remote));
    reject_message = NULL;
    if (RemoteTimer != 0) {
	now = time(NULL);
	i = remotefirst;
	count = remotecount;
	found = 0;
	while (count--) {
	    if (remotetable[i].Expires < now) {
		remotecount--;
		remotefirst = (remotefirst + 1) & (REMOTETABLESIZE - 1);
		i = (i + 1) & (REMOTETABLESIZE - 1);
		continue;
	    }
	    if (RCaddressmatch(&remotetable[i].Address, &remote))
		found++;
	    i = (i + 1) & (REMOTETABLESIZE - 1);
	}
	if (remotecount == RemoteTotal) {
	    reject_val = NNTP_SERVER_TOOBUSY_VAL;
	    reject_message = NNTP_SERVER_TOOBUSY;
	}
	else if (found >= RemoteLimit && !RCnolimit(&tempchan)) {
	    reject_val = NNTP_TOO_MANY_CONNECTS_VAL;
	    reject_message = NNTP_TOO_MANY_CONNECTS;
	}
	else {
	    i = (remotefirst + remotecount) & (REMOTETABLESIZE - 1);
	    memcpy(&remotetable[i].Address, &remote, SA_LEN((struct sockaddr *)&remote));
	    remotetable[i].Expires = now + RemoteTimer;
	    remotecount++;
	}
    }

    /*
    ** Create a reject channel to reject the connection.  This is done
    ** to avoid a call to fork.  
    */
    if (reject_message) {
	new = CHANcreate(fd, CTreject, CSwritegoodbye, RCrejectreader,
	    RCrejectwritedone);
	memcpy(&remotetable[i].Address, &remote, SA_LEN((struct sockaddr *)&remote));
	new->Rejected = reject_val;
	RCHANremove(new);
	WCHANset(new, reject_message, (int)strlen(reject_message));
	WCHANappend(new, RCterm, STRLEN(RCterm));
	WCHANadd(new);
	return;
    }

    /* See if it's one of our servers. */
    for (name = NULL, rp = RCpeerlist, i = RCnpeerlist; --i >= 0; rp++)
	if (RCaddressmatch(&rp->Address, &remote)) {
	    name = rp->Name;
	    break;
	}

    /* If not a server, and not allowing anyone, hand him off unless
       not spawning nnrpd in which case we return an error. */
    if ((i >= 0) && !rp->Skip) {

	/* We check now the identd if we have to */
	if(! GoodIdent(fd, rp->Identd))
	{
	    if (!innconf->noreader) {
		RChandoff(fd, HOnntpd);
		if (close(fd) < 0)
		    syslog(L_ERROR, "%s cant close %d %m", LogName, fd);
		return;
	    }
	}
	
	if ((new = NCcreate(fd, rp->Password[0] != '\0', FALSE)) != NULL) {
            new->Streaming = rp->Streaming;
            new->Skip = rp->Skip;
            new->NoResendId = rp->NoResendId;
            new->Nolist = rp->Nolist;
            new->MaxCnx = rp->MaxCnx;
            new->HoldTime = rp->HoldTime;
	    memcpy(&new->Address, &remote, SA_LEN((struct sockaddr *)&remote));
	    if (new->MaxCnx > 0 && new->HoldTime == 0) {
		CHANsetActiveCnx(new);
		if((new->ActiveCnx > new->MaxCnx) && (new->fd > 0)) {
		    snprintf(buff, sizeof(buff),
                             "You are limited to %d connection%s",
                             new->MaxCnx, (new->MaxCnx != 1) ? "s" : "");
		    NCwriteshutdown(new, buff);
		    syslog(L_NOTICE, "too many connections from %s", rp->Label);
		} else {
		    NCwritereply(new, (char *)NCgreeting);
		}
	    } else {
		NCwritereply(new, (char *)NCgreeting);
	    }
	}
    } else if (AnyIncoming && !rp->Skip) {
	if ((new = NCcreate(fd, FALSE, FALSE)) != NULL) {
	    NCwritereply(new, (char *)NCgreeting);
	}
    } else if (!innconf->noreader) {
	RChandoff(fd, HOnntpd);
	if (close(fd) < 0)
	    syslog(L_ERROR, "%s cant close %d %m", LogName, fd);
	return;
    } else {
	reject_val = NNTP_ACCESS_VAL;
	reject_message = NNTP_ACCESS;
        new = CHANcreate(fd, CTreject, CSwritegoodbye, RCrejectreader,
            RCrejectwritedone);
	memcpy(&new->Address, &remote, SA_LEN((struct sockaddr *)&remote));
        new->Rejected = reject_val;
        RCHANremove(new);
        WCHANset(new, reject_message, (int)strlen(reject_message));
        WCHANappend(new, RCterm, STRLEN(RCterm));
        WCHANadd(new);
        return;
    }

    if (new != NULL) {
	memcpy(&new->Address, &remote, SA_LEN((struct sockaddr *)&remote));
	syslog(L_NOTICE, "%s connected %d streaming %s",
           name ? name : sprint_sockaddr((struct sockaddr *)&new->Address),
	   new->fd, (!StreamingOff && new->Streaming) ? "allowed" : "not allowed");
    }
}


/*
**  Write-done function.  Shouldn't happen.
*/
static void
RCwritedone(CHANNEL *unused)
{
    unused = unused;		/* ARGSUSED */
    syslog(L_ERROR, "%s internal RCwritedone", LogName);
}

/*
 *  New config file style. Old hosts.nntp and hosts.nntp.nolimit are merged
 *  into one file called incoming.conf (to avoid confusion).
 *  See ../samples/incoming.conf for the new syntax.
 *
 *  Fabien Tassin <fta@sofaraway.org>, 21-Dec-1997.
 */


/*
 * Read something (a word or a double quoted string) from a file.
 */
static char *
RCreaddata(int *num, FILE *F, bool *toolong)
{
  char	*p;
  char	*s;
  char	*t;
  char	*word;
  bool	flag;

  *toolong = FALSE;
  if (*RCbuff == '\0') {
    if (feof (F)) return (NULL);
    fgets(RCbuff, sizeof RCbuff, F);
    (*num)++;
    if (strlen (RCbuff) == sizeof RCbuff) {
      *toolong = TRUE;
      return (NULL); /* Line too long */
    }
  }
  p = RCbuff;
  do {
     /* Ignore blank and comment lines. */
     if ((p = strchr(RCbuff, '\n')) != NULL)
       *p = '\0';
     if ((p = strchr(RCbuff, COMMENT_CHAR)) != NULL) {
       if (p == RCbuff || (p > RCbuff && *(p - 1) != '\\'))
	   *p = '\0';
     }
     for (p = RCbuff; *p == ' ' || *p == '\t' ; p++);
     flag = TRUE;
     if (*p == '\0' && !feof (F)) {
       flag = FALSE;
       fgets(RCbuff, sizeof RCbuff, F);
       (*num)++;
       if (strlen (RCbuff) == sizeof RCbuff) {
	 *toolong = TRUE;
	 return (NULL); /* Line too long */
       }
       continue;
     }
     break;
  } while (!feof (F) || !flag);

  if (*p == '"') { /* double quoted string ? */
    p++;
    do {
      for (t = p; (*t != '"' || (*t == '"' && *(t - 1) == '\\')) &&
	     *t != '\0'; t++);
      if (*t == '\0') {
	*t++ = '\n';
	fgets(t, sizeof RCbuff - strlen (RCbuff), F);
	(*num)++;
	if (strlen (RCbuff) == sizeof RCbuff) {
	  *toolong = TRUE;
	  return (NULL); /* Line too long */
	}
	if ((s = strchr(t, '\n')) != NULL)
	  *s = '\0';
      }
      else 
	break;
    } while (!feof (F));
    *t++ = '\0';
  }
  else {
    for (t = p; *t != ' ' && *t != '\t' && *t != '\0'; t++);
    if (*t != '\0')
      *t++ = '\0';
  }
  if (*p == '\0' && feof (F)) return (NULL);
  word = COPY (p);
  for (p = RCbuff; *t != '\0'; t++)
    *p++ = *t;
  *p = '\0';

  return (word);
}

/*
 *  Add all data into RCpeerlistfile.
 */
static void
RCadddata(REMOTEHOST_DATA **d, int *count, int Key, int Type, char* Value)
{
  (*d)[*count].key = Key;
  (*d)[*count].type = Type;
  (*d)[*count].value = Value;
  (*count)++;
  RENEW(*d, REMOTEHOST_DATA, *count + 1);
}

/*
**  Read in the file listing the hosts we take news from, and fill in the
**  global list of their Internet addresses.  On modern systems a host can
**  have multiple addresses, so we take care to add all of them to the list.
**  We can distinguish between the two because h_addr is a #define for the
**  first element of the address list in modern systems, while it's a field
**  name in old ones.
*/
static void
RCreadfile (REMOTEHOST_DATA **data, REMOTEHOST **list, int *count, 
	    char *filename)
{
    static char		NOPASS[] = "";
    static char		NOIDENTD[] = "";
    static char		NOEMAIL[] = "";
    static char		NOCOMMENT[] = "";
    FILE		*F;
    char 		*p;
    char 		**q;
    char 		**r;
#if     !defined( HAVE_INET6)
    struct hostent	*hp;
#endif
#if	!defined(HAVE_UNIX_DOMAIN_SOCKETS) || !defined(HAVE_INET6)
    struct in_addr      addr;
#endif
    int                 i;
    int                 j;
    int			linecount;
    int			infocount;
    int                 groupcount;
    int                 maxgroup;
    REMOTEHOST_DATA 	*dt;
    REMOTEHOST		*rp;
    char		*word;
    REMOTEHOST		*groups;
    REMOTEHOST		*group_params = NULL;
    REMOTEHOST		peer_params;
    REMOTEHOST		default_params;
    bool		flag, bit, toolong;
 
    *RCbuff = '\0';
    if (*list) {
	for (rp = *list, i = *count; --i >= 0; rp++) {
	    DISPOSE(rp->Name);
	    DISPOSE(rp->Label);
	    DISPOSE(rp->Email);
	    DISPOSE(rp->Comment);
	    DISPOSE(rp->Password);
	    DISPOSE(rp->Identd);
	    if (rp->Patterns) {
		DISPOSE(rp->Patterns[0]);
		DISPOSE(rp->Patterns);
	    }
	}
	DISPOSE(*list);
	*list = NULL;
	*count = 0;
    }
    if (*data) {
        for (i = 0; (*data)[i].key != K_END; i++)
	    if ((*data)[i].value != NULL)
	        DISPOSE((*data)[i].value);
        DISPOSE(*data);
	*data = NULL;
    }

    *count = 0;
    maxgroup = 0;
    /* Open the server file. */
    if ((F = Fopen(filename, "r", TEMPORARYOPEN)) == NULL) {
	syslog(L_FATAL, "%s cant read %s: %m", LogName, filename);
	exit(1);
    }
    dt = *data = NEW(REMOTEHOST_DATA, 1);
    rp = *list = NEW(REMOTEHOST, 1);

#if	!defined(HAVE_UNIX_DOMAIN_SOCKETS)
    addr.s_addr = INADDR_LOOPBACK;
    make_sin( (struct sockaddr_in *)&rp->Address, &addr );
    rp->Name = COPY("localhost");
    rp->Label = COPY("localhost");
    rp->Email = COPY(NOEMAIL);
    rp->Comment = COPY(NOCOMMENT);
    rp->Password = COPY(NOPASS);
    rp->Identd = COPY(NOIDENTD);
    rp->Patterns = NULL;
    rp->MaxCnx = 0;
    rp->Streaming = TRUE;
    rp->Skip = FALSE;
    rp->NoResendId = FALSE;
    rp->Nolist = FALSE;
    rp->HoldTime = 0;
    rp++;
    (*count)++;
#endif	/* !defined(HAVE_UNIX_DOMAIN_SOCKETS) */

    linecount = 0;
    infocount = 0;
    groupcount = 0; /* no group defined yet */
    groups = 0;
    peer_params.Label = NULL;
    default_params.Streaming = TRUE;
    default_params.Skip = FALSE;
    default_params.NoResendId = FALSE;
    default_params.Nolist = FALSE;
    default_params.MaxCnx = 0;
    default_params.HoldTime = 0;
    default_params.Password = COPY(NOPASS);
    default_params.Identd = COPY(NOIDENTD);
    default_params.Email = COPY(NOEMAIL);
    default_params.Comment = COPY(NOCOMMENT);
    default_params.Pattern = NULL;
    peer_params.Keysetbit = 0;

    /* Read the file to add all the hosts. */
    while ((word = RCreaddata (&linecount, F, &toolong)) != NULL) {

      /* group */
      if (!strncmp (word, GROUP,  sizeof GROUP)) {
	DISPOSE(word);
	/* name of the group */
	if ((word = RCreaddata (&linecount, F, &toolong)) == NULL) {
	  syslog(L_ERROR, GROUP_NAME, LogName, filename, linecount);
	  break;
	}
	RCadddata(data, &infocount, K_BEGIN_GROUP, T_STRING, word);
	groupcount++;
	if (groupcount == 1) {
	  group_params = groups = NEW(REMOTEHOST, 1);
	}
	else if (groupcount >= maxgroup) {
	  RENEW(groups, REMOTEHOST, groupcount + 4); /* alloc 5 groups */
	  maxgroup += 5;
	  group_params = groups + groupcount - 1;
	}
	group_params->Label = word;
	group_params->Skip = groupcount > 1 ?
	  groups[groupcount - 2].Skip : default_params.Skip;
	group_params->Streaming = groupcount > 1 ?
	  groups[groupcount - 2].Streaming : default_params.Streaming;
	group_params->NoResendId = groupcount > 1 ?
	  groups[groupcount - 2].NoResendId : default_params.NoResendId;
	group_params->Nolist = groupcount > 1 ?
	  groups[groupcount - 2].Nolist : default_params.Nolist;
	group_params->Email = groupcount > 1 ?
	  groups[groupcount - 2].Email : default_params.Email;
	group_params->Comment = groupcount > 1 ?
	  groups[groupcount - 2].Comment : default_params.Comment;
	group_params->Pattern = groupcount > 1 ?
	  groups[groupcount - 2].Pattern : default_params.Pattern;
	group_params->Password = groupcount > 1 ?
	  groups[groupcount - 2].Password : default_params.Password;
	group_params->Identd = groupcount > 1 ?
	  groups[groupcount - 2].Identd : default_params.Identd;
	group_params->MaxCnx = groupcount > 1 ?
	  groups[groupcount - 2].MaxCnx : default_params.MaxCnx;
	group_params->HoldTime = groupcount > 1 ?
	  groups[groupcount - 2].HoldTime : default_params.HoldTime;

	if ((word = RCreaddata (&linecount, F, &toolong)) == NULL) {
	  syslog(L_ERROR, LEFT_BRACE, LogName, filename, linecount);
	  break;
	}
	/* left brace */
	if (strncmp (word, "{", 1)) {
	  DISPOSE(word);
	  syslog(L_ERROR, LEFT_BRACE, LogName, filename, linecount);
	  break;
	}
	else
	  DISPOSE(word);
	peer_params.Keysetbit = 0;
	continue;
      }

      /* peer */
      if (!strncmp (word, PEER, sizeof PEER)) {
	DISPOSE(word);
	if (peer_params.Label != NULL) {
	  /* peer can't contain peer */
	  syslog(L_ERROR, PEER_IN_PEER, LogName, 
	      filename, linecount);
	  break;
	}
	if ((word = RCreaddata (&linecount, F, &toolong)) == NULL)
	{
	  syslog(L_ERROR, PEER_NAME, LogName, filename, linecount);
	  break;
	}
	RCadddata(data, &infocount, K_BEGIN_PEER, T_STRING, word);
	/* name of the peer */
	peer_params.Label = word;
	peer_params.Name = NULL;
	peer_params.Skip = groupcount > 0 ?
	  group_params->Skip : default_params.Skip;
	peer_params.Streaming = groupcount > 0 ?
	  group_params->Streaming : default_params.Streaming;
	peer_params.NoResendId = groupcount > 0 ?
	  group_params->NoResendId : default_params.NoResendId;
	peer_params.Nolist = groupcount > 0 ?
	  group_params->Nolist : default_params.Nolist;
	peer_params.Email = groupcount > 0 ?
	  group_params->Email : default_params.Email;
	peer_params.Comment = groupcount > 0 ?
	  group_params->Comment : default_params.Comment;
	peer_params.Pattern = groupcount > 0 ?
	  group_params->Pattern : default_params.Pattern;
	peer_params.Password = groupcount > 0 ?
	  group_params->Password : default_params.Password;
	peer_params.Identd = groupcount > 0 ?
	  group_params->Identd : default_params.Identd;
	peer_params.MaxCnx = groupcount > 0 ?
	  group_params->MaxCnx : default_params.MaxCnx;
	peer_params.HoldTime = groupcount > 0 ?
	  group_params->HoldTime : default_params.HoldTime;

	peer_params.Keysetbit = 0;

	if ((word = RCreaddata (&linecount, F, &toolong)) == NULL)
	{
	  syslog(L_ERROR, LEFT_BRACE, LogName, filename, linecount);
	  break;
	}
	/* left brace */
	if (strncmp (word, "{", 1)) {
	  syslog(L_ERROR, LEFT_BRACE, LogName, filename, linecount);
	  DISPOSE(word);
	  break;
	}
	else
	  DISPOSE(word);
	continue;
      }

      /* right brace */
      if (!strncmp (word, "}", 1)) {
	DISPOSE(word);
	if (peer_params.Label != NULL) {
	  RCadddata(data, &infocount, K_END_PEER, T_STRING, NULL);

	  /* Hostname defaults to label if not given */
	  if (peer_params.Name == NULL)
            peer_params.Name = COPY(peer_params.Label);

	  for(r = q = RCCommaSplit(COPY(peer_params.Name)); *q != NULL; q++) {
#ifdef HAVE_INET6
	      struct addrinfo *res, *res0, hints;
	      int gai_ret;
#endif
	    (*count)++;

	    /* Grow the array */
	    j = rp - *list;
	    RENEW (*list, REMOTEHOST, *count);
	    rp = *list + j;

#ifdef HAVE_INET6
	    memset( &hints, 0, sizeof( hints ) );
	    hints.ai_socktype = SOCK_STREAM;
	    hints.ai_family = PF_UNSPEC;
	    if ((gai_ret = getaddrinfo(*q, NULL, &hints, &res0)) != 0) {
		syslog(L_ERROR, "%s cant getaddrinfo %s %s", LogName, *q,
				gai_strerror( gai_ret ) );
		/* decrement *count, since we never got to add this record. */
		(*count)--;
		continue;
	    }
	    /* Count the addresses and see if we have to grow the list */
	    i = 0;
	    for (res = res0; res != NULL; res = res->ai_next)
		i++;
	    /* Grow the array */
	    j = rp - *list;
	    *count += i - 1;
	    RENEW(*list, REMOTEHOST, *count);
	    rp = *list + j;

	    /* Add all hosts */
	    for (res = res0; res != NULL; res = res->ai_next) {
		(void)memcpy(&rp->Address, res->ai_addr, res->ai_addrlen);
		rp->Name = COPY (*q);
		rp->Label = COPY (peer_params.Label);
		rp->Email = COPY(peer_params.Email);
		rp->Comment = COPY(peer_params.Comment);
		rp->Streaming = peer_params.Streaming;
		rp->Skip = peer_params.Skip;
		rp->NoResendId = peer_params.NoResendId;
		rp->Nolist = peer_params.Nolist;
		rp->Password = COPY(peer_params.Password);
		rp->Identd = COPY(peer_params.Identd);
		rp->Patterns = peer_params.Pattern != NULL ?
		    RCCommaSplit(COPY(peer_params.Pattern)) : NULL;
		rp->MaxCnx = peer_params.MaxCnx;
		rp->HoldTime = peer_params.HoldTime;
		rp++;
	    }
	    freeaddrinfo(res0);
#else /* HAVE_INET6 */
	    /* Was host specified as a dotted quad ? */
	    if (inet_aton(*q, &addr)) {
	      make_sin( (struct sockaddr_in *)&rp->Address, &addr );
	      rp->Name = COPY (*q);
	      rp->Label = COPY (peer_params.Label);
	      rp->Password = COPY(peer_params.Password);
	      rp->Identd = COPY(peer_params.Identd);
	      rp->Skip = peer_params.Skip;
	      rp->Streaming = peer_params.Streaming;
	      rp->NoResendId = peer_params.NoResendId;
	      rp->Nolist = peer_params.Nolist;
	      rp->Email = COPY(peer_params.Email);
	      rp->Comment = COPY(peer_params.Comment);
	      rp->Patterns = peer_params.Pattern != NULL ?
		    RCCommaSplit(COPY(peer_params.Pattern)) : NULL;
	      rp->MaxCnx = peer_params.MaxCnx;
	      rp->HoldTime = peer_params.HoldTime;
	      rp++;
	      continue;
	    }
	    
	    /* Host specified as a text name ? */
	    if ((hp = gethostbyname(*q)) == NULL) {
	      syslog(L_ERROR, "%s cant gethostbyname %s %m", LogName, *q);
	      /* decrement *count, since we never got to add this record. */
	      (*count)--;
	      continue;
	    }

#if	    defined(h_addr)
	    /* Count the adresses and see if we have to grow the list */
	    for (i = 0; hp->h_addr_list[i]; i++)
	      continue;
	    if (i == 0) {
	      syslog(L_ERROR, "%s no_address %s %m", LogName, *q);
	      continue;
	    }
	    if (i == 1) {
	      char **rr;
	      int    t = 0;
	      /* Strange DNS ? try this.. */
	      for (rr = hp->h_aliases; *rr != 0; rr++) {
                if (!inet_aton(*rr, &addr))
		  continue;
		(*count)++;
		/* Grow the array */
		j = rp - *list;
		RENEW (*list, REMOTEHOST, *count);
		rp = *list + j;

		make_sin( (struct sockaddr_in *)&rp->Address, &addr );
		rp->Name = COPY (*q);
		rp->Label = COPY (peer_params.Label);
		rp->Email = COPY(peer_params.Email);
		rp->Comment = COPY(peer_params.Comment);
		rp->Streaming = peer_params.Streaming;
		rp->Skip = peer_params.Skip;
		rp->NoResendId = peer_params.NoResendId;
		rp->Nolist = peer_params.Nolist;
		rp->Password = COPY(peer_params.Password);
		rp->Identd = COPY(peer_params.Identd);
		rp->Patterns = peer_params.Pattern != NULL ?
		  RCCommaSplit(COPY(peer_params.Pattern)) : NULL;
		rp->MaxCnx = peer_params.MaxCnx;
		rp->HoldTime = peer_params.HoldTime;
		rp++;
		t++;
	      }
	      if (t == 0) {
		/* Just one, no need to grow. */
		make_sin( (struct sockaddr_in *)&rp->Address,
				(struct in_addr *)hp->h_addr_list[0] );
		rp->Name = COPY (*q);
		rp->Label = COPY (peer_params.Label);
		rp->Email = COPY(peer_params.Email);
		rp->Comment = COPY(peer_params.Comment);
		rp->Streaming = peer_params.Streaming;
		rp->Skip = peer_params.Skip;
		rp->NoResendId = peer_params.NoResendId;
		rp->Nolist = peer_params.Nolist;
		rp->Password = COPY(peer_params.Password);
		rp->Identd = COPY(peer_params.Identd);
		rp->Patterns = peer_params.Pattern != NULL ?
		  RCCommaSplit(COPY(peer_params.Pattern)) : NULL;
		rp->MaxCnx = peer_params.MaxCnx;
		rp->HoldTime = peer_params.HoldTime;
		rp++;
		continue;
	      }
	    }
	    /* Grow the array */
	    j = rp - *list;
	    *count += i - 1;
	    RENEW (*list, REMOTEHOST, *count);
	    rp = *list + j;

	    /* Add all the hosts. */
	    for (i = 0; hp->h_addr_list[i]; i++) {
	      make_sin( (struct sockaddr_in *)&rp->Address,
			      (struct in_addr *)hp->h_addr_list[i] );
	      rp->Name = COPY (*q);
	      rp->Label = COPY (peer_params.Label);
	      rp->Email = COPY(peer_params.Email);
	      rp->Comment = COPY(peer_params.Comment);
	      rp->Streaming = peer_params.Streaming;
	      rp->Skip = peer_params.Skip;
	      rp->NoResendId = peer_params.NoResendId;
	      rp->Nolist = peer_params.Nolist;
	      rp->Password = COPY(peer_params.Password);
	      rp->Identd = COPY(peer_params.Identd);
	      rp->Patterns = peer_params.Pattern != NULL ?
		RCCommaSplit(COPY(peer_params.Pattern)) : NULL;
	      rp->MaxCnx = peer_params.MaxCnx;
	      rp->HoldTime = peer_params.HoldTime;
	      rp++;
	    }
#else
	    /* Old-style, single address, just add it. */
	    make_sin( (struct sockaddr_in *)&rp->Address,
			    (struct in_addr *)hp->h_addr );
	    rp->Name = COPY(*q);
	    rp->Label = COPY (peer_params.Label);
	    rp->Email = COPY(peer_params.Email);
	    rp->Comment = COPY(peer_params.Comment);
	    rp->Streaming = peer_params.Streaming;
	    rp->Skip = peer_params.Skip;
	    rp->NoResendId = peer_params.NoResendId;
	    rp->Nolist = peer_params.Nolist;
	    rp->Password = COPY(peer_params.Password);
	    rp->Identd = COPY(peer_params.Identd);
	    rp->Patterns = peer_params.Pattern != NULL ?
	      RCCommaSplit(COPY(peer_params.Pattern)) : NULL;
	    rp->MaxCnx = peer_params.MaxCnx;
	    rp->HoldTime = peer_params.HoldTime;
	    rp++;
#endif	    /* defined(h_addr) */
#endif /* HAVE_INET6 */
	  }
	  DISPOSE(r[0]);
	  DISPOSE(r);
	  peer_params.Label = NULL;
	}
	else if (groupcount > 0 && group_params->Label != NULL) {
	  RCadddata(data, &infocount, K_END_GROUP, T_STRING, NULL);
	  group_params->Label = NULL;
	  groupcount--;
	  if (groupcount == 0)
	    DISPOSE(groups);
	  else
	    group_params--;
	}
	else {
	  syslog(L_ERROR, RIGHT_BRACE, LogName, linecount, filename);
	}
	continue;
      }

      /* streaming */
      if (!strncmp (word, STREAMING, sizeof STREAMING)) {
	DISPOSE(word);
	TEST_CONFIG(K_STREAM, bit);
        if (bit) {
	  syslog(L_ERROR, DUPLICATE_KEY, LogName, filename, linecount);
	  break;
	}
	if ((word = RCreaddata (&linecount, F, &toolong)) == NULL) {
	  break;
	}
	if (!strcmp (word, "true"))
	  flag = TRUE;
	else
	  if (!strcmp (word, "false"))
	    flag = FALSE;
	  else {
	    syslog(L_ERROR, MUST_BE_BOOL, LogName, filename, linecount);
	    break;
	  }
	RCadddata(data, &infocount, K_STREAM, T_STRING, word);
	if (peer_params.Label != NULL)
	  peer_params.Streaming = flag;
	else
	  if (groupcount > 0 && group_params->Label != NULL)
	    group_params->Streaming = flag;
	  else
	    default_params.Streaming = flag;
	SET_CONFIG(K_STREAM);
	continue;
      }

      /* skip */
      if (!strncmp (word, SKIP, sizeof SKIP)) {
	DISPOSE(word);
	TEST_CONFIG(K_SKIP, bit);
        if (bit) {
	  syslog(L_ERROR, DUPLICATE_KEY, LogName, filename, linecount);
	  break;
	}
	if ((word = RCreaddata (&linecount, F, &toolong)) == NULL) {
	  break;
	}
	if (!strcmp (word, "true"))
	  flag = TRUE;
	else
	  if (!strcmp (word, "false"))
	    flag = FALSE;
	  else {
	    syslog(L_ERROR, MUST_BE_BOOL, LogName, filename, linecount);
	    break;
	  }
	RCadddata(data, &infocount, K_SKIP, T_STRING, word);
	if (peer_params.Label != NULL)
	  peer_params.Skip = flag;
	else
	  if (groupcount > 0 && group_params->Label != NULL)
	    group_params->Skip = flag;
	  else
	    default_params.Skip = flag;
	SET_CONFIG(K_SKIP);
	continue;
      }

      /* noresendid */
      if (!strncmp (word, NORESENDID, sizeof NORESENDID)) {
	DISPOSE(word);
	TEST_CONFIG(K_NORESENDID, bit);
        if (bit) {
	  syslog(L_ERROR, DUPLICATE_KEY, LogName, filename, linecount);
	  break;
	}
	if ((word = RCreaddata (&linecount, F, &toolong)) == NULL) {
	  break;
	}
	if (!strcmp (word, "true"))
	  flag = TRUE;
	else
	  if (!strcmp (word, "false"))
	    flag = FALSE;
	  else {
	    syslog(L_ERROR, MUST_BE_BOOL, LogName, filename, linecount);
	    break;
	  }
	RCadddata(data, &infocount, K_NORESENDID, T_STRING, word);
	if (peer_params.Label != NULL)
	  peer_params.NoResendId = flag;
	else
	  if (groupcount > 0 && group_params->Label != NULL)
	    group_params->NoResendId = flag;
	  else
	    default_params.NoResendId = flag;
	SET_CONFIG(K_NORESENDID);
	continue;
      }

      /* nolist */
      if (!strncmp (word, NOLIST, sizeof NOLIST)) {
	DISPOSE(word);
	TEST_CONFIG(K_NOLIST, bit);
        if (bit) {
	  syslog(L_ERROR, DUPLICATE_KEY, LogName, filename, linecount);
	  break;
	}
	if ((word = RCreaddata (&linecount, F, &toolong)) == NULL) {
	  break;
	}
	if (!strcmp (word, "true"))
	  flag = TRUE;
	else
	  if (!strcmp (word, "false"))
	    flag = FALSE;
	  else {
	    syslog(L_ERROR, MUST_BE_BOOL, LogName, filename, linecount);
	    break;
	  }
	RCadddata(data, &infocount, K_NOLIST, T_STRING, word);
	if (peer_params.Label != NULL)
	  peer_params.Nolist = flag;
	else
	  if (groupcount > 0 && group_params->Label != NULL)
	    group_params->Nolist = flag;
	  else
	    default_params.Nolist = flag;
	SET_CONFIG(K_NOLIST);
	continue;
      }

      /* max-connections */
      if (!strncmp (word, MAX_CONN, sizeof MAX_CONN)) {
	int max;
	DISPOSE(word);
	TEST_CONFIG(K_MAX_CONN, bit);
        if (bit) {
	  syslog(L_ERROR, DUPLICATE_KEY, LogName, filename, linecount);
	  break;
	}
	if ((word = RCreaddata (&linecount, F, &toolong)) == NULL) {
	  break;
	}
	RCadddata(data, &infocount, K_MAX_CONN, T_STRING, word);
	for (p = word; CTYPE(isdigit, *p) && *p != '\0'; p++);
	if (!strcmp (word, "none") || !strcmp (word, "unlimited")) {
	  max = 0;
	} else {
	  if (*p != '\0') {
	    syslog(L_ERROR, MUST_BE_INT, LogName, filename, linecount);
	    break;
	  }
	  max = atoi(word);
	}
	if (peer_params.Label != NULL)
	  peer_params.MaxCnx = max;
	else
	  if (groupcount > 0 && group_params->Label != NULL)
	    group_params->MaxCnx = max;
	  else
	    default_params.MaxCnx = max;
	SET_CONFIG(K_MAX_CONN);
	continue;
      }

      /* hold-time */
      if (!strncmp (word, HOLD_TIME, sizeof HOLD_TIME)) {
	DISPOSE(word);
	TEST_CONFIG(K_HOLD_TIME, bit);
        if (bit) {
	  syslog(L_ERROR, DUPLICATE_KEY, LogName, filename, linecount);
	  break;
	}
	if ((word = RCreaddata (&linecount, F, &toolong)) == NULL) {
	  break;
	}
	RCadddata(data, &infocount, K_HOLD_TIME, T_STRING, word);
	for (p = word; CTYPE(isdigit, *p) && *p != '\0'; p++);
	if (*p != '\0') {
	  syslog(L_ERROR, MUST_BE_INT, LogName, filename, linecount);
	  break;
	}
	if (peer_params.Label != NULL)
	  peer_params.HoldTime = atoi(word);
	else
	  if (groupcount > 0 && group_params->Label != NULL)
	    group_params->HoldTime = atoi(word);
	  else
	    default_params.HoldTime = atoi(word);
	SET_CONFIG(K_HOLD_TIME);
	continue;
      }

      /* hostname */
      if (!strncmp (word, HOSTNAME, sizeof HOSTNAME)) {
	DISPOSE(word);
	TEST_CONFIG(K_HOSTNAME, bit);
        if (bit) {
	  syslog(L_ERROR, DUPLICATE_KEY, LogName, filename, linecount);
	  break;
	}
	if ((word = RCreaddata (&linecount, F, &toolong)) == NULL) {
	  break;
	}
	RCadddata(data, &infocount, K_HOSTNAME, T_STRING, word);
	peer_params.Name = word;
	SET_CONFIG(K_HOSTNAME);
	continue;
      }

      /* password */
      if (!strncmp (word, PASSWORD, sizeof PASSWORD)) {
	DISPOSE(word);
	TEST_CONFIG(K_PASSWORD, bit);
        if (bit) {
	  syslog(L_ERROR, DUPLICATE_KEY, LogName, filename, linecount);
	  break;
	}
	if ((word = RCreaddata (&linecount, F, &toolong)) == NULL) {
	  break;
	}
	RCadddata(data, &infocount, K_PASSWORD, T_STRING, word);
	if (peer_params.Label != NULL)
	  peer_params.Password = word;
	else
	  if (groupcount > 0 && group_params->Label != NULL)
	    group_params->Password = word;
	  else
	    default_params.Password = word;
	SET_CONFIG(K_PASSWORD);
	continue;
      }

      /* identd */
      if (!strncmp (word, IDENTD, sizeof IDENTD)) {
	DISPOSE(word);
	TEST_CONFIG(K_IDENTD, bit);
        if (bit) {
	  syslog(L_ERROR, DUPLICATE_KEY, LogName, filename, linecount);
	  break;
	}
	if ((word = RCreaddata (&linecount, F, &toolong)) == NULL) {
	  break;
	}
	RCadddata(data, &infocount, K_IDENTD, T_STRING, word);
	if (peer_params.Label != NULL)
	  peer_params.Identd = word;
	else
	  if (groupcount > 0 && group_params->Label != NULL)
	    group_params->Identd = word;
	  else
	    default_params.Identd = word;
	SET_CONFIG(K_IDENTD);
	continue;
      }

      /* patterns */
      if (!strncmp (word, PATTERNS, sizeof PATTERNS)) {
	TEST_CONFIG(K_PATTERNS, bit);
        if (bit) {
	  syslog(L_ERROR, DUPLICATE_KEY, LogName, filename, linecount);
	  break;
	}
	DISPOSE(word);
	if ((word = RCreaddata (&linecount, F, &toolong)) == NULL) {
	  break;
	}
	RCadddata(data, &infocount, K_PATTERNS, T_STRING, word);
	if (peer_params.Label != NULL)
	  peer_params.Pattern = word;
	else
	  if (groupcount > 0 && group_params->Label != NULL)
	    group_params->Pattern = word;
	  else
	    default_params.Pattern = word;
	SET_CONFIG(K_PATTERNS);
	continue;
      }

      /* email */
      if (!strncmp (word, EMAIL, sizeof EMAIL)) {
	DISPOSE(word);
	TEST_CONFIG(K_EMAIL, bit);
        if (bit) {
	  syslog(L_ERROR, DUPLICATE_KEY, LogName, filename, linecount);
	  break;
	}
	if ((word = RCreaddata (&linecount, F, &toolong)) == NULL) {
	  break;
	}
	RCadddata(data, &infocount, K_EMAIL, T_STRING, word);
	if (peer_params.Label != NULL)
	  peer_params.Email = word;
	else
	  if (groupcount > 0 && group_params->Label != NULL)
	    group_params->Email = word;
	  else
	    default_params.Email = word;
	SET_CONFIG(K_EMAIL);
	continue;
      }

      /* comment */
      if (!strncmp (word, COMMENT, sizeof COMMENT)) {
	DISPOSE(word);
	TEST_CONFIG(K_COMMENT, bit);
        if (bit) {
	  syslog(L_ERROR, DUPLICATE_KEY, LogName, filename, linecount);
	  break;
	}
	if ((word = RCreaddata (&linecount, F, &toolong)) == NULL) {
	  break;
	}
	RCadddata(data, &infocount, K_COMMENT, T_STRING, word);
	if (peer_params.Label != NULL)
	  peer_params.Comment = word;
	else
	  if (groupcount > 0 && group_params->Label != NULL)
	    group_params->Comment = word;
	  else
	    default_params.Comment = word;
	SET_CONFIG(K_COMMENT);
	continue;
      }

      if (toolong)
	syslog(L_ERROR, "%s line too long at %d: %s",
	     LogName, --linecount, filename);
      else
	syslog(L_ERROR, "%s Unknown value line %d: %s",
	     LogName, linecount, filename);
      DISPOSE(word);
      break;
    }
    DISPOSE(default_params.Email);
    DISPOSE(default_params.Comment);
    RCadddata(data, &infocount, K_END, T_STRING, NULL);

    if (feof (F)) {
      if (peer_params.Label != NULL)
	syslog(L_ERROR, INCOMPLETE_PEER, LogName, peer_params.Label,
	       filename, linecount);
      if (groupcount > 0 && group_params->Label != NULL)
	syslog(L_ERROR, INCOMPLETE_GROUP, LogName, group_params->Label,
	       filename, linecount);
    }
    else
      syslog(L_ERROR, "%s Syntax error in %s at or before line %d", LogName, 
	     filename, linecount);

    if (Fclose(F) == EOF)
	syslog(L_ERROR, "%s cant fclose %s %m", LogName, filename);

    DISPOSE(default_params.Password);
    DISPOSE(default_params.Identd);
}


/*
**  Indent a line with 3 * c blanks.
**  Used by RCwritelist().
*/
static void
RCwritelistindent(FILE *F, int c)
{
    int		i;

    for (i = 0; i < c; i++)
        fprintf(F, "   ");
}

/*
**  Add double quotes around a string, if needed.
**  Used by RCwritelist().
*/
static void
RCwritelistvalue(FILE *F, char *value)
{
    if (*value == '\0' || strchr (value, '\n') ||
	strchr (value, ' ') || strchr (value, '\t'))
	fprintf(F, "\"%s\"", value);
    else
        fprintf(F, "%s", value);
}

/*
**  Write the incoming configuration (memory->disk)
*/
static void UNUSED
RCwritelist(char *filename)
{
    FILE	*F;
    int		i;
    int		inc;
    char	*p;
    char	*q;
    char	*r;

    if ((F = Fopen(filename, "w", TEMPORARYOPEN)) == NULL) {
        syslog(L_FATAL, "%s cant write %s: %m", LogName, filename);
        return;
    }

    /* Write a standard header.. */

    /* Find the filename */
    p = concatpath(innconf->pathetc, _PATH_INNDHOSTS);
    for (r = q = p; *p; p++)
        if (*p == '/')
	   q = p + 1;

    fprintf (F, "##  $Revision$\n");
    fprintf (F, "##  %s - names and addresses that feed us news\n", q);
    DISPOSE(r);
    fprintf (F, "##\n\n");

    /* ... */

    inc = 0;
    for (i = 0; RCpeerlistfile[i].key != K_END; i++) {
        switch (RCpeerlistfile[i].key) {
	  case K_BEGIN_PEER:
	    fputc ('\n', F);
	    RCwritelistindent (F, inc);
	    fprintf(F, "%s %s {\n", PEER, RCpeerlistfile[i].value);
	    inc++;
	    break;
	  case K_BEGIN_GROUP:
	    fputc ('\n', F);
	    RCwritelistindent (F, inc);
	    fprintf(F, "%s %s {\n", GROUP, RCpeerlistfile[i].value);
	    inc++;
	    break;
	  case K_END_PEER:
	  case K_END_GROUP:
	    inc--;
	    RCwritelistindent (F, inc);
	    fprintf(F, "}\n");
	    break;
	  case K_STREAM:
	    RCwritelistindent (F, inc);
	    fprintf(F, "%s\t", STREAMING);
	    RCwritelistvalue (F, RCpeerlistfile[i].value);
	    fputc ('\n', F);
	    break;
	  case K_SKIP:
	    RCwritelistindent (F, inc);
	    fprintf(F, "%s\t", SKIP);
	    RCwritelistvalue (F, RCpeerlistfile[i].value);
	    fputc ('\n', F);
	    break;
	  case K_NORESENDID:
	    RCwritelistindent (F, inc);
	    fprintf(F, "%s\t", NORESENDID);
	    RCwritelistvalue (F, RCpeerlistfile[i].value);
	    fputc ('\n', F);
	    break;
	  case K_NOLIST:
	    RCwritelistindent (F, inc);
	    fprintf(F, "%s\t", NOLIST);
	    RCwritelistvalue (F, RCpeerlistfile[i].value);
	    fputc ('\n', F);
	    break;
	  case K_HOSTNAME:
	    RCwritelistindent (F, inc);
	    fprintf(F, "%s\t", HOSTNAME);
	    RCwritelistvalue (F, RCpeerlistfile[i].value);
	    fputc ('\n', F);
	    break;
	  case K_MAX_CONN:
	    RCwritelistindent (F, inc);
	    fprintf(F, "%s\t", MAX_CONN);
	    RCwritelistvalue (F, RCpeerlistfile[i].value);
	    fputc ('\n', F);
	    break;
	  case K_HOLD_TIME:
	    RCwritelistindent (F, inc);
	    fprintf(F, "%s\t", HOLD_TIME);
	    RCwritelistvalue (F, RCpeerlistfile[i].value);
	    fputc ('\n', F);
	    break;
	  case K_PASSWORD:
	    RCwritelistindent (F, inc);
	    fprintf(F, "%s\t", PASSWORD);
	    RCwritelistvalue (F, RCpeerlistfile[i].value);
	    fputc ('\n', F);
	    break;
	  case K_IDENTD:
	    RCwritelistindent (F, inc);
	    fprintf(F, "%s\t", IDENTD);
	    RCwritelistvalue (F, RCpeerlistfile[i].value);
	    fputc ('\n', F);
	    break;
	  case K_EMAIL:
	    RCwritelistindent (F, inc);
	    fprintf(F, "%s\t", EMAIL);
	    RCwritelistvalue (F, RCpeerlistfile[i].value);
	    fputc ('\n', F);
	    break;
	  case K_PATTERNS:
	    RCwritelistindent (F, inc);
	    fprintf(F, "%s\t", PATTERNS);
	    RCwritelistvalue (F, RCpeerlistfile[i].value);
	    fputc ('\n', F);
	    break;
	  case K_COMMENT:
	    RCwritelistindent (F, inc);
	    fprintf(F, "%s\t", COMMENT);
	    RCwritelistvalue (F, RCpeerlistfile[i].value);
	    fputc ('\n', F);
	    break;
	  default:
	    fprintf(F, "# ***ERROR***\n");
	}
    }
    if (Fclose(F) == EOF)
        syslog(L_ERROR, "%s cant fclose %s %m", LogName, filename);

}

void
RCreadlist(void)
{
    static char	*INNDHOSTS = NULL;

    if (INNDHOSTS == NULL)
	INNDHOSTS = concatpath(innconf->pathetc, _PATH_INNDHOSTS);
    StreamingOff = FALSE;
    RCreadfile(&RCpeerlistfile, &RCpeerlist, &RCnpeerlist, INNDHOSTS);
    /* RCwritelist("/tmp/incoming.conf.new"); */
}

/*
**  Find the name of a remote host we've connected to.
*/
char *
RChostname(const CHANNEL *cp)
{
    static char	buff[SMBUF];
    REMOTEHOST	*rp;
    int		i;

    for (rp = RCpeerlist, i = RCnpeerlist; --i >= 0; rp++)
	if (RCaddressmatch(&cp->Address, &rp->Address))
	    return rp->Name;
    (void)strcpy(buff, sprint_sockaddr((struct sockaddr *)&cp->Address));
    return buff;
}

/*
**  Find the label name of a remote host we've connected to.
*/
char *
RClabelname(CHANNEL *cp) {
    REMOTEHOST	*rp;
    int		i;

    for (rp = RCpeerlist, i = RCnpeerlist; --i >= 0; rp++) {
	if (RCaddressmatch(&cp->Address, &rp->Address))
	    return rp->Label;
    }
    return NULL;
}

/*
**  Is the remote site allowed to post to this group?
*/
int
RCcanpost(CHANNEL *cp, char *group)
{
    REMOTEHOST	        *rp;
    char	        match;
    char	        subvalue;
    char	        **argv;
    char	        *pat;
    int	                i;

    /* Connections from lc.c are from local nnrpd and should always work */
    if (cp->Address.ss_family == 0)
	return 1;

    for (rp = RCpeerlist, i = RCnpeerlist; --i >= 0; rp++) {
	if (!RCaddressmatch(&cp->Address, &rp->Address))
	    continue;
	if (rp->Patterns == NULL)
	    break;
	for (match = 0, argv = rp->Patterns; (pat = *argv++) != NULL; ) {
	    subvalue = (*pat != SUB_NEGATE) && (*pat != SUB_POISON) ?
	      0 : *pat;
	    if (subvalue)
		pat++;
	    if ((match != subvalue) && uwildmat(group, pat)) {
		if (subvalue == SUB_POISON)
		    return -1;
		match = subvalue;
	    }
	}
	return !match;
    }
    return 1;
}


/*
**  Create the channel.
*/
void
RCsetup(int i)
{
#if	defined(SO_REUSEADDR)
    int		on;
#endif	/* defined(SO_REUSEADDR) */
    int		j;
    CHANNEL	*rcchan;

    /* This code is called only when inndstart is not being used */
    if (i < 0) {
#ifdef HAVE_INET6
	syslog(L_FATAL, "%s innd MUST be started with inndstart", LogName);
	exit(1);
#else
	/* Create a socket and name it. */
	struct sockaddr_in	server;

	if ((i = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	    syslog(L_FATAL, "%s cant socket RCreader %m", LogName);
	    exit(1);
	}
#if	defined(SO_REUSEADDR)
	on = 1;
	if (setsockopt(i, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on) < 0)
	    syslog(L_ERROR, "%s cant setsockopt RCreader %m", LogName);
#endif	/* defined(SO_REUSEADDR) */
	memset(&server, 0, sizeof server);
	server.sin_port = htons(innconf->port);
	server.sin_family = AF_INET;
#ifdef HAVE_SOCKADDR_LEN
	server.sin_len = sizeof( struct sockaddr_in );
#endif
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	if (innconf->bindaddress) {
            if (!inet_aton(innconf->bindaddress, &server.sin_addr)) {
                syslog(L_FATAL, "unable to determine bind ip (%s) %m",
                       innconf->bindaddress);
		exit(1);
	    }
	}
	if (bind(i, (struct sockaddr *)&server, sizeof server) < 0) {
	    syslog(L_FATAL, "%s cant bind RCreader %m", LogName);
	    exit(1);
	}
#endif /* HAVE_INET6 */
    }

    /* Set it up to wait for connections. */
    if (listen(i, MAXLISTEN) < 0) {
	j = errno;
	syslog(L_FATAL, "%s cant listen RCreader %m", LogName);
	/* some IPv6 systems already listening on any address will 
	   return EADDRINUSE when trying to listen on the IPv4 socket */
	if (j == EADDRINUSE)
	   return;
	exit(1);
    }

    rcchan = CHANcreate(i, CTremconn, CSwaiting, RCreader, RCwritedone);
    syslog(L_NOTICE, "%s rcsetup %s", LogName, CHANname(rcchan));
    RCHANadd(rcchan);

    for (j = 0 ; j < chanlimit ; j++ ) {
	if (RCchan[j] == NULL) {
	    break;
	}
    }
    if (j < chanlimit) {
	RCchan[j] = rcchan;
    } else if (chanlimit == 0) {
	/* assuming two file descriptors(AF_INET and AF_INET6) */
	chanlimit = 2;
        RCchan = NEW(CHANNEL **, chanlimit);
	for (j = 0 ; j < chanlimit ; j++ ) {
	    RCchan[j] = NULL;
	}
	RCchan[0] = rcchan;
    } else {
	/* extend to double size */
	RENEW(RCchan, CHANNEL **, chanlimit * 2);
	for (j = chanlimit ; j < chanlimit * 2 ; j++ ) {
	    RCchan[j] = NULL;
	}
	RCchan[chanlimit] = rcchan;
	chanlimit *= 2;
    }

    /* Get the list of hosts we handle. */
    RCreadlist();
}


/*
**  Cleanly shut down the channel.
*/
void
RCclose(void)
{
    REMOTEHOST	*rp;
    int		i;

    for (i = 0 ; i < chanlimit ; i++) {
	if (RCchan[i] != NULL) {
	    CHANclose(RCchan[i], CHANname(RCchan[i]));
	} else {
	    break;
	}
    }
    if (chanlimit != 0)
	DISPOSE(RCchan);
    RCchan = NULL;
    chanlimit = 0;
    if (RCpeerlist) {
	for (rp = RCpeerlist, i = RCnpeerlist; --i >= 0; rp++) {
	    DISPOSE(rp->Name);
	    DISPOSE(rp->Label);
	    DISPOSE(rp->Email);
	    DISPOSE(rp->Password);
	    DISPOSE(rp->Identd);
	    DISPOSE(rp->Comment);
	    if (rp->Patterns) {
		DISPOSE(rp->Patterns[0]);
		DISPOSE(rp->Patterns);
	    }
	}
	DISPOSE(RCpeerlist);
	RCpeerlist = NULL;
	RCnpeerlist = 0;
    }

    if (RCpeerlistfile) {
        for (i = 0; RCpeerlistfile[i].key != K_END; i++)
        if (RCpeerlistfile[i].value != NULL)
	   DISPOSE(RCpeerlistfile[i].value);
	DISPOSE(RCpeerlistfile);
	RCpeerlistfile = NULL;
    }
}

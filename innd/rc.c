/*  $Id$
**
**  Routines for the remote connect channel.  Create an Internet stream
**  socket that processes connect to.  If the incoming site is not one of
**  our feeds, then we optionally pass the connection off to the standard
**  NNTP daemon.
*/
#include "config.h"
#include "clibrary.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>

#include "innd.h"

/* Error returns from inet_addr. */
#ifndef INADDR_NONE
# define INADDR_NONE 0xffffffff
#endif

#define COPYADDR(dest, src) \
	    (void)memcpy((POINTER)dest, (POINTER)src, (SIZE_T)sizeof (INADDR))

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
    INADDR	Address;        /* List of ip adresses */
    char	*Password;      /* Optional password */
    BOOL	Streaming;      /* Streaming allowed ? */
    BOOL	Skip;	        /* Skip this peer ? */
    BOOL	NoResendId;	/* Don't send RESEND responses ? */
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
    INADDR	Address;
    time_t	Expires;
} REMOTETABLE;

STATIC char		*RCslaveflag;
STATIC char		*RCnnrpd = NULL;
STATIC char		*RCnntpd = NULL;
STATIC CHANNEL		*RCchan;
STATIC REMOTEHOST_DATA	*RCpeerlistfile;
STATIC REMOTEHOST	*RCpeerlist;
STATIC int		RCnpeerlist;
STATIC char		RCbuff[BIG_BUFFER];

#define PEER	        "peer"
#define GROUP	        "group"
#define HOSTNAME        "hostname:"
#define STREAMING       "streaming:"
#define MAX_CONN        "max-connections:"
#define PASSWORD        "password:"
#define PATTERNS        "patterns:"
#define EMAIL	        "email:"
#define COMMENT	        "comment:"
#define SKIP		"skip:"
#define NORESENDID	"noresendid:"
#define HOLD_TIME	"hold-time:"

typedef enum {K_END, K_BEGIN_PEER, K_BEGIN_GROUP, K_END_PEER, K_END_GROUP,
	      K_STREAM, K_HOSTNAME, K_MAX_CONN, K_PASSWORD, K_EMAIL,
	      K_PATTERNS, K_COMMENT, K_SKIP, K_NORESENDID, K_HOLD_TIME
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
STATIC char		RCterm[] = "\r\n";
STATIC REMOTETABLE	remotetable[REMOTETABLESIZE];
STATIC int		remotecount;
STATIC int		remotefirst;


/*
 * Split text into comma-separated fields.  Return an allocated
 * NULL-terminated array of the fields within the modified argument that
 * the caller is expected to save or free.  We don't use strchr() since
 * the text is expected to be either relatively short or "comma-dense."
 * (This function is different from CommaSplit because spaces are allowed
 * and removed here)
 */

char **RCCommaSplit(char *text)
{
    register int        i;
    register char       *p;
    register char       *q;
    register char       *r;
    register char       **av;
    char                **save;
 
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

int
RCfix_options(int fd, struct sockaddr_in *remote)
{
#if IP_OPTIONS
    unsigned char optbuf[BUFSIZ / 3], *cp;
    char    lbuf[BUFSIZ], *lp;
    ARGTYPE optsize = sizeof(optbuf);
    int     ipproto;
    struct protoent *ip;

    if ((ip = getprotobyname("ip")) != 0)
	ipproto = ip->p_proto;
    else
	ipproto = IPPROTO_IP;

    if (getsockopt(fd, ipproto, IP_OPTIONS, (char *) optbuf, &optsize) == 0
	&& optsize != 0) {
	lp = lbuf;
	for (cp = optbuf; optsize > 0; cp++, optsize--, lp += 3)
	    sprintf(lp, " %2.2x", *cp);
	syslog(LOG_NOTICE,
	       "connect from %s with IP options (ignored):%s",
	       inet_ntoa(remote->sin_addr), lbuf);
	if (setsockopt(fd, ipproto, IP_OPTIONS, (char *) 0, optsize) != 0) {
	    syslog(LOG_ERR, "setsockopt IP_OPTIONS NULL: %m");
	    return -1;
	}
    }
#endif
    return 0;
}


/*
**  See if the site properly entered the password.
*/
BOOL
RCauthorized(register CHANNEL *cp, char *pass)
{
    register REMOTEHOST	*rp;
    register int	i;

    for (rp = RCpeerlist, i = RCnpeerlist; --i >= 0; rp++)
	if (cp->Address.s_addr == rp->Address.s_addr) {
	    if (rp->Password[0] == '\0' || EQ(pass, rp->Password))
		return TRUE;
	    syslog(L_ERROR, "%s (%s) bad_auth", rp->Label,
		   inet_ntoa(cp->Address));
	    return FALSE;
	}

    if (!AnyIncoming)
	/* Not found in our table; this can't happen. */
	syslog(L_ERROR, "%s not_found", inet_ntoa(cp->Address));

    /* Anonymous hosts should not authenticate. */
    return FALSE;
}


/*
**  See if a host is limited or not.
*/
BOOL
RCnolimit(register CHANNEL *cp)
{
    register REMOTEHOST	*rp;
    register int	i;

    for (rp = RCpeerlist, i = RCnpeerlist; --i >= 0; rp++)
	if (cp->Address.s_addr == rp->Address.s_addr)
	    if (rp->MaxCnx)
	        return FALSE;
            else
	        return TRUE;
    /* Not found in our table; this can't happen. */
    return FALSE;
}

/*
**  Return the limit (max number of connections) for a host.
*/
int
RClimit(register CHANNEL *cp)
{
    register REMOTEHOST	*rp;
    register int	i;

    for (rp = RCpeerlist, i = RCnpeerlist; --i >= 0; rp++)
	if (cp->Address.s_addr == rp->Address.s_addr)
	    return (rp->MaxCnx);
    /* Not found in our table; this can't happen. */
    return RemoteLimit;
}


/*
**  Called when input is ready to read.  Shouldn't happen.
*/
STATIC FUNCTYPE
RCrejectreader(CHANNEL *cp)
{
    syslog(L_ERROR, "%s internal RCrejectreader (%s)", LogName,
	   inet_ntoa(cp->Address));
}


/*
**  Write-done function for rejects.
*/
STATIC FUNCTYPE
RCrejectwritedone(register CHANNEL *cp)
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
    STRING	argv[6];
    char	buff[SMBUF];
    int		i;

    if (RCnnrpd == NULL)
	RCnnrpd = COPY(cpcatpath(innconf->pathbin, "nnrpd"));
    if (RCnntpd == NULL)
	RCnntpd = COPY(cpcatpath(innconf->pathbin, "nnrpd"));
#if	defined(SOL_SOCKET) && defined(SO_KEEPALIVE)
    /* Set KEEPALIVE to catch broken socket connections. */
    i = 1;
    if (setsockopt(fd, SOL_SOCKET,  SO_KEEPALIVE, (char *)&i, sizeof i) < 0)
        syslog(L_ERROR, "fd %d cant setsockopt(KEEPALIVE) %m", fd);
#endif /* defined(SOL_SOCKET) && defined(SO_KEEPALIVE) */

    if (SetNonBlocking(fd, FALSE) < 0)
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
	(void)sprintf(buff, "-r%s", NNRPReason);
	argv[i++] = buff;
    }
    if (NNRPTracing)
	argv[i++] = "-t";
    if (RCslaveflag)
	argv[i++] = RCslaveflag;
    argv[i] = NULL;

    /* Call NNRP; don't send back a QUIT message if Spawn fails since  
     * that's a major error we want to find out about quickly. */
    (void)Spawn(innconf->nicekids, fd, fd, fd, argv);
}


/*
**  Read function.  Accept the connection and either create an NNTP channel
**  or spawn an nnrpd to handle it.
*/
STATIC FUNCTYPE
RCreader(CHANNEL *cp)
{
    int			fd;
    struct sockaddr_in	remote;
    ARGTYPE		size;
    register int	i;
    register REMOTEHOST	*rp;
    CHANNEL		*new;
    char		*name;
    long		reject_val = 0;
    char		*reject_message;
    int			count;
    int			found;
    time_t		now;
    CHANNEL		tempchan;
    char		buff[SMBUF];

    if (cp != RCchan) {
	syslog(L_ERROR, "%s internal RCreader wrong channel 0x%x not 0x%x",
	    LogName, cp, RCchan);
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
    if (RCfix_options(fd, &remote) != 0) {
	/* shouldn't happen, but we're bit paranoid at this point */
	if (close(fd) < 0)
	    syslog(L_ERROR, "%s cant close %d %m", LogName, fd);
	return;
    }

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
    tempchan.Address.s_addr = remote.sin_addr.s_addr;
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
	    if (remotetable[i].Address.s_addr == remote.sin_addr.s_addr)
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
	    remotetable[i].Address = remote.sin_addr;
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
	new->Address.s_addr = remote.sin_addr.s_addr;
	new->Rejected = reject_val;
	RCHANremove(new);
	WCHANset(new, reject_message, (int)strlen(reject_message));
	WCHANappend(new, RCterm, STRLEN(RCterm));
	WCHANadd(new);
	return;
    }

    /* See if it's one of our servers. */
    for (name = NULL, rp = RCpeerlist, i = RCnpeerlist; --i >= 0; rp++)
	if (rp->Address.s_addr == remote.sin_addr.s_addr) {
	    name = rp->Name;
	    break;
	}

    /* If not a server, and not allowing anyone, hand him off unless
       not spawning nnrpd in which case we return an error. */
    if ((i >= 0) && !rp->Skip) {
	if ((new = NCcreate(fd, rp->Password[0] != '\0', FALSE)) != NULL) {
            new->Streaming = rp->Streaming;
            new->Skip = rp->Skip;
            new->NoResendId = rp->NoResendId;
            new->MaxCnx = rp->MaxCnx;
            new->HoldTime = rp->HoldTime;
	    new->Address.s_addr = remote.sin_addr.s_addr;
	    if (new->MaxCnx > 0 && new->HoldTime == 0) {
		CHANsetActiveCnx(new);
		if((new->ActiveCnx > new->MaxCnx) && (new->fd > 0)) {
		    sprintf(buff, "You are limited to %d connection%s", new->MaxCnx, (new->MaxCnx != 1) ? "s" : "");
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
        new->Address.s_addr = remote.sin_addr.s_addr;
        new->Rejected = reject_val;
        RCHANremove(new);
        WCHANset(new, reject_message, (int)strlen(reject_message));
        WCHANappend(new, RCterm, STRLEN(RCterm));
        WCHANadd(new);
        return;
    }

    if (new != NULL) {
	new->Address.s_addr = remote.sin_addr.s_addr;
	syslog(L_NOTICE, "%s connected %d streaming %s",
           name ? name : inet_ntoa(new->Address), new->fd,
           (!StreamingOff && new->Streaming) ? "allowed" : "not allowed");
    }
}


/*
**  Write-done function.  Shouldn't happen.
*/
STATIC FUNCTYPE
RCwritedone()
{
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
char *RCreaddata (int *num, FILE *F, BOOL *toolong)
{
  register char *p;
  register char *s;
  register char *t;
  char          *word;
  register BOOL flag;

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
void RCadddata(REMOTEHOST_DATA **d, int *count, int Key, int Type, char* Value)
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
STATIC void
RCreadfile (REMOTEHOST_DATA **data, REMOTEHOST **list, int *count, 
	    char *filename)
{
    static char			NOPASS[] = "";
    static char			NOEMAIL[] = "";
    static char			NOCOMMENT[] = "";
    register FILE		*F;
    register char 		*p;
    register char 		**q;
    register char 		**r;
    struct hostent		*hp;
    register int		i;
    register int		j;
    int				linecount;
    int				infocount;
    register int		groupcount;
    register int		maxgroup;
    register REMOTEHOST_DATA 	*dt;
    register REMOTEHOST		*rp;
    register char		*word;
    register REMOTEHOST		*groups;
    register REMOTEHOST		*group_params = NULL;
    register REMOTEHOST		peer_params;
    register REMOTEHOST		default_params;
    BOOL			flag, bit, toolong;

 
    *RCbuff = '\0';
    if (*list) {
	for (rp = *list, i = *count; --i >= 0; rp++) {
	    DISPOSE(rp->Name);
	    DISPOSE(rp->Label);
	    DISPOSE(rp->Email);
	    DISPOSE(rp->Comment);
	    DISPOSE(rp->Password);
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
    rp->Address.s_addr = inet_addr(LOOPBACK_HOST);
    rp->Name = COPY("localhost");
    rp->Label = COPY("localhost");
    rp->Email = COPY(NOEMAIL);
    rp->Comment = COPY(NOCOMMENT);
    rp->Password = COPY(NOPASS);
    rp->Patterns = NULL;
    rp->MaxCnx = 0;
    rp->Streaming = TRUE;
    rp->Skip = FALSE;
    rp->NoResendId = FALSE;
    rp->HoldTime = 0;
    rp++;
    (*count)++;
#endif	/* !defined(HAVE_UNIX_DOMAIN_SOCKETS) */

    linecount = 0;
    infocount = 0;
    groupcount = 0; /* no group defined yet */
    peer_params.Label = NULL;
    default_params.Streaming = TRUE;
    default_params.Skip = FALSE;
    default_params.NoResendId = FALSE;
    default_params.MaxCnx = 0;
    default_params.HoldTime = 0;
    default_params.Password = COPY(NOPASS);
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
	group_params->Email = groupcount > 1 ?
	  groups[groupcount - 2].Email : default_params.Email;
	group_params->Comment = groupcount > 1 ?
	  groups[groupcount - 2].Comment : default_params.Comment;
	group_params->Pattern = groupcount > 1 ?
	  groups[groupcount - 2].Pattern : default_params.Pattern;
	group_params->Password = groupcount > 1 ?
	  groups[groupcount - 2].Password : default_params.Password;
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
	peer_params.Email = groupcount > 0 ?
	  group_params->Email : default_params.Email;
	peer_params.Comment = groupcount > 0 ?
	  group_params->Comment : default_params.Comment;
	peer_params.Pattern = groupcount > 0 ?
	  group_params->Pattern : default_params.Pattern;
	peer_params.Password = groupcount > 0 ?
	  group_params->Password : default_params.Password;
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
	    (*count)++;

	    /* Grow the array */
	    j = rp - *list;
	    RENEW (*list, REMOTEHOST, *count);
	    rp = *list + j;

	    /* Was host specified as a dotted quad ? */
	    if ((rp->Address.s_addr = inet_addr(*q)) != INADDR_NONE) {
	      /* syslog(LOG_NOTICE, "think it's a dotquad: %s", *q); */
	      rp->Name = COPY (*q);
	      rp->Label = COPY (peer_params.Label);
	      rp->Password = COPY(peer_params.Password);
	      rp->Skip = peer_params.Skip;
	      rp->Streaming = peer_params.Streaming;
	      rp->NoResendId = peer_params.NoResendId;
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
	      char **r;
	      int    t = 0;
	      /* Strange DNS ? try this.. */
	      for (r = hp->h_aliases; *r != 0; r++) {
		if (inet_addr(*r) == INADDR_NONE) /* IP address ? */
		  continue;
		(*count)++;
		/* Grow the array */
		j = rp - *list;
		RENEW (*list, REMOTEHOST, *count);
		rp = *list + j;

		rp->Address.s_addr = inet_addr(*r);
		rp->Name = COPY (*q);
		rp->Label = COPY (peer_params.Label);
		rp->Email = COPY(peer_params.Email);
		rp->Comment = COPY(peer_params.Comment);
		rp->Streaming = peer_params.Streaming;
		rp->Skip = peer_params.Skip;
		rp->NoResendId = peer_params.NoResendId;
		rp->Password = COPY(peer_params.Password);
		rp->Patterns = peer_params.Pattern != NULL ?
		  RCCommaSplit(COPY(peer_params.Pattern)) : NULL;
		rp->MaxCnx = peer_params.MaxCnx;
		rp->HoldTime = peer_params.HoldTime;
		rp++;
		t++;
	      }
	      if (t == 0) {
		/* Just one, no need to grow. */
		COPYADDR(&rp->Address, hp->h_addr_list[0]);
		rp->Name = COPY (*q);
		rp->Label = COPY (peer_params.Label);
		rp->Email = COPY(peer_params.Email);
		rp->Comment = COPY(peer_params.Comment);
		rp->Streaming = peer_params.Streaming;
		rp->Skip = peer_params.Skip;
		rp->NoResendId = peer_params.NoResendId;
		rp->Password = COPY(peer_params.Password);
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
	      COPYADDR(&rp->Address, hp->h_addr_list[i]);
	      rp->Name = COPY (*q);
	      rp->Label = COPY (peer_params.Label);
	      rp->Email = COPY(peer_params.Email);
	      rp->Comment = COPY(peer_params.Comment);
	      rp->Streaming = peer_params.Streaming;
	      rp->Skip = peer_params.Skip;
	      rp->NoResendId = peer_params.NoResendId;
	      rp->Password = COPY(peer_params.Password);
	      rp->Patterns = peer_params.Pattern != NULL ?
		RCCommaSplit(COPY(peer_params.Pattern)) : NULL;
	      rp->MaxCnx = peer_params.MaxCnx;
	      rp->HoldTime = peer_params.HoldTime;
	      rp++;
	    }
#else
	    /* Old-style, single address, just add it. */
	    COPYADDR(&rp->Address, hp->h_addr);
	    rp->Name = COPY(*q);
	    rp->Label = COPY (peer_params.Label);
	    rp->Email = COPY(peer_params.Email);
	    rp->Comment = COPY(peer_params.Comment);
	    rp->Streaming = peer_params.Streaming;
	    rp->Skip = peer_params.Skip;
	    rp->NoResendId = peer_params.NoResendId;
	    rp->Password = COPY(peer_params.Password);
	    rp->Patterns = peer_params.Pattern != NULL ?
	      RCCommaSplit(COPY(peer_params.Pattern)) : NULL;
	    rp->MaxCnx = peer_params.MaxCnx;
	    rp->HoldTime = peer_params.HoldTime;
	    rp++;
#endif	    /* defined(h_addr) */
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
	for (p = word; isdigit(*p) && *p != '\0'; p++);
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
	for (p = word; isdigit(*p) && *p != '\0'; p++);
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
}


/*
**  Indent a line with 3 * c blanks.
**  Used by RCwritelist().
*/
void
RCwritelistindent(FILE *F, int c)
{
    register int		i;

    for (i = 0; i < c; i++)
        fprintf(F, "   ");
}

/*
**  Add double quotes around a string, if needed.
**  Used by RCwritelist().
*/
void
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
void
RCwritelist(char *filename)
{
    register FILE               *F;
    register int		i;
    register int		inc;
    register char		*p;
    register char		*q;
    register char		*r;

    if ((F = Fopen(filename, "w", TEMPORARYOPEN)) == NULL) {
        syslog(L_FATAL, "%s cant write %s: %m", LogName, filename);
        return;
    }

    /* Write a standard header.. */

    /* Find the filename */
    p = COPY(cpcatpath(innconf->pathetc, _PATH_INNDHOSTS));
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
RCreadlist()
{
    static char	*INNDHOSTS = NULL;

    if (INNDHOSTS == NULL)
	INNDHOSTS = COPY(cpcatpath(innconf->pathetc, _PATH_INNDHOSTS));
    StreamingOff = FALSE;
    RCreadfile(&RCpeerlistfile, &RCpeerlist, &RCnpeerlist, INNDHOSTS);
    /* RCwritelist("/tmp/incoming.conf.new"); */
}

/*
**  Find the name of a remote host we've connected to.
*/
char *
RChostname(cp)
    register CHANNEL	*cp;
{
    static char		buff[SMBUF];
    register REMOTEHOST	*rp;
    register int	i;

    for (rp = RCpeerlist, i = RCnpeerlist; --i >= 0; rp++)
	if (cp->Address.s_addr == rp->Address.s_addr)
	    return rp->Name;
    (void)strcpy(buff, inet_ntoa(cp->Address));
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
	if (cp->Address.s_addr == rp->Address.s_addr)
	    return rp->Label;
    }
    return NULL;
}

/*
**  Is the remote site allowed to post to this group?
*/
int RCcanpost(CHANNEL *cp, char *group)
{
    REMOTEHOST	        *rp;
    char	        match;
    char	        subvalue;
    char	        **argv;
    char	        *pat;
    int	                i;

    for (rp = RCpeerlist, i = RCnpeerlist; --i >= 0; rp++) {
	if (cp->Address.s_addr != rp->Address.s_addr)
	    continue;
	if (rp->Patterns == NULL)
	    break;
	for (match = 0, argv = rp->Patterns; (pat = *argv++) != NULL; ) {
	    subvalue = (*pat != SUB_NEGATE) && (*pat != SUB_POISON) ?
	      0 : *pat;
	    if (subvalue)
		pat++;
	    if ((match != subvalue) && wildmat(group, pat)) {
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
RCsetup(i)
    register int	i;
{
    struct sockaddr_in	server;
#if	defined(SO_REUSEADDR)
    int			on;
#endif	/* defined(SO_REUSEADDR) */

    if (i < 0) {
	/* Create a socket and name it. */
	if ((i = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	    syslog(L_FATAL, "%s cant socket RCreader %m", LogName);
	    exit(1);
	}
#if	defined(SO_REUSEADDR)
	on = 1;
	if (setsockopt(i, SOL_SOCKET, SO_REUSEADDR,
		(caddr_t)&on, sizeof on) < 0)
	    syslog(L_ERROR, "%s cant setsockopt RCreader %m", LogName);
#endif	/* defined(SO_REUSEADDR) */
	(void)memset((POINTER)&server, 0, sizeof server);
	server.sin_port = htons(innconf->port);
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	if (innconf->bindaddress) {
	    server.sin_addr.s_addr = inet_addr(innconf->bindaddress);
	    if (server.sin_addr.s_addr == INADDR_NONE) {
		syslog(L_FATAL, "unable to determine bind ip (%s) %m",
					innconf->bindaddress);
		exit(1);
	    }
	}
	if (bind(i, (struct sockaddr *)&server, sizeof server) < 0) {
	    syslog(L_FATAL, "%s cant bind RCreader %m", LogName);
	    exit(1);
	}
    }
    

    /* Set it up to wait for connections. */
    if (listen(i, MAXLISTEN) < 0) {
	syslog(L_FATAL, "%s cant listen RCreader %m", LogName);
	exit(1);
    }
    RCchan = CHANcreate(i, CTremconn, CSwaiting, RCreader, RCwritedone);
    syslog(L_NOTICE, "%s rcsetup %s", LogName, CHANname(RCchan));
    RCHANadd(RCchan);

    /* Get the list of hosts we handle. */
    RCreadlist();
}


/*
**  Cleanly shut down the channel.
*/
void
RCclose()
{
    register REMOTEHOST	*rp;
    register int	i;

    CHANclose(RCchan, CHANname(RCchan));
    RCchan = NULL;
    if (RCpeerlist) {
	for (rp = RCpeerlist, i = RCnpeerlist; --i >= 0; rp++) {
	    DISPOSE(rp->Name);
	    DISPOSE(rp->Label);
	    DISPOSE(rp->Email);
	    DISPOSE(rp->Password);
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

/*  $Revision$
**
**  Routines for the remote connect channel.  Create an Internet stream socket
**  that processes connect to.  If the incoming site is not one of our feeds,
**  then we pass the connection off to the standard nntp daemon.
*/
#include <stdio.h>
#include <sys/types.h>
#include "configdata.h"
#include "clibrary.h"
#include "innd.h"
#include <netdb.h>


#if	!defined(NETSWAP)
#if	!defined(htons)
extern unsigned short	htons();
#endif	/* !defined(htons) */
#if	!defined(htonl)
#if 0   
extern unsigned long	htonl(); /* nobody should really need this anymore */
#endif
#endif	/* !defined(htonl) */
#endif	/* !defined(NETSWAP) */

#define COPYADDR(dest, src) \
	    (void)memcpy((POINTER)dest, (POINTER)src, (SIZE_T)sizeof (INADDR))

/*
**  A remote host has an address and a password.
*/
typedef struct _REMOTEHOST {
    char	*Name;
    INADDR	Address;
    char	*Password;
    BOOL	Streaming ;
    char	**Patterns;
} REMOTEHOST;

typedef struct _REMOTETABLE {
    INADDR	Address;
    time_t	Expires;
} REMOTETABLE;

STATIC INADDR		*RCmaster;
STATIC int		RCnmaster;
STATIC char		*RCslaveflag;
STATIC char		RCnnrpd[] = _PATH_NNRPD;
STATIC char		RCnntpd[] = _PATH_NNTPD;
STATIC CHANNEL		*RCchan;
STATIC REMOTEHOST	*RCpeerlist;
STATIC int		RCnpeerlist;
STATIC REMOTEHOST	*RCnolimitlist;
STATIC int		RCnnolimitlist;

/*
** Stuff needed for limiting incoming connects.
*/
STATIC char		RCterm[] = "\r\n";
STATIC REMOTETABLE	remotetable[REMOTETABLESIZE];
STATIC int		remotecount;
STATIC int		remotefirst;


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
RCfix_options(fd, remote)
int fd;
struct sockaddr_in	*remote;
{
#if IP_OPTIONS
    unsigned char optbuf[BUFSIZ / 3], *cp;
    char    lbuf[BUFSIZ], *lp;
    int     optsize = sizeof(optbuf), ipproto;
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
RCauthorized(cp, pass)
    register CHANNEL	*cp;
    char		*pass;
{
    register REMOTEHOST	*rp;
    register int	i;

    for (rp = RCpeerlist, i = RCnpeerlist; --i >= 0; rp++)
	/* SUPPRESS 112 *//* Retrieving long where char is stored */
	if (cp->Address.s_addr == rp->Address.s_addr) {
	    if (rp->Password[0] == '\0' || EQ(pass, rp->Password))
		return TRUE;
	    syslog(L_ERROR, "%s bad_auth", inet_ntoa(cp->Address));
	    return FALSE;
	}

    if (!AnyIncoming)
	/* Not found in our table; this can't happen. */
	syslog(L_ERROR, "%s not_found", inet_ntoa(cp->Address));

    /* Anonymous hosts should not authenticate. */
    return FALSE;
}


/*
**  See if a host is in the "nolimit" file.
*/
BOOL
RCnolimit(cp)
    register CHANNEL	*cp;
{
    register REMOTEHOST	*rp;
    register int	i;

    for (rp = RCnolimitlist, i = RCnnolimitlist; --i >= 0; rp++)
	/* SUPPRESS 112 *//* Retrieving long where char is stored */
	if (cp->Address.s_addr == rp->Address.s_addr)
	    return TRUE;
    return FALSE;
}


/*
**  Is this an address of the master?
*/
BOOL
RCismaster(addr)
    INADDR		addr;
{
    register INADDR	*ip;
    register int	i;

    if (AmSlave)
	for (i = RCnmaster, ip = RCmaster; --i >= 0; ip++)
	    /* SUPPRESS 112 *//* Retrieving long where char is stored */
	    if (addr.s_addr == ip->s_addr)
		return TRUE;
    return FALSE;
}


/*
**  Called when input is ready to read.  Shouldn't happen.
*/
/* ARGSUSED0 */
STATIC FUNCTYPE
RCrejectreader(cp)
    CHANNEL	*cp;
{
    syslog(L_ERROR, "%s internal RCrejectreader", LogName);
}


/*
**  Write-done function for rejects.
*/
STATIC FUNCTYPE
RCrejectwritedone(cp)
    register CHANNEL	*cp;
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
RChandoff(fd, h)
    int		fd;
    HANDOFF	h;
{
    STRING	argv[6];
    char	buff[SMBUF];
    int		i;

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
    (void)Spawn(INND_NICE_VALUE, fd, fd, fd, argv);
}


/*
**  Read function.  Accept the connection and either create an NNTP channel
**  or spawn an nnrpd to handle it.
*/
STATIC FUNCTYPE
RCreader(cp)
    CHANNEL		*cp;
{
    int			fd;
    struct sockaddr_in	remote;
    int			size;
    register int	i;
    register REMOTEHOST	*rp;
    CHANNEL		*new;
    char		*name;
    long		reject_val;
    char		*reject_message;
    int			count;
    int			found;
    time_t		now;
    CHANNEL		tempchan;

    if (cp != RCchan) {
	syslog(L_ERROR, "%s internal RCreader wrong channel 0x%x not 0x%x",
	    LogName, cp, RCchan);
	return;
    }

    /* Get the connection. */
    size = sizeof remote;
    if ((fd = accept(cp->fd, (struct sockaddr *)&remote, &size)) < 0) {
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
    ** L = per host incoming connects per X seconds allowed
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
    ** the incoming connection's host address is equal to the ``L''
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
	/* SUPPRESS 112 *//* Retrieving long where char is stored */
	if (rp->Address.s_addr == remote.sin_addr.s_addr) {
	    name = rp->Name;
	    break;
	}

    /* If not a server, and not allowing anyone, hand him off. */
    if (i >= 0) {
	new = NCcreate(fd, rp->Password[0] != '\0', FALSE);
        new->Streaming = rp->Streaming ;
    } else if (AnyIncoming) {
	new = NCcreate(fd, FALSE, FALSE);
    } else {
	RChandoff(fd, HOnntpd);
	if (close(fd) < 0)
	    syslog(L_ERROR, "%s cant close %d %m", LogName, fd);
	return;
    }

    /* SUPPRESS 112 *//* Retrieving long where char is stored */
    new->Address.s_addr = remote.sin_addr.s_addr;
    syslog(L_NOTICE, "%s connected %d streaming %s",
           name ? name : inet_ntoa(new->Address), new->fd,
           (!StreamingOff || new->Streaming) ? "allowed" : "not allowed");
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
**  Read in the file listing the hosts we take news from, and fill in the
**  global list of their Internet addresses.  On modern systems a host can
**  have multiple addresses, so we take care to add all of them to the list.
**  We can distinguish between the two because h_addr is a #define for the
**  first element of the address list in modern systems, while it's a field
**  name in old ones.
*/
STATIC void
RCreadfile(list, count, filename)
    REMOTEHOST		**list;
    int			*count;
    char		*filename;
{
    static char		NOPASS[] = "";
    char		buff[SMBUF];
    register FILE	*F;
    register char	*p;
    struct hostent	*hp;
    register int	i;
    register REMOTEHOST	*rp;
    register int	j;
    int			k ;
    char		*pass;
    char		*pats;
    int			errors;

    /* Free anything that might have been there. */
    if (*list) {
	for (rp = *list, i = *count; --i >= 0; rp++) {
	    DISPOSE(rp->Name);
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

    /* Open the server file, count the lines. */
    if ((F = fopen(filename, "r")) == NULL) {
	syslog(L_FATAL, "%s cant read %s %m", LogName, filename);
	exit(1);
    }
    for (i = 1; fgets(buff, sizeof buff, F) != NULL; )
	if (buff[0] != COMMENT_CHAR && buff[0] != '\n')
	    i++;
    *count = i;
    rp = *list = NEW(REMOTEHOST, *count);
#if	!defined(DO_HAVE_UNIX_DOMAIN)
    rp->Address.s_addr = inet_addr(LOOPBACK_HOST);
    rp->Name = COPY("localhost");
    rp->Password = COPY(NOPASS);
    rp->Patterns = NULL;
    rp++;
#endif	/* !defined(DO_HAVE_UNIX_DOMAIN) */

    /* Now read the file to add all the hosts. */
    (void)fseek(F, (OFFSET_T)0, SEEK_SET);
    for (errors = 0; fgets(buff, sizeof buff, F) != NULL; ) {
	/* Ignore blank and comment lines. */
	if ((p = strchr(buff, '\n')) != NULL)
	    *p = '\0';
	if ((p = strchr(buff, COMMENT_CHAR)) != NULL)
	    *p = '\0';
	if (buff[0] == '\0')
	    continue;
	if ((pass = strchr(buff, ':')) != NULL) {
	    *pass++ = '\0';
	    if ((pats = strchr(pass, ':')) != NULL)
		*pats++ = '\0';
	    else
		pats = NULL;
	}
	else {
	    pass = NOPASS;
	    pats = NULL;
	}

        /* Check if the host name ends with '/s' which means that streaming is
           specifically permitted (as opposed to defaulted). The default
           for the global StreamingOff is FALSE, meaning any host can use
           streaming commands. If any entry is hosts.nntp has a suffix of
           '/s' then StreamingOff is set to TRUE, and then only those
           hosts.nntp entries with '/s' can use streaming commands. */
        rp->Streaming = FALSE;
        if ((k = strlen(buff)) > 2) {
            if (buff[k - 1] == 's' && buff[k - 2] == '/') {
                buff[k - 2] = '\0';
                rp->Streaming = TRUE;
                StreamingOff = TRUE ;
            }
        }

	/* Was host specified as as dotted quad? */
	if ((rp->Address.s_addr = inet_addr(buff)) != (unsigned int) -1) {
 	  syslog(LOG_NOTICE, "think it's a dotquad: %s",buff);
	    rp->Name = COPY(buff);
	    rp->Password = COPY(pass);
	    rp->Patterns = (pats && *pats) ? CommaSplit(COPY(pats)) : NULL;
	    rp++;
	    continue;
	}

	/* Host specified as a text name? */
	if ((hp = gethostbyname(buff)) == NULL) {
	    syslog(L_ERROR, "%s cant gethostbyname %s %m", LogName, buff);
	    errors++;
	    continue;
	}

#if	defined(h_addr)
	/* Count the addresses and see if we have to grow the list. */
	for (i = 0; hp->h_addr_list[i]; i++)
	    continue;
	if (i == 0) {
	    syslog(L_ERROR, "%s no_address %s %m", LogName, buff);
	    errors++;
	    continue;
	}
	if (i == 1) {
	    /* Just one, no need to grow. */
	    COPYADDR(&rp->Address, hp->h_addr_list[0]);
	    rp->Name = COPY(hp->h_name);
	    rp->Password = COPY(pass);
	    rp->Patterns = (pats && *pats) ? CommaSplit(COPY(pats)) : NULL;
	    rp++;
	    continue;
	}

	/* Note the relative position, grow the array, and restore it. */
	j = rp - *list;
	*count += i - 1;
	RENEW(*list, REMOTEHOST, *count);
	rp = *list + j;

	/* Add all the hosts. */
	for (i = 0; hp->h_addr_list[i]; i++) {
	    COPYADDR(&rp->Address, hp->h_addr_list[i]);
	    rp->Name = COPY(hp->h_name);
	    rp->Password = COPY(pass);
	    rp->Patterns = (pats && *pats) ? CommaSplit(COPY(pats)) : NULL;
            rp->Streaming = (*list + j)->Streaming ;
	    rp++;
	}
#else
	/* Old-style, single address, just add it. */
	COPYADDR(&rp->Address, hp->h_addr);
	rp->Name = COPY(hp->h_name);
	rp->Password = COPY(pass);
	rp->Patterns = (pats && *pats) ? CommaSplit(COPY(pats)) : NULL;
	rp++;
#endif	/* defined(h_addr) */
    }
    *count = rp - *list;

    if (fclose(F) == EOF)
	syslog(L_ERROR, "%s cant fclose %s %m", LogName, filename);

    if (errors)
	syslog(L_ERROR, "%s bad_hosts %d in %s", LogName, errors, filename);
}


void
RCreadlist()
{
    static char	INNDHOSTS[] = _PATH_INNDHOSTS;
    char	name[sizeof _PATH_INNDHOSTS + sizeof ".nolimit"];
    struct stat	Sb;

    StreamingOff = FALSE ;
    RCreadfile(&RCpeerlist, &RCnpeerlist, INNDHOSTS);
    FileGlue(name, INNDHOSTS, '.', "nolimit");
    if (stat(name, &Sb) >= 0)
	RCreadfile(&RCnolimitlist, &RCnnolimitlist, name);
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
	/* SUPPRESS 112 *//* Retrieving long where char is stored */
	if (cp->Address.s_addr == rp->Address.s_addr)
	    return rp->Name;
    (void)strcpy(buff, inet_ntoa(cp->Address));
    return buff;
}


/*
**  Is the remote site allowed to post to this group?
*/
BOOL RCcanpost(CHANNEL *cp, char *group)
{
    REMOTEHOST	        *rp;
    BOOL	        match;
    BOOL	        subvalue;
    char	        **argv;
    char	        *pat;
    int	                i;

    for (rp = RCpeerlist, i = RCnpeerlist; --i >= 0; rp++) {
	/* SUPPRESS 112 *//* Retrieving long where char is stored */
	if (cp->Address.s_addr != rp->Address.s_addr)
	    continue;
	if (rp->Patterns == NULL)
	    break;
	for (match = TRUE, argv = rp->Patterns; (pat = *argv++) != NULL; ) {
	    subvalue = *pat != SUB_NEGATE;
	    if (!subvalue)
		pat++;
	    if ((match != subvalue) && wildmat(group, pat))
		match = subvalue;
	}
	return match;
    }
    return TRUE;
}


/*
**  Create the channel.
*/
void
RCsetup(i, master)
    register int	i;
    char		*master;
{
    struct sockaddr_in	server;
    struct hostent	*hp;
    INADDR		a;
    char		buff[SMBUF];
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
	server.sin_port = htons(NNTP_PORT);
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = htonl(INADDR_ANY);
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

    /* If we have a master, get all his addresses. */
    AmSlave = master != NULL;
    if (AmSlave) {
	/* Dotted quad? */
	if ((a.s_addr = inet_addr(master)) != (unsigned int) -1) {
	    RCnmaster = 1;
	    RCmaster = NEW(INADDR, 1);
	    COPYADDR(&RCmaster[0], &a);
	}
	else {
	    /* Must be a text name. */
	    if ((hp = gethostbyname(master)) == NULL) {
		syslog(L_FATAL, "%s cant gethostbyname %s %m", LogName, master);
		exit(1);
	    }
#if	defined(h_addr)
	    /* Count the addresses. */
	    for (i = 0; hp->h_addr_list[i]; i++)
		continue;
	    if (i == 0) {
		syslog(L_FATAL, "%s no_address %s %m", LogName, master);
		exit(1);
	    }
	    RCnmaster = i;
	    RCmaster = NEW(INADDR, RCnmaster);
	    for (i = 0; hp->h_addr_list[i]; i++)
		COPYADDR(&RCmaster[i], hp->h_addr_list[i]);
#else
	    RCnmaster = 1;
	    RCmaster = NEW(INADDR, 1)
	    COPYADDR(&RCmaster[0], &a);
#endif	/* defined(h_addr) */
	}

	/* Set flag for nnrp. */
	(void)sprintf(buff, "-S%s", master);
	RCslaveflag = COPY(buff);
    }
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
	    DISPOSE(rp->Password);
	    if (rp->Patterns)
		DISPOSE(rp->Patterns);
	}
	DISPOSE(RCpeerlist);
	RCpeerlist = NULL;
	RCnpeerlist = 0;
    }

    if (RCmaster) {
	DISPOSE(RCmaster);
	RCmaster = NULL;
	RCnmaster = 0;
    }
}

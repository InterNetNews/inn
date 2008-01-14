/*  $Id$
**
**  Send an article (prepared by someone on the local site) to the
**  master news server.
*/

#include "config.h"
#include "clibrary.h"
#include "portable/time.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <sys/stat.h>

#include "inn/innconf.h"
#include "inn/libinn.h"
#include "inn/messages.h"
#include "inn/newsuser.h"
#include "inn/paths.h"
#include "nntp.h"

/* Signature handling.  The separator will be appended before the signature,
   and at most SIG_MAXLINES will be appended. */
#define SIG_MAXLINES		4
#define SIG_SEPARATOR		"-- \n"

#define FLUSH_ERROR(F)		(fflush((F)) == EOF || ferror((F)))
#define LPAREN			'('	/* For vi :-) */
#define HEADER_DELTA		20
#define GECOSTERM(c)		\
	    ((c) == ',' || (c) == ';' || (c) == ':' || (c) == LPAREN)
#define HEADER_STRLEN		998

typedef enum _HEADERTYPE {
    HTobs,
    HTreq,
    HTstd
} HEADERTYPE;

typedef struct _HEADER {
    const char *Name;
    bool	CanSet;
    HEADERTYPE	Type;
    int		Size;
    char	*Value;
} HEADER;

static bool	Dump;
static bool	Revoked;
static bool	Spooling;
static char	**OtherHeaders;
static char	SIGSEP[] = SIG_SEPARATOR;
static FILE	*FromServer;
static FILE	*ToServer;
static int	OtherCount;
static int	OtherSize;
static const char *Exclusions = "";
static const char * const BadDistribs[] = {
    BAD_DISTRIBS
};

static HEADER	Table[] = {
    /* 	Name			Canset	Type	*/
    {	"Path",			true,	HTstd,  0, NULL },
#define _path		 0
    {	"From",			true,	HTstd,  0, NULL },
#define _from		 1
    {	"Newsgroups",		true,	HTreq,  0, NULL },
#define _newsgroups	 2
    {	"Subject",		true,	HTreq,  0, NULL },
#define _subject	 3
    {	"Control",		true,	HTstd,  0, NULL },
#define _control	 4
    {	"Supersedes",		true,	HTstd,  0, NULL },
#define _supersedes	 5
    {	"Followup-To",		true,	HTstd,  0, NULL },
#define _followupto	 6
    {	"Date",			true,	HTstd,  0, NULL },
#define _date		 7
    {	"Organization",		true,	HTstd,  0, NULL },
#define _organization	 8
    {	"Lines",		true,	HTstd,  0, NULL },
#define _lines		 9
    {	"Sender",		true,	HTstd,  0, NULL },
#define _sender		10
    {	"Approved",		true,	HTstd,  0, NULL },
#define _approved	11
    {	"Distribution",		true,	HTstd,  0, NULL },
#define _distribution	12
    {	"Expires",		true,	HTstd,  0, NULL },
#define _expires	13
    {	"Message-ID",		true,	HTstd,  0, NULL },
#define _messageid	14
    {	"References",		true,	HTstd,  0, NULL },
#define _references	15
    {	"Reply-To",		true,	HTstd,  0, NULL },
#define _replyto	16
    {	"Also-Control",		true,	HTstd,  0, NULL },
#define _alsocontrol	17
    {	"Xref",			false,	HTstd,  0, NULL },
    {	"Summary",		true,	HTstd,  0, NULL },
    {	"Keywords",		true,	HTstd,  0, NULL },
    {	"Date-Received",	false,	HTobs,  0, NULL },
    {	"Received",		false,	HTobs,  0, NULL },
    {	"Posted",		false,	HTobs,  0, NULL },
    {	"Posting-Version",	false,	HTobs,  0, NULL },
    {	"Relay-Version",	false,	HTobs,  0, NULL },
};

#define HDR(_x)	(Table[(_x)].Value)



/*
**  Send the server a quit message, wait for a reply.
*/
static void
QuitServer(int x)
{
    char	buff[HEADER_STRLEN];
    char	*p;

    if (Spooling)
	exit(x);
    if (x)
        warn("article not posted");
    fprintf(ToServer, "quit\r\n");
    if (FLUSH_ERROR(ToServer))
        sysdie("cannot send quit to server");
    if (fgets(buff, sizeof buff, FromServer) == NULL)
        sysdie("warning: server did not reply to quit");
    if ((p = strchr(buff, '\r')) != NULL)
	*p = '\0';
    if ((p = strchr(buff, '\n')) != NULL)
	*p = '\0';
    if (atoi(buff) != NNTP_OK_QUIT)
        die("server did not reply to quit properly: %s", buff);
    fclose(FromServer);
    fclose(ToServer);
    exit(x);
}


/*
**  Failure handler, called by die.  Calls QuitServer to cleanly shut down the
**  connection with the remote server before exiting.
*/
static int
fatal_cleanup(void)
{
    /* Don't recurse. */
    message_fatal_cleanup = NULL;

    /* QuitServer does all the work. */
    QuitServer(1);
    return 1;
}


/*
**  Flush a stdio FILE; exit if there are any errors.
*/
static void
SafeFlush(FILE *F)
{
    if (FLUSH_ERROR(F))
        sysdie("cannot send text to server");
}


/*
**  Trim trailing spaces, return pointer to first non-space char.
*/
static char *
TrimSpaces(char *p)
{
    char	*start;

    for (start = p; ISWHITE(*start); start++)
	continue;
    for (p = start + strlen(start); p > start && CTYPE(isspace, p[-1]); )
	*--p = '\0';
    return start;
}


/*
**  Mark the end of the header starting at p, and return a pointer
**  to the start of the next one.  Handles continuations.
*/
static char *
NextHeader(char *p)
{
    for ( ; ; p++) {
	if ((p = strchr(p, '\n')) == NULL)
            die("article is all headers");
	if (!ISWHITE(p[1])) {
	    *p = '\0';
	    return p + 1;
	}
    }
}


/*
**  Strip any headers off the article and dump them into the table.
*/
static char *
StripOffHeaders(char *article)
{
    char	*p;
    char	*q;
    HEADER	*hp;
    char	c;
    int	i;

    /* Set up the other headers list. */
    OtherSize = HEADER_DELTA;
    OtherHeaders = xmalloc(OtherSize * sizeof(char *));
    OtherCount = 0;

    /* Scan through buffer, a header at a time. */
    for (i = 0, p = article; ; i++) {

	if ((q = strchr(p, ':')) == NULL)
            die("no colon in header line \"%.30s...\"", p);
	if (q[1] == '\n' && !ISWHITE(q[2])) {
	    /* Empty header; ignore this one, get next line. */
	    p = NextHeader(p);
	    if (*p == '\n')
		break;
	}

	if (q[1] != '\0' && !ISWHITE(q[1])) {
	    if ((q = strchr(q, '\n')) != NULL)
		*q = '\0';
            die("no space after colon in \"%.30s...\"", p);
	}

	/* See if it's a known header. */
	c = CTYPE(islower, *p) ? toupper(*p) : *p;
	for (hp = Table; hp < ARRAY_END(Table); hp++)
	    if (c == hp->Name[0]
	     && p[hp->Size] == ':'
	     && ISWHITE(p[hp->Size + 1])
	     && strncasecmp(p, hp->Name, hp->Size) == 0) {
		if (hp->Type == HTobs)
                    die("obsolete header: %s", hp->Name);
		if (hp->Value)
                    die("duplicate header: %s", hp->Name);
		for (q = &p[hp->Size + 1]; ISWHITE(*q); q++)
		    continue;
		hp->Value = q;
		break;
	    }

	/* Too many headers? */
	if (++i > 5 * HEADER_DELTA)
            die("more than %d lines of header", i);

	/* No; add it to the set of other headers. */
	if (hp == ARRAY_END(Table)) {
	    if (OtherCount >= OtherSize - 1) {
		OtherSize += HEADER_DELTA;
                OtherHeaders = xrealloc(OtherHeaders, OtherSize * sizeof(char *));
	    }
	    OtherHeaders[OtherCount++] = p;
	}

	/* Get start of next header; if it's a blank line, we hit the end. */
	p = NextHeader(p);
	if (*p == '\n')
	    break;
    }

    return p + 1;
}



/*
**  See if the user is allowed to cancel the indicated message.  Assumes
**  that the Sender or From line has already been filled in.
*/
static void
CheckCancel(char *msgid, bool JustReturn)
{
    char		localfrom[SMBUF];
    char	*p;
    char		buff[BUFSIZ];
    char		remotefrom[SMBUF];

    /* Ask the server for the article. */
    fprintf(ToServer, "head %s\r\n", msgid);
    SafeFlush(ToServer);
    if (fgets(buff, sizeof buff, FromServer) == NULL
     || atoi(buff) != NNTP_OK_HEAD) {
	if (JustReturn)
	    return;
        die("server has no such article");
    }

    /* Read the headers, looking for the From or Sender. */
    remotefrom[0] = '\0';
    while (fgets(buff, sizeof buff, FromServer) != NULL) {
	if ((p = strchr(buff, '\r')) != NULL)
	    *p = '\0';
	if ((p = strchr(buff, '\n')) != NULL)
	    *p = '\0';
	if (buff[0] == '.' && buff[1] == '\0')
	    break;
        if (strncmp(buff, "Sender:", 7) == 0)
            strlcpy(remotefrom, TrimSpaces(&buff[7]), SMBUF);
        else if (remotefrom[0] == '\0' && strncmp(buff, "From:", 5) == 0)
            strlcpy(remotefrom, TrimSpaces(&buff[5]), SMBUF);
    }
    if (remotefrom[0] == '\0') {
	if (JustReturn)
	    return;
        die("article is garbled");
    }
    HeaderCleanFrom(remotefrom);

    /* Get the local user. */
    strlcpy(localfrom, HDR(_sender) ? HDR(_sender) : HDR(_from), SMBUF);
    HeaderCleanFrom(localfrom);

    /* Is the right person cancelling? */
    if (strcasecmp(localfrom, remotefrom) != 0)
        die("article was posted by \"%s\" and you are \"%s\"", remotefrom,
            localfrom);
}


/*
**  See if the user is the news administrator.
*/
static bool
AnAdministrator(void)
{
    uid_t               news_uid;
    gid_t               news_gid;

    if (Revoked)
	return false;

    /* Find out who we are. */
    if (get_news_uid_gid(&news_uid, &news_gid, false) != 0) {
        /* Silent failure; clients might not have the group. */
        return false;
    }
    if (getuid() == news_uid)
        return true;

    /* See if we are in the right group and examine process
     * supplementary groups, rather than the group(5) file entry.
     */
    {
        int ngroups = getgroups(0, 0);
        gid_t *groups, *gp;
        int rv;
        int rest;

        groups = (gid_t *) xmalloc(ngroups * sizeof(*groups));
        if ((rv = getgroups(ngroups, groups)) < 0) {
            /* Silent failure; client doesn't have the group. */
            return false;
        }
        for (rest = ngroups, gp = groups; rest > 0; rest--, gp++) {
            if (*gp == news_gid)
                return true;
        }
    }
    
    return false;
}


/*
**  Check the control message, and see if it's legit.
*/
static void
CheckControl(char *ctrl)
{
    char	*p;
    char	*q;
    char		save;

    /* Snip off the first word. */
    for (p = ctrl; ISWHITE(*p); p++)
	continue;
    for (ctrl = p; *p && !ISWHITE(*p); p++)
	continue;
    if (p == ctrl)
        die("emtpy control message");
    save = *p;
    *p = '\0';

    if (strcmp(ctrl, "cancel") == 0) {
	for (q = p + 1; ISWHITE(*q); q++)
	    continue;
	if (*q == '\0')
            die("message ID missing in cancel");
	if (!Spooling)
	    CheckCancel(q, false);
    }
    else if (strcmp(ctrl, "checkgroups") == 0
	  || strcmp(ctrl, "ihave")       == 0
	  || strcmp(ctrl, "sendme")      == 0
	  || strcmp(ctrl, "newgroup")    == 0
	  || strcmp(ctrl, "rmgroup")     == 0
	  || strcmp(ctrl, "sendsys")     == 0
	  || strcmp(ctrl, "senduuname")  == 0
	  || strcmp(ctrl, "version")     == 0) {
	if (!AnAdministrator())
            die("ask your news administrator to do the %s for you", ctrl);
    }
    else {
        die("%s is not a valid control message", ctrl);
    }
    *p = save;
}



/*
**  Parse the GECOS field to get the user's full name.  This comes Sendmail's
**  buildfname routine.  Ignore leading stuff like "23-" "stuff]-" or
**  "stuff -" as well as trailing whitespace, or anything that comes after
**  a comma, semicolon, or in parentheses.  This seems to strip off most of
**  the UCB or ATT stuff people fill out the entries with.  Also, turn &
**  into the login name, with perhaps an initial capital.  (Everyone seems
**  to hate that, but everyone also supports it.)
*/
static char *
FormatUserName(struct passwd *pwp, char *node)
{
    char	outbuff[SMBUF];
    char        *buff;
    char	*out;
    char	*p;
    int         left;

#if	!defined(DONT_MUNGE_GETENV)
    memset(outbuff, 0, SMBUF);
    if ((p = getenv("NAME")) != NULL)
	strlcpy(outbuff, p, SMBUF);
    if (strlen(outbuff) == 0) {
#endif	/* !defined(DONT_MUNGE_GETENV) */


#ifndef DO_MUNGE_GECOS
    strlcpy(outbuff, pwp->pw_gecos, SMBUF);
#else
    /* Be very careful here.  If we're not, we can potentially overflow our
     * buffer.  Remember that on some Unix systems, the content of the GECOS
     * field is under (untrusted) user control and we could be setgid. */
    p = pwp->pw_gecos;
    left = SMBUF - 1;
    if (*p == '*')
	p++;
    for (out = outbuff; *p && !GECOSTERM(*p) && left; p++) {
	if (*p == '&') {
	    strncpy(out, pwp->pw_name, left);
	    if (CTYPE(islower, *out)
	     && (out == outbuff || !CTYPE(isalpha, out[-1])))
		*out = toupper(*out);
	    while (*out) {
		out++;
                left--;
            }
	}
	else if (*p == '-'
	      && p > pwp->pw_gecos
              && (CTYPE(isdigit, p[-1]) || CTYPE(isspace, p[-1])
                  || p[-1] == ']')) {
	    out = outbuff;
            left = SMBUF - 1;
        }
	else {
	    *out++ = *p;
            left--;
        }
    }
    *out = '\0';
#endif /* DO_MUNGE_GECOS */

#if	!defined(DONT_MUNGE_GETENV)
    }
#endif	/* !defined(DONT_MUNGE_GETENV) */

    out = TrimSpaces(outbuff);
    if (out[0])
        buff = concat(pwp->pw_name, "@", node, " (", out, ")", (char *) 0);
    else
        buff = concat(pwp->pw_name, "@", node, (char *) 0);
    return buff;
}


/*
**  Check the Distribution header, and exit on error.
*/
static void CheckDistribution(char *p)
{
    static char	SEPS[] = " \t,";
    const char  * const *dp;

    if ((p = strtok(p, SEPS)) == NULL)
        die("cannot parse Distribution header");
    do {
	for (dp = BadDistribs; *dp; dp++)
	    if (uwildmat(p, *dp))
                die("illegal distribution %s", p);
    } while ((p = strtok((char *)NULL, SEPS)) != NULL);
}


/*
**  Process all the headers.  FYI, they're done in RFC-order.
*/
static void
ProcessHeaders(bool AddOrg, int linecount, struct passwd *pwp)
{
    static char		PATHFLUFF[] = PATHMASTER;
    HEADER              *hp;
    char                *p;
    char		buff[SMBUF];
    char		from[SMBUF];

    /* Do some preliminary fix-ups. */
    for (hp = Table; hp < ARRAY_END(Table); hp++) {
	if (!hp->CanSet && hp->Value)
            die("cannot set system header %s", hp->Name);
	if (hp->Value) {
	    hp->Value = TrimSpaces(hp->Value);
	    if (*hp->Value == '\0')
		hp->Value = NULL;
	}
    }

    /* Set From or Sender. */
    if ((p = innconf->fromhost) == NULL)
        sysdie("cannot get hostname");
    if (HDR(_from) == NULL)
	HDR(_from) = FormatUserName(pwp, p);
    else {
      if (strlen(pwp->pw_name) + strlen(p) + 2 > sizeof(buff))
          die("username and host are too long");
      sprintf(buff, "%s@%s", pwp->pw_name, p);
      strlcpy(from, HDR(_from), SMBUF);
      HeaderCleanFrom(from);
      if (strcmp(from, buff) != 0)
        HDR(_sender) = xstrdup(buff);
    }

    if (HDR(_date) == NULL) {
	/* Set Date. */
	if (!makedate(-1, true, buff, sizeof(buff)))
	    die("cannot generate Date header");
	HDR(_date) = xstrdup(buff);
    }

    /* Newsgroups are checked later. */

    /* Set Subject; Control overrides the subject. */
    if (HDR(_control)) {
	CheckControl(HDR(_control));
    }
    else {
	p = HDR(_subject);
	if (p == NULL)
            die("required Subject header is missing or empty");
	else if (HDR(_alsocontrol))
	    CheckControl(HDR(_alsocontrol));
#if	0
	if (strncmp(p, "Re: ", 4) == 0 && HDR(_references) == NULL)
            die("article subject begins with \"Re: \" but has no references");
#endif	/* 0 */
    }

    /* Set Message-ID */
    if (HDR(_messageid) == NULL) {
	if ((p = GenerateMessageID(innconf->domain)) == NULL)
            die("cannot generate Message-ID header");
	HDR(_messageid) = xstrdup(p);
    }
    else if ((p = strchr(HDR(_messageid), '@')) == NULL
             || strchr(++p, '@') != NULL) {
        die("message ID must have exactly one @");
    }

    /* Set Path */
    if (HDR(_path) == NULL) {
#if	defined(DO_INEWS_PATH)
	if ((p = innconf->pathhost) != NULL) {
	    if (*p)
                HDR(_path) = concat(Exclusions, p, "!", PATHFLUFF, (char *) 0);
	    else
                HDR(_path) = concat(Exclusions, PATHFLUFF, (char *) 0);
	}
	else if (innconf->server != NULL) {
	    if ((p = GetFQDN(innconf->domain)) == NULL)
                sysdie("cannot get hostname");
	    HDR(_path) = concat(Exclusions, p, "!", PATHFLUFF, (char *) 0);
	}
	else {
	    HDR(_path) = concat(Exclusions, PATHFLUFF, (char *) 0);
	}
#else
	HDR(_path) = concat(Exclusions, PATHFLUFF, (char *) 0);
#endif	/* defined(DO_INEWS_PATH) */
    }

    /* Reply-To; left alone. */
    /* Sender; set above. */
    /* Followup-To; checked with Newsgroups. */

    /* Check Expires. */
    if (HDR(_expires) && parsedate_rfc2822_lax(HDR(_expires)) == -1)
        die("cannot parse \"%s\" as an expiration date", HDR(_expires));

    /* References; left alone. */
    /* Control; checked above. */

    /* Distribution. */
    if ((p = HDR(_distribution)) != NULL) {
	p = xstrdup(p);
	CheckDistribution(p);
	free(p);
    }

    /* Set Organization. */
    if (AddOrg
     && HDR(_organization) == NULL
     && (p = innconf->organization) != NULL) {
	HDR(_organization) = xstrdup(p);
    }

    /* Keywords; left alone. */
    /* Summary; left alone. */
    /* Approved; left alone. */

    /* Set Lines */
    sprintf(buff, "%d", linecount);
    HDR(_lines) = xstrdup(buff);

    /* Check Supersedes. */
    if (HDR(_supersedes))
	CheckCancel(HDR(_supersedes), true);

    /* Now make sure everything is there. */
    for (hp = Table; hp < ARRAY_END(Table); hp++)
	if (hp->Type == HTreq && hp->Value == NULL)
            die("required header %s is missing or empty", hp->Name);
}


/*
**  Try to append $HOME/.signature to the article.  When in doubt, exit
**  out in order to avoid postings like "Sorry, I forgot my .signature
**  -- here's the article again."
*/
static char *
AppendSignature(bool UseMalloc, char *article, char *homedir, int *linesp)
{
    static char	NOSIG[] = "Can't add your .signature (%s), article not posted";
    int		i;
    int		length;
    size_t      artsize;
    char	*p;
    char	buff[BUFSIZ];
    FILE	*F;

    /* Open the file. */
    *linesp = 0;
    if (strlen(homedir) > sizeof(buff) - 14)
        die("home directory path too long");
    sprintf(buff, "%s/.signature", homedir);
    if ((F = fopen(buff, "r")) == NULL) {
	if (errno == ENOENT)
	    return article;
	fprintf(stderr, NOSIG, strerror(errno));
	QuitServer(1);
    }

    /* Read it in. */
    length = fread(buff, 1, sizeof buff - 2, F);
    i = feof(F);
    fclose(F);
    if (length == 0)
        die("signature file is empty");
    if (length < 0)
        sysdie("cannot read signature file");
    if (length == sizeof buff - 2 && !i)
        die("signature is too large");

    /* Make sure the buffer ends with \n\0. */
    if (buff[length - 1] != '\n')
	buff[length++] = '\n';
    buff[length] = '\0';

    /* Count the lines. */
    for (i = 0, p = buff; (p = strchr(p, '\n')) != NULL; p++)
	if (++i > SIG_MAXLINES)
            die("signature has too many lines");
    *linesp = 1 + i;

    /* Grow the article to have the signature. */
    i = strlen(article);
    artsize = i + sizeof(SIGSEP) - 1 + length + 1;
    if (UseMalloc) {
        p = xmalloc(artsize);
        strlcpy(p, article, artsize);
	article = p;
    }
    else
        article = xrealloc(article, artsize);
    strlcat(article, SIGSEP, artsize);
    strlcat(article, buff, artsize);
    return article;
}


/*
**  See if the user has more included text than new text.  Simple-minded, but
**  reasonably effective for catching neophyte's mistakes.  A line starting
**  with > is included text.  Decrement the count on lines starting with <
**  so that we don't reject diff(1) output.
*/
static void
CheckIncludedText(char *p, int lines)
{
    int	i;

    for (i = 0; ; p++) {
	switch (*p) {
	case '>':
	    i++;
	    break;
	case '|':
	    i++;
	    break;
	case ':':
	    i++;
	    break;
	case '<':
	    i--;
	    break;
	}
	if ((p = strchr(p, '\n')) == NULL)
	    break;
    }
    if ((i * 2 > lines) && (lines > 40))
        die("more included text than new text");
}



/*
**  Read stdin into a string and return it.  Can't use ReadInDescriptor
**  since that will fail if stdin is a tty.
*/
static char *
ReadStdin(void)
{
    int	size;
    char	*p;
    char		*article;
    char	*end;
    int	i;

    size = BUFSIZ;
    article = xmalloc(size);
    end = &article[size - 3];
    for (p = article; (i = getchar()) != EOF; *p++ = (char)i)
	if (p == end) {
            article = xrealloc(article, size + BUFSIZ);
	    p = &article[size - 3];
	    size += BUFSIZ;
	    end = &article[size - 3];
	}

    /* Force a \n terminator. */
    if (p > article && p[-1] != '\n')
	*p++ = '\n';
    *p = '\0';
    return article;
}



/*
**  Offer the article to the server, return its reply.
*/
static int
OfferArticle(char *buff, bool Authorized)
{
    fprintf(ToServer, "post\r\n");
    SafeFlush(ToServer);
    if (fgets(buff, HEADER_STRLEN, FromServer) == NULL)
        sysdie(Authorized ? "Can't offer article to server (authorized)"
                          : "Can't offer article to server");
    return atoi(buff);
}


/*
**  Spool article to temp file.
*/
static void
Spoolit(char *article, size_t Length, char *deadfile)
{
    HEADER *hp;
    FILE *F;
    int i;

    /* Try to write to the deadfile. */
    if (deadfile == NULL)
        return;
    F = xfopena(deadfile);
    if (F == NULL)
        sysdie("cannot create spool file");

    /* Write the headers and a blank line. */
    for (hp = Table; hp < ARRAY_END(Table); hp++)
	if (hp->Value)
	    fprintf(F, "%s: %s\n", hp->Name, hp->Value);
    for (i = 0; i < OtherCount; i++)
	fprintf(F, "%s\n", OtherHeaders[i]);
    fprintf(F, "\n");
    if (FLUSH_ERROR(F))
        sysdie("cannot write headers");

    /* Write the article and exit. */
    if (fwrite(article, 1, Length, F) != Length)
        sysdie("cannot write article");
    if (FLUSH_ERROR(F))
        sysdie("cannot write article");
    if (fclose(F) == EOF)
        sysdie("cannot close spool file");
}


/*
**  Print usage message and exit.
*/
static void
Usage(void)
{
    fprintf(stderr, "Usage: inews [-D] [-h] [header_flags] [article]\n");
    /* Don't call QuitServer here -- connection isn't open yet. */
    exit(1);
}


int
main(int ac, char *av[])
{
    static char		NOCONNECT[] = "cannot connect to server";
    int                 i;
    char                *p;
    HEADER              *hp;
    int			j;
    int			port;
    int			Mode;
    int			SigLines;
    struct passwd	*pwp;
    char		*article;
    char		*deadfile;
    char		buff[HEADER_STRLEN];
    char		SpoolMessage[HEADER_STRLEN];
    bool		DoSignature;
    bool		AddOrg;
    size_t		Length;
    uid_t               uid;

    /* First thing, set up logging and our identity. */
    message_program_name = "inews";

    /* Find out who we are. */
    uid = geteuid();
    if (uid == (uid_t) -1)
        sysdie("cannot get your user ID");
    if ((pwp = getpwuid(uid)) == NULL)
        sysdie("cannot get your passwd entry");

    /* Set defaults. */
    Mode = '\0';
    Dump = false;
    DoSignature = true;
    AddOrg = true;
    port = 0;

    if (!innconf_read(NULL))
        exit(1);

    umask(NEWSUMASK);

    /* Parse JCL. */
    while ((i = getopt(ac, av, "DNAVWORShx:a:c:d:e:f:n:p:r:t:F:o:w:")) != EOF)
	switch (i) {
	default:
	    Usage();
	    /* NOTREACHED */
	case 'D':
	case 'N':
	    Dump = true;
	    break;
	case 'A':
	case 'V':
	case 'W':
	    /* Ignore C News options. */
	    break;
	case 'O':
	    AddOrg = false;
	    break;
	case 'R':
	    Revoked = true;
	    break;
	case 'S':
	    DoSignature = false;
	    break;
	case 'h':
	    Mode = i;
	    break;
	case 'x':
            Exclusions = concat(optarg, "!", (char *) 0);
	    break;
	 case 'p':
	    port = atoi(optarg);
	    break;
	/* Header lines that can be specified on the command line. */
	case 'a':	HDR(_approved) = optarg;		break;
	case 'c':	HDR(_control) = optarg;			break;
	case 'd':	HDR(_distribution) = optarg;		break;
	case 'e':	HDR(_expires) = optarg;			break;
	case 'f':	HDR(_from) = optarg;			break;
	case 'n':	HDR(_newsgroups) = optarg;		break;
	case 'r':	HDR(_replyto) = optarg;			break;
	case 't':	HDR(_subject) = optarg;			break;
	case 'F':	HDR(_references) = optarg;		break;
	case 'o':	HDR(_organization) = optarg;		break;
	case 'w':	HDR(_followupto) = optarg;		break;
	}
    ac -= optind;
    av += optind;

    /* Parse positional arguments; at most one, the input file. */
    switch (ac) {
    default:
	Usage();
	/* NOTREACHED */
    case 0:
	/* Read stdin. */
	article = ReadStdin();
	break;
    case 1:
	/* Read named file. */
	article = ReadInFile(av[0], (struct stat *)NULL);
	if (article == NULL)
            sysdie("cannot read input file");
	break;
    }

    if (port == 0)
        port = NNTP_PORT;

    /* Try to open a connection to the server. */
    if (NNTPremoteopen(port, &FromServer, &ToServer, buff, sizeof(buff)) < 0) {
	Spooling = true;
	if ((p = strchr(buff, '\n')) != NULL)
	    *p = '\0';
	if ((p = strchr(buff, '\r')) != NULL)
	    *p = '\0';
	strlcpy(SpoolMessage, buff[0] ? buff : NOCONNECT,
                sizeof(SpoolMessage));
        deadfile = concatpath(pwp->pw_dir, "dead.article");
    }
    else {
        /* We now have an open server connection, so close it on failure. */
        message_fatal_cleanup = fatal_cleanup;

	/* See if we can post. */
	i = atoi(buff);

	/* Tell the server we're posting. */
	setbuf(FromServer, xmalloc(BUFSIZ));
	setbuf(ToServer, xmalloc(BUFSIZ));
	fprintf(ToServer, "mode reader\r\n");
	SafeFlush(ToServer);
	if (fgets(buff, HEADER_STRLEN, FromServer) == NULL)
            sysdie("cannot tell server we're reading");
	if ((j = atoi(buff)) != NNTP_ERR_COMMAND)
	    i = j;

	if (i != NNTP_OK_BANNER_POST)
            die("you do not have permission to post");
	deadfile = NULL;
    }

    /* Basic processing. */
    for (hp = Table; hp < ARRAY_END(Table); hp++)
	hp->Size = strlen(hp->Name);
    if (Mode == 'h')
	article = StripOffHeaders(article);
    for (i = 0, p = article; (p = strchr(p, '\n')) != NULL; i++, p++)
	continue;
    if (innconf->checkincludedtext)
	CheckIncludedText(article, i);
    if (DoSignature)
	article = AppendSignature(Mode == 'h', article, pwp->pw_dir, &SigLines);
    else
	SigLines = 0;
    ProcessHeaders(AddOrg, i + SigLines, pwp);
    Length = strlen(article);
    if ((innconf->localmaxartsize > 0)
	    && (Length > (size_t)innconf->localmaxartsize))
        die("article is larger than local limit of %ld bytes",
            innconf->localmaxartsize);

    /* Do final checks. */
    if (i == 0 && HDR(_control) == NULL)
        die("article is empty");
    for (hp = Table; hp < ARRAY_END(Table); hp++)
	if (hp->Value && (int)strlen(hp->Value) + hp->Size > HEADER_STRLEN)
            die("%s header is too long", hp->Name);
    for (i = 0; i < OtherCount; i++)
	if ((int)strlen(OtherHeaders[i]) > HEADER_STRLEN)
            die("header too long (maximum length is %d): %.40s...",
                HEADER_STRLEN, OtherHeaders[i]);

    if (Dump) {
	/* Write the headers and a blank line. */
	for (hp = Table; hp < ARRAY_END(Table); hp++)
	    if (hp->Value)
		printf("%s: %s\n", hp->Name, hp->Value);
	for (i = 0; i < OtherCount; i++)
	    printf("%s\n", OtherHeaders[i]);
	printf("\n");
	if (FLUSH_ERROR(stdout))
            sysdie("cannot write headers");

	/* Write the article and exit. */
	if (fwrite(article, 1, Length, stdout) != Length)
            sysdie("cannot write article");
	SafeFlush(stdout);
	QuitServer(0);
    }

    if (Spooling) {
        warn("warning: %s", SpoolMessage);
        warn("article will be spooled");
	Spoolit(article, Length, deadfile);
	exit(0);
    }

    /* Article is prepared, offer it to the server. */
    i = OfferArticle(buff, false);
    if (i == NNTP_FAIL_AUTH_NEEDED) {
	/* Posting not allowed, try to authorize. */
	if (NNTPsendpassword((char *)NULL, FromServer, ToServer) < 0)
            sysdie("authorization error");
	i = OfferArticle(buff, true);
    }
    if (i != NNTP_CONT_POST)
        die("server doesn't want the article: %s", buff);

    /* Write the headers, a blank line, then the article. */
    for (hp = Table; hp < ARRAY_END(Table); hp++)
	if (hp->Value)
	    fprintf(ToServer, "%s: %s\r\n", hp->Name, hp->Value);
    for (i = 0; i < OtherCount; i++)
	fprintf(ToServer, "%s\r\n", OtherHeaders[i]);
    fprintf(ToServer, "\r\n");
    if (NNTPsendarticle(article, ToServer, true) < 0)
        sysdie("cannot send article to server");
    SafeFlush(ToServer);

    if (fgets(buff, sizeof buff, FromServer) == NULL)
        sysdie("no reply from server after sending the article");
    if ((p = strchr(buff, '\r')) != NULL)
	*p = '\0';
    if ((p = strchr(buff, '\n')) != NULL)
	*p = '\0';
    if (atoi(buff) != NNTP_OK_POST)
        die("cannot send article to server: %s", buff);

    /* Close up. */
    QuitServer(0);
    /* NOTREACHED */
    return 1;
}

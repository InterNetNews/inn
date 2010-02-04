/*  $Id$
**
**  Check article, send it to the local server.
*/
#include "config.h"
#include "clibrary.h"

#include "inn/innconf.h"
#include "nnrpd.h"
#include "inn/ov.h"
#include "post.h"

#define FLUSH_ERROR(F)		(fflush((F)) == EOF || ferror((F)))
#define HEADER_DELTA		20

static char     *tmpPtr ;
static char	Error[SMBUF];
static char	NGSEPS[] = NG_SEPARATOR;
char	**OtherHeaders;
int	OtherCount;
bool   HeadersModified;
static int	OtherSize;
static const char * const BadDistribs[] = {
    BAD_DISTRIBS
};

/*
**  Do not modify the table without also looking at post.h for potential
**  changes in the order of the fields.
*/
HEADER Table[] = {
    /*  Name                    CanSet  Type    Size  Value */
    {   "Path",                 true,   HTstd,  0,    NULL,    NULL, 0 },
    {   "From",                 true,   HTreq,  0,    NULL,    NULL, 0 },
    {   "Newsgroups",           true,   HTreq,  0,    NULL,    NULL, 0 },
    {   "Subject",              true,   HTreq,  0,    NULL,    NULL, 0 },
    {   "Control",              true,   HTstd,  0,    NULL,    NULL, 0 },
    {   "Supersedes",           true,   HTstd,  0,    NULL,    NULL, 0 },
    {   "Followup-To",          true,   HTstd,  0,    NULL,    NULL, 0 },
    {   "Date",                 true,   HTstd,  0,    NULL,    NULL, 0 },
    {   "Organization",         true,   HTstd,  0,    NULL,    NULL, 0 },
    {   "Lines",                true,   HTstd,  0,    NULL,    NULL, 0 },
    {   "Sender",               true,   HTstd,  0,    NULL,    NULL, 0 },
    {   "Approved",             true,   HTstd,  0,    NULL,    NULL, 0 },
    {   "Archive",              true,   HTstd,  0,    NULL,    NULL, 0 },
    {   "Distribution",         true,   HTstd,  0,    NULL,    NULL, 0 },
    {   "Expires",              true,   HTstd,  0,    NULL,    NULL, 0 },
    {   "Message-ID",           true,   HTstd,  0,    NULL,    NULL, 0 },
    {   "References",           true,   HTstd,  0,    NULL,    NULL, 0 },
    {   "Reply-To",             true,   HTstd,  0,    NULL,    NULL, 0 },
    {   "NNTP-Posting-Host",    false,  HTobs,  0,    NULL,    NULL, 0 },
    {   "Mime-Version",         true,   HTstd,  0,    NULL,    NULL, 0 },
    {   "Content-Type",         true,   HTstd,  0,    NULL,    NULL, 0 },
    {   "Content-Transfer-Encoding", true, HTstd,  0,    NULL,    NULL, 0 },
    {   "X-Trace",              false,  HTobs,  0,    NULL,    NULL, 0 },
    {   "X-Complaints-To",      false,  HTobs,  0,    NULL,    NULL, 0 },
    {   "NNTP-Posting-Date",    false,  HTobs,  0,    NULL,    NULL, 0 },
    {   "Xref",                 false,  HTstd,  0,    NULL,    NULL, 0 },
    {   "Injection-Date",       true,   HTstd,  0,    NULL,    NULL, 0 },
    {   "Injection-Info",       false,  HTstd,  0,    NULL,    NULL, 0 },
    {   "Summary",              true,   HTstd,  0,    NULL,    NULL, 0 },
    {   "Keywords",             true,   HTstd,  0,    NULL,    NULL, 0 },
    {   "User-Agent",           true,   HTstd,  0,    NULL,    NULL, 0 },
    {   "Date-Received",        false,  HTobs,  0,    NULL,    NULL, 0 },
    {   "Received",             false,  HTobs,  0,    NULL,    NULL, 0 },
    {   "Posted",               false,  HTobs,  0,    NULL,    NULL, 0 },
    {   "Posting-Version",      false,  HTobs,  0,    NULL,    NULL, 0 },
    {   "Relay-Version",        false,  HTobs,  0,    NULL,    NULL, 0 },
    {   "Cc",                   true,   HTstd,  0,    NULL,    NULL, 0 },
    {   "Bcc",                  true,   HTstd,  0,    NULL,    NULL, 0 },
    {   "To",                   true,   HTstd,  0,    NULL,    NULL, 0 },
    {   "Archived-At",          true,   HTstd,  0,    NULL,    NULL, 0 },
    {   "Also-Control",         false,  HTobs,  0,    NULL,    NULL, 0 },
    {   "Article-Names",        false,  HTobs,  0,    NULL,    NULL, 0 },
    {   "Article-Updates",      false,  HTobs,  0,    NULL,    NULL, 0 },
    {   "See-Also",             false,  HTobs,  0,    NULL,    NULL, 0 },
/* The Comments: and Original-Sender: header fields can appear more than once
 * in the headers of an article.  Consequently, we MUST NOT put them here. */
};

HEADER *EndOfTable = ARRAY_END(Table);


/*
**  Turn any \r or \n in text into spaces.  Used to splice back multi-line
**  headers into a single line.
**  Taken from innd.c.
*/
static char *
Join(char *text)
{
    char	*p;

    for (p = text; *p; p++)
	if (*p == '\n' || *p == '\r')
	    *p = ' ';
    return text;
}


/*
**  Return a short name that won't overrun our buffer or syslog's buffer.
**  q should either be p, or point into p where the "interesting" part is.
**  Taken from innd.c.
*/
static char *
MaxLength(char *p, char *q)
{
    static char		buff[80];
    unsigned int	i;


    /* Return an empty string when p is NULL. */
    if (p == NULL) {
        *buff = '\0';
        return buff;
    }

    /* Already short enough? */
    i = strlen(p);
    if (i < sizeof buff - 1)
	return Join(p);

    /* Don't want casts to unsigned to go horribly wrong. */
    if (q < p || q > p + i)
	q = p;

    /* Simple case of just want the beginning? */
    if (q == NULL || (size_t)(q - p) < sizeof(buff) - 4) {
	strlcpy(buff, p, sizeof(buff) - 3);
        strlcat(buff, "...", sizeof(buff));
    } else if ((p + i) - q < 10) {
	/* Is getting last 10 characters good enough? */
	strlcpy(buff, p, sizeof(buff) - 13);
        strlcat(buff, "...", sizeof(buff) - 10);
        strlcat(buff, &p[i - 10], sizeof(buff));
    } else {
	/* Not in last 10 bytes, so use double ellipses. */
        strlcpy(buff, p, sizeof(buff) - 16);
        strlcat(buff, "...", sizeof(buff) - 13);
        strlcat(buff, &q[-5], sizeof(buff) - 3);
        strlcat(buff, "...", sizeof(buff));
    }
    return Join(buff);
}


/*
**  Trim leading and trailing spaces, return the length of the result.
*/
int
TrimSpaces(char *p)
{
    char	*start;

    for (start = p; ISWHITE(*start) || *start == '\n'; start++)
	continue;
    for (p = start + strlen(start); p > start && CTYPE(isspace, (int)p[-1]); p--)
	continue;
    return (int)(p - start);
}


/*
**  Mark the end of the header starting at p, and return a pointer
**  to the start of the next one or NULL.  Handles continuations.
*/
static char *
NextHeader(char *p)
{
    for ( ; (p = strchr(p, '\n')) != NULL; p++) {
	if (ISWHITE(p[1]))
	    continue;
	*p = '\0';
	return p + 1;
    }
    return NULL;
}


/*
**  Strip any headers off the article and dump them into the table.
**  On error, return NULL and fill in Error.
*/
static char *
StripOffHeaders(char *article)
{
    char	*p;
    char	*q;
    HEADER	*hp;
    char	c;

    /* Scan through buffer, a header at a time. */
    for (p = article; ; ) {

	/* See if it's a known header. */
	c = CTYPE(islower, (int)*p) ? toupper(*p) : *p;
	for (hp = Table; hp < ARRAY_END(Table); hp++) {
	    if (c == hp->Name[0]
	     && p[hp->Size] == ':'
	     && strncasecmp(p, hp->Name, hp->Size) == 0) {
		if (hp->Type == HTobs) {
		    snprintf(Error, sizeof(Error), "Obsolete %s: header",
                             hp->Name);
		    return NULL;
		}
		if (hp->Value) {
		    snprintf(Error, sizeof(Error), "Duplicate %s: header",
                             hp->Name);
		    return NULL;
		}
		hp->Value = &p[hp->Size + 1];
		/* '\r\n' is replaced with '\n', and unnecessary to consider
		 * '\r'. */
		for (q = &p[hp->Size + 1]; ISWHITE(*q) || *q == '\n'; q++)
		    continue;
		hp->Body = q;
		break;
	    }
	}

	/* No; add it to the set of other headers. */
	if (hp == ARRAY_END(Table)) {
	    if (OtherCount >= OtherSize - 1) {
		OtherSize += HEADER_DELTA;
                OtherHeaders = xrealloc(OtherHeaders, OtherSize * sizeof(char *));
	    }
	    OtherHeaders[OtherCount++] = p;
	}

	/* Get start of next header; if it's a blank line, we hit the end. */
	if ((p = NextHeader(p)) == NULL) {
	    strlcpy(Error, "Article has no body -- just headers",
                    sizeof(Error));
	    return NULL;
	}
	if (*p == '\n')
	    break;
    }

    return p + 1;
}



/*
**  Check the control message, and see if it's legit.  Return pointer to
**  error message if not.
*/
static const char *
CheckControl(char *ctrl)
{
    char	*p;
    char	*q;
    char	save;

    /* Snip off the first word. */
    for (p = ctrl; ISWHITE(*p); p++)
	continue;
    for (ctrl = p; *p && !ISWHITE(*p); p++)
	continue;
    if (p == ctrl)
	return "Empty control message";
    save = *p;
    *p = '\0';

    if (strcasecmp(ctrl, "cancel") == 0) {
	for (q = p + 1; ISWHITE(*q); q++)
	    continue;
	if (*q == '\0')
	    return "Message-ID missing in cancel";
    } else if (strcasecmp(ctrl, "checkgroups") == 0
               || strcasecmp(ctrl, "ihave")    == 0
               || strcasecmp(ctrl, "sendme")   == 0
               || strcasecmp(ctrl, "newgroup") == 0
               || strcasecmp(ctrl, "rmgroup")  == 0)
        ;
    else {
	snprintf(Error, sizeof(Error),
                 "\"%s\" is not a valid control message",
                 MaxLength(ctrl, ctrl));
	return Error;
    }
    *p = save;
    return NULL;
}


/*
**  Check the Distribution: header, and exit on error.
*/
static const char *
CheckDistribution(char *p)
{
    static char	SEPS[] = " \t,";
    const char * const *dp;

    if ((p = strtok(p, SEPS)) == NULL)
	return "Can't parse Distribution: header";
    do {
	for (dp = BadDistribs; *dp; dp++)
	    if (uwildmat(p, *dp)) {
		snprintf(Error, sizeof(Error), "Illegal distribution \"%s\"",
                         MaxLength(p,p));
		return Error;
	    }
    } while ((p = strtok((char *)NULL, SEPS)) != NULL);
    return NULL;
}


/*
**  Process all the headers.  FYI, they're done in RFC-order.
**  Return NULL if okay, or an error message.
*/
static const char *
ProcessHeaders(char *idbuff)
{
    static char		datebuff[40];
    static char		localdatebuff[40];
    static char		orgbuff[SMBUF];
    static char         pathidentitybuff[SMBUF];
    static char 	complaintsbuff[SMBUF];
    static char         postinghostbuff[SMBUF];
    static char		sendbuff[SMBUF];
    static char         injectioninfobuff[SMBUF];
    static char		*newpath = NULL;
    HEADER		*hp;
    char		*p;
    time_t		t, now;
    const char          *error;
    pid_t               pid;
    bool		addvirtual = false;

    /* Get the current time, used for creating and checking dates. */
    now = time(NULL);

    /* Do some preliminary fix-ups. */
    for (hp = Table; hp < ARRAY_END(Table); hp++) {
	if (!hp->CanSet && hp->Value) {
	    snprintf(Error, sizeof(Error),
		     "Can't set system %s: header", hp->Name);
	    return Error;
	}
	if (hp->Value) {
	    hp->Len = TrimSpaces(hp->Value);
	    if (hp->Len == 0)
		hp->Value = hp->Body = NULL;
	}
    }

    /* If authorized, add the header based on our info.  If not authorized,
     * zap the Sender: header so we don't put out unauthenticated data. */
    if (PERMaccessconf->nnrpdauthsender) {
	if (PERMauthorized && PERMuser[0] != '\0') {
            p = strchr(PERMuser, '@');
            if (p == NULL) {
                snprintf(sendbuff, sizeof(sendbuff), "%s@%s", PERMuser,
                         Client.host);
            } else {
                snprintf(sendbuff, sizeof(sendbuff), "%s", PERMuser);
            }
	    HDR_SET(HDR__SENDER, sendbuff);
	} else {
	    HDR_CLEAR(HDR__SENDER);
	}
    }

    /* Set the Date: header.  datebuff is used later for Injection-Date:,
     * so we have to set it, and it has to be the UTC date. */
    if (!makedate(-1, false, datebuff, sizeof(datebuff)))
        return "Can't generate Date: header";
    if (HDR(HDR__DATE) == NULL) {
        if (PERMaccessconf->localtime) {
            if (!makedate(-1, true, localdatebuff, sizeof(localdatebuff)))
                return "Can't generate local date header";
	    HDR_SET(HDR__DATE, localdatebuff);
	} else {
	    HDR_SET(HDR__DATE, datebuff);
	}
    } else {
        t = parsedate_rfc5322_lax(HDR(HDR__DATE));
	if (t == (time_t) -1)
	    return "Can't parse Date: header";
	if (t > now + DATE_FUZZ)
	    return "Article posted in the future";
    }

    /* Newsgroups: is checked later. */

    if (HDR(HDR__CONTROL) != NULL) {
	if ((error = CheckControl(HDR(HDR__CONTROL))) != NULL)
	    return error;
    }

    /* Set the Message-ID: header. */
    if (HDR(HDR__MESSAGEID) == NULL) {
	HDR_SET(HDR__MESSAGEID, idbuff);
    }
    if (!IsValidMessageID(HDR(HDR__MESSAGEID), true)) {
        return "Can't parse Message-ID: header";
    }

    /* Set the Path: header. */
    if (HDR(HDR__PATH) == NULL || PERMaccessconf->strippath) {
	/* Note that innd will put host name here for us. */
	HDR_SET(HDR__PATH, (char *) PATHMASTER);
	if (VirtualPathlen > 0)
	    addvirtual = true;
    } else {
        /* Check that the article has not been injected yet. */
        for (p = HDR(HDR__PATH); *p != '\0'; p++) {
            if (*p == '.' && strncasecmp(p, ".POSTED", 7) == 0
                && (p[7] == '.' || p[7] == '!' || p[7] == ' ' || p[7] == '\t'
                    || p[7] == '\r' || p[7] == '\n')
                && (p == HDR(HDR__PATH) || p[-1] == '!')) {
                return "Path: header shows a previous injection of the article";
            }
        }

        /* Check whether the virtual host name is required. */
	if ((VirtualPathlen > 0) &&
	    (p = strchr(HDR(HDR__PATH), '!')) != NULL) {
	    *p = '\0';
	    if (strcasecmp(HDR(HDR__PATH), PERMaccessconf->pathhost) != 0)
		addvirtual = true;
	    *p = '!';
	} else if (VirtualPathlen > 0)
	    addvirtual = true;
    }

    if (newpath != NULL)
        free(newpath);
    if (PERMaccessconf->addinjectionpostinghost) {
        if (addvirtual) {
            newpath = concat(VirtualPath, ".POSTED.", Client.host, "!",
                             HDR(HDR__PATH), (char *) 0);
        } else {
            newpath = concat(".POSTED.", Client.host, "!", HDR(HDR__PATH),
                             (char *) 0);
        }
    } else {
        if (addvirtual) {
            newpath = concat(VirtualPath, ".POSTED!", HDR(HDR__PATH),
                             (char *) 0);
        } else {
            newpath = concat(".POSTED!", HDR(HDR__PATH), (char *) 0);
        }
    }
    HDR_SET(HDR__PATH, newpath);

    /* Reply-To: is left alone. */
    /* Sender: is set above. */

    /* Check the Expires: header. */
    if (HDR(HDR__EXPIRES) && parsedate_rfc5322_lax(HDR(HDR__EXPIRES)) == -1)
	return "Can't parse Expires: header";

    /* References: is left alone. */
    /* Control: is checked above. */

    /* Check the Distribution: header. */
    if ((p = HDR(HDR__DISTRIBUTION)) != NULL) {
	p = xstrdup(p);
	error = CheckDistribution(p);
	free(p);
	if (error != NULL)
	    return error;
    }

    /* Set the Organization: header. */
    if (HDR(HDR__ORGANIZATION) == NULL
        && (p = PERMaccessconf->organization) != NULL) {
	strlcpy(orgbuff, p, sizeof(orgbuff));
	HDR_SET(HDR__ORGANIZATION, orgbuff);
    }

    /* Keywords: is left alone. */
    /* Summary: is left alone. */
    /* Approved: is left alone. */

    /* Lines: should not be generated. */

    /* Supersedes: is left alone. */

    /* Set the Injection-Date: header. */
    if (HDR(HDR__INJECTION_DATE) == NULL) {
        if (PERMaccessconf->addinjectiondate) {
            HDR_SET(HDR__INJECTION_DATE, datebuff);
        }
    } else {
        t = parsedate_rfc5322_lax(HDR(HDR__INJECTION_DATE));
        if (t == (time_t) -1)
            return "Can't parse Injection-Date: header";
        if (t > now + DATE_FUZZ)
            return "Article injected in the future";
    }

    /* Set the Injection-Info: header. */
    /* Set the path identity. */
    if (VirtualPathlen > 0) {
        p = PERMaccessconf->domain;
    } else {
        if ((p = GetFQDN(PERMaccessconf->domain)) == NULL) {
            p = (char *) "unknown";
        }
    }
    snprintf(pathidentitybuff, sizeof(pathidentitybuff), "%s", p);

    /* Set the posting-host identity. */
    if (strcmp(Client.host, Client.ip) == 0) {
        snprintf(postinghostbuff, sizeof(postinghostbuff),
                 "; posting-host=\"%s\"", Client.ip);
    } else {
        snprintf(postinghostbuff, sizeof(postinghostbuff),
                 "; posting-host=\"%s:%s\"", Client.host, Client.ip);
    }

    /* Set the logging-data attribute. */
    pid = getpid();

    /* Set the mail-complaints-to attribute. */
    if ((p = PERMaccessconf->complaints) != NULL) {
        snprintf(complaintsbuff, sizeof(complaintsbuff), "%s", p);
    } else {
        static const char newsmaster[] = NEWSMASTER;

        if ((p = PERMaccessconf->fromhost) != NULL && strchr(newsmaster, '@') == NULL) {
            snprintf(complaintsbuff, sizeof(complaintsbuff), "%s@%s",
                     newsmaster, p);
        } else {
            snprintf(complaintsbuff, sizeof(complaintsbuff), "%s",
                     newsmaster);
        }
    }

    /* ARTpost() will convert bare LF to CRLF.  Do not use CRLF here.*/
    snprintf(injectioninfobuff, sizeof(injectioninfobuff),
             "%s%s;\n\tlogging-data=\"%ld\"; mail-complaints-to=\"%s\"",
             pathidentitybuff,
             PERMaccessconf->addinjectionpostinghost ? postinghostbuff : "",
             (long) pid, complaintsbuff);

    HDR_SET(HDR__INJECTION_INFO, injectioninfobuff);

    /* Clear out some headers that should not be here. */
    if (PERMaccessconf->strippostcc) {
	HDR_CLEAR(HDR__CC);
	HDR_CLEAR(HDR__BCC);
	HDR_CLEAR(HDR__TO);
    }

    /* Now make sure everything is there. */
    for (hp = Table; hp < ARRAY_END(Table); hp++)
	if (hp->Type == HTreq && hp->Value == NULL) {
	    snprintf(Error, sizeof(Error),
                     "Missing required %s: header", hp->Name);
	    return Error;
	}

    return NULL;
}


/*
**  See if the user has more included text than new text.  Simple-minded,
**  but reasonably effective for catching neophyte's mistakes.  Son-of-1036
**  says:
**
**      NOTE:  While encouraging trimming is desirable, the 50% rule imposed
**      by some old posting agents is both inadequate and counterproductive.
**      Posters do not respond to it by being more selective about quoting;
**      they respond by padding short responses, or by using different
**      quoting styles to defeat automatic analysis.  The former adds
**      unnecessary noise and volume, while the latter also defeats more
**      useful forms of automatic analysis that reading agents might wish to
**      do.
**
**      NOTE:  At the very least, if a minimum-unquoted quota is being set,
**      article bodies shorter than (say) 20 lines, or perhaps articles
**      which exceed the quota by only a few lines, should be exempt.  This
**      avoids the ridiculous situation of complaining about a 5-line
**      response to a 6-line quote.
**
**  Accordingly, bodies shorter than 20 lines are exempt.  A line starting
**  with >, |, or : is included text.  Decrement the count on lines starting
**  with < so that we don't reject diff(1) output.
*/
static const char *
CheckIncludedText(const char *p, int lines)
{
    int i;

    if (lines < 20)
        return NULL;
    for (i = 0; ; p++) {
        switch (*p) {
            case '>': i++; break;
            case '|': i++; break;
            case ':': i++; break;
            case '<': i--; break;
            default:       break;
        }
        p = strchr(p, '\n');
        if (p == NULL)
            break;
    }
    if (i * 2 > lines)
        return "Article not posted -- more included text than new text";
    return NULL;
}



/*
**  Try to mail an article to the moderator of the group.
*/
static const char *
MailArticle(char *group, char *article)
{
    static char	CANTSEND[] = "Can't send text to mailer";
    FILE	*F;
    HEADER	*hp;
    int		i;
    char	*address;
    char	buff[SMBUF];
    char	*mta;

    /* Try to get the address first. */
    if ((address = GetModeratorAddress(NULL, NULL, group, PERMaccessconf->moderatormailer)) == NULL) {
	snprintf(Error, sizeof(Error), "No mailing address for \"%s\" -- %s",
                 group, "ask your news administrator to fix this");
	free(group);
	return Error;
    }
    free(group);

    /* Now build up the command (ignore format/argument mismatch errors,
     * in case %s isn't in inconf->mta) and send the headers. */
    if ((mta = innconf->mta) == NULL)
	return "Can't start mailer -- mta not set";
    snprintf(buff, sizeof(buff), innconf->mta, address);
    if ((F = popen(buff, "w")) == NULL)
	return "Can't start mailer";
    fprintf(F, "To: %s\n", address);
    if (FLUSH_ERROR(F)) {
	pclose(F);
	return CANTSEND;
    }

    /* Write the headers, a blank line, then the article. */
    for (hp = Table; hp < ARRAY_END(Table); hp++)
	if (hp->Value) {
 	    if (*hp->Value == ' ' || *hp->Value == '\t')
                fprintf(F, "%s:%s\n", hp->Name, hp->Value);
 	    else
                fprintf(F, "%s: %s\n", hp->Name, hp->Value);
	    if (FLUSH_ERROR(F)) {
		pclose(F);
		return CANTSEND;
	    }
	}
    for (i = 0; i < OtherCount; i++) {
	fprintf(F, "%s\n", OtherHeaders[i]);
	if (FLUSH_ERROR(F)) {
	    pclose(F);
	    return CANTSEND;
	}
    }
    fprintf(F, "\n");
    i = strlen(article);
    if (fwrite(article, 1, i, F) != (size_t)i)
	return "Can't send article";
    if (FLUSH_ERROR(F)) {
	pclose(F);
	return CANTSEND;
    }
    i = pclose(F);
    if (i) {
	snprintf(Error, sizeof(Error), "Mailer exited with status %d -- %s",
                 i, "Article might not have been mailed");
	return Error;
    }
    return NULL;
}


/*
**  Check the newsgroups and make sure they're all valid, that none are
**  moderated, etc.
*/
static const char *
ValidNewsgroups(char *hdr, char **modgroup)
{
    static char		distbuff[SMBUF];
    char	        *groups;
    char	        *p;
    bool	        approved;
    struct _DDHANDLE	*h;
    char		*grplist[2];
    bool		IsNewgroup;
    bool		FoundOne;
    int                 flag;
    bool                hookpresent = false;

#ifdef DO_PYTHON
    hookpresent = PY_use_dynamic;
#endif /* DO_PYTHON */

    p = HDR(HDR__CONTROL);
    IsNewgroup = (p && strncasecmp(p, "newgroup", 8) == 0);
    groups = xstrdup(hdr);
    if ((p = strtok(groups, NGSEPS)) == NULL) {
        free(groups);
	return "Can't parse Newsgroups: header";
    }
    Error[0] = '\0';

    /* Reject all articles with Approved: headers unless the user is allowed to
     * add them, even to unmoderated or local groups.  We want to reject them
     * to unmoderated groups in case there's a disagreement of opinion
     * between various sites as to the moderation status. */
    approved = HDR(HDR__APPROVED) != NULL;
    if (approved && !PERMaccessconf->allowapproved) {
        snprintf(Error, sizeof(Error),
                 "You are not allowed to approve postings");
    }

    FoundOne = false;
    h = DDstart((FILE *)NULL, (FILE *)NULL);
    do {
	if (innconf->mergetogroups && strncmp(p, "to.", 3) == 0)
	    p = (char *) "to";
        if (!hookpresent && PERMspecified) {
	    grplist[0] = p;
	    grplist[1] = NULL;
	    if (!PERMmatch(PERMpostlist, grplist)) {
		snprintf(Error, sizeof(Error),
                         "You are not allowed to post to %s\r\n", p);
	    }
        }
	if (!OVgroupstats(p, NULL, NULL, NULL, &flag))
	    continue;
	FoundOne = true;
	DDcheck(h, p);
	switch (flag) {
	case NF_FLAG_OK:
#ifdef DO_PYTHON
	if (PY_use_dynamic) {
	    char    *reply;

	    /* Authorize user using Python module method dynamic. */
	    if (PY_dynamic(PERMuser, p, true, &reply) < 0) {
	        syslog(L_NOTICE, "PY_dynamic(): authorization skipped due to no Python dynamic method defined");
	    } else {
	        if (reply != NULL) {
		    syslog(L_TRACE, "PY_dynamic() returned a refuse string for user %s at %s who wants to post to %s: %s", PERMuser, Client.host, p, reply);
		    snprintf(Error, sizeof(Error), "%s\r\n", reply);
                    free(reply);
		    break;
		}
	    }
	}
#endif /* DO_PYTHON */
	    break;
	case NF_FLAG_MODERATED:
	    if (!approved && modgroup != NULL && !*modgroup)
		*modgroup = xstrdup(p);
	    break;
	case NF_FLAG_IGNORE:
	case NF_FLAG_NOLOCAL:
	    if (!PERMaccessconf->locpost)
		snprintf(Error, sizeof(Error),
                         "Postings to \"%s\" are not allowed here", p);
	    break;
	case NF_FLAG_JUNK:
	    /* Do NOT return an error. */
	    break;
	case NF_FLAG_ALIAS:
	    snprintf(Error, sizeof(Error),
                     "The newsgroup \"%s\" has been renamed\n", p);
	    break;
	}
    } while ((p = strtok((char *)NULL, NGSEPS)) != NULL);
    free(groups);

    if (!FoundOne && !IsNewgroup)
	snprintf(Error, sizeof(Error), "No valid newsgroups in \"%s\"",
                 MaxLength(hdr,hdr));
    if (Error[0]) {
        tmpPtr = DDend(h);
	free(tmpPtr);
        if (modgroup != NULL && *modgroup != NULL) {
            free(*modgroup);
            *modgroup = NULL;
        }
	return Error;
    }

    p = DDend(h);
    if (HDR(HDR__DISTRIBUTION) == NULL && *p) {
	strlcpy(distbuff, p, sizeof(distbuff));
	HDR_SET(HDR__DISTRIBUTION, distbuff);
    }
    free(p);
    return NULL;
}


/*
**  Send a QUIT message to the server, eat its reply.
*/
static void
SendQuit(FILE *FromServer, FILE *ToServer)
{
    char	buff[NNTP_MAXLEN_COMMAND];

    fprintf(ToServer, "QUIT\r\n");
    fflush(ToServer);
    fclose(ToServer);
    fgets(buff, sizeof buff, FromServer);
    fclose(FromServer);
}


/*
**  Offer the article to the server, return its reply.
*/
static int
OfferArticle(char *buff, int buffsize, FILE *FromServer, FILE *ToServer)
{
    static char		CANTSEND[] = "Can't send %s to server, %s";

    /* We have a valid message-ID here (checked beforehand). */
    fprintf(ToServer, "IHAVE %s\r\n", HDR(HDR__MESSAGEID));
    if (FLUSH_ERROR(ToServer)
     || fgets(buff, buffsize, FromServer) == NULL) {
	snprintf(buff, sizeof(buff), CANTSEND, "IHAVE", strerror(errno));
	return -1;
    }
    return atoi(buff);
}


/*
**  Spool article to temp file.
*/
static const char *
SpoolitTo(char *article, char *err, char *SpoolDir)
{
    static char	CANTSPOOL[NNTP_MAXLEN_COMMAND+2];
    HEADER *hp;
    FILE *F = NULL;
    int	i, fd;
    char *tmpspool = NULL;
    char *spoolfile = NULL;
    char *q;

    /* Initialize the returned error message. */
    snprintf(CANTSPOOL, sizeof(CANTSPOOL),
             "%s and can't write text to local spool file", err);

    /* Try to write it to the spool dir. */
    tmpspool = concatpath(SpoolDir, ".XXXXXX");
    fd = mkstemp(tmpspool);
    if (fd < 0) {
        syslog(L_FATAL, "can't create temporary spool file %s %m", tmpspool);
        goto fail;
    }
    F = fdopen(fd, "w");
    if (F == NULL) {
        syslog(L_FATAL, "can't open %s %m", tmpspool);
        goto fail;
    }
    fchmod(fileno(F), BATCHFILE_MODE);

    /* Write the headers and a blank line. */
    for (hp = Table; hp < ARRAY_END(Table); hp++)
	if (hp->Value) {
            q = xstrndup(hp->Value, hp->Body - hp->Value + hp->Len);
	    if (*hp->Value == ' ' || *hp->Value == '\t')
		fprintf(F, "%s:%s\n", hp->Name, q);
	    else
		fprintf(F, "%s: %s\n", hp->Name, q);
	    if (FLUSH_ERROR(F)) {
		fclose(F);
		free(q);
                goto fail;
	    }
	    free(q);
	}
    for (i = 0; i < OtherCount; i++) {
	fprintf(F, "%s\n", OtherHeaders[i]);
	if (FLUSH_ERROR(F)) {
	    fclose(F);
	    goto fail;
	}
    }
    fprintf(F, "\n");

    /* Write the article body. */
    i = strlen(article);
    if (fwrite(article, 1, i, F) != (size_t)i) {
        fclose(F);
        goto fail;
    }

    /* Flush and catch any errors. */
    if (fclose(F))
        goto fail;

    /* Rename the spool file to something rnews will pick up. */
    spoolfile = concatpath(SpoolDir, "XXXXXX");
    fd = mkstemp(spoolfile);
    if (fd < 0) {
        syslog(L_FATAL, "can't create spool file %s %m", spoolfile);
        goto fail;
    }
    close(fd);
    if (rename(tmpspool, spoolfile) < 0) {
        syslog(L_FATAL, "can't rename %s %s %m", tmpspool, spoolfile);
	goto fail;
    }

    /* Article has been spooled. */
    free(tmpspool);
    free(spoolfile);
    return NULL;

 fail:
    if (tmpspool != NULL)
        free(tmpspool);
    if (spoolfile != NULL)
        free(spoolfile);
    return CANTSPOOL;
}


/*
**  Spool article to temp file.
*/
static const char *
Spoolit(char *article, char *err)
{
    return SpoolitTo(article, err, innconf->pathincoming);
}


static char *
Towire(char *p)
{
    char	*q, *r, *s;
    int		curlen, len = BIG_BUFFER;

    for (r = p, q = s = xmalloc(len); *r != '\0' ;) {
	curlen = q - s;
	if (curlen + 3 > len) {
	    len += BIG_BUFFER;
            s = xrealloc(s, len);
	    q = s + curlen;
	}
	if (*r == '\n') {
	    if (r > p) {
		if (*(r - 1) != '\r')
		    *q++ = '\r';
	    } else {
		/* This should not happen. */
		free(s);
		return NULL;
	    }
	}
	*q++ = *r++;
    }
    curlen = q - s;
    if (curlen + 1 > len) {
	len++;
        s = xrealloc(s, len);
	q = s + curlen;
    }
    *q = '\0';
    return s;
}


/*
**  The main function which handles POST and IHAVE.
*/
const char *
ARTpost(char *article, char *idbuff, bool *permanent)
{
    static char	CANTSEND[] = "Can't send %s to server, %s";
    int		i;
    char	*p, *q;
    char	*next;
    HEADER	*hp;
    FILE	*ToServer;
    FILE	*FromServer;
    char	buff[NNTP_MAXLEN_COMMAND + 2], frombuf[SMBUF];
    char	*modgroup = NULL;
    const char	*error;
    char	*TrackID;
    char	*DirTrackID;
    FILE	*ftd;

    /* Assume errors are permanent, until we discover otherwise. */
    *permanent = true;

    /* Set up the other headers list. */
    if (OtherHeaders == NULL) {
	OtherSize = HEADER_DELTA;
	OtherHeaders = xmalloc(OtherSize * sizeof(char *));
    }

    /* Basic processing. */
    OtherCount = 0;
    for (hp = Table; hp < ARRAY_END(Table); hp++) {
	hp->Size = strlen(hp->Name);
	hp->Value = hp->Body = NULL;
    }
    if ((article = StripOffHeaders(article)) == NULL)
	return Error;
    for (i = 0, p = article; p; i++, p = next + 1)
	if ((next = strchr(p, '\n')) == NULL)
	    break;
    if (PERMaccessconf->checkincludedtext) {
	if ((error = CheckIncludedText(article, i)) != NULL)
		return error;
    }
    if ((error = ProcessHeaders(idbuff)) != NULL)
	return error;
    if (i == 0 && HDR(HDR__CONTROL) == NULL)
	return "Article is empty";

    if ((error = ValidNewsgroups(HDR(HDR__NEWSGROUPS), &modgroup)) != NULL)
	return error;

    strlcpy(frombuf, HDR(HDR__FROM), sizeof(frombuf));
    for (i = 0, p = frombuf;p < frombuf + sizeof(frombuf);)
	if ((p = strchr(p, '\n')) == NULL)
	    break;
	else
	    *p++ = ' ';
    HeaderCleanFrom(frombuf);
    p = strchr(frombuf, '@');
    if (p) {
	strlcpy(frombuf, p+1, sizeof(frombuf));
	p = strrchr(frombuf, '.');
	if (!p) {
	    if (modgroup)
		free(modgroup);
	    return "From: address not in Internet syntax";
	}
    }
    else {
	if (modgroup)
	    free(modgroup);
	return "From: address not in Internet syntax";
    }
    if ((p = HDR(HDR__FOLLOWUPTO)) != NULL
     && strcmp(p, "poster") != 0
     && (error = ValidNewsgroups(p, (char **)NULL)) != NULL) {
	if (modgroup)
	    free(modgroup);
	return error;
    }
    if ((PERMaccessconf->localmaxartsize != 0) &&
		(strlen(article) > PERMaccessconf->localmaxartsize)) {
        snprintf(Error, sizeof(Error),
                 "Article is bigger than local limit of %lu bytes\n",
                 PERMaccessconf->localmaxartsize);
        if (modgroup)
            free(modgroup);
        return Error;
    }

#if defined(DO_PERL)
    /* Calls the Perl subroutine for headers management. */
    p = PERMaccessconf->nnrpdperlfilter ? HandleHeaders(article) : NULL;
    if (p != NULL) {
        char SDir[255];

	if (idbuff) {
	    if (modgroup)
		snprintf(idbuff, SMBUF, "(mailed to moderator for %s)",
                         modgroup);
	    else
		strlcpy(idbuff, HDR(HDR__MESSAGEID), SMBUF);
	}
	if (strncmp(p, "DROP", 4) == 0) {
	    syslog(L_NOTICE, "%s post failed %s", Client.host, p);
	    if (modgroup)
		free(modgroup);
	    return NULL;
	}
	else if (strncmp(p, "SPOOL", 5) == 0) {
	    syslog(L_NOTICE, "%s post failed %s", Client.host, p);
	    strlcpy(SDir, innconf->pathincoming, sizeof(SDir));
	    if (modgroup) {
		free(modgroup);
                strlcat(SDir, "/spam/mod", sizeof(SDir));
		return SpoolitTo(article, p, SDir);
	    }
	    else {
                strlcat(SDir, "/spam", sizeof(SDir));
		return SpoolitTo(article, p, SDir);
            }
	}
        else if (strncmp(p, "CLOSE", 5) == 0) {
            syslog(L_NOTICE, "%s post failed %s", Client.host, p);
            Reply("%d NNTP server unavailable; no posting\r\n",
                  NNTP_FAIL_TERMINATING);
            POSTrejected++;
            ExitWithStats(1, true);
        }
	else
	{
	    if (modgroup)
		free(modgroup);
	    return p;
	}
    }
#endif /* defined(DO_PERL) */

    /* Handle mailing to moderated groups. */

    if (modgroup) {
	if (idbuff != NULL) {
	    const char *retstr;
	    retstr = MailArticle(modgroup, article);
	    strlcpy (idbuff, "(mailed to moderator)", SMBUF);
	    return retstr;
	}
	return MailArticle(modgroup, article);
    }

    if (idbuff)
	strlcpy(idbuff, HDR(HDR__MESSAGEID), SMBUF);

    if (PERMaccessconf->spoolfirst)
	return Spoolit(article, Error);

    if (Offlinepost)
         return Spoolit(article,Error);

    /* Open a local connection to the server. */
    if (PERMaccessconf->nnrpdposthost != NULL)
	i = NNTPconnect(PERMaccessconf->nnrpdposthost,
                        PERMaccessconf->nnrpdpostport, &FromServer,
                        &ToServer, buff, sizeof(buff));
    else {
#if	defined(HAVE_UNIX_DOMAIN_SOCKETS)
	i = NNTPlocalopen(&FromServer, &ToServer, buff, sizeof(buff));
#else
	i = NNTPremoteopen(innconf->port, &FromServer, &ToServer, buff,
                           sizeof(buff));
#endif	/* defined(HAVE_UNIX_DOMAIN_SOCKETS) */
    }

    /* If we cannot open the connection, initialize the error message and
     * attempt to recover from this by spooling it locally. */
    if (i < 0) {
	if (buff[0])
	    strlcpy(Error, buff, sizeof(Error));
	else
	    snprintf(Error, sizeof(Error), CANTSEND, "connect request",
                     strerror(errno));
        return Spoolit(article,Error);
    }

    if (Tracing)
	syslog(L_TRACE, "%s post_connect %s",
	    Client.host, PERMaccessconf->nnrpdposthost ? PERMaccessconf->nnrpdposthost : "localhost");

    /* The code below ignores too many return values for my tastes.  At least
     * they are all inside cases that are most likely never going to happen --
     * for example, if the server crashes. */

    /* Offer article to server. */
    i = OfferArticle(buff, (int)sizeof buff, FromServer, ToServer);
    if (i == NNTP_FAIL_AUTH_NEEDED) {
        /* Send authorization. */
        if (NNTPsendpassword(PERMaccessconf->nnrpdposthost, FromServer, ToServer) < 0) {
            snprintf(Error, sizeof(Error), "Can't authorize with %s",
                     PERMaccessconf->nnrpdposthost ? PERMaccessconf->nnrpdposthost : "innd");
            return Spoolit(article,Error);
        }
        i = OfferArticle(buff, (int)sizeof buff, FromServer, ToServer);
    }
    if (i != NNTP_CONT_IHAVE) {
        strlcpy(Error, buff, sizeof(Error));
        SendQuit(FromServer, ToServer);
	if (i == NNTP_FAIL_IHAVE_REJECT || i == NNTP_FAIL_IHAVE_DEFER) {
	    *permanent = false;
	}
        /* As the syntax of the IHAVE command sent by nnrpd is valid,
         * the only valid case of response is a refusal. */
        if (i != NNTP_FAIL_IHAVE_REFUSE)
            return Spoolit(article, Error);
        return Error;
    }
    if (Tracing)
	syslog(L_TRACE, "%s post starting", Client.host);

    /* Write the headers and a blank line. */
    for (hp = Table; hp < ARRAY_END(Table); hp++)
	if (hp->Value) {
            q = xstrndup(hp->Value, hp->Body - hp->Value + hp->Len);
	    if (strchr(q, '\n') != NULL) {
		if ((p = Towire(q)) != NULL) {
		    /* There is no white space, if hp->Value and hp->Body are the same. */
		    if (*hp->Value == ' ' || *hp->Value == '\t')
			fprintf(ToServer, "%s:%s\r\n", hp->Name, p);
		    else
			fprintf(ToServer, "%s: %s\r\n", hp->Name, p);
		    free(p);
		}
	    } else {
		/* There is no white space, if hp->Value and hp->Body are the same. */
		if (*hp->Value == ' ' || *hp->Value == '\t')
		    fprintf(ToServer, "%s:%s\r\n", hp->Name, q);
		else
		    fprintf(ToServer, "%s: %s\r\n", hp->Name, q);
	    }
	    free(q);
	}
    for (i = 0; i < OtherCount; i++) {
	if (strchr(OtherHeaders[i], '\n') != NULL) {
	    if ((p = Towire(OtherHeaders[i])) != NULL) {
		fprintf(ToServer, "%s\r\n", p);
		free(p);
	    }
	} else {
	    fprintf(ToServer, "%s\r\n", OtherHeaders[i]);
	}
    }
    fprintf(ToServer, "\r\n");
    if (FLUSH_ERROR(ToServer)) {
	snprintf(Error, sizeof(Error), CANTSEND, "headers", strerror(errno));
	fclose(FromServer);
	fclose(ToServer);
	return Spoolit(article, Error);
    }

    /* Send the article, get the server's reply. */
    if (NNTPsendarticle(article, ToServer, true) < 0
     || fgets(buff, sizeof buff, FromServer) == NULL) {
	snprintf(Error, sizeof(Error), CANTSEND, "article", strerror(errno));
	fclose(FromServer);
	fclose(ToServer);
	return Spoolit(article, Error);
    }

    /* Did the server want the article? */
    if ((i = atoi(buff)) != NNTP_OK_IHAVE) {
	strlcpy(Error, buff, sizeof(Error));
	SendQuit(FromServer, ToServer);
	syslog(L_TRACE, "%s server rejects %s from %s", Client.host, HDR(HDR__MESSAGEID), HDR(HDR__PATH));
	if (i != NNTP_FAIL_IHAVE_REJECT && i != NNTP_FAIL_IHAVE_REFUSE)
	    return Spoolit(article, Error);
	if (i == NNTP_FAIL_IHAVE_REJECT || i == NNTP_FAIL_IHAVE_DEFER) {
	    *permanent = false;
	}
	return Error;
    }

    /* Send a quit and close down. */
    SendQuit(FromServer, ToServer);

    /* Tracking. */
    if (PERMaccessconf->readertrack) {
        TrackID = concat(innconf->pathlog, "/trackposts/track.",
                         HDR(HDR__MESSAGEID), (char *) 0);
	if ((ftd = fopen(TrackID,"w")) == NULL) {
	    DirTrackID = concatpath(innconf->pathlog, "trackposts");
	    MakeDirectory(DirTrackID, false);
	    free(DirTrackID);
	}
	if (ftd == NULL && (ftd = fopen(TrackID,"w")) == NULL) {
	    syslog(L_ERROR, "%s (%s) open %s: %m",
		Client.host, Username, TrackID);
	    free(TrackID);
	    return NULL;
	}
	for (hp = Table; hp < ARRAY_END(Table); hp++)
	    if (hp->Value) {
                q = xstrndup(hp->Value, hp->Body - hp->Value + hp->Len);
		if (strchr(q, '\n') != NULL) {
		    if ((p = Towire(q)) != NULL) {
			/* There is no white space, if hp->Value and hp->Body are the same. */
			if (*hp->Value == ' ' || *hp->Value == '\t')
			    fprintf(ftd, "%s:%s\r\n", hp->Name, p);
			else
			    fprintf(ftd, "%s: %s\r\n", hp->Name, p);
			free(p);
		    }
		} else {
		    /* There is no white space, if hp->Value and hp->Body are the same. */
		    if (*hp->Value == ' ' || *hp->Value == '\t')
			fprintf(ftd, "%s:%s\r\n", hp->Name, q);
		    else
			fprintf(ftd, "%s: %s\r\n", hp->Name, q);
		}
		free(q);
	    }
	for (i = 0 ; i < OtherCount ; i++) {
	    if (strchr(OtherHeaders[i], '\n') != NULL) {
	        if ((p = Towire(OtherHeaders[i])) != NULL) {
		    fprintf(ftd, "%s\r\n", p);
		    free(p);
	        }
	    } else {
	        fprintf(ftd, "%s\r\n", OtherHeaders[i]);
	    }
	}
	fprintf(ftd,"\r\n");
	NNTPsendarticle(article, ftd, true);
	if (fclose(ftd) != EOF) {
	    syslog(L_NOTICE, "%s (%s) posttrack ok %s",
		Client.host, Username, TrackID);
	    if (LLOGenable)
		fprintf(locallog, "%s (%s) posttrack ok %s\n",
		    Client.host, Username, TrackID);
	} else {
	    syslog(L_ERROR, "%s (%s) posttrack error 2 %s",
		Client.host, Username, TrackID);
	}
	free(TrackID);
    }

    return NULL;
}

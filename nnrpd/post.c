/*  $Revision$
**
**  Check article, send it to the local server.
*/
#include "config.h"
#include "clibrary.h"

#include "nnrpd.h"
#include "ov.h"
#include "post.h"

#define FLUSH_ERROR(F)		(fflush((F)) == EOF || ferror((F)))
#define HEADER_DELTA		20

extern int LLOGenable;

static char     *tmpPtr ;
static char	Error[SMBUF];
static char	NGSEPS[] = NG_SEPARATOR;
char	**OtherHeaders;
int	OtherCount;
bool   HeadersModified;
static int	OtherSize;
static bool	WasMailed;
static const char * const BadDistribs[] = {
    BAD_DISTRIBS
};

HEADER	Table[] = {
    /* 	Name			Canset	Type	Size  Value */
    {	"Path",			TRUE,	HTstd,  0,    NULL,    NULL, 0 },
#define HDR__PATH	      0
    {	"From",			TRUE,	HTreq,  0,    NULL,    NULL, 0 },
#define HDR__FROM	      1
    {	"Newsgroups",	 	TRUE,	HTreq,  0,    NULL,    NULL, 0 },
#define HDR__NEWSGROUPS	      2
    {	"Subject",		TRUE,	HTreq,  0,    NULL,    NULL, 0 },
#define HDR__SUBJECT	      3
    {	"Control",		TRUE,	HTstd,  0,    NULL,    NULL, 0 },
#define HDR__CONTROL	      4
    {	"Supersedes",		TRUE,	HTstd,  0,    NULL,    NULL, 0 },
    {	"Followup-To",		TRUE,	HTstd,  0,    NULL,    NULL, 0 },
#define HDR__FOLLOWUPTO	      6
    {	"Date",			TRUE,	HTstd,  0,    NULL,    NULL, 0 },
#define HDR__DATE	      7
    {	"Organization",		TRUE,	HTstd,  0,    NULL,    NULL, 0 },
#define HDR__ORGANIZATION     8
    {	"Lines",		TRUE,	HTstd,  0,    NULL,    NULL, 0 },
#define HDR__LINES	      9
    {	"Sender",		TRUE,	HTstd,  0,    NULL,    NULL, 0 },
#define HDR__SENDER	     10
    {	"Approved",		TRUE,	HTstd,  0,    NULL,    NULL, 0 },
#define HDR__APPROVED	     11
    {	"Distribution",		TRUE,	HTstd,  0,    NULL,    NULL, 0 },
#define HDR__DISTRIBUTION    12
    {	"Expires",		TRUE,	HTstd,  0,    NULL,    NULL, 0 },
#define HDR__EXPIRES	     13
    {	"Message-ID",		TRUE,	HTstd,  0,    NULL,    NULL, 0 },
#define HDR__MESSAGEID	     14
    {	"References",		TRUE,	HTstd,  0,    NULL,    NULL, 0 },
    {	"Reply-To",		TRUE,	HTstd,  0,    NULL,    NULL, 0 },
    {	"NNTP-Posting-Host",	FALSE,	HTstd,  0,    NULL,    NULL, 0 },
#define HDR__NNTPPOSTINGHOST 17
    {	"Mime-Version",		TRUE,	HTstd,  0,    NULL,    NULL, 0 },
    {	"Content-Type",		TRUE,	HTstd,  0,    NULL,    NULL, 0 },
    {	"Content-Transfer-Encoding", TRUE, HTstd,  0,    NULL,    NULL, 0 },
    {   "X-Trace",              FALSE, HTstd,  0,    NULL,    NULL, 0 },
#define HDR__XTRACE          21
    {   "X-Complaints-To",	FALSE, HTstd,  0,    NULL,    NULL, 0 },
#define HDR__XCOMPLAINTSTO   22
    {   "NNTP-Posting-Date",	FALSE, HTstd,  0,    NULL,    NULL, 0 },
#define HDR__NNTPPOSTINGDATE 23
    {	"Xref",			FALSE,	HTstd,  0,    NULL,    NULL, 0 },
    {	"Summary",		TRUE,	HTstd,  0,    NULL,    NULL, 0 },
    {	"Keywords",		TRUE,	HTstd,  0,    NULL,    NULL, 0 },
    {	"Date-Received",	FALSE,	HTobs,  0,    NULL,    NULL, 0 },
    {	"Received",		FALSE,	HTobs,  0,    NULL,    NULL, 0 },
    {	"Posted",		FALSE,	HTobs,  0,    NULL,    NULL, 0 },
    {	"Posting-Version",	FALSE,	HTobs,  0,    NULL,    NULL, 0 },
    {	"Relay-Version",	FALSE,	HTobs,  0,    NULL,    NULL, 0 },
    {   "Cc",			TRUE, HTstd,  0,    NULL,    NULL, 0 },
#define HDR__CC		     32
    {   "Bcc",			TRUE, HTstd,  0,    NULL,    NULL, 0 },
#define HDR__BCC		    33
    {   "To",			TRUE, HTstd,  0,    NULL,    NULL, 0 },
#define HDR__TO		     34
};

HEADER *EndOfTable = ENDOF(Table);



/* Join() and MaxLength() are taken from innd.c */
/*
**  Turn any \r or \n in text into spaces.  Used to splice back multi-line
**  headers into a single line.
*/
static char *
Join(char	*text)
{
    char	*p;

    for (p = text; *p; p++)
	if (*p == '\n' || *p == '\r')
	    *p = ' ';
    return text;
}

/*
**  Return a short name that won't overrun our bufer or syslog's buffer.
**  q should either be p, or point into p where the "interesting" part is.
*/
static char *
MaxLength(char *p, char *q)
{
    static char		buff[80];
    unsigned int	i;

    /* Already short enough? */
    i = strlen(p);
    if (i < sizeof buff - 1)
	return Join(p);

    /* Don't want casts to unsigned to go horribly wrong. */
    if (q < p || q > p + i)
	q = p;

    /* Simple case of just want the begining? */
    if ((size_t)(q - p) < sizeof buff - 4) {
	(void)strncpy(buff, p, sizeof buff - 4);
	(void)strcpy(&buff[sizeof buff - 4], "...");
    } else if ((p + i) - q < 10) {
	/* Is getting last 10 characters good enough? */
	(void)strncpy(buff, p, sizeof buff - 14);
	(void)strcpy(&buff[sizeof buff - 14], "...");
	(void)strcpy(&buff[sizeof buff - 11], &p[i - 10]);
    } else {
	/* Not in last 10 bytes, so use double elipses. */
	(void)strncpy(buff, p, sizeof buff - 17);
	(void)strcpy(&buff[sizeof buff - 17], "...");
	(void)strncpy(&buff[sizeof buff - 14], &q[-5], 10);
	(void)strcpy(&buff[sizeof buff - 4], "...");
    }
    return Join(buff);
}
/*
**  Trim trailing spaces, return pointer to first non-space char.
*/
static int
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
	for (hp = Table; hp < ENDOF(Table); hp++) {
	    if (c == hp->Name[0]
	     && p[hp->Size] == ':'
	     && caseEQn(p, hp->Name, hp->Size)) {
		if (hp->Type == HTobs) {
		    snprintf(Error, sizeof(Error), "Obsolete \"%s\" header",
                             hp->Name);
		    return NULL;
		}
		if (hp->Value) {
		    snprintf(Error, sizeof(Error), "Duplicate \"%s\" header",
                             hp->Name);
		    return NULL;
		}
		hp->Value = &p[hp->Size + 1];
		/* '\r\n' is replaced with '\n', and unnecessary to consider
		   '\r' */
		for (q = &p[hp->Size + 1]; ISWHITE(*q) || *q == '\n'; q++)
		    continue;
		hp->Body = q;
		break;
	    }
	}

	/* No; add it to the set of other headers. */
	if (hp == ENDOF(Table)) {
	    if (OtherCount >= OtherSize - 1) {
		OtherSize += HEADER_DELTA;
		RENEW(OtherHeaders, char*, OtherSize);
	    }
	    OtherHeaders[OtherCount++] = p;
	}

	/* Get start of next header; if it's a blank line, we hit the end. */
	if ((p = NextHeader(p)) == NULL) {
	    (void)strcpy(Error, "Article has no body -- just headers");
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

    if (EQ(ctrl, "cancel")) {
	for (q = p + 1; ISWHITE(*q); q++)
	    continue;
	if (*q == '\0')
	    return "Message-ID missing in cancel";
    }
    else if (EQ(ctrl, "sendsys") || EQ(ctrl, "senduuname")
	  || EQ(ctrl, "version") || EQ(ctrl, "checkgroups")
	  || EQ(ctrl, "ihave") || EQ(ctrl, "sendme")
	  || EQ(ctrl, "newgroup") || EQ(ctrl, "rmgroup"))
	/* SUPPRESS 530 *//* Empty body for statement */
	;
    else {
	snprintf(Error, sizeof(Error),
                 "\"%s\" is not a valid control message",
                 MaxLength(ctrl,ctrl));
	return Error;
    }
    *p = save;
    return NULL;
}


/*
**  Check the Distribution header, and exit on error.
*/
static const char *
CheckDistribution(char *p)
{
    static char	SEPS[] = " \t,";
    const char * const *dp;

    if ((p = strtok(p, SEPS)) == NULL)
	return "Can't parse Distribution line.";
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
ProcessHeaders(int linecount, char *idbuff, bool ihave)
{
    static char		MONTHS[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    static char		datebuff[40];
    static char		localdatebuff[40];
    static char		orgbuff[SMBUF];
    static char		linebuff[40];
    static char		tracebuff[SMBUF];
    static char 	complaintsbuff[SMBUF];
    static char		sendbuff[SMBUF];
    static char		*newpath = NULL;
    HEADER		*hp;
    char		*p;
    time_t		t;
    struct tm		*gmt;
    TIMEINFO		Now;
    const char          *error;
    pid_t               pid;
    bool		addvirtual = FALSE;

    /* Various things need Now to be set. */
    if (GetTimeInfo(&Now) < 0) {
        snprintf(Error, sizeof(Error), "Can't get the time, %s",
                 strerror(errno));
        return Error;
    }

    if (!ihave) {
	/* Do some preliminary fix-ups. */
	for (hp = Table; hp < ENDOF(Table); hp++) {
	    if (!hp->CanSet && hp->Value) {
		snprintf(Error, sizeof(Error),
                         "Can't set system \"%s\" header", hp->Name);
		return Error;
	    }
	    if (hp->Value) {
		hp->Len = TrimSpaces(hp->Value);
		if (hp->Len == 0)
		    hp->Value = hp->Body = NULL;
	    }
	}
    }

    if (PERMaccessconf->nnrpdauthsender) {
	/* If authorized, add the header based on our info.  If not
         * authorized, zap the Sender so we don't put out unauthenticated
         * data. */
	if (PERMauthorized) {
	    if (PERMuser[0] == '\0') {
		snprintf(sendbuff, sizeof(sendbuff), "%s@%s", "UNKNOWN",
                         ClientHost);
	    } else {
		if ((p = strchr(PERMuser, '@')) == NULL) {
		    snprintf(sendbuff, sizeof(sendbuff), "%s@%s", PERMuser,
                             ClientHost);
		} else {
		    snprintf(sendbuff, sizeof(sendbuff), "%s", PERMuser);
		}
	    }
	    HDR_SET(HDR__SENDER, sendbuff);
	}
	else
	    HDR_SET(HDR__SENDER, NULL);
    }

    /* Set Date.  datebuff is used later for NNTP-Posting-Date, so we have
       to set it and it has to be the UTC date. */
    if (!makedate(-1, false, datebuff, sizeof(datebuff)))
        return "Can't generate date header";
    if (HDR(HDR__DATE) == NULL) {
	if (ihave)
	    return "Missing \"Date\" header";
        if (PERMaccessconf->localtime) {
            if (!makedate(-1, true, localdatebuff, sizeof(localdatebuff)))
                return "Can't generate local date header";
	    HDR_SET(HDR__DATE, localdatebuff);
	} else {
	    HDR_SET(HDR__DATE, datebuff);
	}
    } else {
	if ((t = parsedate(HDR(HDR__DATE), &Now)) == -1)
	    return "Can't parse \"Date\" header";
	if (t > Now.time + DATE_FUZZ)
	    return "Article posted in the future";
    }

    /* Newsgroups are checked later. */

    if (HDR(HDR__CONTROL)) {
	if ((error = CheckControl(HDR(HDR__CONTROL))) != NULL)
	    return error;
    } else {
	p = HDR(HDR__SUBJECT);
	if (p == NULL)
	    return "Required \"Subject\" header is missing";
        if (EQn(p, "cmsg ", 5)) {
            HDR_SET(HDR__CONTROL, p + 5);
            if ((error = CheckControl(HDR(HDR__CONTROL))) != NULL)
                return error;
        }
    }

    /* Set Message-ID */
    if (HDR(HDR__MESSAGEID) == NULL) {
	if (ihave)
	    return "Missing \"Message-ID\" header";
	HDR_SET(HDR__MESSAGEID, idbuff);
    }

    /* Set Path */
    if (HDR(HDR__PATH) == NULL) {
 	if (ihave)
	    return "Missing \"Path\" header";
	/* Note that innd will put host name here for us. */
	HDR_SET(HDR__PATH, PATHMASTER);
	if (VirtualPathlen > 0)
	    addvirtual = TRUE;
    } else if (PERMaccessconf->strippath) {
	/* Here's where to do Path changes for new Posts. */
	if ((p = strrchr(HDR(HDR__PATH), '!')) != NULL) {
	    p++;
	    if (*p == '\0') {
		HDR_SET(HDR__PATH, PATHMASTER);
		if (VirtualPathlen > 0)
		    addvirtual = TRUE;
	    } else {
		HDR_SET(HDR__PATH, p);
		if ((VirtualPathlen > 0) &&
		    !EQ(p, PERMaccessconf->pathhost))
		    addvirtual = TRUE;
	    }
	} else if (VirtualPathlen > 0)
	    addvirtual = TRUE;
    } else {
	if ((VirtualPathlen > 0) &&
	    (p = strchr(HDR(HDR__PATH), '!')) != NULL) {
	    *p = '\0';
	    if (!EQ(HDR(HDR__PATH), PERMaccessconf->pathhost))
		addvirtual = TRUE;
	    *p = '!';
	} else if (VirtualPathlen > 0)
	    addvirtual = TRUE;
    }
    if (addvirtual) {
	if (newpath != NULL)
	    DISPOSE(newpath);
        newpath = concat(VirtualPath, HDR(HDR__PATH), (char *) 0);
	HDR_SET(HDR__PATH, newpath);
    }
    

    /* Reply-To; left alone. */
    /* Sender; set above. */

    /* Check Expires. */
    if (HDR(HDR__EXPIRES) && parsedate(HDR(HDR__EXPIRES), &Now) == -1)
	return "Can't parse \"Expires\" header";

    /* References; left alone. */
    /* Control; checked above. */

    /* Distribution. */
    if ((p = HDR(HDR__DISTRIBUTION)) != NULL) {
	p = COPY(p);
	error = CheckDistribution(p);
	DISPOSE(p);
	if (error != NULL)
	    return error;
    }

    /* Set Organization */
    if (!ihave && HDR(HDR__ORGANIZATION) == NULL
     && (p = PERMaccessconf->organization) != NULL) {
	(void)strcpy(orgbuff, p);
	HDR_SET(HDR__ORGANIZATION, orgbuff);
    }

    /* Keywords; left alone. */
    /* Summary; left alone. */
    /* Approved; left alone. */

    /* Set Lines */
    if (!ihave) {
	snprintf(linebuff, sizeof(linebuff), "%d", linecount);
	HDR_SET(HDR__LINES, linebuff);
    }

    /* Supersedes; left alone. */

    /* NNTP-Posting host; set. */
    if (!ihave && PERMaccessconf->addnntppostinghost) 
    HDR_SET(HDR__NNTPPOSTINGHOST, ClientHost);
    /* NNTP-Posting-Date - not in RFC (yet) */
    if (!ihave && PERMaccessconf->addnntppostingdate)
    HDR_SET(HDR__NNTPPOSTINGDATE, datebuff);

    /* X-Trace; set */
    t = time((time_t *)NULL) ;
    pid = (long) getpid() ;
    if ((gmt = gmtime(&Now.time)) == NULL)
	return "Can't get the time";
    if (VirtualPathlen > 0)
	p = PERMaccessconf->domain;
    else
	if ((p = GetFQDN(PERMaccessconf->domain)) == NULL)
	    p = "unknown";
    snprintf(tracebuff, sizeof(tracebuff),
             "%s %ld %ld %s (%d %3.3s %d %02d:%02d:%02d GMT)",
             p, (long) t, (long) pid, ClientIpString,
             gmt->tm_mday, &MONTHS[3 * gmt->tm_mon], 1900 + gmt->tm_year,
             gmt->tm_hour, gmt->tm_min, gmt->tm_sec);
    HDR_SET(HDR__XTRACE, tracebuff);

    /* X-Complaints-To; set */
    if ((p = PERMaccessconf->complaints) != NULL)
	snprintf (complaintsbuff, sizeof(complaintsbuff), "%s", p);
    else {
	static const char newsmaster[] = NEWSMASTER;

	if ((p = PERMaccessconf->fromhost) != NULL && strchr(newsmaster, '@') == NULL)
	    snprintf (complaintsbuff, sizeof(complaintsbuff), "%s@%s",
                      newsmaster, p);
	else
	    snprintf (complaintsbuff, sizeof(complaintsbuff), "%s",
                      newsmaster);
    }
    HDR_SET(HDR__XCOMPLAINTSTO, complaintsbuff);

    /* Clear out some headers that should not be here */
    if (!ihave && PERMaccessconf->strippostcc) {
	HDR_SET(HDR__CC, NULL);
	HDR_SET(HDR__BCC, NULL);
	HDR_SET(HDR__TO, NULL);
    }
    /* Now make sure everything is there. */
    for (hp = Table; hp < ENDOF(Table); hp++)
	if (hp->Type == HTreq && hp->Value == NULL) {
	    snprintf(Error, sizeof(Error),
                     "Required \"%s\" header is missing", hp->Name);
	    return Error;
	}

    return NULL;
}


/*
**  See if the user has more included text than new text.  Simple-minded,
**  but reasonably effective for catching neophyte's mistakes.  Son-of-1036
**  says:
**
**      NOTE: While encouraging trimming is desirable, the 50% rule imposed
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
	DISPOSE(group);  
	return Error;
    }
    DISPOSE(group);

    /* Now build up the command (ignore format/argument mismatch errors,
     * in case %s isn't in inconf->mta) and send the headers. */
    if ((mta = innconf->mta) == NULL)
	return "Can't start mailer - mta not set";
    snprintf(buff, sizeof(buff), innconf->mta, address);
    if ((F = popen(buff, "w")) == NULL)
	return "Can't start mailer";
    (void)fprintf(F, "To: %s\n", address);
    if (FLUSH_ERROR(F)) {
	(void)pclose(F);
	return CANTSEND;
    }

    /* Write the headers, a blank line, then the article. */
    for (hp = Table; hp < ENDOF(Table); hp++)
	if (hp->Value) {
	    (void)fprintf(F, "%s: %s\n", hp->Name, hp->Value);
	    if (FLUSH_ERROR(F)) {
		(void)pclose(F);
		return CANTSEND;
	    }
	}
    for (i = 0; i < OtherCount; i++) {
	(void)fprintf(F, "%s\n", OtherHeaders[i]);
	if (FLUSH_ERROR(F)) {
	    (void)pclose(F);
	    return CANTSEND;
	}
    }
    (void)fprintf(F, "\n");
    i = strlen(article);
    if (fwrite(article, 1, i, F) != (size_t)i)
	return "Can't send article";
    if (FLUSH_ERROR(F)) {
	(void)pclose(F);
	return CANTSEND;
    }
    i = pclose(F);
    if (i) {
	snprintf(Error, sizeof(Error), "Mailer exited with status %d -- %s",
                 i, "Article might not have been mailed");
	return Error;
    }
    WasMailed = TRUE;
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

    p = HDR(HDR__CONTROL);
    IsNewgroup = p && EQn(p, "newgroup", 8);
    groups = COPY(hdr);
    if ((p = strtok(groups, NGSEPS)) == NULL)
	return "Can't parse newsgroups line";

    /* Don't mail article if just checking Followup-To line. */
    approved = HDR(HDR__APPROVED) != NULL || modgroup == NULL;

    Error[0] = '\0';
    FoundOne = FALSE;
    h = DDstart((FILE *)NULL, (FILE *)NULL);
    do {
	if (innconf->mergetogroups && p[0] == 't' && p[1] == 'o' && p[2] == '.')
	    p = "to";
        if (PERMspecified) {
	    grplist[0] = p;
	    grplist[1] = NULL;
	    if (!PERMmatch(PERMpostlist, grplist)) {
		snprintf(Error, sizeof(Error),
                         "You are not allowed to post to %s\r\n", p);
	    }
        }
	if (!OVgroupstats(p, NULL, NULL, NULL, &flag))
	    continue;
	FoundOne = TRUE;
	DDcheck(h, p);
	switch (flag) {
	case NF_FLAG_OK:
#ifdef DO_PYTHON
	if (innconf->nnrppythonauth) {
	    char    *reply;

	    /* Authorize user at a Python authorization module */
	    if (PY_authorize(ClientHost, ClientIpString, ServerHost, PERMuser, p, TRUE, &reply) < 0) {
	        syslog(L_NOTICE, "PY_authorize(): authorization skipped due to no Python authorization method defined.");
	    } else {
	        if (reply != NULL) {
		    syslog(L_TRACE, "PY_authorize() returned a refuse string for user %s at %s who wants to read %s: %s", PERMuser, ClientHost, p, reply);
		    snprintf(Error, sizeof(Error), "%s\r\n", reply);
		    break;
		}
	    }
	}
#endif /* DO_PYTHON */
	    break;
	case NF_FLAG_MODERATED:
	    if (approved && !PERMaccessconf->allowapproved) {
		snprintf(Error, sizeof(Error),
                         "You are not allowed to approve postings");
	    } else if (!approved && !*modgroup) {
		*modgroup = COPY(p);
	    }
	    break;
	case NF_FLAG_IGNORE:
	case NF_FLAG_NOLOCAL:
	    if (!PERMaccessconf->locpost)
		snprintf(Error, sizeof(Error),
                         "Postings to \"%s\" are not allowed here.", p);
	    break;
	case NF_FLAG_EXCLUDED:
	    /* Do NOT return an error. */
	    break;
	case NF_FLAG_ALIAS:
	    snprintf(Error, sizeof(Error),
                     "The newsgroup \"%s\" has been renamed.\n", p);
	    break;
	}
    } while ((p = strtok((char *)NULL, NGSEPS)) != NULL);
    DISPOSE(groups);

    if (!FoundOne && !IsNewgroup)
	snprintf(Error, sizeof(Error), "No valid newsgroups in \"%s\"",
                 MaxLength(hdr,hdr));
    if (Error[0]) {
        tmpPtr = DDend(h);
	DISPOSE(tmpPtr) ;
	return Error;
    }

    p = DDend(h);
    if (HDR(HDR__DISTRIBUTION) == NULL && *p) {
	(void)strcpy(distbuff, p);
	HDR_SET(HDR__DISTRIBUTION, distbuff);
    }
    DISPOSE(p);
    return NULL;
}


/*
**  Send a quit message to the server, eat its reply.
*/
static void
SendQuit(FILE *FromServer, FILE *ToServer)
{
    char	buff[NNTP_STRLEN];

    (void)fprintf(ToServer, "quit\r\n");
    (void)fflush(ToServer);
    (void)fclose(ToServer);
    (void)fgets(buff, sizeof buff, FromServer);
    (void)fclose(FromServer);
}


/*
**  Offer the article to the server, return its reply.
*/
static int
OfferArticle(char *buff, int buffsize, FILE *FromServer, FILE *ToServer)
{
    static char		CANTSEND[] = "Can't send %s to server, %s";

    (void)fprintf(ToServer, "ihave %s\r\n", HDR(HDR__MESSAGEID));
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
    static char	CANTSPOOL[NNTP_STRLEN+2];
    HEADER *hp;
    FILE *F = NULL;
    int	i, fd;
    char *tmpspool = NULL;
    char *spoolfile = NULL;
    char *q;

    /* Initialize the returned error message */
    snprintf(CANTSPOOL, sizeof(CANTSPOOL),
             "%s and can't write text to local spool file", err);

    /* Try to write it to the spool dir. */
    tmpspool = concatpath(SpoolDir, ".XXXXXX");
    fd = mkstemp(tmpspool);
    if (fd < 0) {
        syslog(L_FATAL, "cant create temporary spool file %s %m", tmpspool);
        goto fail;
    }
    F = fdopen(fd, "w");
    if (F == NULL) {
        syslog(L_FATAL, "cant open %s %m", tmpspool);
        goto fail;
    }
    fchmod(fileno(F), BATCHFILE_MODE);

    /* Write the headers and a blank line. */
    for (hp = Table; hp < ENDOF(Table); hp++)
	if (hp->Value) {
	    q = NEW(char, hp->Body - hp->Value + hp->Len + 1);
            strncpy(q, hp->Value, hp->Body - hp->Value + hp->Len);
	    *(q + (int)(hp->Body - hp->Value) + hp->Len) = '\0';
	    if (*hp->Value == ' ' || *hp->Value == '\t')
		fprintf(F, "%s:%s\n", hp->Name, q);
	    else
		fprintf(F, "%s: %s\n", hp->Name, q);
	    if (FLUSH_ERROR(F)) {
		fclose(F);
		DISPOSE(q);
                goto fail;
	    }
	    DISPOSE(q);
	}
    for (i = 0; i < OtherCount; i++) {
	fprintf(F, "%s\n", OtherHeaders[i]);
	if (FLUSH_ERROR(F)) {
	    fclose(F);
	    goto fail;
	}
    }
    fprintf(F, "\n");

    /* Write the article body */
    i = strlen(article);
    if (fwrite(article, 1, i, F) != (size_t)i) {
        fclose(F);
        goto fail;
    }

    /* Flush and catch any errors */
    if (fclose(F))
        goto fail;

    /* Rename the spool file to something rnews will pick up. */
    spoolfile = concatpath(SpoolDir, "XXXXXX");
    fd = mkstemp(spoolfile);
    if (fd < 0) {
        syslog(L_FATAL, "cant create spool file %s %m", spoolfile);
        goto fail;
    }
    close(fd);
    if (rename(tmpspool, spoolfile) < 0) {
        syslog(L_FATAL, "cant rename %s %s %m", tmpspool, spoolfile);
	goto fail;
    }

    /* Article has been spooled */
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
 
static char *Towire(char *p) {
    char	*q, *r, *s;
    int		curlen, len = BIG_BUFFER;

    for (r = p, q = s = NEW(char, len); *r != '\0' ;) {
	curlen = q - s;
	if (curlen + 3 > len) {
	    len += BIG_BUFFER;
	    RENEW(s, char, len);
	    q = s + curlen;
	}
	if (*r == '\n') {
	    if (r > p) {
		if (*(r - 1) != '\r')
		    *q++ = '\r';
	    } else {
		/* this should not happen */
		DISPOSE(s);
		return NULL;
	    }
	}
	*q++ = *r++;
    }
    curlen = q - s;
    if (curlen + 1 > len) {
	len++;
	RENEW(s, char, len);
	q = s + curlen;
    }
    *q = '\0';
    return s;
}

const char *
ARTpost(char *article,
	char *idbuff,
	bool ihave,
	bool *permanent)
{
    static char	CANTSEND[] = "Can't send %s to server, %s";
    int		i;
    char	*p, *q;
    char	*next;
    HEADER	*hp;
    FILE	*ToServer;
    FILE	*FromServer;
    char	buff[NNTP_STRLEN + 2], frombuf[SMBUF];
    char	*modgroup = NULL;
    const char	*error;
    char	*TrackID;
    char	*DirTrackID;
    FILE	*ftd;
    char	SDir[255];

    /* Assume errors are permanent, until we discover otherwise */
    *permanent = true;

    /* Set up the other headers list. */
    if (OtherHeaders == NULL) {
	OtherSize = HEADER_DELTA;
	OtherHeaders = NEW(char*, OtherSize);
    }

    /* Basic processing. */
    OtherCount = 0;
    WasMailed = FALSE;
    for (hp = Table; hp < ENDOF(Table); hp++) {
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
    if ((error = ProcessHeaders(i, idbuff, ihave)) != NULL)
	return error;
    if (i == 0 && HDR(HDR__CONTROL) == NULL)
	return "Article is empty";

    if ((error = ValidNewsgroups(HDR(HDR__NEWSGROUPS), &modgroup)) != NULL)
	return error;
    
    strncpy(frombuf, HDR(HDR__FROM), sizeof(frombuf) - 1);
    frombuf[sizeof(frombuf) - 1] = '\0';
    for (i = 0, p = frombuf;p < frombuf + sizeof(frombuf);)
	if ((p = strchr(p, '\n')) == NULL)
	    break;
	else
	    *p++ = ' ';
    HeaderCleanFrom(frombuf);
    p = strchr(frombuf, '@');
    if (p) {
	strcpy(frombuf, p+1);
	p = strrchr(frombuf, '.');
	if (!p) {
	    if (modgroup)
		DISPOSE(modgroup);
	    return "From: address not in Internet syntax";
	}
    }
    else {
	if (modgroup)
	    DISPOSE(modgroup);
	return "From: address not in Internet syntax";
    }
    if ((p = HDR(HDR__FOLLOWUPTO)) != NULL
     && !EQ(p, "poster")
     && (error = ValidNewsgroups(p, (char **)NULL)) != NULL) {
	if (modgroup)
	    DISPOSE(modgroup);
	return error;
    }
    if ((PERMaccessconf->localmaxartsize > 0) &&
		(strlen(article) > (unsigned)PERMaccessconf->localmaxartsize)) {
        snprintf(Error, sizeof(Error),
                 "Article is bigger then local limit of %ld bytes\n",
                 PERMaccessconf->localmaxartsize);
        if (modgroup)
            DISPOSE(modgroup);
        return Error;
    }

#if defined(DO_PERL)
    /* Calls the Perl subroutine for headers management */
    if ((p = (char *)HandleHeaders(article)) != NULL) {
	if (idbuff) {
	    if (modgroup)
		snprintf(idbuff, sizeof(idbuff),
                         "(mailed to moderator for %s)", modgroup);
	    else
		(void)strncpy(idbuff, HDR(HDR__MESSAGEID), SMBUF - 1);
	    idbuff[SMBUF - 1] = '\0';
	}
	if (strncmp(p, "DROP", 4) == 0) {
	    syslog(L_NOTICE, "%s post %s", ClientHost, p);
	    if (modgroup)
		DISPOSE(modgroup);
	    return NULL;
	}
	else if (strncmp(p, "SPOOL", 5) == 0) {
	    syslog(L_NOTICE, "%s post %s", ClientHost, p);
	    strcpy(SDir, innconf->pathincoming);
	    if (modgroup)
	    {
		DISPOSE(modgroup);
		return SpoolitTo(article, p, strcat(SDir,"/spam/mod"));
	    }
	    else
		return SpoolitTo(article, p, strcat(SDir,"/spam"));
	}
	else
	{
	    if (modgroup)
		DISPOSE(modgroup);
	    return p;
	}
    }
#endif /* defined(DO_PERL) */

    /* handle mailing to moderated groups */

    if (modgroup) {
	if (idbuff != NULL) {
	    const char *retstr;
	    retstr = MailArticle(modgroup, article);
	    strcpy (idbuff,"(mailed to moderator)") ;
	    return retstr;
	}
	return MailArticle(modgroup, article);
    }

    if (PERMaccessconf->spoolfirst)
	return Spoolit(article, Error);

    if (Offlinepost)
         return Spoolit(article,Error);

    /* Open a local connection to the server. */
    if (PERMaccessconf->nnrpdposthost != NULL)
	i = NNTPconnect(PERMaccessconf->nnrpdposthost, PERMaccessconf->nnrpdpostport,
					&FromServer, &ToServer, buff);
    else {
#if	defined(HAVE_UNIX_DOMAIN_SOCKETS)
	i = NNTPlocalopen(&FromServer, &ToServer, buff);
#else
	i = NNTPremoteopen(innconf->port, &FromServer,
					&ToServer, buff);
#endif	/* defined(HAVE_UNIX_DOMAIN_SOCKETS) */
    }

    /* If we cannot open the connection, initialize the error message and
     * attempt to recover from this by spooling it locally */
    if (i < 0) {
	if (buff[0])
	    (void)strcpy(Error, buff);
	else
	    snprintf(Error, sizeof(Error), CANTSEND, "connect request",
                     strerror(errno));
        return Spoolit(article,Error);
    }
    if (Tracing)
	syslog(L_TRACE, "%s post_connect %s",
	    ClientHost, PERMaccessconf->nnrpdposthost ? PERMaccessconf->nnrpdposthost : "localhost");

    /* The code below has too many (void) casts for my tastes.  At least
     * they are all inside cases that are most likely never going to
     * happen -- for example, if the server crashes. */

    /* Offer article to server. */
    i = OfferArticle(buff, (int)sizeof buff, FromServer, ToServer);
    if (i == NNTP_AUTH_NEEDED_VAL) {
        /* Send authorization. */
        if (NNTPsendpassword(PERMaccessconf->nnrpdposthost, FromServer, ToServer) < 0) {
            snprintf(Error, sizeof(Error), "Can't authorize with %s",
                     PERMaccessconf->nnrpdposthost ? PERMaccessconf->nnrpdposthost : "innd");
            return Spoolit(article,Error);
        }
        i = OfferArticle(buff, (int)sizeof buff, FromServer, ToServer);
    }
    if (i != NNTP_SENDIT_VAL) {
        (void)strcpy(Error, buff);
        SendQuit(FromServer, ToServer);
	if (i != NNTP_HAVEIT_VAL)
	    return Spoolit(article, Error);
	*permanent = false;
        return Error;
    }
    if (Tracing)
	syslog(L_TRACE, "%s post starting", ClientHost);

    /* Write the headers and a blank line. */
    for (hp = Table; hp < ENDOF(Table); hp++)
	if (hp->Value) {
	    q = NEW(char, hp->Body - hp->Value + hp->Len + 1);
            strncpy(q, hp->Value, hp->Body - hp->Value + hp->Len);
	    *(q + (int)(hp->Body - hp->Value) + hp->Len) = '\0';
	    if (strchr(q, '\n') != NULL) {
		if ((p = Towire(q)) != NULL) {
		    /* there is no white space, if hp->Value and hp->Body is the same */
		    if (*hp->Value == ' ' || *hp->Value == '\t')
			(void)fprintf(ToServer, "%s:%s\r\n", hp->Name, p);
		    else
			(void)fprintf(ToServer, "%s: %s\r\n", hp->Name, p);
		    DISPOSE(p);
		}
	    } else {
		/* there is no white space, if hp->Value and hp->Body is the same */
		if (*hp->Value == ' ' || *hp->Value == '\t')
		    (void)fprintf(ToServer, "%s:%s\r\n", hp->Name, q);
		else
		    (void)fprintf(ToServer, "%s: %s\r\n", hp->Name, q);
	    }
	    DISPOSE(q);
	}
    for (i = 0; i < OtherCount; i++) {
	if (strchr(OtherHeaders[i], '\n') != NULL) {
	    if ((p = Towire(OtherHeaders[i])) != NULL) {
		(void)fprintf(ToServer, "%s\r\n", p);
		DISPOSE(p);
	    }
	} else {
	    (void)fprintf(ToServer, "%s\r\n", OtherHeaders[i]);
	}
    }
    (void)fprintf(ToServer, "\r\n");
    if (FLUSH_ERROR(ToServer)) {
	snprintf(Error, sizeof(Error), CANTSEND, "headers", strerror(errno));
	(void)fclose(FromServer);
	(void)fclose(ToServer);
	return Spoolit(article, Error);
    }

    /* Send the article, get the server's reply. */
    if (NNTPsendarticle(article, ToServer, TRUE) < 0
     || fgets(buff, sizeof buff, FromServer) == NULL) {
	snprintf(Error, sizeof(Error), CANTSEND, "article", strerror(errno));
	(void)fclose(FromServer);
	(void)fclose(ToServer);
	return Spoolit(article, Error);
    }

    /* Did the server want the article? */
    if ((i = atoi(buff)) != NNTP_TOOKIT_VAL) {
	(void)strcpy(Error, buff);
	SendQuit(FromServer, ToServer);
	syslog(L_TRACE, "%s server rejects %s from %s", ClientHost, HDR(HDR__MESSAGEID), HDR(HDR__PATH));
	if (i != NNTP_REJECTIT_VAL && i != NNTP_HAVEIT_VAL)
	    return Spoolit(article, Error);
	*permanent = false;
	return Error;
    }

    /* Send a quit and close down */
    SendQuit(FromServer, ToServer);
    if (idbuff) {
	(void)strncpy(idbuff, HDR(HDR__MESSAGEID), SMBUF - 1);
	idbuff[SMBUF - 1] = '\0';
    }

    /* Tracking */
    if (PERMaccessconf->readertrack) {
        TrackID = concat(innconf->pathlog, "/trackposts/track.",
                         HDR(HDR__MESSAGEID), (char *) 0);
	if ((ftd = fopen(TrackID,"w")) == NULL) {
	    DirTrackID = concatpath(innconf->pathlog, "trackposts");
	    MakeDirectory(DirTrackID, FALSE);
	    DISPOSE(DirTrackID);
	}
	if (ftd == NULL && (ftd = fopen(TrackID,"w")) == NULL) {
	    syslog(L_ERROR, "%s (%s) open %s: %m",
		ClientHost, Username, TrackID);
	    DISPOSE(TrackID);
	    return NULL;
	}
	for (hp = Table; hp < ENDOF(Table); hp++)
	    if (hp->Value) {
		q = NEW(char, hp->Body - hp->Value + hp->Len + 1);
                strncpy(q, hp->Value, hp->Body - hp->Value + hp->Len);
		*(q + (int)(hp->Body - hp->Value) + hp->Len) = '\0';
		if (strchr(q, '\n') != NULL) {
		    if ((p = Towire(q)) != NULL) {
			/* there is no white space, if hp->Value and hp->Body is the same */
			if (*hp->Value == ' ' || *hp->Value == '\t')
			    (void)fprintf(ftd, "%s:%s\r\n", hp->Name, p);
			else
			    (void)fprintf(ftd, "%s: %s\r\n", hp->Name, p);
			DISPOSE(p);
		    }
		} else {
		    /* there is no white space, if hp->Value and hp->Body is the same */
		    if (*hp->Value == ' ' || *hp->Value == '\t')
			(void)fprintf(ftd, "%s:%s\r\n", hp->Name, q);
		    else
			(void)fprintf(ftd, "%s: %s\r\n", hp->Name, q);
		}
		DISPOSE(q);
	    }
	for (i = 0 ; i < OtherCount ; i++) {
	    if (strchr(OtherHeaders[i], '\n') != NULL) {
	        if ((p = Towire(OtherHeaders[i])) != NULL) {
		    (void)fprintf(ftd, "%s\r\n", p);
		    DISPOSE(p);
	        }
	    } else {
	        (void)fprintf(ftd, "%s\r\n", OtherHeaders[i]);
	    }
	}
	(void)fprintf(ftd,"\r\n");
	(void)NNTPsendarticle(article, ftd, TRUE);
	if (fclose(ftd) != EOF) {
	    syslog(L_NOTICE, "%s (%s) posttrack ok %s",
		ClientHost, Username, TrackID);
	    if (LLOGenable)
		fprintf(locallog, "%s (%s) posttrack ok %s\n",
		    ClientHost, Username, TrackID);
	} else {
	    syslog(L_ERROR, "%s (%s) posttrack error 2 %s",
		ClientHost, Username, TrackID);
	}
	DISPOSE(TrackID);
    }

    return NULL;
}

/*  $Revision$
**
**  Check article, send it to the local server.
*/
#include "config.h"
#include "clibrary.h"
#include <netinet/in.h>

#include "nnrpd.h"
#include "ov.h"
#include "post.h"

#define FLUSH_ERROR(F)		(fflush((F)) == EOF || ferror((F)))
#define HEADER_DELTA		20

extern int LLOGenable;

STATIC char     *tmpPtr ;
STATIC char	Error[SMBUF];
STATIC char	NGSEPS[] = NG_SEPARATOR;
char	**OtherHeaders;
int	OtherCount;
BOOL   HeadersModified;
STATIC int	OtherSize;
STATIC BOOL	WasMailed;
STATIC STRING	BadDistribs[] = {
    BAD_DISTRIBS
};

HEADER	Table[] = {
    /* 	Name			Canset	Type	*/
    {	"Path",			TRUE,	HTstd },
#define _path		 0
    {	"From",			TRUE,	HTreq },
#define _from		 1
    {	"Newsgroups",		TRUE,	HTreq },
#define _newsgroups	 2
    {	"Subject",		TRUE,	HTreq },
#define _subject	 3
    {	"Control",		TRUE,	HTstd },
#define _control	 4
    {	"Supersedes",		TRUE,	HTstd },
#define _supersedes	 5
    {	"Followup-To",		TRUE,	HTstd },
#define _followupto	 6
    {	"Date",			TRUE,	HTstd },
#define _date		 7
    {	"Organization",		TRUE,	HTstd },
#define _organization	 8
    {	"Lines",		TRUE,	HTstd },
#define _lines		 9
    {	"Sender",		TRUE,	HTstd },
#define _sender		10
    {	"Approved",		TRUE,	HTstd },
#define _approved	11
    {	"Distribution",		TRUE,	HTstd },
#define _distribution	12
    {	"Expires",		TRUE,	HTstd },
#define _expires	13
    {	"Message-ID",		TRUE,	HTstd },
#define _messageid	14
    {	"References",		TRUE,	HTstd },
#define _references	15
    {	"Reply-To",		TRUE,	HTstd },
#define _replyto	16
    {	"NNTP-Posting-Host",	FALSE,	HTstd },
#define _nntpposthost	17
    {	"Mime-Version",		TRUE,	HTstd },
#define _mimeversion	18
    {	"Content-Type",		TRUE,	HTstd },
#define _contenttype	19
    {	"Content-Transfer-Encoding", TRUE, HTstd },
#define _contenttransferencoding 20
    {   "X-Trace",              FALSE, HTstd },
#define _xtrace         21
    {   "X-Complaints-To",	FALSE, HTstd },
#define _xcomplaintsto	22
    {   "NNTP-Posting-Date",	FALSE, HTstd },
#define _nntppostdate	23
    {   "Injector-Info",        FALSE,  HTstd },
    {	"Xref",			FALSE,	HTstd },
    {	"Summary",		TRUE,	HTstd },
    {	"Keywords",		TRUE,	HTstd },
    {	"Date-Received",	FALSE,	HTobs },
    {	"Received",		FALSE,	HTobs },
    {	"Posted",		FALSE,	HTobs },
    {	"Posting-Version",	FALSE,	HTobs },
    {	"Relay-Version",	FALSE,	HTobs },
    {   "Cc",			TRUE, HTstd },
#define _cc		32
    {   "Bcc",			TRUE, HTstd },
#define _bcc		33
    {   "To",			TRUE, HTstd },
#define _to		34
};

HEADER *EndOfTable = ENDOF(Table);



/* Join() and MaxLength() are taken from innd.c */
/*
**  Turn any \r or \n in text into spaces.  Used to splice back multi-line
**  headers into a single line.
*/
STATIC char *
Join(char *text)
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
char *
MaxLength(char *p, char *q)
{
    static char	buff[80];
    int		i;

    /* Already short enough? */
    i = strlen(p);
    if (i < sizeof buff - 1)
	return Join(p);

    /* Simple case of just want the begining? */
    if (q - p < sizeof buff - 4) {
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
STATIC char *
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
STATIC char *
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
		    (void)sprintf(Error, "Obsolete \"%s\" header", hp->Name);
		    return NULL;
		}
		if (hp->Value) {
		    (void)sprintf(Error, "Duplicate \"%s\" header", hp->Name);
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
STATIC STRING
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
	(void)sprintf(Error, "\"%s\" is not a valid control message",
		MaxLength(ctrl,ctrl));
	return Error;
    }
    *p = save;
    return NULL;
}


/*
**  Check the Distribution header, and exit on error.
*/
STATIC STRING
CheckDistribution(char *p)
{
    static char	SEPS[] = " \t,";
    STRING	*dp;

    if ((p = strtok(p, SEPS)) == NULL)
	return "Can't parse Distribution line.";
    do {
	for (dp = BadDistribs; *dp; dp++)
	    if (wildmat(p, *dp)) {
		(void)sprintf(Error, "Illegal distribution \"%s\"", MaxLength(p,p));
		return Error;
	    }
    } while ((p = strtok((char *)NULL, SEPS)) != NULL);
    return NULL;
}


/*
**  Process all the headers.  FYI, they're done in RFC-order.
**  Return NULL if okay, or an error message.
*/
STATIC STRING
ProcessHeaders(int linecount, char *idbuff)
{
    static char	MONTHS[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    static char	datebuff[40];
    static char	localdatebuff[40];
    static char	orgbuff[SMBUF];
    static char	linebuff[40];
    static char	tracebuff[SMBUF];
    static char	complaintsbuff[SMBUF];
    static char	sendbuff[SMBUF];
    static char	*newpath = NULL;
    HEADER	*hp;
    char	*p;
    time_t	t;
    struct tm	*gmt;
    TIMEINFO	Now;
    STRING	error;
    pid_t	pid;
    BOOL	addvirtual = FALSE;

    /* Various things need Now to be set. */
    if (GetTimeInfo(&Now) < 0) {
        sprintf(Error, "Can't get the time, %s", strerror(errno));
        return Error;
    }

    /* Do some preliminary fix-ups. */
    for (hp = Table; hp < ENDOF(Table); hp++) {
	if (!hp->CanSet && hp->Value) {
	    (void)sprintf(Error, "Can't set system \"%s\" header", hp->Name);
	    return Error;
	}
	if (hp->Value) {
	    hp->Len = TrimSpaces(hp->Value);
	    if (hp->Len == 0)
		hp->Value = hp->Body = NULL;
	}
    }

    if (PERMaccessconf->nnrpdauthsender) {
	/* If authorized, add the header based on our info.  If not
         * authorized, zap the Sender so we don't put out unauthenticated
         * data. */
	if (PERMauthorized) {
	    if (PERMuser[0] == '\0') {
		(void)sprintf(sendbuff, "%s@%s", "UNKNOWN", ClientHost);
	    } else {
		if ((p = strchr(PERMuser, '@')) == NULL) {
		    (void)sprintf(sendbuff, "%s@%s", PERMuser, ClientHost);
		} else {
		    (void)sprintf(sendbuff, "%s", PERMuser);
		}
	    }
	    HDR_SET(_sender ,sendbuff);
	} else {
	    HDR_SET(_sender ,NULL);
	}
    }

    /* Set Date.  datebuff is used later for NNTP-Posting-Date, so we have
       to set it and it has to be the UTC date. */
    if (!makedate(0, FALSE, datebuff, sizeof(datebuff)))
        return "Can't generate date header";
    if (HDR(_date) == NULL) {
        if (PERMaccessconf->localtime) {
            if (!makedate(0, TRUE, localdatebuff, sizeof(localdatebuff)))
                return "Can't generate local date header";
	    HDR_SET(_date ,localdatebuff);
	} else {
	    HDR_SET(_date ,datebuff);
	}
    } else {
	if ((t = parsedate(HDR(_date), &Now)) == -1)
	    return "Can't parse \"Date\" header";
	if (t > Now.time + DATE_FUZZ)
	    return "Article posted in the future";
    }

    /* Newsgroups are checked later. */

    if (HDR(_control)) {
	if ((error = CheckControl(HDR(_control))) != NULL)
	    return error;
    } else {
	p = HDR(_subject);
	if (p == NULL)
	    return "Required \"Subject\" header is missing";
        if (EQn(p, "cmsg ", 5)) {
            HDR_SET(_control ,(p + 5));
            if ((error = CheckControl(HDR(_control))) != NULL)
                return error;
        }
    }

    /* Set Message-ID */
    if (HDR(_messageid) == NULL) {
	HDR_SET(_messageid ,idbuff);
    }

    /* Set Path */
    if (HDR(_path) == NULL) {
	/* Note that innd will put host name here for us. */
	HDR_SET(_path ,PATHMASTER);
	if (VirtualPathlen > 0)
	    addvirtual = TRUE;
    } else if (PERMaccessconf->strippath) {
	/* Here's where to do Path changes for new Posts. */
	if ((p = strrchr(HDR(_path), '!')) != NULL) {
	    p++;
	    if (*p == '\0') {
		HDR_SET(_path ,PATHMASTER);
		if (VirtualPathlen > 0)
		    addvirtual = TRUE;
	    } else {
		HDR_SET(_path ,p);
		if ((VirtualPathlen > 0) &&
		    !EQ(p, PERMaccessconf->pathhost))
		    addvirtual = TRUE;
	    }
	} else if (VirtualPathlen > 0)
	    addvirtual = TRUE;
    } else {
	if ((VirtualPathlen > 0) &&
	    (p = strchr(HDR(_path), '!')) != NULL) {
	    *p = '\0';
	    if (!EQ(HDR(_path), PERMaccessconf->pathhost))
		addvirtual = TRUE;
	    *p = '!';
	} else if (VirtualPathlen > 0)
	    addvirtual = TRUE;
    }
    if (addvirtual) {
	if (newpath != NULL)
	    DISPOSE(newpath);
	newpath = NEW(char, VirtualPathlen + strlen(HDR(_path)) + 1);
	sprintf(newpath, "%s%s", VirtualPath, HDR(_path));
	HDR_SET(_path ,newpath);
    }
    

    /* Reply-To; left alone. */
    /* Sender; set above. */

    /* Check Expires. */
    if (HDR(_expires) && parsedate(HDR(_expires), &Now) == -1)
	return "Can't parse \"Expires\" header";

    /* References; left alone. */
    /* Control; checked above. */

    /* Distribution. */
    if ((p = HDR(_distribution)) != NULL) {
	p = COPY(p);
	error = CheckDistribution(p);
	DISPOSE(p);
	if (error != NULL)
	    return error;
    }

    /* Set Organization */
    if (HDR(_organization) == NULL
     && (p = PERMaccessconf->organization) != NULL) {
	(void)strcpy(orgbuff, p);
	HDR_SET(_organization ,orgbuff);
    }

    /* Keywords; left alone. */
    /* Summary; left alone. */
    /* Approved; left alone. */

    /* Set Lines */
    (void)sprintf(linebuff, "%d", linecount);
    HDR_SET(_lines ,linebuff);

    /* Supersedes; left alone. */

    /* NNTP-Posting host; set. */
    if (PERMaccessconf->addnntppostinghost) 
    HDR_SET(_nntpposthost ,ClientHost);
    /* NNTP-Posting-Date - not in RFC (yet) */
    if (PERMaccessconf->addnntppostingdate)
    HDR_SET(_nntppostdate ,datebuff);

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
    sprintf(tracebuff, "%s %ld %ld %s (%d %3.3s %d %02d:%02d:%02d GMT)",
	p, (long) t, (long) pid, ClientIp,
	gmt->tm_mday, &MONTHS[3 * gmt->tm_mon], 1900 + gmt->tm_year,
	gmt->tm_hour, gmt->tm_min, gmt->tm_sec);
    HDR_SET(_xtrace ,tracebuff);

    /* X-Complaints-To; set */
    if ((p = PERMaccessconf->complaints) != NULL)
      sprintf (complaintsbuff, "%s", p);
    else {
      if ((p = PERMaccessconf->fromhost) != NULL && strchr(NEWSMASTER, '@') == NULL)
	sprintf (complaintsbuff, "%s@%s", NEWSMASTER, p);
      else
	sprintf (complaintsbuff, "%s", NEWSMASTER);
    }
    HDR_SET(_xcomplaintsto ,complaintsbuff);

    /* Clear out some headers that should not be here */
    if (PERMaccessconf->strippostcc) {
	HDR_SET(_cc ,NULL);
	HDR_SET(_bcc ,NULL);
	HDR_SET(_to ,NULL);
    }
    /* Now make sure everything is there. */
    for (hp = Table; hp < ENDOF(Table); hp++)
	if (hp->Type == HTreq && hp->Value == NULL) {
	    (void)sprintf(Error, "Required \"%s\" header is missing", hp->Name);
	    return Error;
	}

    return NULL;
}


/*
**  See if the user has more included text than new text.  Simple-minded, but
**  reasonably effective for catching neophyte's mistakes.  A line starting
**  with > is included text.  Decrement the count on lines starting with <
**  so that we don't reject diff(1) output.
*/
STATIC STRING
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
    if (i * 2 > lines)
	return "Article not posted -- more included text than new text";
    return NULL;
}



/*
**  Try to mail an article to the moderator of the group.
*/
STATIC STRING
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
	(void)sprintf(Error, "No mailing address for \"%s\" -- %s",
		group, "ask your news administrator to fix this");
	DISPOSE(group);  
	return Error;
    }
    DISPOSE(group);

    /* Now build up the command (ignore format/argument mismatch errors,
     * in case %s isn't in inconf->mta) and send the headers. */
    if ((mta = innconf->mta) == NULL)
	return "Can't start mailer - mta not set";
    (void)sprintf(buff, innconf->mta, address);
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
    if (fwrite((POINTER)article, (SIZE_T)1, (SIZE_T)i, F) != i)
	return "Can't send article";
    if (FLUSH_ERROR(F)) {
	(void)pclose(F);
	return CANTSEND;
    }
    i = pclose(F);
    if (i) {
	(void)sprintf(Error, "Mailer exited with status %d -- %s",
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
STATIC STRING ValidNewsgroups(char *hdr, char **modgroup)
{
    static char		distbuff[SMBUF];
    char		*groups;
    char		*p;
    BOOL		approved;
    struct _DDHANDLE	*h;
    char		*grplist[2];
    BOOL		IsNewgroup;
    BOOL		FoundOne;
    int			flag;

    p = HDR(_control);
    IsNewgroup = p && EQn(p, "newgroup", 8);
    groups = COPY(hdr);
    if ((p = strtok(groups, NGSEPS)) == NULL)
	return "Can't parse newsgroups line";

    approved = HDR(_approved) != NULL;

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
		sprintf(Error, "You are not allowed to post to %s\r\n", p);
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
	    if (PY_authorize(ClientHost, ClientIp, ServerHost, PERMuser, p, TRUE, &reply) < 0) {
	        syslog(L_NOTICE, "PY_authorize(): authorization skipped due to no Python authorization method defined.");
	    } else {
	        if (reply != NULL) {
		    syslog(L_TRACE, "PY_authorize() returned a refuse string for user %s at %s who wants to read %s: %s", PERMuser, ClientHost, p, reply);
		    (void)sprintf(Error, "%s\r\n", reply);
		    break;
		}
	    }
	}
#endif /* DO_PYTHON */
	    break;
	case NF_FLAG_MODERATED:
	    if (approved && !PERMaccessconf->allowapproved) {
		(void)sprintf(Error, "You are not allowed to approve postings");
	    } else if (!approved && modgroup != NULL && !*modgroup) {
		*modgroup = COPY(p);
	    }
	    break;
	case NF_FLAG_IGNORE:
	case NF_FLAG_NOLOCAL:
	    if (!PERMaccessconf->locpost)
		(void)sprintf(Error, "Postings to \"%s\" are not allowed here.",
			      p);
	    break;
	case NF_FLAG_EXCLUDED:
	    /* Do NOT return an error. */
	    break;
	case NF_FLAG_ALIAS:
	    (void)sprintf(Error,
		    "The newsgroup \"%s\" has been renamed.\n", p);
	    break;
	}
    } while ((p = strtok((char *)NULL, NGSEPS)) != NULL);
    DISPOSE(groups);

    if (!FoundOne && !IsNewgroup)
	(void)sprintf(Error, "No valid newsgroups in \"%s\"", MaxLength(hdr,hdr));
    if (Error[0]) {
        tmpPtr = DDend(h);
	DISPOSE(tmpPtr) ;
	return Error;
    }

    p = DDend(h);
    if (HDR(_distribution) == NULL && *p) {
	(void)strcpy(distbuff, p);
	HDR_SET(_distribution ,distbuff);
    }
    DISPOSE(p);
    return NULL;
}


/*
**  Send a quit message to the server, eat its reply.
*/
STATIC void
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
STATIC int
OfferArticle(char *buff, int buffsize, FILE *FromServer, FILE *ToServer)
{
    static char	CANTSEND[] = "Can't send %s to server, %s";

    (void)fprintf(ToServer, "ihave %s\r\n", HDR(_messageid));
    if (FLUSH_ERROR(ToServer)
     || fgets(buff, buffsize, FromServer) == NULL) {
	(void)sprintf(buff, CANTSEND, "IHAVE", strerror(errno));
	return -1;
    }
    return atoi(buff);
}


/*
**  Spool article to temp file.
*/
STATIC STRING
SpoolitTo(char *article, char *Error, char *SpoolDir)
{
    static char	CANTSPOOL[NNTP_STRLEN+2];
    HEADER	*hp;
    FILE	*F = NULL;
    int		i;
    char	temp[BUFSIZ];
    char	path[BUFSIZ];
    char	*q;

    /* Initialize the returned error message */
    sprintf(CANTSPOOL, "%s and can't write text to local spool file", Error);

    /* Try to write it to the spool dir. */
    TempName(SpoolDir, temp);
    /* rnews -U ignores files starting with . */
    strrchr(temp, '/')[1] = '.';
    if ((F = fopen(temp, "w")) == NULL) {
        syslog(L_FATAL, "cant open %s %m", temp);
        return CANTSPOOL;
    }
    fchmod(fileno(F), BATCHFILE_MODE);

    /* Write the headers and a blank line. */
    for (hp = Table; hp < ENDOF(Table); hp++)
	if (hp->Value) {
	    q = NEW(char, hp->Body - hp->Value + hp->Len + 1);
	    strncpy(q, hp->Value, hp->Body - hp->Value + hp->Len);
	    *(q + (int)(hp->Body - hp->Value) + hp->Len) = '\0';
	    if (*hp->Value == ' ' || *hp->Value == '\t')
		(void)fprintf(F, "%s:%s\n", hp->Name, hp->Value);
	    else
		(void)fprintf(F, "%s: %s\n", hp->Name, hp->Value);
	    if (FLUSH_ERROR(F)) {
		(void)fclose(F);
		DISPOSE(q);
		return CANTSPOOL;
	    }
	    DISPOSE(q);
	}
    for (i = 0; i < OtherCount; i++) {
	(void)fprintf(F, "%s\n", OtherHeaders[i]);
	if (FLUSH_ERROR(F)) {
	    (void)fclose(F);
	    return CANTSPOOL;
	}
    }
    (void)fprintf(F, "\n");

    /* Write the article body */
    i = strlen(article);
    if (fwrite((POINTER)article, (SIZE_T)1, (SIZE_T)i, F) != i) {
        fclose(F);
        return CANTSPOOL;
    }

    /* Flush and catch any errors */
    if (fclose(F))
	return CANTSPOOL;

    TempName(SpoolDir, path);
    if (rename(temp, path) < 0) {
        syslog(L_FATAL, "cant rename %s %s %m", temp, path);
	return CANTSPOOL;
    }

    /* Article has been spooled */
    return NULL;
}

/*
**  Spool article to temp file.
*/
STATIC STRING
Spoolit(char *article, char *Error)
{
    return SpoolitTo(article, Error, innconf->pathincoming);
}
 
STATIC char *Towire(char *p) {
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

STRING
ARTpost(char *article, char *idbuff)
{
    static char	CANTSEND[] = "Can't send %s to server, %s";
    int		i;
    char	*p, *q, c;
    char	*next;
    HEADER	*hp;
    FILE	*ToServer;
    FILE	*FromServer;
    char	buff[NNTP_STRLEN + 2], frombuf[SMBUF];
    char	*modgroup = NULL;
    STRING	error;
    char	*TrackID;
    char	*DirTrackID;
    FILE	*ftd;
    int		result, len;
    char	SDir[255];

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
    if ((error = ProcessHeaders(i, idbuff)) != NULL)
	return error;
    if (i == 0 && HDR(_control) == NULL)
	return "Article is empty";

    if ((error = ValidNewsgroups(HDR(_newsgroups), &modgroup)) != NULL)
	return error;
    
    strncpy(frombuf, HDR(_from), sizeof(frombuf) - 1);
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
    if ((p = HDR(_followupto)) != NULL
     && !EQ(p, "poster")
     && (error = ValidNewsgroups(p, (char **)NULL)) != NULL) {
	if (modgroup)
	    DISPOSE(modgroup);
	return error;
    }
    if ((PERMaccessconf->localmaxartsize > 0) &&
		(strlen(article) > PERMaccessconf->localmaxartsize)) {
	    (void)sprintf(Error,
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
		sprintf(idbuff, "(mailed to moderator for %s)", modgroup);
	    else
		(void)strncpy(idbuff, HDR(_messageid), SMBUF - 1);
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

    if (modgroup)
    {
      if (idbuff != NULL) {
          STRING retstr;
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
	    (void)sprintf(Error, CANTSEND, "connect request", strerror(errno));
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
            (void)sprintf(Error, "Can't authorize with %s",
                          PERMaccessconf->nnrpdposthost ? PERMaccessconf->nnrpdposthost : "innd");
            return Spoolit(article,Error);
        }
        i = OfferArticle(buff, (int)sizeof buff, FromServer, ToServer);
    }
    if (i != NNTP_SENDIT_VAL) {
        (void)strcpy(Error, buff);
        SendQuit(FromServer, ToServer);
        return (i != NNTP_HAVEIT_VAL ? Spoolit(article, Error) : Error) ;
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
	(void)sprintf(Error, CANTSEND, "headers", strerror(errno));
	(void)fclose(FromServer);
	(void)fclose(ToServer);
	return Spoolit(article, Error);
    }

    /* Send the article, get the server's reply. */
    if (NNTPsendarticle(article, ToServer, TRUE) < 0
     || fgets(buff, sizeof buff, FromServer) == NULL) {
	(void)sprintf(Error, CANTSEND, "article", strerror(errno));
	(void)fclose(FromServer);
	(void)fclose(ToServer);
	return Spoolit(article, Error);
    }

    /* Did the server want the article? */
    if ((i = atoi(buff)) != NNTP_TOOKIT_VAL) {
	(void)strcpy(Error, buff);
	SendQuit(FromServer, ToServer);
	return (i != NNTP_REJECTIT_VAL ? Spoolit(article, Error) : Error) ;
    }

    /* Send a quit and close down */
    SendQuit(FromServer, ToServer);
    if (idbuff) {
	(void)strncpy(idbuff, HDR(_messageid), SMBUF - 1);
	idbuff[SMBUF - 1] = '\0';
    }

    /* Tracking */
    if (PERMaccessconf->readertrack) {
	len = strlen(innconf->pathlog) + strlen("/trackposts/track.") + strlen(HDR(_messageid)) + 1;
	TrackID = NEW(char, len);
	sprintf(TrackID, "%s/trackposts/track.%s", innconf->pathlog, HDR(_messageid));
	if ((ftd = fopen(TrackID,"w")) == NULL) {
	    DirTrackID = NEW(char, len);
	    sprintf(DirTrackID, "%s/trackposts", innconf->pathlog);
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
		if (strchr(hp->Value, '\n') != NULL) {
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
	fclose(ftd);
	if (result != EOF) {
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

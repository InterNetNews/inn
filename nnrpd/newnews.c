/*  $Revision$
**
**  The newnews command.
*/
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "configdata.h"
#include "clibrary.h"
#include "nnrpd.h"

#define FILE_LIST_DELTA		10
#define GET_DATE(p, line)	\
	((p = strchr(line, HIS_FIELDSEP)) == NULL ? 0 : atol(++p))


/*
**  Open an article and see if its distribution is in the list.
*/
STATIC BOOL
DistMatches(ARTHANDLE *art, char **distribs, char **files)
{
    char		**dp;
    QIOSTATE		*qp;
    char		*p, *p1, *p2;
    char		*q;
    char		buff[BIG_BUFFER];
    char		*save;

    /* Loop through the file list, trying to open one of them.. */
    if (art != NULL) {
	if ((p = q = (char *)HeaderFindMem(art->data, art->len, "distribution", sizeof("distribution")-1)) == NULL)
	    return FALSE;
	for (p1 = p2 = NULL; p < art->data + art->len; p++) {
	    if (p2 != (char *)NULL && *p2 == '\r' &&
		p1 != (char *)NULL && *p1 == '\n' &&
		!ISWHITE(*p))
		break;
	    p2 = p1;
	    p1 = p;
	}
	if (p >= art->data + art->len)
	    return FALSE;
	for (dp = distribs; *dp; dp++)
	    if (caseEQn(p, *dp, p2 - q))
		return TRUE;
	return FALSE;
    } else {
	for (save = files[0]; *files; files++) {
	    /* this next line makes me nervous--from unoff4... */
	    (void)sprintf(buff, "%s/%s/%s", innconf->patharticles, files[0],
		files[0] + strlen(files[0]) + 1);
	    for (p = &buff[SPOOLlen]; *p; p++)
		if (*p == '.')
		    *p = '/';
	    if ((qp = QIOopen(buff)) != NULL)
		break;
	}
	if (*files == NULL) {
	    return FALSE;
	}

	/* Scan the article for the Distribution header. */
	while ((p = QIOread(qp)) != NULL) {
	    if (*p == '\n')
		/* End of headers. */
		break;
	    if (*p != 'd' && *p != 'D')
		continue;
	    if ((q = strchr(p, '\n')) != NULL)
		*q = '\0';
	    if ((q = strchr(p, ':')) == NULL)
		continue;
	    *q = '\0';
	    if (caseEQ(p, "distribution")) {
		for (q += 2, dp = distribs; *dp; dp++)
		    if (caseEQ(q, *dp)) {
			QIOclose(qp);
			return TRUE;
		    }
		break;
	    }
	}
	QIOclose(qp);
	return FALSE;
    }
}


/*
**  Split file list into array of newsgroups.  Return static pointer,
**  or NULL if there are no filenames.
*/
STATIC char **
GetFiles(TOKEN *token, char *p)
{
    static int		size;
    static char		**list;
    int			i;
    char		*q, *slash = p;
    int			linelen;
    static char		*OVERline = NULL;
    static char		*Xrefbuf = NULL;
    char		*Xref;

    if (size == 0) {
	size = FILE_LIST_DELTA;
	list = NEW(char*, size + 1);
    }
    if (token->type != TOKEN_EMPTY) {
	if (!(token->index < OVER_NONE))
	    return NULL;
	if ((q = OVERretrieve(token, &linelen)) == (char *)NULL)
	    return NULL;
	if (innconf->overviewmmap) {
	    if (!OVERline)
		OVERline = NEW(char, MAXOVERLINE);
	    if (linelen > MAXOVERLINE - 1)
		linelen = MAXOVERLINE - 1;
	    memcpy(OVERline, q, linelen);
	    OVERline[linelen] = '\0';
	} else {
	    OVERline = q;
	}
	if ((Xref = strstr(OVERline, "\tXref:")) == NULL)
	    return NULL;
	if ((Xref = strchr(Xref, ' ')) == NULL)
	    return NULL;
	for (Xref++; *Xref == ' '; Xref++);
	if ((Xref = strchr(Xref, ' ')) == NULL)
	    return NULL;
	for (Xref++; *Xref == ' '; Xref++);
	if (!Xrefbuf)
	    Xrefbuf = NEW(char, MAXOVERLINE);
	memcpy(Xrefbuf, Xref, linelen - (OVERline - Xref));
	Xrefbuf[linelen - (OVERline - Xref)] = '\0';
	if ((q = strchr(Xrefbuf, '\t')) != NULL)
	    *q = '\0';
	p = Xrefbuf;
    }

    for (i = 0 ; ; ) {
	while (ISWHITE(*p))
	    p++;
	if (*p == '\0' || *p == '\n')
	    break;

	if (i >= size - 1) {
	    size += FILE_LIST_DELTA;
	    RENEW(list, char *, size + 1);
	}
	for (list[i] = p; *p && *p != '\n' && !ISWHITE(*p); p++) {
	    if (*p == '/' || *p == ':')
		*(slash = p) = '\0';
	}
	if (*p) *p++ = '\0';
	if (slash > list[i])  /* Only save filenames with '/' */
	    i++;
    }
    list[i] = NULL;
    return i ? list : NULL;
}

/*
**  Seek to first line in the history file where the date is after the
**  desired one.   Returns FALSE on failure.
*/
STATIC BOOL
FindLinesAfter(long date, char *line, int linesize, FILE *F)
{
    char	*p;
    long	upper;
    long	lower;
    long	middle;

    /* Read first line -- is it in our range? */
    (void)fseek(F, 0L, SEEK_SET);
    if (fgets(line, linesize, F) == NULL)
	return FALSE;
    if (GET_DATE(p, line) >= date)
	return TRUE;

    /* Set search ranges and go. */
    lower = 0;
    (void)fseek(F, 0L, SEEK_END);
    upper = ftell(F);
    for ( ; ; ) {
	/* Seek to middle line. */
	middle = (upper + lower) / 2;
	(void)fseek(F, middle, SEEK_SET);
	while (getc(F) != '\n' && ++middle <= upper)
	    continue;

	if (middle >= upper)
	    break;

	if (fgets(line, linesize, F) != NULL && GET_DATE(p, line) > date)
	    upper = middle;
	else if (lower == middle)
	    break;
	else
	    lower = middle;
    }

    /* Move to lower bound; we know this will always be the start of a line. */
    (void)fseek(F, lower, SEEK_SET);
    while (fgets(line, linesize, F) != NULL)
	if (GET_DATE(p, line) >= date)
	    return TRUE;

    return FALSE;
}


/*
**  NEWNEWS newsgroups date time ["GMT"] [<distributions>]
**  Return the Message-ID of any articles after the specified date,
**  and within the specified distributions.
*/
FUNCTYPE
CMDnewnews(int ac, char *av[])
{
    static char		**groups;
    char		*start;
    char		*p, *p1, *p2, *q;
    FILE		*F;
    BOOL		AllDists;
    BOOL		AllGroups;
    char		**distribs;
    char		**files;
    char		line[BIG_BUFFER];
    long		date;
    TOKEN		token;
    ARTHANDLE		*art;
    static int		allocatedsize = 0;
    static char		*msgid = NULL;
    BOOL		overviewinitialized = FALSE;
    BOOL		overviewfailed = FALSE;

    if (!PERMnewnews) {
	Reply("%d NEWNEWS command disabled by administrator\r\n",
          NNTP_ACCESS_VAL);
	return;
    }

    if (!PERMcanread) {
	Reply("%s\r\n", NNTP_ACCESS);
	return;
    }

    /* Make other processes happier if someone uses NEWNEWS */
    if (innconf->nicenewnews > 0)
	nice(innconf->nicenewnews);

    (void)sprintf(line, "%s %s %s %s %s",
	    av[1], av[2], av[3],
	    (ac >= 5 && *av[4] == 'G') ? "GMT" : "local",
	    (ac >= 5 && *av[ac - 1] == '<') ? av[ac - 1] : "none");
    syslog(L_NOTICE, "%s newnews %s", ClientHost, line);

    /* Optimization in case client asks for !* (no groups) */
    if (EQ(av[1], "!*")) {
	Reply("%s\r\n", NNTP_NEWNEWSOK);
	Printf(".\r\n");
	return;
    }

    /* Parse the newsgroups. */
    AllGroups = EQ(av[1], "*");
    if (!AllGroups && !NGgetlist(&groups, av[1])) {
	Reply("%d Bad newsgroup specifier %s\r\n", NNTP_SYNTAX_VAL, av[1]);
	return;
    }

    /* Parse the date. */
    date = NNTPtoGMT(av[2], av[3]);
    if (date < 0) {
	Reply("%d Bad date\r\n", NNTP_SYNTAX_VAL);
	return;
    }
    ac -= 4;
    av += 4;
    if (ac > 0 && caseEQ(*av, "GMT")) {
	ac--;
	av++;
    }
    else
	date = LOCALtoGMT(date);

    /* Parse the distributions. */
    if (ac == 0)
	AllDists = TRUE;
    else {
	if (!ParseDistlist(&distribs, *av)) {
	    Reply("%d Bad distribution %s\r\n", NNTP_SYNTAX_VAL, *av);
	    return;
	}
	AllDists = FALSE;
    }

    if ((F = fopen(HISTORY, "r")) == NULL) {
	syslog(L_ERROR, "%s cant fopen %s %m",
	    ClientHost, HISTORY);
	Reply("%d Can't open history\r\n", NNTP_TEMPERR_VAL);
	return;
    }

    Reply("%s\r\n", NNTP_NEWNEWSOK);

    files = NULL;
    art = NULL;
    if (FindLinesAfter(date, line, sizeof line, F))
	do {
	    /* Skip two tab-separated fields. */
	    if ((p = strchr(line, HIS_FIELDSEP)) == NULL
	     || (start = strchr(p + 1, HIS_FIELDSEP)) == NULL)
		continue;
	    if ((q = strchr(++start, '\n')) != NULL)
		*q = '\0';

	    /* Get the file list. */
	    if (IsToken(start)) {
		if (overviewfailed)
		    continue;
		token = TextToToken(start);
		if (token.type == TOKEN_EMPTY)
		    /* this should not happen */
		    continue;
		if (!overviewinitialized) {
		    if (OVERinit())
			overviewinitialized = TRUE;
		    else {
			overviewfailed = TRUE;
			continue;
		    }
		}
	    } else
		token.type = TOKEN_EMPTY;
	    if (*start == '\0' || (files = GetFiles(&token, start)) == NULL)
		continue;

	    /* Check permissions. */
	    if (art != NULL) {
		SMfreearticle(art);
		art = NULL;
	    }
	    if (token.type != TOKEN_EMPTY) {
		if (token.cancelled)
		    continue;
		if ((art = SMretrieve(token, RETR_HEAD)) == NULL)
		    continue;
	    } else
		    art = NULL;
	    if (!AllGroups && !PERMmatch(groups, files))
		continue;
	    if (!AllDists && !DistMatches(art, distribs, files))
		continue;
	    if (token.type != TOKEN_EMPTY) {
		if ((p = q = (char *)HeaderFindMem(art->data, art->len, "message-id", sizeof("message-id")-1)) == NULL)
		    continue;
		for (p1 = p2 = NULL; p < art->data + art->len; p++) {
		    if (p2 != (char *)NULL && *p2 == '\r' &&
			p1 != (char *)NULL && *p1 == '\n' &&
			!ISWHITE(*p))
			break;
		    p2 = p1;
		    p1 = p;
		}
		if (p >= art->data + art->len)
		    continue;
		if (allocatedsize == 0)
		    msgid = NEW(char, p2 - q + 1);
		else if (allocatedsize < p2 - q + 1)
		    RENEW(msgid, char, p2 - q + 1);
		memcpy(msgid, q, p2 - q);
		msgid[p2-q] = '\0';
		Printf("%s\r\n", msgid);
	    } else {
		*(--p) = '\0';
		Printf("%s\r\n", line);
	    }
	} while (fgets(line, sizeof line, F) != NULL);
	if (art != NULL)
	    SMfreearticle(art);
	if (overviewinitialized)
	    OVERshutdown();

    (void)fclose(F);
    Printf(".\r\n");
}


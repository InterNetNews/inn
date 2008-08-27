/*  $Id$
**
**  LIST commands.
*/

#include "config.h"
#include "clibrary.h"

#include "nnrpd.h"
#include "inn/ov.h"
#include "inn/innconf.h"
#include "inn/messages.h"

#ifdef HAVE_SSL
extern int nnrpd_starttls_done;
#endif /* HAVE_SSL */

typedef struct _LISTINFO {
    const char *method;
    const char * File;
    void (*impl)(struct _LISTINFO *);
    bool         Required;
    const char * Items;
    const char * Format;
} LISTINFO;

static void cmd_list_schema(LISTINFO *lp);
static void cmd_list_extensions(LISTINFO *lp);

static LISTINFO		INFOactive = {
    "ACTIVE", INN_PATH_ACTIVE, NULL, true, "active newsgroups",
    "Newsgroups in form \"group high low flags\""
};
static LISTINFO		INFOactivetimes = {
    "ACTIVE.TIMES", INN_PATH_ACTIVETIMES, NULL, false, "creation times",
    "Group creations in form \"name time who\""
};
static LISTINFO		INFOdistribs = {
    "DISTRIBUTIONS", INN_PATH_NNRPDIST, NULL, false, "newsgroup distributions",
    "Distributions in form \"area description\""
};
static LISTINFO               INFOsubs = {
    "SUBSCRIPTIONS", INN_PATH_NNRPSUBS, NULL, false,
    "automatic group subscriptions", "Subscriptions in form \"group\""
};
static LISTINFO		INFOdistribpats = {
    "DISTRIB.PATS", INN_PATH_DISTPATS, NULL, false, "distribution patterns",
    "Default distributions in form \"weight:pattern:value\""
};
static LISTINFO		INFOextensions = {
    "EXTENSIONS", NULL, cmd_list_extensions, false, "supported extensions",
    "Supported NNTP extensions"
};
static LISTINFO		INFOgroups = {
    "NEWSGROUPS", INN_PATH_NEWSGROUPS, NULL, false, "newsgroup descriptions",
    "Descriptions in form \"group description\""
};
static LISTINFO		INFOmoderators = {
    "MODERATORS", INN_PATH_MODERATORS, NULL, false, "moderator patterns",
    "Newsgroup moderators in form \"group-pattern:mail-address-pattern\""
};
static LISTINFO		INFOschema = {
    "OVERVIEW.FMT", NULL, cmd_list_schema, true, "overview format",
    "Order of fields in overview database"
};
static LISTINFO		INFOmotd = {
    "MOTD", INN_PATH_MOTD, NULL, false, "motd",
    "Message of the day text"
};

static LISTINFO *info[] = {
    &INFOactive,
    &INFOactivetimes,
    &INFOdistribs,
    &INFOsubs,
    &INFOdistribpats,
    &INFOextensions,
    &INFOgroups,
    &INFOmoderators,
    &INFOschema,
    &INFOmotd,
};


/*
**  List the overview schema (standard and extra fields).
*/
static void
cmd_list_schema(LISTINFO *lp)
{
    const struct cvector *standard;
    unsigned int i;

    Reply("%d %s.\r\n", NNTP_OK_LIST, lp->Format);
    standard = overview_fields();
    for (i = 0; i < standard->count; ++i) {
	Printf("%s:\r\n", standard->strings[i]);
    }
    for (i = 0; i < OVextra->count; ++i) {
	Printf("%s:full\r\n", OVextra->strings[i]);
    }
    Printf(".\r\n");
}


/*
**  List supported extensions.
*/
static void
cmd_list_extensions(LISTINFO *lp)
{
    const char *mechlist = NULL;

    Reply("%d %s.\r\n", NNTP_OK_LIST, lp->Format);

#ifdef HAVE_SSL
    if (!nnrpd_starttls_done && PERMauthorized != true)
	Printf("STARTTLS\r\n");
#endif /* HAVE_SSL */

#ifdef HAVE_SASL
    /* Check for SASL mechs. */
    sasl_listmech(sasl_conn, NULL, " SASL:", ",", "", &mechlist, NULL, NULL);
#endif /* HAVE_SASL */

    if (PERMauthorized != true || mechlist != NULL) {
	Printf("AUTHINFO%s%s\r\n",
	       PERMauthorized != true ? " USER" : "",
	       mechlist != NULL ? mechlist : "");
    }

    Printf(".\r\n");
}


/*
**  List a single newsgroup.  Called by LIST ACTIVE with a single argument.
**  This is quicker than parsing the whole active file, but only works with
**  single groups.  It also doesn't work for aliased groups, since overview
**  doesn't know what group the group is aliased to (yet).  Returns whether we
**  were able to answer the command.
*/
static bool
CMD_list_single(char *group)
{
    char *grplist[2] = { NULL, NULL };
    int lo, hi, flag;

    if (PERMspecified) {
        grplist[0] = group;
        if (!PERMmatch(PERMreadlist, grplist))
            return false;
    }
    if (OVgroupstats(group, &lo, &hi, NULL, &flag) && flag != '=') {
        Reply("%d %s.\r\n", NNTP_OK_LIST, INFOactive.Format);
        Printf("%s %010u %010u %c\r\n.\r\n", group, hi, lo, flag);
        return true;
    }
    return false;
}


/*
**  List active newsgroups, newsgroup descriptions, and distributions.
*/
void
CMDlist(int ac, char *av[])
{
    QIOSTATE	*qp;
    char	*p;
    char	*save;
    char        *path;
    char		*q;
    char		*grplist[2];
    LISTINFO		*lp;
    char		*wildarg = NULL;
    char		savec;
    unsigned int i;

    p = av[1];
    /* LIST ACTIVE is the default LIST command.  If a keyword is provided,
     * we check whether it is defined. */
    if (p == NULL) {
	lp = &INFOactive;
    } else {
	lp = NULL;
	for (i = 0; i < ARRAY_SIZE(info); ++i) {
	    if (strcasecmp(p, info[i]->method) == 0) {
		lp = info[i];
		break;
	    }
	}
    }
    /* If no defined LIST keyword is found, we return. */
    if (lp == NULL) {
	Reply("%s\r\n", NNTP_SYNTAX_USE);
	return;
    }

    if (lp == &INFOactive) {
	if (ac == 3) {
	    wildarg = av[2];
            /* No need to parse the active file for a single group. */
            if (CMD_list_single(wildarg))
		return;
	}
    } else if (lp == &INFOgroups || lp == &INFOactivetimes) {
	if (ac == 3)
	    wildarg = av[2];
    }
    /* Three arguments can be passed only when ACTIVE, ACTIVE.TIMES
     * or NEWSGROUPS keywords are used. */
    if (ac > 2 && !wildarg) {
	Reply("%s\r\n", NNTP_SYNTAX_USE);
	return;
    }

    /* If a function is provided for the given keyword, we call it. */
    if (lp->impl != NULL) {
	lp->impl(lp);
	return;
    }

    path = innconf->pathetc;
    /* The active, active.times and newsgroups files are in pathdb. */
    if ((strstr(lp->File, "active") != NULL) ||
	(strstr(lp->File, "newsgroups") != NULL))
	path = innconf->pathdb;
    path = concatpath(path, lp->File);
    qp = QIOopen(path);
    free(path);
    if (qp == NULL) {
        Reply("%d No list of %s available.\r\n",
              NNTP_ERR_UNAVAILABLE, lp->Items);
        /* Only the active and overview.fmt files are required (but the last
         * one has already called cmd_list_schema). */
        if (lp->Required || errno != ENOENT) {
            /* %m outputs strerror(errno). */
            syslog(L_ERROR, "%s cant fopen %s %m", Client.host, lp->File);
        }
        return;
    }

    Reply("%d %s.\r\n", NNTP_OK_LIST, lp->Format);
    if (!PERMspecified) {
	/* Optimize for unlikely case of no permissions and false default. */
	QIOclose(qp);
	Printf(".\r\n");
	return;
    }

    /* Set up group list terminator. */
    grplist[1] = NULL;

    /* Read lines, ignore long ones. */
    while ((p = QIOread(qp)) != NULL) {
	if (lp == &INFOmotd) {
	    Printf("%s\r\n", p);
	    continue;
	}
	if (p[0] == '.' && p[1] == '\0') {
	    syslog(L_ERROR, "%s single dot in %s", Client.host, lp->File);
	    continue;
	}
	/* Matching patterns against patterns is not that
	 * good but it is better than nothing... */
	if (lp == &INFOdistribpats) {
	    if (*p == '\0' || *p == '#' || *p == ';' || *p == ' ')
		continue;
	    if (PERMspecified) {
		if ((q = strchr(p, ':')) == NULL)
	    	    continue;
		q++;
		if ((save = strchr(q, ':')) == NULL)
		    continue;
		*save = '\0';
		grplist[0] = q;
		if (!PERMmatch(PERMreadlist, grplist))
		    continue;
		*save = ':';
	    }
	    Printf("%s\r\n", p);
	    continue;
	}
	if (lp == &INFOdistribs || lp == &INFOmoderators) {
	    if (*p != '\0' && *p != '#' && *p != ';' && *p != ' ')
		Printf("%s\r\n", p);
	    continue;
	}
	savec = '\0';
	for (save = p; *save != '\0'; save++) {
	    if (*save == ' ' || *save == '\t') {
		savec = *save;
		*save = '\0';
		break;
	    }
	}

        /* Check whether the reader has access to the newsgroup. */
	if (PERMspecified) {
	    grplist[0] = p;
	    if (!PERMmatch(PERMreadlist, grplist))
		continue;
	}
        /* Check whether the newsgroup matches the wildmat pattern,
         * if given. */
	if (wildarg && !uwildmat(p, wildarg))
	    continue;
	if (savec != '\0')
	    *save = savec;
	Printf("%s\r\n", p);
    }
    QIOclose(qp);

    Printf(".\r\n");
}

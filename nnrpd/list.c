/*  $Id$
**
**  List commands.
*/

#include "config.h"
#include "clibrary.h"

#include "nnrpd.h"
#include "ov.h"
#include "inn/innconf.h"
#include "inn/messages.h"

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
    "active", _PATH_ACTIVE, NULL, true, "active newsgroups",
    "Newsgroups in form \"group high low flags\""
};
static LISTINFO		INFOactivetimes = {
    "active.times", _PATH_ACTIVETIMES, NULL, false, "creation times",
    "Group creations in form \"name time who\""
};
static LISTINFO		INFOdistribs = {
    "distributions", _PATH_NNRPDIST, NULL, false, "newsgroup distributions",
    "Distributions in form \"area description\""
};
static LISTINFO               INFOsubs = {
    "subscriptions", _PATH_NNRPSUBS, NULL, false, "automatic group subscriptions",
    "Subscriptions in form \"group\""
};
static LISTINFO		INFOdistribpats = {
    "distrib.pats", _PATH_DISTPATS, NULL, false, "distribution patterns",
    "Default distributions in form \"weight:pattern:value\""
};
static LISTINFO		INFOextensions = {
    "extensions", NULL, cmd_list_extensions, false, "supported extensions",
    "Supported NNTP extensions"
};
static LISTINFO		INFOgroups = {
    "newsgroups", _PATH_NEWSGROUPS, NULL, false, "newsgroup descriptions",
    "Descriptions in form \"group description\""
};
static LISTINFO		INFOmoderators = {
    "moderators", _PATH_MODERATORS, NULL, false, "moderator patterns",
    "Newsgroup moderators in form \"group-pattern:mail-address-pattern\""
};
static LISTINFO		INFOschema = {
    "overview.fmt", NULL, cmd_list_schema, true, "overview format",
    "Order of fields in overview database"
};
static LISTINFO		INFOmotd = {
    "motd", _PATH_MOTD, NULL, false, "motd",
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
**  List the overview schema
*/
static void
cmd_list_schema(LISTINFO *lp)
{
    const struct cvector *standard;
    unsigned int i;

    Reply("%d %s.\r\n", NNTP_LIST_FOLLOWS_VAL, lp->Format);
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
**  List supported extensions
*/
static void
cmd_list_extensions(LISTINFO *lp)
{
    Reply("%d %s.\r\n", NNTP_SLAVEOK_VAL, lp->Format);
    if (PERMauthorized != true)
        Printf("AUTHINFO USER\r\n");
    Printf("LISTGROUP\r\n");
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
        Reply("%d %s.\r\n", NNTP_LIST_FOLLOWS_VAL, INFOactive.Format);
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
    if (lp == NULL) {
	Reply("%s\r\n", NNTP_SYNTAX_USE);
	return;
    }
    if (lp == &INFOactive) {
	if (ac == 3) {
	    wildarg = av[2];
            if (CMD_list_single(wildarg))
		return;
	}
    } else if (lp == &INFOgroups || lp == &INFOactivetimes) {
	if (ac == 3)
	    wildarg = av[2];
    }

    if (ac > 2 && !wildarg) {
	Reply("%s\r\n", NNTP_SYNTAX_USE);
	return;
    }

    if (lp->impl != NULL) {
	lp->impl(lp);
	return;
    }

    path = innconf->pathetc;
    if ((strstr(lp->File, "active") != NULL) ||
	(strstr(lp->File, "newsgroups") != NULL))
	path = innconf->pathdb;
    if (strchr(lp->File, '/') != NULL)
	path = "";
    path = concatpath(path, lp->File);
    qp = QIOopen(path);
    free(path);
    if (qp == NULL) {
        Reply("%d No list of %s available.\r\n",
              NNTP_TEMPERR_VAL, lp->Items);
	    if (lp->Required || errno != ENOENT) {
            syslog(L_ERROR, "%s cant fopen %s %m", ClientHost, lp->File);
        }
        return;
    }

    Reply("%d %s.\r\n", NNTP_LIST_FOLLOWS_VAL, lp->Format);
    if (!PERMspecified) {
	/* Optmize for unlikely case of no permissions and false default. */
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
	    syslog(L_ERROR, "%s single dot in %s", ClientHost, lp->File);
	    continue;
	}
	/* matching patterns against patterns is not that
	   good but it's better than nothing ... */
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
	      
	if (PERMspecified) {
	    grplist[0] = p;
	    if (!PERMmatch(PERMreadlist, grplist))
		continue;
	}
	if (wildarg && !uwildmat(p, wildarg))
	    continue;
	if (savec != '\0')
	    *save = savec;
	Printf("%s\r\n", p);
    }
    QIOclose(qp);

    Printf(".\r\n");
}

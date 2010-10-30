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

typedef struct _LISTINFO {
    const char *method;
    const char * File;
    void (*impl)(struct _LISTINFO *, int ac, char *av[]);
    bool         Required;
    const char * Items;
    const char * Format;
} LISTINFO;

static void cmd_list_schema(LISTINFO *lp, int ac, char *av[]);
static void cmd_list_headers(LISTINFO *lp, int ac, char *av[]);

static LISTINFO		INFOactive = {
    "ACTIVE", INN_PATH_ACTIVE, NULL, true, "list of active newsgroups",
    "Newsgroups in form \"group high low status\""
};
static LISTINFO		INFOactivetimes = {
    "ACTIVE.TIMES", INN_PATH_ACTIVETIMES, NULL, false, "list of newsgroup creation times",
    "Newsgroup creation times in form \"group time who\""
};
static LISTINFO         INFOcounts = {
    "COUNTS", INN_PATH_ACTIVE, NULL, true, "list of active newsgroups with estimated counts",
    "Newsgroups in form \"group high low count status\""
};
static LISTINFO		INFOdistribs = {
    "DISTRIBUTIONS", INN_PATH_NNRPDIST, NULL, false, "list of newsgroup distributions",
    "Distributions in form \"distribution description\""
};
static LISTINFO         INFOheaders = {
    "HEADERS", NULL, cmd_list_headers, true, "list of supported headers and metadata items",
    "Headers and metadata items supported"
};
static LISTINFO         INFOsubs = {
    "SUBSCRIPTIONS", INN_PATH_NNRPSUBS, NULL, false,
    "list of recommended newsgroup subscriptions",
    "Recommended subscriptions in form \"group\""
};
static LISTINFO		INFOdistribpats = {
    "DISTRIB.PATS", INN_PATH_DISTPATS, NULL, false, "list of distribution patterns",
    "Default distributions in form \"weight:group-pattern:distribution\""
};
static LISTINFO		INFOgroups = {
    "NEWSGROUPS", INN_PATH_NEWSGROUPS, NULL, true, "list of newsgroup descriptions",
    "Newsgroup descriptions in form \"group description\""
};
static LISTINFO		INFOmoderators = {
    "MODERATORS", INN_PATH_MODERATORS, NULL, false, "list of submission templates",
    "Newsgroup moderators in form \"group-pattern:submission-template\""
};
static LISTINFO		INFOschema = {
    "OVERVIEW.FMT", NULL, cmd_list_schema, true, "overview format",
    "Order of fields in overview database"
};
static LISTINFO		INFOmotd = {
    "MOTD", INN_PATH_MOTD_NNRPD, NULL, false, "message of the day",
    "Message of the day text in UTF-8"
};

static LISTINFO *info[] = {
    &INFOactive,
    &INFOactivetimes,
    &INFOcounts,
    &INFOdistribs,
    &INFOheaders,
    &INFOsubs,
    &INFOdistribpats,
    &INFOgroups,
    &INFOmoderators,
    &INFOschema,
    &INFOmotd,
};


/*
**  List the overview schema (standard and extra fields).
*/
static void
cmd_list_schema(LISTINFO *lp, int ac UNUSED, char *av[] UNUSED)
{
    const struct cvector *standard;
    unsigned int i;

    Reply("%d %s\r\n", NNTP_OK_LIST, lp->Format);
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
**  List supported headers and metadata information.
*/
static void
cmd_list_headers(LISTINFO *lp, int ac, char *av[])
{
    bool range;

    range = (ac > 2 && strcasecmp(av[2], "RANGE") == 0);

    if (ac > 2 && (strcasecmp(av[2], "MSGID") != 0)
        && !range) {
        Reply("%d Syntax error in arguments\r\n", NNTP_ERR_SYNTAX);
        return;
    }
    Reply("%d %s\r\n", NNTP_OK_LIST, lp->Format);
    Printf(":\r\n");
    if (range) {
        /* These information are only known by the overview system,
         * and are only accessible with a range. */
        Printf(":bytes\r\n");
        Printf(":lines\r\n");
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
    int lo, hi, count, flag;

    if (PERMspecified) {
        grplist[0] = group;
        if (!PERMmatch(PERMreadlist, grplist))
            return false;
    }
    if (OVgroupstats(group, &lo, &hi, &count, &flag) && flag != NF_FLAG_ALIAS) {
        /* When the connected user has the right to locally post, mention it. */
        if (PERMaccessconf->locpost && (flag == NF_FLAG_IGNORE
                                        || flag == NF_FLAG_JUNK
                                        || flag == NF_FLAG_NOLOCAL))
            flag = NF_FLAG_OK;
        /* When a newsgroup is empty, the high water mark should be one less
         * than the low water mark according to RFC 3977. */
        if (count == 0)
            lo = hi + 1;
        Reply("%d %s\r\n", NNTP_OK_LIST, INFOactive.Format);
        Printf("%s %0*u %0*u %c\r\n", group, ARTNUMPRINTSIZE, hi,
               ARTNUMPRINTSIZE, lo, flag);
        Printf(".\r\n");
        return true;
    }
    return false;
}


/*
**  Main LIST function.
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
    int         lo, hi, count, flag;

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
        Reply("%d Unknown LIST keyword\r\n", NNTP_ERR_SYNTAX);
        return;
    }

    if (lp == &INFOactive) {
	if (ac == 3) {
	    wildarg = av[2];
            /* No need to parse the active file for a single group. */
            if (CMD_list_single(wildarg))
		return;
	}
    } else if (lp == &INFOgroups || lp == &INFOactivetimes || lp == &INFOcounts
               || lp == &INFOheaders || lp == &INFOsubs) {
	if (ac == 3)
	    wildarg = av[2];
    }

    /* Three arguments can be passed only when ACTIVE, ACTIVE.TIMES, COUNTS
     * HEADERS, NEWSGROUPS or SUBSCRIPTIONS keywords are used. */
    if (ac > 2 && !wildarg) {
        Reply("%d Unexpected wildmat or argument\r\n", NNTP_ERR_SYNTAX);
        return;
    }

    /* If a function is provided for the given keyword, we call it. */
    if (lp->impl != NULL) {
	lp->impl(lp, ac, av);
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
        Reply("%d No %s available\r\n",
              NNTP_ERR_UNAVAILABLE, lp->Items);
        /* Only the active and newsgroups files are required. */
        if (lp->Required || errno != ENOENT) {
            /* %m outputs strerror(errno). */
            syslog(L_ERROR, "%s can't fopen %s %m", Client.host, lp->File);
        }
        return;
    }

    Reply("%d %s\r\n", NNTP_OK_LIST, lp->Format);
    if (!PERMspecified && lp != &INFOmotd) {
	/* Optimize for unlikely case of no permissions and false default. */
	QIOclose(qp);
	Printf(".\r\n");
	return;
    }

    /* Set up group list terminator. */
    grplist[1] = NULL;

    /* Read lines, ignore long ones. */
    while ((p = QIOread(qp)) != NULL) {
        /* Check that the output does not break the NNTP protocol. */
        if (p[0] == '.' && p[1] != '.') {
            syslog(L_ERROR, "%s bad dot-stuffing in %s", Client.host, lp->File);
            continue;
        }
        if (lp == &INFOmotd) {
            if (is_valid_utf8(p)) {
                Printf("%s\r\n", p);
            } else {
                syslog(L_ERROR, "%s bad encoding in %s (UTF-8 expected)",
                       Client.host, lp->File);
            }
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
            if (*p != '\0' && *p != '#' && *p != ';' && *p != ' ') {
                if (is_valid_utf8(p)) {
                    Printf("%s\r\n", p);
                } else if (lp == &INFOdistribs) {
                    syslog(L_ERROR, "%s bad encoding in %s (UTF-8 expected)",
                           Client.host, lp->File);
                }
            }
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

        if (lp == &INFOcounts) {
            if (OVgroupstats(p, &lo, &hi, &count, &flag)) {
                /* When a newsgroup is empty, the high water mark should be
                 * one less than the low water mark according to RFC 3977. */
                if (count == 0)
                    lo = hi + 1;

                if (flag != NF_FLAG_ALIAS) {
                    Printf("%s %u %u %u %c\r\n", p, hi, lo, count,
                           PERMaccessconf->locpost
                           && (flag == NF_FLAG_IGNORE
                               || flag == NF_FLAG_JUNK
                               || flag == NF_FLAG_NOLOCAL)
                           ? NF_FLAG_OK : flag);
                } else if (savec != '\0') {
                    *save = savec;

                    if ((q = strrchr(p, NF_FLAG_ALIAS)) != NULL) {
                        *save = '\0';
                        Printf("%s %u %u %u %s\r\n", p, hi, lo, count, q);
                    }
                }
            }

            continue;
        }

	if (savec != '\0')
	    *save = savec;

        if (lp == &INFOactive) {
            /* When the connected user has the right to locally post, mention it. */
            if (PERMaccessconf->locpost && (q = strrchr(p, ' ')) != NULL) {
                q++;
                if (*q == NF_FLAG_IGNORE
                    || *q == NF_FLAG_JUNK
                    || *q == NF_FLAG_NOLOCAL)
                    *q = NF_FLAG_OK;
            }
        }

	Printf("%s\r\n", p);
    }
    QIOclose(qp);

    Printf(".\r\n");
}

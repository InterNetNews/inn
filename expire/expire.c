/*
**  Expire news articles.
*/

#include "portable/system.h"

#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <syslog.h>
#include <time.h>

#include "inn/hashtab.h"
#include "inn/history.h"
#include "inn/innconf.h"
#include "inn/inndcomm.h"
#include "inn/libinn.h"
#include "inn/messages.h"
#include "inn/newsuser.h"
#include "inn/paths.h"
#include "inn/storage.h"
#include "inn/tombstone.h"


typedef struct _EXPIRECLASS {
    time_t Keep;
    time_t Default;
    time_t Purge;
    bool Missing;
    bool ReportedMissing;
} EXPIRECLASS;

/*
**  Expire-specific stuff.
*/
#define MAGIC_TIME 49710.

static bool EXPtracing;
static bool EXPusepost;
static bool Ignoreselfexpire = false;
static FILE *EXPunlinkfile;
static EXPIRECLASS EXPclasses[NUM_STORAGE_CLASSES + 1];
static char *EXPreason;
static time_t EXPremember;
static time_t Now;
static time_t RealNow;

/* Statistics; for -v flag. */
static char *EXPgraph;
static int EXPverbose;
static long EXPprocessed;
static long EXPunlinked;
static long EXPtombstoned;
static long EXPallgone;
static long EXPstillhere;
static struct history *History;
static char *NHistory;

static void CleanupAndExit(bool Server, bool Paused, int x)
    __attribute__((__noreturn__));
static void Usage(void) __attribute__((__noreturn__));

static int EXPsplit(char *p, char sep, char **argv, int count);

enum KR {
    Keep,
    Remove
};


/*
**  Open a file or give up.
*/
static FILE *
EXPfopen(bool Unlink, const char *Name, const char *Mode, bool Needclean,
         bool Server, bool Paused)
{
    FILE *F;

    if (Unlink && unlink(Name) < 0 && errno != ENOENT)
        syswarn("cannot remove %s", Name);
    if ((F = fopen(Name, Mode)) == NULL) {
        syswarn("cannot open %s in %s mode", Name, Mode);
        if (Needclean)
            CleanupAndExit(Server, Paused, 1);
        else
            exit(1);
    }
    return F;
}


/*
**  Split a line at a specified field separator into a vector and return
**  the number of fields found, or -1 on error.
*/
static int
EXPsplit(char *p, char sep, char **argv, int count)
{
    int i;

    if (!p)
        return 0;

    while (*p == sep)
        ++p;

    if (*p == '\0')
        return 0;

    for (i = 1, *argv++ = p; *p;)
        if (*p++ == sep) {
            p[-1] = '\0';
            for (; *p == sep; p++)
                ;
            if (!*p)
                return i;
            if (++i == count)
                /* Overflow. */
                return -1;
            *argv++ = p;
        }
    return i;
}


/*
**  Parse a number field converting it into a "when did this start?".
**  This makes the "keep it" tests fast, but inverts the logic of
**  just about everything you expect.  Print a message and return false
**  on error.
*/
static bool
EXPgetnum(int line, char *word, time_t *v, const char *name)
{
    char *p;
    bool SawDot;
    double d;

    if (strcasecmp(word, "never") == 0) {
        *v = (time_t) 0;
        return true;
    }

    /* Check the number.  We don't have strtod yet. */
    for (p = word; ISWHITE(*p); p++)
        ;
    if (*p == '+' || *p == '-')
        p++;
    for (SawDot = false; *p; p++)
        if (*p == '.') {
            if (SawDot)
                break;
            SawDot = true;
        } else if (!isdigit((unsigned char) *p))
            break;
    if (*p) {
        warn("bad '%c' character in %s field on line %d", *p, name, line);
        return false;
    }
    d = atof(word);
    if (d > MAGIC_TIME)
        *v = (time_t) 0;
    else
        *v = Now - (time_t) (d * 86400.);
    return true;
}


/*
**  Parse the expiration control file.  Return true if okay.
*/
static bool
EXPreadfile(FILE *F)
{
    char *p;
    int i;
    int j;
    bool SawDefault;
    char buff[BUFSIZ];
    char *fields[7];

    /* Scan all lines. */
    EXPremember = -1;
    SawDefault = false;

    for (i = 0; i <= NUM_STORAGE_CLASSES; i++) {
        EXPclasses[i].ReportedMissing = false;
        EXPclasses[i].Missing = true;
    }

    for (i = 1; fgets(buff, sizeof buff, F) != NULL; i++) {
        if ((p = strchr(buff, '\n')) == NULL) {
            warn("line %d too long", i);
            return false;
        }
        *p = '\0';
        p = strchr(buff, '#');
        if (p)
            *p = '\0';
        else
            p = buff + strlen(buff);
        while (--p >= buff) {
            if (isspace((unsigned char) *p))
                *p = '\0';
            else
                break;
        }
        if (buff[0] == '\0')
            continue;
        if ((j = EXPsplit(buff, ':', fields, ARRAY_SIZE(fields))) == -1) {
            warn("too many fields on line %d", i);
            return false;
        }

        /* Expired-article remember line? */
        if (strcmp(fields[0], "/remember/") == 0) {
            if (j != 2) {
                warn("invalid format on line %d", i);
                return false;
            }
            if (EXPremember != -1) {
                warn("duplicate /remember/ on line %d", i);
                return false;
            }
            if (!EXPgetnum(i, fields[1], &EXPremember, "remember"))
                return false;
            continue;
        }

        /* Storage class line? */
        if (j == 4) {
            /* Is this the default line? */
            if (fields[0][0] == '*' && fields[0][1] == '\0') {
                if (SawDefault) {
                    warn("duplicate default on line %d", i);
                    return false;
                }
                j = NUM_STORAGE_CLASSES;
                SawDefault = true;
            } else {
                j = atoi(fields[0]);
                if ((j < 0) || (j >= NUM_STORAGE_CLASSES))
                    warn("bad storage class %d on line %d", j, i);
            }

            if (!EXPgetnum(i, fields[1], &EXPclasses[j].Keep, "keep")
                || !EXPgetnum(i, fields[2], &EXPclasses[j].Default, "default")
                || !EXPgetnum(i, fields[3], &EXPclasses[j].Purge, "purge"))
                return false;
            /* These were turned into offsets, so the test is the opposite
             * of what you think it should be.  If Purge isn't forever,
             * make sure it's greater than the other two fields. */
            if (EXPclasses[j].Purge) {
                /* Some value not forever; make sure other values are in range.
                 */
                if (EXPclasses[j].Keep
                    && EXPclasses[j].Keep < EXPclasses[j].Purge) {
                    warn("keep time longer than purge time on line %d", i);
                    return false;
                }
                if (EXPclasses[j].Default
                    && EXPclasses[j].Default < EXPclasses[j].Purge) {
                    warn("default time longer than purge time on line %d", i);
                    return false;
                }
            }
            EXPclasses[j].Missing = false;
            continue;
        }

        /* Regular expiration line -- right number of fields? */
        if (j != 5) {
            warn("bad format on line %d", i);
            return false;
        }
        continue; /* don't process this line--per-group expiry is done by
                     expireover */
    }

    return true;
}

/*
**  Should we keep the specified article?
*/
static enum KR
EXPkeepit(const TOKEN *token, time_t when, time_t Expires)
{
    EXPIRECLASS class;

    class = EXPclasses[token->class];
    if (class.Missing) {
        if (EXPclasses[NUM_STORAGE_CLASSES].Missing) {
            /* no default */
            if (!class.ReportedMissing) {
                warn("class definition for %d missing from control file,"
                     " assuming it should never expire",
                     token->class);
                EXPclasses[token->class].ReportedMissing = true;
            }
            return Keep;
        } else {
            /* use the default */
            class = EXPclasses[NUM_STORAGE_CLASSES];
            EXPclasses[token->class] = class;
        }
    }
    /* Bad posting date? */
    if (when > (RealNow + 86400)) {
        /* Yes -- force the article to go to right now */
        when = Expires ? class.Purge : class.Default;
    }
    if (EXPverbose > 2) {
        if (EXPverbose > 3)
            printf("%s age = %0.2f\n", TokenToText(*token),
                   (double) (Now - when) / 86400.);
        if (Expires == 0) {
            if (when <= class.Default)
                printf("%s too old (no exp)\n", TokenToText(*token));
        } else {
            if (when <= class.Purge)
                printf("%s later than purge\n", TokenToText(*token));
            if (when >= class.Keep)
                printf("%s earlier than min\n", TokenToText(*token));
            if (Now >= Expires)
                printf("%s later than header\n", TokenToText(*token));
        }
    }

    /* If no expiration, make sure it wasn't posted before the default. */
    if (Expires == 0) {
        if (when >= class.Default)
            return Keep;

        /* Make sure it's not posted before the purge cut-off and
         * that it's not due to expire. */
    } else {
        if (when >= class.Purge && (Expires >= Now || when >= class.Keep))
            return Keep;
    }
    return Remove;
}


/*
**  An article can be removed.  Either print a note, or actually remove it.
**  Also fill in the article size.
*/
static void
EXPremove(const TOKEN *token)
{
    /* Turn into a filename and get the size if we need it. */
    if (EXPverbose > 1)
        printf("\tunlink %s\n", TokenToText(*token));

    if (EXPtracing) {
        EXPunlinked++;
        printf("%s\n", TokenToText(*token));
        return;
    }

    EXPunlinked++;
    if (EXPunlinkfile) {
        fprintf(EXPunlinkfile, "%s\n", TokenToText(*token));
        if (!ferror(EXPunlinkfile))
            return;
        syswarn("cannot write to -z file (will ignore it for rest of run)");
        fclose(EXPunlinkfile);
        EXPunlinkfile = NULL;
    }
    if (!SMcancel(*token) && SMerrno != SMERR_NOENT && SMerrno != SMERR_UNINIT)
        warn("cannot unlink %s", TokenToText(*token));
}


/*
**  Load both tombstone logs into a hashset:
**    - ${pathdb}/expireover.tombstone : written by expireover/expirerm
**      (atomic .NEW -> final rename); unlinked after a successful
**      expire run
**    - ${pathdb}/cancels.tombstone    : appended continuously by innd
**      and sm for cancels outside the expireover pipeline; renamed
**      to .processing at load time and unlinked after a successful
**      expire run (rename-and-process avoids the read/truncate race
**      that drops cancels arriving during HISexpire)
**
**  At startup, also recovers any leftover .processing snapshot from a
**  previous expire run that crashed before unlinking.
**
**  consuming=true means this is a real run that will replace the
**  history file; we rename cancels.tombstone to .processing under
**  the consumer's lock so concurrent appenders cannot lose cancels
**  written between our read and the eventual unlink.
**  consuming=false (dry-run, tracing, or alternate-output expire) skips
**  the rename and reads the live file directly: no consume happens, so
**  there is nothing to atomically detach.  This avoids both the
**  TOCTOU race in restoring the snapshot and the operator surprise
**  of cancels.tombstone disappearing for the duration of a -x run.
**
**  Returns NULL if no tombstone could be loaded (no file present or
**  all reads failed); that is not an error, and expire just falls back
**  to the per-article SMretrieve check.
**  *out_expireover_path and *out_cancels_snapshot are set to the file
**  paths (caller frees and unlinks after a successful run); both are
**  NULL when consuming is false.
*/
static struct hash *
EXPloadtombstone(bool consuming, char **out_expireover_path,
                 char **out_cancels_snapshot)
{
    char *expireover_path = NULL;
    char *cancels_path = NULL;
    char *cancels_snapshot = NULL;
    char *leftover = NULL;
    struct hash *h = NULL;
    struct stat sb;
    unsigned long n_expireover = 0;
    unsigned long n_cancels = 0;
    unsigned long n_leftover = 0;
    bool expireover_present = false;
    bool leftover_present = false;
    bool cancels_present = false;

    /* Default outputs to NULL; only set on the success-return path
       below.  This avoids double-frees if a future maintainer adds an
       early return between here and that single point. */
    *out_expireover_path = NULL;
    *out_cancels_snapshot = NULL;

    expireover_path = concatpath(innconf->pathdb, "expireover.tombstone");
    cancels_path = concatpath(innconf->pathdb, "cancels.tombstone");
    leftover = concat(cancels_path, ".processing", (char *) 0);

    /* Probe each file's presence (independent of content) so we can
       distinguish "tombstone subsystem is actively tracking but had
       no cancels this cycle" (trust the empty hashset, skip
       SMretrieve for everything) from "no files at all" (subsystem
       not in use, fall back to slow path).  Without this, a cycle
       with zero cancels would needlessly run SMretrieve on every
       history entry. */
    if (stat(expireover_path, &sb) == 0)
        expireover_present = true;
    if (stat(leftover, &sb) == 0)
        leftover_present = true;
    if (stat(cancels_path, &sb) == 0)
        cancels_present = true;

    /* Initial size: tokens are typically ~38 bytes per line; we let the
       hash expand if we underestimate.  4096 covers the common case. */
    h = tombstone_hash_create(4096);

    /* Recover a leftover .processing from a previous run that crashed
       between rename and unlink. */
    n_leftover = tombstone_read(h, leftover, NULL);

    /* On a real run, atomically snapshot the live cancels.tombstone,
       then read it.  On dry-run / tracing / alt-output, just read
       the live file directly: no consume happens, so there is no
       need to detach it (and detach-then-restore has a TOCTOU race
       with concurrent appenders). */
    if (consuming) {
        cancels_snapshot = tombstone_rename_for_processing(cancels_path);
        if (cancels_snapshot != NULL)
            n_cancels = tombstone_read(h, cancels_snapshot, NULL);
    } else {
        n_cancels = tombstone_read(h, cancels_path, NULL);
    }

    /* Read the expireover-side log. */
    n_expireover = tombstone_read(h, expireover_path, NULL);

    if (EXPverbose)
        printf("Loaded %lu + %lu + %lu tombstone entries (%lu unique)\n",
               n_expireover, n_cancels, n_leftover, hash_count(h));

    /* Reconcile the leftover with the active snapshot.  Only relevant
       on a real consuming run; dry-run leaves the leftover where it
       is for the next consuming run to pick up.
       (a) leftover empty: just free the path string
       (b) leftover non-empty + no live snapshot: promote leftover to
           be the snapshot so the caller's single unlink covers it
       (c) leftover non-empty + live snapshot: unlink the leftover now
           (its contents are already in the hashset); the snapshot
           remains and is unlinked on success */
    if (consuming) {
        if (n_leftover > 0 && cancels_snapshot == NULL) {
            cancels_snapshot = leftover;
            leftover = NULL;
        } else if (n_leftover > 0) {
            if (unlink(leftover) < 0)
                syswarn("can't unlink %s", leftover);
        } else if (leftover_present && cancels_snapshot == NULL) {
            /* Leftover existed but was empty.  Treat it as the
               snapshot so the caller cleans it up after a
               successful run. */
            cancels_snapshot = leftover;
            leftover = NULL;
        } else if (leftover_present) {
            /* Both leftover and live snapshot existed; leftover
               empty.  Unlink it now; snapshot will be cleaned up
               on success. */
            if (unlink(leftover) < 0)
                syswarn("can't unlink %s", leftover);
        }
    }
    free(leftover);
    leftover = NULL;

    /* Decide between "fall back to slow path" (return NULL) and
       "trust the (possibly empty) tombstone".  A file that exists
       (regardless of content) is taken as evidence the tombstone
       subsystem is active for this site: innd/sm/expireover are
       writing it.  An empty active tombstone correctly says "nothing
       was cancelled in the last cycle"; trust it.  In dry-run mode
       cancels.tombstone is not renamed, so cancels_snapshot stays
       NULL; use the cancels_present probe instead. */
    if (!expireover_present && !leftover_present && !cancels_present
        && cancels_snapshot == NULL) {
        hash_free(h);
        free(expireover_path);
        free(cancels_path);
        return NULL;
    }

    /* Single success-return: hand both paths to the caller for
       cleanup after a successful run. */
    free(cancels_path);
    *out_expireover_path = expireover_path;
    *out_cancels_snapshot = cancels_snapshot;
    return h;
}


/*
**  Do the work of expiring one line.
**  Returns true when the article should be kept for the time being.
*/
static bool
EXPdoline(void *cookie, time_t arrived, time_t posted, time_t expires,
          TOKEN *token)
{
    struct hash *tombstone = (struct hash *) cookie;
    time_t when;
    bool HasSelfexpire = false;
    bool Selfexpired = false;
    bool selfexpiring;
    ARTHANDLE *article;
    enum KR kr;
    bool r;

    /* Tombstone fast path: if expireover already cancelled this article,
       drop the history entry without doing any storage I/O.  Bump
       EXPunlinked too so the news.daily summary's "Articles dropped"
       count matches the slow path's accounting (EXPremove also bumps
       it). */
    if (tombstone != NULL && hash_lookup(tombstone, token) != NULL) {
        EXPprocessed++;
        if (EXPverbose > 3)
            printf("%s (tombstoned by expireover)\n", TokenToText(*token));
        EXPallgone++;
        EXPunlinked++;
        EXPtombstoned++;
        return false;
    }

    selfexpiring = SMprobe(SELFEXPIRE, token, NULL);

    if (innconf->groupbaseexpiry || selfexpiring) {
        if (tombstone != NULL && !selfexpiring) {
            /* Backend does not self-expire and the tombstone log is
               complete: not in the log means the article still exists.
               Skip the SMretrieve to avoid a per-article syscall (the
               main speedup for tradspool / timehash / timecaf). */
            HasSelfexpire = true;
            Selfexpired = false;
        } else if ((article = SMretrieve(*token, RETR_STAT))
                   == (ARTHANDLE *) NULL) {
            HasSelfexpire = true;
            Selfexpired = true;
        } else {
            /* The article is still alive, free it. */
            SMfreearticle(article);
            /* Per-group expiry is done by expireover, remember!
             * That's why if groupbaseexpiry is set, expire won't verify
             * the expiration rules set in expire.ctl. */
            if (innconf->groupbaseexpiry || !Ignoreselfexpire)
                HasSelfexpire = true;
        }
    }

    if (EXPusepost && posted != 0)
        when = posted;
    else
        when = arrived;
    EXPprocessed++;

    if (HasSelfexpire) {
        if (Selfexpired || token->type == TOKEN_EMPTY) {
            if (EXPverbose > 3)
                printf("%s (to remember or forget, not existing)\n",
                       TokenToText(*token));
            EXPallgone++;
            r = false;
        } else {
            if (EXPverbose > 3)
                printf("%s (to keep, may self-expire)\n", TokenToText(*token));
            EXPstillhere++;
            r = true;
        }
    } else {
        kr = EXPkeepit(token, when, expires);
        if (kr == Remove) {
            if (EXPverbose > 3)
                printf("%s (to remember or forget)\n", TokenToText(*token));
            EXPremove(token);
            EXPallgone++;
            r = false;
        } else {
            if (EXPverbose > 3)
                printf("%s (to keep)\n", TokenToText(*token));
            EXPstillhere++;
            r = true;
        }
    }

    return r;
}


/*
**  Clean up link with the server and exit.
*/
static void
CleanupAndExit(bool Server, bool Paused, int x)
{
    FILE *F;

    if (Server) {
        ICCreserve("");
        if (Paused && ICCgo(EXPreason) != 0) {
            syswarn("cannot unpause server");
            x = 1;
        }
    }
    if (Server && ICCclose() < 0) {
        syswarn("cannot close communication link to server");
        x = 1;
    }
    if (EXPunlinkfile && fclose(EXPunlinkfile) == EOF) {
        syswarn("cannot close -z file");
        x = 1;
    }

    /* Report stats. */
    if (EXPverbose) {
        printf("Article lines processed %8ld\n", EXPprocessed);
        printf("Articles retained       %8ld\n", EXPstillhere);
        printf("Entries expired         %8ld\n", EXPallgone);
        if (!innconf->groupbaseexpiry)
            printf("Articles dropped        %8ld\n", EXPunlinked);
        if (innconf->expiretombstone && innconf->groupbaseexpiry)
            printf("Tombstone hits          %8ld\n", EXPtombstoned);
    }

    /* Append statistics to a summary file */
    if (EXPgraph) {
        F = EXPfopen(false, EXPgraph, "a", false, false, false);
        fprintf(F, "%ld %ld %ld %ld %ld\n", (long) Now, EXPprocessed,
                EXPstillhere, EXPallgone, EXPunlinked);
        fclose(F);
    }

    SMshutdown();
    HISclose(History);
    if (EXPreason != NULL)
        free(EXPreason);

    if (NHistory != NULL)
        free(NHistory);
    closelog();
    exit(x);
}

/*
**  Print a usage message and exit.
*/
static void
Usage(void)
{
    fprintf(stderr, "Usage: expire [flags] [expire.ctl]\n");
    exit(1);
}


int
main(int ac, char *av[])
{
    int i;
    char *p;
    FILE *F;
    char *HistoryText;
    const char *NHistoryPath = NULL;
    const char *NHistoryText = NULL;
    char *EXPhistdir;
    char buff[SMBUF];
    bool Server;
    bool Bad;
    bool IgnoreOld;
    bool Writing;
    bool UnlinkFile;
    bool val;
    time_t TimeWarp;
    size_t Size = 0;

    /* First thing, set up logging and our identity. */
    openlog("expire", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);
    message_program_name = "expire";

    /* Set defaults. */
    Server = true;
    IgnoreOld = false;
    Writing = true;
    TimeWarp = 0;
    UnlinkFile = false;

    if (!innconf_read(NULL))
        exit(1);

    /* Warn about expiretombstone-without-groupbaseexpiry: in that
       configuration there is no consumable tombstone since OVEXPremove
       (which writes one of the two logs) is never called.  The
       cancels.tombstone written by innd/sm could still be loaded, but
       only with groupbaseexpiry to make the result coherent with
       expire's overall semantics. */
    if (innconf->expiretombstone && !innconf->groupbaseexpiry)
        notice("expiretombstone has no effect when groupbaseexpiry"
               " is false - check your inn.conf configuration");

    HistoryText = concatpath(innconf->pathdb, INN_PATH_HISTORY);

    umask(NEWSUMASK);

    /* find the default history file directory */
    EXPhistdir = xstrdup(HistoryText);
    p = strrchr(EXPhistdir, '/');
    if (p != NULL) {
        *p = '\0';
    }

    /* Parse JCL. */
    while ((i = getopt(ac, av, "d:f:g:h:iNnpr:s:tv:w:xz:")) != EOF)
        switch (i) {
        default:
            Usage();
            /* NOTREACHED */
        case 'd':
            NHistoryPath = optarg;
            break;
        case 'f':
            NHistoryText = optarg;
            break;
        case 'g':
            EXPgraph = optarg;
            break;
        case 'h':
            HistoryText = optarg;
            break;
        case 'i':
            IgnoreOld = true;
            break;
        case 'N':
            Ignoreselfexpire = true;
            break;
        case 'n':
            Server = false;
            break;
        case 'p':
            EXPusepost = true;
            break;
        case 'r':
            EXPreason = xstrdup(optarg);
            break;
        case 's':
            Size = atoi(optarg);
            break;
        case 't':
            EXPtracing = true;
            break;
        case 'v':
            EXPverbose = atoi(optarg);
            break;
        case 'w':
            TimeWarp = (time_t) (atof(optarg) * 86400.);
            break;
        case 'x':
            Writing = false;
            break;
        case 'z':
            EXPunlinkfile = EXPfopen(true, optarg, "a", false, false, false);
            UnlinkFile = true;
            break;
        }
    ac -= optind;
    av += optind;
    if ((ac != 0 && ac != 1))
        Usage();

    /* if EXPtracing is set, then pass in a path, this ensures we
     * don't replace the existing history files */
    if (EXPtracing || NHistoryText || NHistoryPath) {
        if (NHistoryPath == NULL)
            NHistoryPath = innconf->pathdb;
        if (NHistoryText == NULL)
            NHistoryText = INN_PATH_HISTORY;
        NHistory = concatpath(NHistoryPath, NHistoryText);
    } else {
        NHistory = NULL;
    }

    time(&Now);
    RealNow = Now;
    Now += TimeWarp;

    /* Change to the runasuser user and runasgroup group if necessary. */
    ensure_news_user_grp(true, true);

    /* Parse the control file. */
    if (av[0]) {
        if (strcmp(av[0], "-") == 0)
            F = stdin;
        else
            F = EXPfopen(false, av[0], "r", false, false, false);
    } else {
        char *path;

        path = concatpath(innconf->pathetc, INN_PATH_EXPIRECTL);
        F = EXPfopen(false, path, "r", false, false, false);
        free(path);
    }
    if (!EXPreadfile(F)) {
        fclose(F);
        die("format error in expire.ctl");
    }
    fclose(F);

    /* Set up the link, reserve the lock. */
    if (Server) {
        if (EXPreason == NULL) {
            snprintf(buff, sizeof(buff), "Expiring process %ld",
                     (long) getpid());
            EXPreason = xstrdup(buff);
        }
    } else {
        EXPreason = NULL;
    }

    if (Server) {
        /* If we fail, leave evidence behind. */
        if (ICCopen() < 0) {
            syswarn("cannot open channel to server");
            CleanupAndExit(false, false, 1);
        }
        if (ICCreserve((char *) EXPreason) != 0) {
            warn("cannot reserve server");
            CleanupAndExit(false, false, 1);
        }
    }

    History = HISopen(HistoryText, innconf->hismethod, HIS_RDONLY);
    if (!History) {
        warn("cannot open history");
        CleanupAndExit(Server, false, 1);
    }

    /* Ignore failure on the HISctl()s, if the underlying history
     * manager doesn't implement them its not a disaster */
    HISctl(History, HISCTLS_IGNOREOLD, &IgnoreOld);
    if (Size != 0) {
        HISctl(History, HISCTLS_NPAIRS, &Size);
    }

    val = true;
    if (!SMsetup(SM_RDWR, (void *) &val)
        || !SMsetup(SM_PREOPEN, (void *) &val)) {
        warn("cannot set up storage manager");
        CleanupAndExit(Server, false, 1);
    }
    if (!SMinit()) {
        warn("cannot initialize storage manager: %s", SMerrorstr);
        CleanupAndExit(Server, false, 1);
    }
    if (chdir(EXPhistdir) < 0) {
        syswarn("cannot chdir to %s", EXPhistdir);
        CleanupAndExit(Server, false, 1);
    }

    /* Consume the tombstone logs produced since the last expire run.
       Lets EXPdoline drop history entries for articles cancelled by
       expireover, expirerm, innd, or sm without per-article SMretrieve
       calls.  Gated on groupbaseexpiry as well: if the admin flipped
       that off since the last expireover, a stale tombstone written
       under the old config could drop history entries for articles
       still alive. */
    {
        bool consuming = Writing && !EXPtracing && NHistory == NULL;
        char *expireover_path = NULL;
        char *cancels_snapshot = NULL;
        struct hash *tombstone = NULL;

        if (innconf->expiretombstone && innconf->groupbaseexpiry)
            tombstone = EXPloadtombstone(consuming, &expireover_path,
                                         &cancels_snapshot);

        Bad = HISexpire(History, NHistory, EXPreason, Writing, tombstone,
                        EXPremember, EXPdoline)
              == false;

        if (tombstone != NULL)
            hash_free(tombstone);

        /* On a successful real run, unlink both consumed snapshots
           and seed header-only successors.  Dry-run / tracing /
           alt-output skipped the rename in the loader so there are
           no snapshot paths to clean up. */
        if (!Bad && consuming) {
            if (expireover_path != NULL) {
                if (unlink(expireover_path) < 0 && errno != ENOENT)
                    syswarn("can't unlink %s", expireover_path);
            }
            if (cancels_snapshot != NULL) {
                if (unlink(cancels_snapshot) < 0 && errno != ENOENT)
                    syswarn("can't unlink %s", cancels_snapshot);
            }

            /* Leave header-only successors behind so both tombstone
               files exist symmetrically in pathdb after every
               successful expire.  For cancels.tombstone this keeps
               nnrpd's per-connection fast path active through quiet
               inter-cycle periods.  For expireover.tombstone, it is
               cosmetic but matches the file's documented presence;
               expireover will overwrite the header-only file the
               next time it runs.  Runs every consuming cycle when
               the feature is enabled, including the brand-new-
               install case where EXPloadtombstone returned NULL and
               there was no snapshot to consume.  Idempotent when
               the file already starts with the header. */
            if (innconf->expiretombstone && innconf->groupbaseexpiry) {
                char *path;

                path = concatpath(innconf->pathdb, "cancels.tombstone");
                tombstone_ensure_header(path);
                free(path);
                path = concatpath(innconf->pathdb, "expireover.tombstone");
                tombstone_ensure_header(path);
                free(path);
            }
        }
        free(expireover_path);
        free(cancels_snapshot);
    }

    if (UnlinkFile && EXPunlinkfile == NULL)
        /* Got -z but file was closed; oops. */
        Bad = true;

    /* If we're done okay, and we're not tracing, slip in the new files. */
    if (EXPverbose) {
        if (Bad)
            printf("Expire errors: history files not updated.\n");
        if (EXPtracing)
            printf("Expire tracing: history files not updated.\n");
    }

    if (!Bad && NHistory != NULL) {
        snprintf(buff, sizeof(buff), "%s.n.done", NHistory);
        fclose(EXPfopen(false, buff, "w", true, Server, false));
    }

    CleanupAndExit(Server, !Bad, Bad ? 1 : 0);
    /* NOTREACHED */
    abort();
}

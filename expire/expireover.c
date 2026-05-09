/*
**  Expire the overview database.
**
**  This program handles the nightly expiration of overview information.  If
**  groupbaseexpiry is true, this program also handles the removal of
**  articles that have expired.  It's separate from the process that scans
**  and expires the history file.
*/

#include "portable/system.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <syslog.h>
#include <time.h>

#include "inn/bloom.h"
#include "inn/buffer.h"
#include "inn/history.h"
#include "inn/innconf.h"
#include "inn/libinn.h"
#include "inn/messages.h"
#include "inn/newsuser.h"
#include "inn/ov.h"
#include "inn/paths.h"
#include "inn/qio.h"
#include "inn/storage.h"
#include "inn/tombstone.h"

/* OVtombstonefile is an internal of the storage library (declared in
 * storage/ovinterface.h), but expireover owns its lifecycle, so we
 * declare it here rather than exporting it via the public ov.h header. */
extern FILE *OVtombstonefile;

static const char usage[] = "\
Usage: expireover [-ekNpqs] [-f file] [-w offset] [-z rmfile] [-Z lowmarkfile]\n";

/* Set to 1 if we've received a signal; expireover then terminates after
   finishing the newsgroup that it's working on (this prevents corruption of
   the overview by killing expireover). */
static volatile sig_atomic_t signalled = 0;


/*
**  Handle a fatal signal and set signalled.  Restore the default signal
**  behavior after receiving a signal so that repeating the signal will kill
**  the program immediately.
*/
static void
fatal_signal(int sig)
{
    signalled = 1;
    xsignal(sig, SIG_DFL);
}


/*
**  Callback for HISwalk that adds history entries with storage tokens to the
**  Bloom filter.  Entries without tokens (remembered message-IDs) are skipped
**  so that OVhisthasmsgid correctly identifies them as missing.
*/
static bool
build_bloom_cb(void *cookie, const HASH *hash, time_t arrived UNUSED,
               time_t posted UNUSED, time_t expires UNUSED, const TOKEN *token)
{
    if (token != NULL)
        bloom_add(cookie, hash);
    return true;
}

/*
**  Verify each line of an in-memory tombstone buffer via SMretrieve and
**  append survivors to kept.  An entry survives if SMretrieve confirms
**  the article is gone (SMERR_NOENT) or returns a non-NOENT error
**  (SMERR_UNINIT, EIO, etc., where we cannot tell whether the article
**  is alive); a transient failure must not silently discard a valid
**  cancel record.  Live articles (SMretrieve returns non-NULL) are
**  dropped: we must not re-tombstone an article that is still on disk.
*/
static void
verify_tombstone_lines(char *raw, ssize_t got, struct buffer *kept)
{
    char *entry, *save_p;

    if (got <= 0)
        return;
    raw[got] = '\0';
    entry = strtok_r(raw, "\n", &save_p);
    while (entry != NULL) {
        size_t len = strlen(entry);
        if (len > 0 && entry[len - 1] == '\r')
            entry[--len] = '\0';
        if (len > 0 && IsToken(entry)) {
            TOKEN t = TextToToken(entry);
            ARTHANDLE *art = SMretrieve(t, RETR_STAT);
            if (art != NULL) {
                /* Article still on disk; drop from tombstone to
                   avoid orphaning history for it. */
                SMfreearticle(art);
            } else {
                /* Either confirmed gone (SMERR_NOENT) or transient
                   error (SMERR_UNINIT, EIO, etc.).  Keep the entry
                   either way: confirmed-gone is the normal case,
                   and on transient error we cannot tell whether
                   the article is alive, so preserve the record so
                   the next run can re-evaluate.  Silently dropping
                   on transient error would leak cancels across a
                   storage outage. */
                buffer_append(kept, entry, len);
                buffer_append(kept, "\n", 1);
            }
        }
        entry = strtok_r(NULL, "\n", &save_p);
    }
}


/*
**  Read an entire file into a freshly allocated buffer.  Used for
**  leftover-recovery of expireover.tombstone (from a cycle where
**  expirerm finalized but expire never consumed) and the .NEW
**  leftover from a crashed prior run.  Returns NULL on missing or
**  empty file (sets *out_size to 0); returns a malloc'd buffer with
**  the file content otherwise (caller frees).
*/
static char *
slurp_tombstone(const char *path, ssize_t *out_size)
{
    int fd;
    struct stat sb;
    char *raw;
    ssize_t got = 0, n;

    *out_size = 0;
    fd = open(path, O_RDONLY);
    if (fd < 0)
        return NULL;
    if (fstat(fd, &sb) < 0 || sb.st_size <= 0) {
        close(fd);
        return NULL;
    }
    raw = xmalloc(sb.st_size + 1);
    while (got < sb.st_size) {
        n = read(fd, raw + got, sb.st_size - got);
        if (n <= 0)
            break;
        got += n;
    }
    close(fd);
    if (got <= 0) {
        free(raw);
        return NULL;
    }
    *out_size = got;
    return raw;
}


int
main(int argc, char *argv[])
{
    int option, low;
    char *line, *p;
    QIOSTATE *qp;
    bool value;
    OVGE ovge;
    char *active_path = NULL;
    char *lowmark_path = NULL;
    char *path;
    char *tombstone_path = NULL;
    char *tombstone_path_new = NULL;
    FILE *lowmark = NULL;
    bool purge_deleted = false;
    bool always_stat = false;
    bool tombstone_clean = true;
    struct history *history;
    struct bloom_filter *bloom = NULL;
    struct bloom_filter *null_bloom = NULL;

    /* First thing, set up logging and our identity. */
    openlog("expireover", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);
    message_program_name = "expireover";

    /* Set up some default options for group-based expiration, although none
       of these will be used if groupbaseexpiry isn't true. */
    ovge.earliest = false;
    ovge.keep = false;
    ovge.ignoreselfexpire = false;
    ovge.usepost = false;
    ovge.quiet = false;
    ovge.timewarp = 0;
    ovge.filename = NULL;
    ovge.delayrm = false;

    /* Parse the command-line options. */
    while ((option = getopt(argc, argv, "ef:kNpqsw:z:Z:")) != EOF) {
        switch (option) {
        case 'e':
            ovge.earliest = true;
            break;
        case 'f':
            active_path = xstrdup(optarg);
            break;
        case 'k':
            ovge.keep = true;
            break;
        case 'N':
            ovge.ignoreselfexpire = true;
            break;
        case 'p':
            ovge.usepost = true;
            break;
        case 'q':
            ovge.quiet = true;
            break;
        case 's':
            always_stat = true;
            break;
        case 'w':
            ovge.timewarp = (float) (atof(optarg) * 86400.);
            break;
        case 'z':
            ovge.filename = optarg;
            ovge.delayrm = true;
            break;
        case 'Z':
            lowmark_path = optarg;
            break;
        default:
            fprintf(stderr, "%s", usage);
            exit(1);
        }
    }
    if (ovge.earliest && ovge.keep)
        die("-e and -k cannot be specified at the same time");

    /* Initialize innconf. */
    if (!innconf_read(NULL))
        exit(1);

    /* Warn about expiretombstone-without-groupbaseexpiry: in that
       configuration OVEXPremove is never called so no entries are
       ever appended.  The setting is silently ignored.  Operators
       reaching for the speedup deserve to know it's not active. */
    if (innconf->expiretombstone && !innconf->groupbaseexpiry)
        notice("expiretombstone has no effect when groupbaseexpiry"
               " is false - check your inn.conf configuration");

    /* Change to the runasuser user and runasgroup group if necessary. */
    ensure_news_user_grp(true, true);

    /* Initialize the lowmark file, if one was requested. */
    if (lowmark_path != NULL) {
        if (unlink(lowmark_path) < 0 && errno != ENOENT)
            syswarn("can't remove %s", lowmark_path);
        lowmark = fopen(lowmark_path, "a");
        if (lowmark == NULL)
            sysdie("can't open %s", lowmark_path);
    }

    /* Set up the path to the list of newsgroups we're going to use and open
       that file.  This could be stdin. */
    if (active_path == NULL) {
        active_path = concatpath(innconf->pathdb, INN_PATH_ACTIVE);
        purge_deleted = true;
    }
    if (strcmp(active_path, "-") == 0) {
        qp = QIOfdopen(fileno(stdin));
        if (qp == NULL)
            sysdie("can't reopen stdin");
    } else {
        qp = QIOopen(active_path);
        if (qp == NULL)
            sysdie("can't open active file (%s)", active_path);
    }
    free(active_path);

    /* open up the history manager */
    path = concatpath(innconf->pathdb, INN_PATH_HISTORY);
    history = HISopen(path, innconf->hismethod, HIS_RDONLY);
    free(path);

    /* Initialize the storage manager.  We only need to initialize it in
       read/write mode if we're not going to be writing a separate file for
       the use of fastrm. */
    if (!ovge.delayrm) {
        value = true;
        if (!SMsetup(SM_RDWR, &value))
            die("can't setup storage manager read/write");
    }
    value = true;
    if (!SMsetup(SM_PREOPEN, &value))
        die("can't setup storage manager");
    if (!SMinit())
        die("can't initialize storage manager: %s", SMerrorstr);

    /* Initialize and configure the overview subsystem. */
    if (!OVopen(OV_READ | OV_WRITE))
        die("can't open overview database");
    if (innconf->groupbaseexpiry) {
        time(&ovge.now);
        if (!OVctl(OVGROUPBASEDEXPIRE, &ovge))
            die("can't configure group-based expire");
    }
    if (!OVctl(OVSTATALL, &always_stat))
        die("can't configure overview stat behavior");

    /* Open the tombstone log.  OVEXPremove appends a line per article
       it cancels (inline) or schedules for removal via the rm file
       (delayrm).  In delayrm mode the .NEW -> final rename is performed
       by expirerm after fastrm succeeds (see expirerm.in); in inline
       mode this process performs the rename below.  Skipped when
       groupbaseexpiry is false because OVEXPremove is not called in
       that mode, and when the admin has disabled the feature via
       expiretombstone.

       Concurrency model: open with O_RDWR|O_CREAT (no truncate), then
       take an exclusive *non-blocking* fcntl POSIX lock via
       inn_lock_file.  Non-blocking is required for correctness, not
       just performance: the truncate below would destroy the previous
       holder's content if we ever waited for the lock.  If we cannot
       acquire the lock, another expireover is running; disable
       tombstone writing for this run and proceed with normal
       expiration.  Manual concurrent invocations are protected
       against tombstone corruption; news.daily's shlock prevents
       them in normal flow.  The lock is released by fclose at
       finalize time.

       Leftover handling: two recovery paths, both verified per-token
       with SMretrieve(RETR_STAT) before merging into the new .NEW.
       (1) An existing .NEW file means a previous run crashed or
       whose expirerm failed.  (2) An existing final tombstone
       (without a .NEW) means the previous cycle's expirerm promoted
       it but expire never consumed.  Naively truncating .NEW or
       overwriting the final would lose cancels that genuinely
       happened, while preserving them blindly would risk dropping
       history for articles still on disk (fastrm failures).  Per-
       token verification keeps confirmed-gone entries (SMERR_NOENT)
       and entries with transient errors (SMERR_UNINIT, EIO, ...) so
       the next run can re-evaluate them without silent loss; live
       articles are dropped.  Cost is one stat per leftover token,
       only on the rare crash-recovery path. */
    if (innconf->expiretombstone && innconf->groupbaseexpiry) {
        int fd;

        tombstone_path = concatpath(innconf->pathdb, "expireover.tombstone");
        tombstone_path_new = concat(tombstone_path, ".NEW", (char *) 0);
        fd = open(tombstone_path_new, O_RDWR | O_CREAT, 0664);
        if (fd < 0) {
            syswarn("can't open tombstone log %s", tombstone_path_new);
        } else if (!inn_lock_file(fd, INN_LOCK_WRITE, false)) {
            /* Distinguish contention (another expireover holds it)
               from other lock errors (NFS lockd outage, kernel
               resource exhaustion, etc.). */
            if (errno == EAGAIN || errno == EACCES) {
                warn("another expireover holds the tombstone log lock"
                     " on %s; disabling tombstone for this run",
                     tombstone_path_new);
            } else {
                syswarn("can't lock %s; disabling tombstone for this"
                        " run",
                        tombstone_path_new);
            }
            close(fd);
            fd = -1;
        } else {
            /* Recover leftover content under the lock, filter through
               SMretrieve, then truncate .NEW and rewrite.  All under
               the same fd/lock so no other writer can race.  Two
               sources to recover:

                 (a) the existing .NEW file: residue from a prior
                     expireover run that crashed, or whose expirerm
                     never promoted .NEW to final.
                 (b) the existing final tombstone: residue from a
                     prior cycle where expirerm did promote .NEW to
                     final but expire never ran (or never finished)
                     so the entries were never consumed.  Without
                     this recovery, this expireover's end-of-run
                     rename(.NEW, final) would silently overwrite
                     the unconsumed file, dropping its records.

               Both use verify_tombstone_lines, which preserves
               entries on transient SMretrieve errors so a momentary
               storage outage doesn't silently lose cancel records. */
            struct buffer kept;
            struct stat sb;
            char *raw;
            ssize_t got = 0;

            kept.data = NULL;
            kept.size = kept.used = kept.left = 0;

            /* Recover unconsumed final tombstone first. */
            raw = slurp_tombstone(tombstone_path, &got);
            if (raw != NULL) {
                verify_tombstone_lines(raw, got, &kept);
                free(raw);
                /* Unlink whether or not we extracted entries: the
                   active .NEW (after this run's rename) becomes the
                   new authoritative final.  Preserving the old one
                   would risk a future expireover folding the same
                   entries in twice. */
                if (unlink(tombstone_path) < 0 && errno != ENOENT)
                    syswarn("can't unlink leftover %s", tombstone_path);
            }

            /* Recover .NEW leftover. */
            if (fstat(fd, &sb) == 0 && sb.st_size > 0) {
                raw = xmalloc(sb.st_size + 1);
                got = 0;
                while (got < sb.st_size) {
                    ssize_t n = read(fd, raw + got, sb.st_size - got);
                    if (n <= 0)
                        break;
                    got += n;
                }
                verify_tombstone_lines(raw, got, &kept);
                free(raw);
            }
            if (ftruncate(fd, 0) < 0) {
                syswarn("can't truncate %s", tombstone_path_new);
                close(fd);
                fd = -1;
            } else if (lseek(fd, 0, SEEK_SET) < 0) {
                syswarn("can't seek %s", tombstone_path_new);
                close(fd);
                fd = -1;
            } else {
                OVtombstonefile = fdopen(fd, "w");
                if (OVtombstonefile == NULL) {
                    syswarn("can't fdopen %s", tombstone_path_new);
                    close(fd);
                    fd = -1;
                } else {
                    /* Format marker.  Readers tolerate comment lines
                       (lines starting with #), so any future format
                       change can be detected by inspecting this
                       header without breaking earlier readers. */
                    if (fputs(TOMBSTONE_HEADER, OVtombstonefile) == EOF)
                        syswarn("can't write header to %s",
                                tombstone_path_new);
                    if (kept.left > 0) {
                        /* Replay verified leftover entries before
                           this run's new entries are appended by
                           OVEXPremove.  (struct buffer convention:
                           appended data lives at data[used ..
                           used+left]; used==0 here so we start at
                           data.) */
                        if (fwrite(kept.data + kept.used, 1, kept.left,
                                   OVtombstonefile)
                                != kept.left
                            || fflush(OVtombstonefile) == EOF)
                            syswarn("can't write recovered leftover"
                                    " to %s",
                                    tombstone_path_new);
                    }
                }
            }
            free(kept.data);
        }
        if (OVtombstonefile == NULL) {
            free(tombstone_path);
            free(tombstone_path_new);
            tombstone_path = NULL;
            tombstone_path_new = NULL;
        }
    }

    /* Set up signal handlers before the Bloom walk, which can take several
       minutes on very large history files. */
    xsignal(SIGTERM, fatal_signal);
    xsignal(SIGINT, fatal_signal);
    xsignal(SIGHUP, fatal_signal);

    /* Build a Bloom filter from the history file for fast existence checks.
       This replaces millions of random pread() calls into the history file
       with a single sequential read, making expireover feasible on large
       spools (1B+ articles).  The Bloom filter is used as a positive-only
       cache: hits skip the slow history lookup, misses fall through to
       HISlookup for correctness (handles articles added after the walk). */
    if (innconf->expirebloomfp > 0 && !always_stat) {
        struct stat st;
        char *histpath;
        size_t estimated = 0;
        /* Minimum history line: 34 (hash) + 1 (tab) + 1 (arrived)
         * + 1 (newline).  Dividing file size by this gives a conservative
         * overestimate of entries, which is what we want for Bloom sizing. */
        const size_t min_history_line = 37;

        histpath = concatpath(innconf->pathdb, INN_PATH_HISTORY);
        if (stat(histpath, &st) == 0)
            estimated = st.st_size / min_history_line;
        else
            warn("can't stat %s, Bloom filter will be undersized", histpath);
        bloom = bloom_create(estimated, innconf->expirebloomfp);
        if (!HISwalk(history, NULL, bloom, build_bloom_cb)) {
            warn("can't walk history for Bloom filter, using per-article"
                 " lookups");
            bloom_free(bloom);
            bloom = NULL;
        }
        OVctl(OVTOKENCACHE, &bloom);
        free(histpath);
    }

    /* Loop through each line of the input file and process each group,
       writing data to the lowmark file if desired. */
    line = QIOread(qp);
    while (line != NULL && !signalled) {
        p = strchr(line, ' ');
        if (p != NULL)
            *p = '\0';
        p = strchr(line, '\t');
        if (p != NULL)
            *p = '\0';
        if (!OVexpiregroup(line, &low, history))
            warn("can't expire %s", line);
        else if (lowmark != NULL && low != 0)
            fprintf(lowmark, "%s %d\n", line, low);
        line = QIOread(qp);
    }
    if (signalled)
        warn("received signal, exiting");

    /* If desired, purge all deleted newsgroups. */
    if (!signalled && purge_deleted)
        if (!OVexpiregroup(NULL, NULL, history))
            warn("can't expire deleted newsgroups");

    /* Close everything down in an orderly fashion. */
    if (bloom) {
        OVctl(OVTOKENCACHE, &null_bloom);
        bloom_free(bloom);
    }
    QIOclose(qp);
    OVclose();
    SMshutdown();
    HISclose(history);
    if (lowmark != NULL)
        if (fclose(lowmark) == EOF)
            syswarn("can't close %s", lowmark_path);

    /* Finalize the tombstone log.  In inline-cancel mode, rename .NEW
       to its final name iff we completed a full pass without errors --
       a partial log would let expire incorrectly drop history entries
       for articles that were not actually cancelled this run.
       In delayrm mode, leave the .NEW alone: expirerm performs the
       rename after fastrm has actually deleted the articles.  If
       expirerm fails or is skipped, the .NEW is wiped by the next
       expireover, so a partial log can never be consumed by expire.
       No fsync: the log is advisory; if a crash loses the buffered
       tail, expire just runs the slow path. */
    if (OVtombstonefile != NULL) {
        if (fclose(OVtombstonefile) == EOF) {
            syswarn("can't finalize tombstone log %s", tombstone_path_new);
            tombstone_clean = false;
        }
        if (signalled)
            tombstone_clean = false;
        if (ovge.delayrm) {
            /* Leave .NEW for expirerm.  If we were signalled or hit
               an error, unlink it so expirerm doesn't promote a
               truncated log. */
            if (!tombstone_clean) {
                if (unlink(tombstone_path_new) < 0)
                    syswarn("can't unlink %s", tombstone_path_new);
            }
        } else if (tombstone_clean) {
            if (rename(tombstone_path_new, tombstone_path) < 0) {
                syswarn("can't rename %s to %s", tombstone_path_new,
                        tombstone_path);
                if (unlink(tombstone_path_new) < 0)
                    syswarn("can't unlink %s", tombstone_path_new);
            }
        } else {
            if (unlink(tombstone_path_new) < 0)
                syswarn("can't unlink %s", tombstone_path_new);
        }
        free(tombstone_path);
        free(tombstone_path_new);
    }

    return 0;
}

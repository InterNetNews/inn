/*  $Id$
**
**  Expire the overview database.
**
**  This program handles the nightly expiration of overview information.  If
**  groupbaseexpiry is true, this program also handles the removal of
**  articles that have expired.  It's separate from the process that scans
**  and expires the history file.
*/

#include "config.h"
#include "clibrary.h"
#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <syslog.h>
#include <time.h>

#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/qio.h"
#include "libinn.h"
#include "ov.h"
#include "paths.h"
#include "storage.h"

static const char usage[] = "\
Usage: expireover [-ekNpqs] [-w offset] [-z rmfile] [-Z lowmarkfile]\n";

/* Set to 1 if we've received a signal; expireover then terminates after
   finishing the newsgroup that it's working on (this prevents corruption of
   the overview by killing expireover). */
static volatile sig_atomic_t signalled = 0;


/*
**  Handle a fatal signal and set signalled.  Restore the default signal
**  behavior after receiving a signal so that repeating the signal will kill
**  the program immediately.
*/
static RETSIGTYPE
fatal_signal(int sig)
{
    signalled = 1;
    xsignal(sig, SIG_DFL);
}


/*
**  Change to the news user if possible, and if not, die.  Used for operations
**  that may create new database files so as not to mess up the ownership.
*/
static void
setuid_news(void)
{
    struct passwd *pwd;

    pwd = getpwnam(NEWSUSER);
    if (pwd == NULL)
        die("can't resolve %s to a UID (account doesn't exist?)", NEWSUSER);
    if (getuid() == 0)
        setuid(pwd->pw_uid);
    if (getuid() != pwd->pw_uid)
        die("must be run as %s", NEWSUSER);
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
    FILE *lowmark = NULL;
    bool purge_deleted = false;
    bool always_stat = false;
    struct history *history;

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
            ovge.timewarp = (time_t) (atof(optarg) * 86400.);
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

    /* Change to the news user if necessary. */
    setuid_news();

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
        active_path = concatpath(innconf->pathdb, _PATH_ACTIVE);
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
    path = concatpath(innconf->pathdb, _PATH_HISTORY);
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

    /* We want to be careful about being interrupted from this point on, so
       set up our signal handlers. */
    xsignal(SIGTERM, fatal_signal);
    xsignal(SIGINT, fatal_signal);
    xsignal(SIGHUP, fatal_signal);

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
            fprintf(lowmark, "%s %u\n", line, low);
        line = QIOread(qp);
    }
    if (signalled)
        warn("received signal, exiting");

    /* If desired, purge all deleted newsgroups. */
    if (!signalled && purge_deleted)
        if (!OVexpiregroup(NULL, NULL, history))
            warn("can't expire deleted newsgroups");

    /* Close everything down in an orderly fashion. */
    QIOclose(qp);
    OVclose();
    SMshutdown();
    HISclose(history);
    if (lowmark != NULL)
        if (fclose(lowmark) == EOF)
            syswarn("can't close %s", lowmark_path);

    return 0;
}

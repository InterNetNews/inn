/*
**  Read batchfiles on standard input and spew out batches.
*/

#include "portable/system.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <syslog.h>

#include "inn/innconf.h"
#include "inn/libinn.h"
#include "inn/messages.h"
#include "inn/paths.h"
#include "inn/storage.h"
#include "inn/timer.h"
#include "inn/wire.h"


/*
**  Global variables.
*/
static bool BATCHopen;
static double STATbegin;
static double STATend;
static char *Host;
static char *InitialString;
static char *Input;
static char *Processor;
static int ArtsInBatch;
static size_t ArtsWritten;
static int BATCHcount;
static int MaxBatches;
static int BATCHstatus;
static size_t BytesInBatch = 60 * 1024;
static size_t BytesWritten;
static size_t MaxArts;
static size_t MaxBytes;
static sig_atomic_t GotInterrupt;
static const char *Separator = "#! rnews %ld";
static char *ERRLOG;

/*
**  Start a batch process.
*/
static FILE *
BATCHstart(void)
{
    FILE *F;
    char buff[SMBUF];

    if (Processor && *Processor) {
#if __GNUC__ > 4 || defined(__llvm__) || defined(__clang__)
#    pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
        snprintf(buff, sizeof(buff), Processor, Host);
#if __GNUC__ > 4 || defined(__llvm__) || defined(__clang__)
#    pragma GCC diagnostic warning "-Wformat-nonliteral"
#endif
        F = popen(buff, "w");
        if (F == NULL)
            return NULL;
    } else
        F = stdout;
    BATCHopen = true;
    BATCHcount++;
    return F;
}


/*
**  Close a batch, return exit status.
*/
static int
BATCHclose(FILE *F)
{
    BATCHopen = false;
    if (F == stdout)
        return fflush(stdout) == EOF ? 1 : 0;
    return pclose(F);
}


/*
**  Update the batch file and exit.
*/
__attribute__((__noreturn__)) static void
RequeueAndExit(off_t Cookie, char *line, long BytesInArt)
{
    char *spool;
    char buff[BIG_BUFFER];
    int i;
    FILE *F;
    double usertime;
    double systime;

    /* Do statistics. */
    STATend = TMRnow_double();
    if (GetResourceUsage(&usertime, &systime) < 0) {
        usertime = 0;
        systime = 0;
    }

    notice("batcher %s times user %.3f system %.3f elapsed %.3f", Host,
           usertime, systime, STATend - STATbegin);
    notice("batcher %s stats batches %d articles %lu bytes %lu", Host,
           BATCHcount, (unsigned long) ArtsWritten,
           (unsigned long) BytesWritten);

    /* Last batch exit okay? */
    if (BATCHstatus == 0) {
        if (feof(stdin) && Cookie != -1) {
            /* Yes, and we're all done -- remove input and exit. */
            fclose(stdin);
            if (Input)
                unlink(Input);
            exit(0);
        }
    }

    /* Make an appropriate spool file. */
    if (Input == NULL)
        spool = concatpath(innconf->pathoutgoing, Host);
    else
        spool = concat(Input, ".bch", (char *) 0);
    if ((F = xfopena(spool)) == NULL)
        sysdie("%s cannot open %s", Host, spool);

    /* If we can back up to where the batch started, do so. */
    i = 0;
    if (Cookie != -1 && fseeko(stdin, Cookie, SEEK_SET) == -1) {
        syswarn("%s cannot seek", Host);
        i = 1;
    }

    /* Write the line we had; if the fseeko worked, this will be an
     * extra line, but that's okay. */
    if (line && fprintf(F, "%s %ld\n", line, BytesInArt) == EOF) {
        syswarn("%s cannot write spool", Host);
        i = 1;
    }

    /* Write rest of stdin to spool. */
    while (fgets(buff, sizeof buff, stdin) != NULL)
        if (fputs(buff, F) == EOF) {
            syswarn("%s cannot write spool", Host);
            i = 1;
            break;
        }
    if (fclose(F) == EOF) {
        syswarn("%s cannot close spool", Host);
        i = 1;
    }

    /* If we had a named input file, try to rename the spool. */
    if (Input != NULL && rename(spool, Input) < 0) {
        syswarn("%s cannot rename spool", Host);
        i = 1;
    }

    free(spool);
    exit(i);
    /* NOTREACHED */
}


/*
**  Mark that we got interrupted.
*/
static void
CATCHinterrupt(int s)
{
    GotInterrupt = true;

    /* Let two interrupts kill us. */
    xsignal(s, SIG_DFL);
}


int
main(int ac, char *av[])
{
    bool Redirect;
    FILE *F;
    char *p;
    char line[BIG_BUFFER];
    char buff[BIG_BUFFER];
    size_t BytesInArt;
    size_t BytesInCB;
    off_t Cookie;
    int i;
    int ArtsInCB;
    int length;
    TOKEN token;
    ARTHANDLE *art;
    char *artdata;
    size_t written;

    /* Set defaults. */
    openlog("batcher", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);
    message_program_name = "batcher";
    if (!innconf_read(NULL))
        exit(1);
    Redirect = true;
    umask(NEWSUMASK);
    ERRLOG = concatpath(innconf->pathlog, INN_PATH_ERRLOG);

    /* By default, statistics only go to syslog. */
    message_handlers_notice(1, message_log_syslog_notice);

    /* Parse JCL. */
    while ((i = getopt(ac, av, "a:A:b:B:i:N:p:rs:v")) != EOF)
        switch (i) {
        default:
            die("usage error");
            /* NOTREACHED */
        case 'a':
            ArtsInBatch = atoi(optarg);
            break;
        case 'A':
            MaxArts = atol(optarg);
            break;
        case 'b':
            BytesInBatch = atol(optarg);
            break;
        case 'B':
            MaxBytes = atol(optarg);
            break;
        case 'i':
            InitialString = optarg;
            break;
        case 'N':
            MaxBatches = atoi(optarg);
            break;
        case 'p':
            Processor = optarg;
            break;
        case 'r':
            Redirect = false;
            break;
        case 's':
            Separator = optarg;
            break;
        case 'v':
            message_handlers_notice(2, message_log_syslog_notice,
                                    message_log_stdout);
            break;
        }
    if (MaxArts && ArtsInBatch == 0)
        ArtsInBatch = MaxArts;
    if (MaxBytes && BytesInBatch == 0)
        BytesInBatch = MaxBytes;

    /* Parse arguments. */
    ac -= optind;
    av += optind;
    if (ac != 1 && ac != 2)
        die("usage error");
    Host = av[0];
    if ((Input = av[1]) != NULL) {
        if (Input[0] != '/')
            Input = concatpath(innconf->pathoutgoing, av[1]);
        if (freopen(Input, "r", stdin) == NULL)
            sysdie("%s cannot open %s", Host, Input);
    }

    if (Redirect) {
        if (freopen(ERRLOG, "a", stderr) == NULL)
            sysdie("cannot open %s for error output", ERRLOG);
    }

    /* Set initial counters, etc. */
    BytesInCB = 0;
    ArtsInCB = 0;
    Cookie = -1;
    GotInterrupt = false;
    xsignal(SIGHUP, CATCHinterrupt);
    xsignal(SIGINT, CATCHinterrupt);
    xsignal(SIGTERM, CATCHinterrupt);
    /* xsignal(SIGPIPE, CATCHinterrupt); */
    STATbegin = TMRnow_double();

    SMinit();
    F = NULL;
    while (fgets(line, sizeof line, stdin) != NULL) {
        /* Record line length in case we do an ftello.  Not portable to
         * systems with non-Unix file formats. */
        length = strlen(line);
        Cookie = ftello(stdin) - length;

        /* Get lines like "name size".  Note that we ignore size but accept
         * it for backward compatibility. */
        if ((p = strchr(line, '\n')) == NULL) {
            warn("%s skipping %.40s: too long", Host, line);
            continue;
        }
        *p = '\0';
        if (line[0] == '\0' || line[0] == '#')
            continue;
        p = strchr(line, ' ');
        if (p != NULL)
            *p = '\0';

        p = line;

        /* Open the article. */
        if (IsToken(p)) {
            token = TextToToken(p);
            if ((art = SMretrieve(token, RETR_ALL)) == NULL) {
                if ((SMerrno != SMERR_NOENT) && (SMerrno != SMERR_UNINIT))
                    warn("%s skipping %.40s: %s", Host, p, SMerrorstr);
                continue;
            }
            artdata = wire_to_native(art->data, art->len, &BytesInArt);
            SMfreearticle(art);
        } else {
            warn("%s skipping %.40s: not token", Host, p);
            continue;
        }

        /* Have an open article, do we need to open a batch?  This code
         * is here (rather than up before the while loop) so that we
         * can avoid sending an empty batch.  The goto makes the code
         * a bit more clear. */
        if (F == NULL) {
            if (GotInterrupt) {
                RequeueAndExit(Cookie, (char *) NULL, 0L);
            }
            if ((F = BATCHstart()) == NULL) {
                syswarn("%s cannot start batch %d", Host, BATCHcount);
                break;
            }
            if (InitialString && *InitialString) {
                fprintf(F, "%s\n", InitialString);
                BytesInCB += strlen(InitialString) + 1;
                BytesWritten += strlen(InitialString) + 1;
            }
            goto SendIt;
        }

        /* We're writing a batch, see if adding the current article
         * would exceed the limits. */
        if ((ArtsInBatch > 0 && ArtsInCB + 1 >= ArtsInBatch)
            || (BytesInBatch > 0 && BytesInCB + BytesInArt >= BytesInBatch)) {
            if ((BATCHstatus = BATCHclose(F)) != 0) {
                if (BATCHstatus == -1)
                    syswarn("%s cannot close batch %d", Host, BATCHcount);
                else
                    syswarn("%s batch %d exit status %d", Host, BATCHcount,
                            BATCHstatus);
                break;
            }
            ArtsInCB = 0;
            BytesInCB = 0;

            /* See if we can start a new batch. */
            if ((MaxBatches > 0 && BATCHcount >= MaxBatches)
                || (MaxBytes > 0 && BytesWritten + BytesInArt >= MaxBytes)
                || (MaxArts > 0 && ArtsWritten + 1 >= MaxArts)) {
                break;
            }

            if (GotInterrupt) {
                RequeueAndExit(Cookie, (char *) NULL, 0L);
            }

            if ((F = BATCHstart()) == NULL) {
                syswarn("%s cannot start batch %d", Host, BATCHcount);
                break;
            }
        }

    SendIt:
        /* Now we can start to send the article! */
        if (Separator && *Separator) {
#if __GNUC__ > 4 || defined(__llvm__) || defined(__clang__)
#    pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
            snprintf(buff, sizeof(buff), Separator, BytesInArt);
#if __GNUC__ > 4 || defined(__llvm__) || defined(__clang__)
#    pragma GCC diagnostic warning "-Wformat-nonliteral"
#endif
            BytesInCB += strlen(buff) + 1;
            BytesWritten += strlen(buff) + 1;
            if (fprintf(F, "%s\n", buff) == EOF || ferror(F)) {
                syswarn("%s cannot write separator", Host);
                break;
            }
        }

        /* Write the article.  In case of interrupts, retry the read but not
         * the fwrite because we can't check that reliably and portably. */
        written = fwrite(artdata, 1, BytesInArt, F);
        if (written != BytesInArt || ferror(F))
            break;

        /* Update the counts. */
        BytesInCB += BytesInArt;
        BytesWritten += BytesInArt;
        ArtsInCB++;
        ArtsWritten++;

        if (GotInterrupt) {
            Cookie = -1;
            BATCHstatus = BATCHclose(F);
            RequeueAndExit(Cookie, line, BytesInArt);
        }
    }

    if (BATCHopen)
        BATCHstatus = BATCHclose(F);
    RequeueAndExit(Cookie, NULL, 0);

    /* NOTREACHED */
    return 0;
}

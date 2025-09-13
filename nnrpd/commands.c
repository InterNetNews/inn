/*
**  Miscellaneous commands.
*/

#include "portable/system.h"

#include <sys/wait.h>

#include "inn/fdflag.h"
#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/ov.h"
#include "inn/version.h"
#include "nnrpd.h"
#include "tls.h"


typedef struct {
    char *name;
    ARTNUM high;
    ARTNUM low;
    unsigned long count;
} GROUPDATA;


/*
**  Check after a successful authentication if the currently selected
**  newsgroup is still readable.  AUTHINFO SASL and STARTTLS do not need
**  it because the NNTP protocol is reset after it.
**
**  Return true if the group must be made invalid.
*/
static bool
makeGroupInvalid(void)
{
    bool hookpresent = false;
    char *grplist[2];

    /* If no group has been selected yet, it is considered as valid. */
    if (GRPcur == NULL) {
        return false;
    }

#ifdef DO_PYTHON
    hookpresent = PY_use_dynamic;
    if (hookpresent) {
        char *reply;

        /* Authorize user using Python module method dynamic. */
        if (PY_dynamic(PERMuser, GRPcur, false, &reply) < 0) {
            syslog(L_NOTICE, "PY_dynamic(): authorization skipped due to no "
                             "Python dynamic method defined");
        } else {
            if (reply != NULL) {
                syslog(L_TRACE,
                       "PY_dynamic() returned a refuse string for user %s at "
                       "%s who wants to read %s: %s",
                       PERMuser, Client.host, GRPcur, reply);
                free(reply);
                return true;
            }
        }
    }
#endif /* DO_PYTHON */

    if (!hookpresent) {
        if (PERMspecified) {
            grplist[0] = GRPcur;
            grplist[1] = NULL;
            if (!PERMmatch(PERMreadlist, grplist)) {
                return true;
            }
        } else {
            return true;
        }
    }

    if (!hookpresent && !PERMcanread) {
        return true;
    }

    return false;
}


/*  Returns:
**    -1 for problem (such as no such authenticator, etc.).
**     1 for authentication succeeded.
**     0 for authentication failed.
*/
static char *PERMauthstring;

static int
PERMgeneric(char *av[], char *accesslist, size_t size)
{
    char path[BIG_BUFFER], *fields[6], *p;
    size_t j;
    int i, pan[2], status;
    pid_t pid;
    struct stat stb;

    av += 2;

    PERMcanread = false;
    PERMcanpost = false;
    PERMaccessconf->locpost = false;
    PERMaccessconf->allowapproved = false;
    PERMaccessconf->allowihave = false;
    PERMaccessconf->allownewnews = false;

    if (!*av) {
        Reply("%d No authenticator provided\r\n", NNTP_ERR_SYNTAX);
        return -1;
    }

    /* Check for ".." (not "../").  Path must not be changed! */
    if (strstr(av[0], "..") != NULL) {
        Reply("%d .. in authenticator %s\r\n", NNTP_ERR_SYNTAX, av[0]);
        return -1;
    }

    /* 502 if authentication will fail. */
    if (!PERMcanauthenticate) {
        if (PERMauthorized && !PERMneedauth)
            Reply("%d Already authenticated\r\n", NNTP_ERR_ACCESS);
        else
            Reply("%d Authentication will fail\r\n", NNTP_ERR_ACCESS);
        return -1;
    }

    if (strchr(INN_PATH_AUTHDIR, '/') == NULL)
        snprintf(path, sizeof(path), "%s/%s/%s/%s", innconf->pathbin,
                 INN_PATH_AUTHDIR, INN_PATH_AUTHDIR_GENERIC, av[0]);
    else
        snprintf(path, sizeof(path), "%s/%s/%s", INN_PATH_AUTHDIR,
                 INN_PATH_AUTHDIR_GENERIC, av[0]);

#if !defined(S_IXUSR) && defined(_S_IXUSR)
#    define S_IXUSR _S_IXUSR
#endif /* !defined(S_IXUSR) && defined(_S_IXUSR) */

#if !defined(S_IXUSR) && defined(S_IEXEC)
#    define S_IXUSR S_IEXEC
#endif /* !defined(S_IXUSR) && defined(S_IEXEC) */

    if (stat(path, &stb) || !(stb.st_mode & S_IXUSR)) {
        Reply("%d No such authenticator %s\r\n", NNTP_ERR_UNAVAILABLE, av[0]);
        return -1;
    }

    /* Create a pipe. */
    if (pipe(pan) < 0) {
        Reply("%d Can't pipe %s\r\n", NNTP_FAIL_ACTION, strerror(errno));
        syslog(L_FATAL, "can't pipe for %s %m", av[0]);
        return -1;
    }

    for (i = 0; (pid = fork()) < 0; i++) {
        if (i == (long) innconf->maxforks) {
            Reply("%d Can't fork %s\r\n", NNTP_FAIL_ACTION, strerror(errno));
            syslog(L_FATAL, "can't fork %s %m", av[0]);
            return -1;
        }
        syslog(L_NOTICE, "can't fork %s -- waiting", av[0]);
        sleep(5);
    }

    /* Run the child, with redirection. */
    if (pid == 0) {
        close(STDERR_FILENO); /* Close existing stderr. */
        close(pan[PIPE_READ]);

        /* stderr goes down the pipe. */
        if (pan[PIPE_WRITE] != STDERR_FILENO) {
            if ((i = dup2(pan[PIPE_WRITE], STDERR_FILENO)) != STDERR_FILENO) {
                syslog(L_FATAL, "can't dup2 %d to %d got %d %m",
                       pan[PIPE_WRITE], STDERR_FILENO, i);
                _exit(1);
            }
            close(pan[PIPE_WRITE]);
        }

        fdflag_close_exec(STDIN_FILENO, false);
        fdflag_close_exec(STDOUT_FILENO, false);
        fdflag_close_exec(STDERR_FILENO, false);

        execv(path, av);
        /* RFC 2980 requires 500 if there are unspecified errors during
         * the execution of the provided program. */
        Reply("%d Program error occurred\r\n", NNTP_ERR_COMMAND);

        syslog(L_FATAL, "can't execv %s %m", path);
        _exit(1);
    }

    close(pan[PIPE_WRITE]);
    if (read(pan[PIPE_READ], path, sizeof(path)) < 0) {
        syslog(L_FATAL, "can't read %s %m", path);
        return 0;
    }

    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        return 0;

    if ((p = strchr(path, '\n')) != NULL)
        *p = '\0';

    if (PERMauthstring)
        free(PERMauthstring);

    PERMauthstring = xstrdup(path);

    // syslog(L_NOTICE, "%s (%ld) returned: %d %s %d\n", av[0], (long) pid, i,
    // path, status);
    /* Split "host:permissions:user:pass:groups" into fields. */
    for (fields[0] = path, j = 0, p = path; *p; p++)
        if (*p == ':') {
            *p = '\0';
            ++j;
            if (j < ARRAY_SIZE(fields)) {
                fields[j] = p + 1;
            } else {
                Reply("%d Program error occurred\r\n", NNTP_FAIL_ACTION);
                syslog(L_FATAL, "over-long response from %s", av[0]);
                return -1;
            }
        }

    if (j < 4) {
        Reply("%d Program error occurred\r\n", NNTP_FAIL_ACTION);
        syslog(L_FATAL, "short response from %s", av[0]);
        return -1;
    }

    PERMcanread = strchr(fields[1], 'R') != NULL;
    PERMcanpost = strchr(fields[1], 'P') != NULL;
    PERMaccessconf->allowapproved = strchr(fields[1], 'A') != NULL;
    PERMaccessconf->locpost = strchr(fields[1], 'L') != NULL;
    PERMaccessconf->allowihave = strchr(fields[1], 'I') != NULL;
    PERMaccessconf->allownewnews = strchr(fields[1], 'N') != NULL;
    strlcpy(PERMuser, fields[2], sizeof(PERMuser));
    strlcat(PERMuser, "@", sizeof(PERMuser));
    strlcat(PERMuser, fields[0], sizeof(PERMuser));
    // strlcpy(PERMpass, fields[3], sizeof(PERMpass));
    strlcpy(accesslist, fields[4], size);

    // for (i = 0; fields[i] && i < 5; i++)
    //    syslog(L_NOTICE, "fields[%d] = %s\n", i, fields[i]);

    return 1;
}


/*
**  The AUTHINFO command.
*/
void
CMDauthinfo(int ac, char *av[])
{
    static char User[SMBUF];
    static char Password[SMBUF];
    /* XXX BIG_BUFFER, if changed, should also be changed in perl.c and
     * python.c. */
    char accesslist[BIG_BUFFER];
    char errorstr[BIG_BUFFER];
    int code;

#if defined(HAVE_ZLIB)
    /* If a compression layer is active, AUTHINFO is not possible. */
    if (compression_layer_on && !tls_compression_on) {
        Reply("%d Already using a compression layer\r\n", NNTP_ERR_ACCESS);
        return;
    }
#endif

    if (strcasecmp(av[1], "GENERIC") == 0) {
        char *logrec = Glom(av);

        /* Go on parsing the command line. */
        ac--;
        (void) reArgify(av[ac], &av[ac], -1, true);

        strlcpy(PERMuser, "<none>", sizeof(PERMuser));

        /* Arguments are checked by PERMgeneric(). */
        switch (PERMgeneric(av, accesslist, sizeof(accesslist))) {
        case 1:
            PERMspecified = NGgetlist(&PERMreadlist, accesslist);
            PERMpostlist = PERMreadlist;
            syslog(L_NOTICE, "%s auth %s (%s -> %s)", Client.host, PERMuser,
                   logrec, PERMauthstring ? PERMauthstring : "");
            Reply("%d Authentication succeeded\r\n", NNTP_OK_AUTHINFO);
            PERMneedauth = false;
            PERMauthorized = true;
            PERMcanauthenticate = false;
            PERMgroupmadeinvalid = makeGroupInvalid();
            free(logrec);
            return;
        case 0:
            syslog(L_NOTICE, "%s bad_auth %s (%s)", Client.host, PERMuser,
                   logrec);
            /* We keep the right 481 code here instead of the wrong 502
             * answer suggested in RFC 2080. */
            Reply("%d Authentication failed\r\n", NNTP_FAIL_AUTHINFO_BAD);
            free(logrec);
            return;
        default:
            /* Lower level (-1) has already issued a reply. */
            return;
        }
    } else if (strcasecmp(av[1], "SASL") == 0) {
#ifdef HAVE_SASL
        /* Go on parsing the command line. */
        ac--;
        ac += reArgify(av[ac], &av[ac], -1, true);

        /* Arguments are checked by SASLauth(). */
        SASLauth(ac, av);
#else
        Reply("%d SASL authentication unsupported\r\n", NNTP_ERR_SYNTAX);
        return;
#endif /* HAVE_SASL */
    } else {
        /* Each time AUTHINFO USER is used, the new username is cached. */
        if (strcasecmp(av[1], "USER") == 0) {
            /* 502 if authentication will fail. */
            if (!PERMcanauthenticate) {
                if (PERMauthorized && !PERMneedauth)
                    Reply("%d Already authenticated\r\n", NNTP_ERR_ACCESS);
                else
                    Reply("%d Authentication will fail\r\n", NNTP_ERR_ACCESS);
                return;
            }

#ifdef HAVE_OPENSSL
            /* Check whether STARTTLS must be used before trying to
             * authenticate. */
            if (PERMcanauthenticate && !PERMcanauthenticatewithoutSSL
                && !encryption_layer_on) {
                Reply("%d Encryption required\r\n", NNTP_FAIL_PRIVACY_NEEDED);
                return;
            }
#endif

            strlcpy(User, av[2], sizeof(User));
            Reply("%d Enter password\r\n", NNTP_CONT_AUTHINFO);
            return;
        }

        /* If it is not AUTHINFO PASS, we do not support the provided
         * subcommand. */
        if (strcasecmp(av[1], "PASS") != 0) {
            Reply("%d Bad AUTHINFO param\r\n", NNTP_ERR_SYNTAX);
            return;
        }

        /* 502 if authentication will fail. */
        if (!PERMcanauthenticate) {
            if (PERMauthorized && !PERMneedauth)
                Reply("%d Already authenticated\r\n", NNTP_ERR_ACCESS);
            else
                Reply("%d Authentication will fail\r\n", NNTP_ERR_ACCESS);
            return;
        }

#ifdef HAVE_OPENSSL
        /* Check whether STARTTLS must be used before trying to authenticate.
         */
        if (PERMcanauthenticate && !PERMcanauthenticatewithoutSSL
            && !encryption_layer_on) {
            Reply("%d Encryption required\r\n", NNTP_FAIL_PRIVACY_NEEDED);
            return;
        }
#endif

        /* AUTHINFO PASS cannot be sent before AUTHINFO USER. */
        if (User[0] == '\0') {
            Reply("%d Authentication commands issued out of sequence\r\n",
                  NNTP_FAIL_AUTHINFO_REJECT);
            return;
        }

        /* There is a cached username and a password is provided. */
        strlcpy(Password, av[2], sizeof(Password));

        errorstr[0] = '\0';
        code = NNTP_FAIL_AUTHINFO_BAD;

        PERMlogin(User, Password, &code, errorstr);
        PERMgetpermissions();

        /* If authentication is successful. */
        if (!PERMneedauth) {
            syslog(L_NOTICE, "%s user %s", Client.host, PERMuser);
            if (LLOGenable) {
                fprintf(locallog, "%s user (%s):%s\n", Client.host, Username,
                        PERMuser);
                fflush(locallog);
            }
            Reply("%d Authentication succeeded\r\n", NNTP_OK_AUTHINFO);
            PERMneedauth = false;
            PERMauthorized = true;
            PERMcanauthenticate = false;
            PERMgroupmadeinvalid = makeGroupInvalid();
            return;
        }

        /* For backward compatibility, we return 481 instead of 502 (which had
         * the same meaning as 481 in RFC 2980). */
        if (code == NNTP_ERR_ACCESS) {
            code = NNTP_FAIL_AUTHINFO_BAD;
        }

        syslog(L_NOTICE, "%s bad_auth", Client.host);
        /* Return 403 in case the return code is not 481. */
        if (errorstr[0] != '\0') {
            syslog(L_NOTICE, "%s script error str: %s", Client.host, errorstr);
            Reply("%d %s\r\n",
                  code != NNTP_FAIL_AUTHINFO_BAD ? NNTP_FAIL_ACTION : code,
                  errorstr);
        } else {
            Reply("%d Authentication failed\r\n",
                  code != NNTP_FAIL_AUTHINFO_BAD ? NNTP_FAIL_ACTION : code);
        }
    }
}


/*
**  The DATE command.  Useful mainly in conjunction with NEWNEWS.
*/
void
CMDdate(int ac UNUSED, char *av[] UNUSED)
{
    time_t now;
    struct tm *gmt;

    now = time(NULL);
    gmt = gmtime(&now);
    if (now == (time_t) -1 || gmt == NULL) {
        Reply("%d Can't get time, %s\r\n", NNTP_FAIL_ACTION, strerror(errno));
        return;
    }
    Reply("%d %04d%02d%02d%02d%02d%02d\r\n", NNTP_INFO_DATE,
          gmt->tm_year + 1900, gmt->tm_mon + 1, gmt->tm_mday, gmt->tm_hour,
          gmt->tm_min, gmt->tm_sec);
}


/*
**  Handle the MODE command.
**  Note that MODE STREAM must return 501 as an unknown MODE variant
**  because nnrpd does not implement STREAMING.
*/
void
CMDmode(int ac UNUSED, char *av[])
{
    if (strcasecmp(av[1], "READER") == 0)
        /* In the case AUTHINFO has already been successfully used,
         * nnrpd must answer as a no-op (it still advertises the READER
         * capability but not MODE-READER). */
        Reply("%d %s InterNetNews NNRP server %s ready (%s)\r\n",
              (PERMcanpost || (PERMcanauthenticate && PERMcanpostgreeting))
                  ? NNTP_OK_BANNER_POST
                  : NNTP_OK_BANNER_NOPOST,
              PERMaccessconf->pathhost, INN_VERSION_STRING,
              (!PERMneedauth && PERMcanpost) ? "posting ok" : "no posting");
    else
        Reply("%d Unknown MODE variant\r\n", NNTP_ERR_SYNTAX);
}


static int
GroupCompare(const void *a1, const void *b1)
{
    const GROUPDATA *a = a1;
    const GROUPDATA *b = b1;

    return strcmp(a->name, b->name);
}


/*
**  Display new newsgroups since a given date and time.
*/
void
CMDnewgroups(int ac, char *av[])
{
    char *p;
    char *q;
    QIOSTATE *qp;
    time_t date;
    char *grplist[2];
    int hi, lo, count, flag;
    GROUPDATA *grouplist = NULL;
    GROUPDATA key;
    GROUPDATA *gd;
    int listsize = 0;
    int numgroups = 0;
    int numfound = 0;
    int i;
    bool local = true;

    /* Check the arguments and parse the date. */
    if (ac > 3) {
        if (strcasecmp(av[3], "GMT") == 0)
            local = false;
        else {
            Reply("%d Syntax error for \"GMT\"\r\n", NNTP_ERR_SYNTAX);
            return;
        }
    }
    date = parsedate_nntp(av[1], av[2], local);
    if (date == (time_t) -1) {
        Reply("%d Bad date\r\n", NNTP_ERR_SYNTAX);
        return;
    }

    /* Log an error if active.times doesn't exist, but don't return an error
       to the client.  The most likely cause of this is a new server
       installation that's yet to have any new groups created, and returning
       an error was causing needless confusion.  Just return the empty list
       of groups. */
    if ((qp = QIOopen(ACTIVETIMES)) == NULL) {
        syslog(L_ERROR, "%s can't fopen %s %m", Client.host, ACTIVETIMES);
        Reply("%d No new newsgroups\r\n", NNTP_OK_NEWGROUPS);
        Printf(".\r\n");
        return;
    }

    /* Read the file, ignoring long lines. */
    while ((p = QIOread(qp)) != NULL) {
        if ((q = strchr(p, ' ')) == NULL)
            continue;
        *q++ = '\0';
        if ((time_t) atoll(q) < date)
            continue;
        if (!OVgroupstats(p, &lo, &hi, &count, &flag))
            continue;

        if (PERMspecified) {
            grplist[0] = p;
            grplist[1] = NULL;
            if (!PERMmatch(PERMreadlist, grplist))
                continue;
        } else
            continue;

        if (grouplist == NULL) {
            grouplist = xmalloc(1000 * sizeof(GROUPDATA));
            listsize = 1000;
        }
        if (listsize <= numgroups) {
            listsize += 1000;
            grouplist = xrealloc(grouplist, listsize * sizeof(GROUPDATA));
        }

        grouplist[numgroups].high = hi;
        grouplist[numgroups].low = lo;
        grouplist[numgroups].count = count;
        grouplist[numgroups].name = xstrdup(p);
        numgroups++;
    }
    QIOclose(qp);

    if ((qp = QIOopen(ACTIVE)) == NULL) {
        syslog(L_ERROR, "%s can't fopen %s %m", Client.host, ACTIVE);
        Reply("%d Can't open active file\r\n", NNTP_FAIL_ACTION);

        for (i = 0; i < numgroups; i++) {
            free(grouplist[i].name);
        }
        free(grouplist);

        return;
    }
    qsort(grouplist, numgroups, sizeof(GROUPDATA), GroupCompare);
    Reply("%d New newsgroups follow\r\n", NNTP_OK_NEWGROUPS);
    for (numfound = numgroups; (p = QIOread(qp)) && numfound;) {
        /* p will contain the name of the newsgroup.
         * When the syntax of the line is not correct, we continue
         * with the following line. */
        if ((q = strchr(p, ' ')) == NULL)
            continue;
        *q++ = '\0';
        /* Find out the end of the high water mark. */
        if ((q = strchr(q, ' ')) == NULL)
            continue;
        q++;
        /* Find out the end of the low water mark.
         * q will contain the flag of the newsgroup. */
        if ((q = strchr(q, ' ')) == NULL)
            continue;
        q++;
        key.name = p;
        if ((gd = bsearch(&key, grouplist, numgroups, sizeof(GROUPDATA),
                          GroupCompare))
            == NULL)
            continue;
        Printf("%s %lu %lu %s\r\n", p, gd->high, gd->low, q);
        numfound--;
    }
    for (i = 0; i < numgroups; i++) {
        free(grouplist[i].name);
    }
    free(grouplist);
    QIOclose(qp);
    Printf(".\r\n");
}


/*
**  Handle the POST and IHAVE commands.
*/
void
CMDpost(int ac, char *av[])
{
    static char *article;
    static int size;
    char *p, *q;
    char *end;
    int longline, missingdotstuffing;
    READTYPE r;
    int i;
    long l;
    long sleeptime;
    char *path;
    const char *response;
    char idbuff[SMBUF];
    static int backoff_inited = false;
    bool ihave, permanent;

    ihave = (strcasecmp(av[0], "IHAVE") == 0);

    /* Check whether the Message-ID is valid for IHAVE. */
    if (ihave && ac == 2 && !IsValidMessageID(av[1], true, laxmid)) {
        Reply("%d Syntax error in Message-ID\r\n", NNTP_ERR_SYNTAX);
        return;
    }

    /* Check authorizations. */
    if (ihave && (!PERMaccessconf->allowihave || !PERMcanpost)) {
        syslog(L_NOTICE, "%s noperm ihave without permission", Client.host);
        Reply("%d IHAVE command disabled by administrator\r\n",
              PERMcanauthenticate ? NNTP_FAIL_AUTH_NEEDED : NNTP_ERR_ACCESS);
        return;
    }
    if (!ihave && !PERMcanpost) {
        syslog(L_NOTICE, "%s noperm post without permission", Client.host);
        Reply("%d Posting not allowed\r\n",
              PERMcanauthenticate && PERMcanpostgreeting
                  ? NNTP_FAIL_AUTH_NEEDED
                  : NNTP_FAIL_POST_AUTH);
        return;
    }

    if (!backoff_inited) {
        /* Exponential posting backoff. */
        InitBackoffConstants();
        backoff_inited = true;
    }

    /* Dave's posting limiter.  Limit postings to a certain rate.
     * And now we support multiprocess rate limits.  Questions?
     * E-mail <dave@jetcafe.org>. */
    if (BACKOFFenabled) {
        /* Acquire lock (this could be in RateLimit but that would
         * invoke the spaghetti factor). */
        if ((path = (char *) PostRecFilename(Client.ip, PERMuser)) == NULL) {
            Reply("%d Retry later\r\n",
                  ihave ? NNTP_FAIL_IHAVE_DEFER : NNTP_FAIL_POST_AUTH);
            return;
        }

        if (LockPostRec(path) == 0) {
            syslog(L_ERROR, "%s error write locking '%s'", Client.host, path);
            Reply("%d Retry later\r\n",
                  ihave ? NNTP_FAIL_IHAVE_DEFER : NNTP_FAIL_POST_AUTH);
            return;
        }

        if (!RateLimit(&sleeptime, path)) {
            syslog(L_ERROR, "%s can't check rate limit info", Client.host);
            Reply("%d Retry later\r\n",
                  ihave ? NNTP_FAIL_IHAVE_DEFER : NNTP_FAIL_POST_AUTH);
            UnlockPostRec(path);
            return;
        } else if (sleeptime != 0L) {
            syslog(L_NOTICE, "%s post sleep time is now %ld", Client.host,
                   sleeptime);
            sleep(sleeptime);
        }

        /* Remove the lock here so that only one nnrpd process does the
         * backoff sleep at once.  Other procs are sleeping for the lock. */
        UnlockPostRec(path);
    } /* End backoff code. */

    /* Start at beginning of buffer. */
    if (article == NULL) {
        size = 4096;
        article = xmalloc(size);
    }
    idbuff[0] = 0;
    if (ihave) {
        /* Check whether it is a duplicate. */
        if (History == NULL) {
            time_t statinterval = 30;
            History = HISopen(HISTORY, innconf->hismethod, HIS_RDONLY);
            if (!History) {
                syslog(L_NOTICE, "can't initialize history");
                Reply("%d NNTP server unavailable; try later\r\n",
                      NNTP_FAIL_TERMINATING);
                ExitWithStats(1, true);
            }
            HISctl(History, HISCTLS_STATINTERVAL, &statinterval);
        }
        if (HIScheck(History, av[1])) {
            Reply("%d Duplicate\r\n", NNTP_FAIL_IHAVE_REFUSE);
            return;
        } else {
            Reply("%d Send it; end with <CR-LF>.<CR-LF>\r\n", NNTP_CONT_IHAVE);
        }
    } else {
        if (PERMaccessconf->domainoverriden)
            p = GenerateMessageID(PERMaccessconf->domain);
        else
            p = GenerateMessageID(NULL);

        if (p != NULL) {
            if (VirtualPathlen > 0) {
                q = p;
                if ((p = strchr(p, '@')) != NULL) {
                    *p = '\0';
                    snprintf(idbuff, sizeof(idbuff), "%s%s@%s>", q,
                             NNRPinstance, PERMaccessconf->domain);
                }
            } else {
                strlcpy(idbuff, p, sizeof(idbuff));
            }
        }
        Reply("%d Ok, recommended Message-ID %s\r\n", NNTP_CONT_POST, idbuff);
    }
    fflush(stdout);

    p = article;
    end = &article[size];

    longline = 0;
    missingdotstuffing = 0;
    for (l = 1;; l++) {
        size_t len;
        const char *line;

        r = line_read(&NNTPline, PERMaccessconf->clienttimeout, &line, &len,
                      NULL);
        switch (r) {
        default:
            warn("%s internal %u in post", Client.host, r);
            goto fallthrough;
        case RTtimeout:
        fallthrough:
            warn("%s timeout in post", Client.host);
            ExitWithStats(1, false);
            /* NOTREACHED */
        case RTeof:
            warn("%s EOF in post", Client.host);
            ExitWithStats(1, false);
            /* NOTREACHED */
        case RTlong:
            if (longline == 0)
                longline = l;
            continue;
        case RTok:
            break;
        }

        /* If it is the terminator, break out. */
        if (strcmp(line, ".") == 0) {
            break;
        }

        /* If they broke our line length limit, there's little point
         * in processing any more of their input. */
        if (longline != 0) {
            continue;
        }

        /* +2 because of the \n\0 we append; note we don't add the 2
         * when increasing the size of the buffer as ART_LINE_MALLOC
         * will always be larger than 2 bytes. */
        if ((len + 2) > (size_t) (end - p)) {
            i = p - article;
            size += len + 4096;
            article = xrealloc(article, size);
            end = &article[size];
            p = i + article;
        }

        /* Reverse any dot-stuffing. */
        if (*line == '.') {
            ++line;
            --len;
            if (*line != '.' && missingdotstuffing == 0)
                missingdotstuffing = l;
        }
        memcpy(p, line, len);
        p += len;
        *p++ = '\n';
        *p = '\0';
    }

    if (longline > 0) {
        warn("%s too long in post", Client.host);
        Reply("%d Line %d too long\r\n",
              ihave ? NNTP_FAIL_IHAVE_REJECT : NNTP_FAIL_POST_REJECT,
              longline);
        POSTrejected++;
        return;
    }

    if (missingdotstuffing > 0) {
        Reply("%d Line %d without its initial dot doubled (dot-stuffing)\r\n",
              ihave ? NNTP_FAIL_IHAVE_REJECT : NNTP_FAIL_POST_REJECT,
              missingdotstuffing);
        POSTrejected++;
        return;
    }

    /* Send the article to the server. */
    response = ARTpost(article, idbuff, &permanent);
    if (response == NULL) {
        notice("%s %s ok %s", Client.host, ihave ? "ihave" : "post", idbuff);
        Reply("%d Article received %s\r\n",
              ihave ? NNTP_OK_IHAVE : NNTP_OK_POST, idbuff);
        POSTreceived++;
    } else {
        if ((p = strchr(response, '\r')) != NULL)
            *p = '\0';
        if ((p = strchr(response, '\n')) != NULL)
            *p = '\0';
        notice("%s %s failed %s", Client.host, ihave ? "ihave" : "post",
               response);
        if (!ihave || permanent) {
            /* For permanent errors, reject the message. */
            Reply("%d %s\r\n",
                  ihave ? NNTP_FAIL_IHAVE_REJECT : NNTP_FAIL_POST_REJECT,
                  response);
        } else {
            /* Non-permanent errors only have relevance to IHAVE, for
             * these we have the error status from the upstream
             * server to report.  It includes the answer code. */
            Reply("%s\r\n", response);
        }
        POSTrejected++;
    }
}

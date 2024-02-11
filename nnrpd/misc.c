/*
**  Miscellaneous support routines.
*/

#include "portable/system.h"

#include "inn/innconf.h"
#include "nnrpd.h"
#include "tls.h"

/* Outside the ifdef so that make depend works even ifndef HAVE_OPENSSL. */
#include "inn/ov.h"


/*
**  Match a list of newsgroup specifiers against a list of newsgroups.
**  func is called to see if there is a match.
*/
bool
PERMmatch(char **Pats, char **list)
{
    int i;
    char *p;
    int match = false;

    if (Pats == NULL || Pats[0] == NULL)
        return true;

    for (; *list; list++) {
        for (i = 0; (p = Pats[i]) != NULL; i++) {
            if (p[0] == '!') {
                if (uwildmat(*list, ++p))
                    match = false;
            } else if (uwildmat(*list, p))
                match = true;
        }
        if (match)
            /* If we can read it in one group, we can read it, period. */
            return true;
    }

    return false;
}


/*
**  Check to see if user is allowed to see this article by matching
**  Xref (or Newsgroups) header field body.
*/
bool
PERMartok(void)
{
    static char **grplist;
    char *p, **grp;

    if (!PERMspecified)
        return false;

    if ((p = GetHeader("Xref", true)) == NULL) {
        /* In case article does not include an Xref header field. */
        if ((p = GetHeader("Newsgroups", true)) != NULL) {
            if (!NGgetlist(&grplist, p))
                /* No newgroups or null entry. */
                return true;
        } else {
            return true;
        }
    } else {
        /* Skip path element. */
        if ((p = strchr(p, ' ')) == NULL)
            return true;
        for (p++; *p == ' '; p++)
            ;
        if (*p == '\0')
            return true;
        if (!NGgetlist(&grplist, p))
            /* No newgroups or null entry. */
            return true;
        /* Chop ':' and article number. */
        for (grp = grplist; *grp != NULL; grp++) {
            if ((p = strchr(*grp, ':')) == NULL)
                return true;
            *p = '\0';
        }
    }

#ifdef DO_PYTHON
    if (PY_use_dynamic) {
        char *reply;

        /* Authorize user at a Python authorization module. */
        if (PY_dynamic(PERMuser, p, false, &reply) < 0) {
            syslog(L_NOTICE, "PY_dynamic(): authorization skipped due to no "
                             "Python dynamic method defined");
        } else {
            if (reply != NULL) {
                syslog(L_TRACE,
                       "PY_dynamic() returned a refuse string for user %s at "
                       "%s who wants to read %s: %s",
                       PERMuser, Client.host, p, reply);
                free(reply);
                return false;
            }
            return true;
        }
    }
#endif /* DO_PYTHON */

    return PERMmatch(PERMreadlist, grplist);
}


/*
**  Parse a newsgroups line, return true if there were any.
*/
bool
NGgetlist(char ***argvp, char *list)
{
    char *p;

    for (p = list; *p; p++)
        if (*p == ',')
            *p = ' ';

    return Argify(list, argvp) != 0;
}


/*********************************************************************
 * POSTING RATE LIMITS -- The following code implements posting rate
 * limits.  News clients are indexed by IP number (or PERMuser, see
 * config file).  After a relatively configurable number of posts, the nnrpd
 * process will sleep for a period of time before posting anything.
 *
 * Each time that IP number posts a message, the time of
 * posting and the previous sleep time is stored.  The new sleep time
 * is computed based on these values.
 *
 * To compute the new sleep time, the previous sleep time is, for most
 * cases multiplied by a factor (backoff_k).
 *
 * See inn.conf(5) for how this code works.
 *
 *********************************************************************/

/* Defaults are pass through, i.e. not enabled .
 * NEW for INN 1.8 -- Use the inn.conf file to specify the following:
 *
 * backoffk: <integer>
 * backoffpostfast: <integer>
 * backoffpostslow: <integer>
 * backofftrigger: <integer>
 * backoffdb: <path>
 * backoffauth: <true|false>
 *
 * You may also specify posting backoffs on a per user basis.  To do this,
 * turn on backoffauth.
 *
 * Now these are runtime constants. <grin>
 */
static char postrec_dir[SMBUF]; /* Where is the post record directory? */

void
InitBackoffConstants(void)
{
    struct stat st;

    /* Default is not to enable this code. */
    BACKOFFenabled = false;

    /* Read the runtime config file to get parameters. */

    if ((PERMaccessconf->backoff_db == NULL)
        || !(PERMaccessconf->backoff_postslow >= 1L))
        return;

    /* Need this database for backing off. */
    strlcpy(postrec_dir, PERMaccessconf->backoff_db, sizeof(postrec_dir));
    if (stat(postrec_dir, &st) < 0) {
        if (ENOENT == errno) {
            if (!MakeDirectory(postrec_dir, true)) {
                syslog(L_ERROR, "%s cannot create backoff_db '%s': %s",
                       Client.host, postrec_dir, strerror(errno));
                return;
            }
        } else {
            syslog(L_ERROR, "%s cannot stat backoff_db '%s': %s", Client.host,
                   postrec_dir, strerror(errno));
            return;
        }
    }
    if (!S_ISDIR(st.st_mode)) {
        syslog(L_ERROR, "%s backoff_db '%s' is not a directory", Client.host,
               postrec_dir);
        return;
    }

    BACKOFFenabled = true;

    return;
}

/*
**  PostRecs are stored in individual files.  I didn't have a better
**  way offhand, don't want to touch DBZ, and the number of posters is
**  small compared to the number of readers.  This is the filename
**  corresponding to an IP number.
*/
char *
PostRecFilename(char *ip, char *user)
{
    static char buff[SPOOLNAMEBUFF];
    char dirbuff[SMBUF + 2 + 3 * 3];
    struct in_addr inaddr;
    unsigned long int addr;
    unsigned char quads[4];
    unsigned int i;

    if (PERMaccessconf->backoff_auth) {
        snprintf(buff, sizeof(buff), "%s/%s", postrec_dir, user);
        return (buff);
    }

    if (inet_aton(ip, &inaddr) < 1) {
        /* If inet_aton() fails, we'll assume it's an IPv6 address.  We'll
         * also assume for now that we're dealing with a limited number of
         * IPv6 clients so we'll place their files all in the same
         * directory for simplicity.  Someday we'll need to change this to
         * something more scalable such as DBZ when IPv6 clients become
         * more popular. */
        snprintf(buff, sizeof(buff), "%s/%s", postrec_dir, ip);
        return (buff);
    }
    /* If it's an IPv4 address just fall through. */

    addr = ntohl(inaddr.s_addr);
    for (i = 0; i < 4; i++)
        quads[i] = (unsigned char) (0xff & (addr >> (i * 8)));

    snprintf(dirbuff, sizeof(dirbuff), "%s/%03u%03u/%03u", postrec_dir,
             quads[3], quads[2], quads[1]);
    if (!MakeDirectory(dirbuff, true)) {
        syslog(L_ERROR, "%s Unable to create postrec directories '%s': %s",
               Client.host, dirbuff, strerror(errno));
        return NULL;
    }
    snprintf(buff, sizeof(buff), "%s/%03u", dirbuff, quads[0]);
    return (buff);
}

/*
**  Lock the post rec file.  Return 1 on lock, 0 on error.
*/
int
LockPostRec(char *path)
{
    char lockname[SPOOLNAMEBUFF];
    char temp[SPOOLNAMEBUFF];
    int statfailed = 0;

    snprintf(lockname, sizeof(lockname), "%s.lock", path);

    for (;; sleep(5)) {
        int fd;
        struct stat st;
        time_t now;

        fd = open(lockname, O_WRONLY | O_EXCL | O_CREAT, 0600);
        if (fd >= 0) {
            /* We got the lock! */
            snprintf(temp, sizeof(temp), "pid:%lu\n",
                     (unsigned long) getpid());
            if (write(fd, temp, strlen(temp)) < (ssize_t) strlen(temp)) {
                syslog(L_ERROR, "%s cannot write to lock file %s", Client.host,
                       strerror(errno));
                close(fd);
                return (0);
            }
            close(fd);
            return (1);
        }

        /* No lock.  See if the file is there. */
        if (stat(lockname, &st) < 0) {
            syslog(L_ERROR, "%s cannot stat lock file %s", Client.host,
                   strerror(errno));
            if (statfailed++ > 5)
                return (0);
            continue;
        }

        /* If lockfile is older than the value of
         * PERMaccessconf->backoff_postslow, remove it. */
        statfailed = 0;
        time(&now);
        if (now < (time_t) (st.st_ctime + PERMaccessconf->backoff_postslow))
            continue;
        syslog(L_ERROR, "%s removing stale lock file %s", Client.host,
               lockname);
        unlink(lockname);
    }
}

void
UnlockPostRec(char *path)
{
    char lockname[SPOOLNAMEBUFF];

    snprintf(lockname, sizeof(lockname), "%s.lock", path);
    if (unlink(lockname) < 0) {
        syslog(L_ERROR, "%s can't unlink lock file: %s", Client.host,
               strerror(errno));
    }
    return;
}

/*
** Get the stored postrecord for that IP.
*/
static int
GetPostRecord(char *path, long *lastpost, long *lastsleep, long *lastn)
{
    static char buff[SMBUF];
    FILE *fp;
    char *s;

    fp = fopen(path, "r");
    if (fp == NULL) {
        if (errno == ENOENT) {
            return 1;
        }
        syslog(L_ERROR, "%s Error opening '%s': %s", Client.host, path,
               strerror(errno));
        return 0;
    }

    if (fgets(buff, SMBUF, fp) == NULL) {
        syslog(L_ERROR, "%s Error reading '%s': %s", Client.host, path,
               strerror(errno));
        fclose(fp);
        return 0;
    }
    *lastpost = atol(buff);

    if ((s = strchr(buff, ',')) == NULL) {
        syslog(L_ERROR, "%s bad data in postrec file: '%s'", Client.host,
               buff);
        fclose(fp);
        return 0;
    }
    s++;
    *lastsleep = atol(s);

    if ((s = strchr(s, ',')) == NULL) {
        syslog(L_ERROR, "%s bad data in postrec file: '%s'", Client.host,
               buff);
        fclose(fp);
        return 0;
    }
    s++;
    *lastn = atol(s);

    fclose(fp);
    return 1;
}

/*
** Store the postrecord for that IP.
*/
static int
StorePostRecord(char *path, time_t lastpost, long lastsleep, long lastn)
{
    FILE *fp;

    fp = fopen(path, "w");
    if (fp == NULL) {
        syslog(L_ERROR, "%s Error opening '%s': %s", Client.host, path,
               strerror(errno));
        return 0;
    }

    fprintf(fp, "%ld,%ld,%ld\n", (long) lastpost, lastsleep, lastn);
    fclose(fp);
    return 1;
}

/*
** Return the proper sleeptime.  Return false on error.
*/
int
RateLimit(long *sleeptime, char *path)
{
    time_t now;
    long prevpost, prevsleep, prevn, n;

    now = time(NULL);
    prevpost = 0L;
    prevsleep = 0L;
    prevn = 0L;
    n = 0L;
    if (!GetPostRecord(path, &prevpost, &prevsleep, &prevn)) {
        syslog(L_ERROR, "%s can't get post record: %s", Client.host,
               strerror(errno));
        return 0;
    }
    /* Just because yer paranoid doesn't mean they ain't out ta get ya.
     * This is called paranoid clipping. */
    if (prevn < 0L)
        prevn = 0L;
    if (prevsleep < 0L)
        prevsleep = 0L;
    if ((unsigned long) prevsleep > PERMaccessconf->backoff_postfast)
        prevsleep = PERMaccessconf->backoff_postfast;

    /* Compute the new sleep time. */
    *sleeptime = 0L;
    if (prevpost <= 0L) {
        prevpost = 0L;
        prevn = 1L;
    } else {
        n = now - prevpost;
        if (n < 0L) {
            syslog(L_NOTICE, "%s previous post was in the future (%ld sec)",
                   Client.host, n);
            n = 0L;
        }
        if ((unsigned long) n < PERMaccessconf->backoff_postfast) {
            if ((unsigned long) prevn >= PERMaccessconf->backoff_trigger) {
                *sleeptime = 1 + (prevsleep * PERMaccessconf->backoff_k);
            }
        } else if ((unsigned long) n < PERMaccessconf->backoff_postslow) {
            if ((unsigned long) prevn >= PERMaccessconf->backoff_trigger) {
                *sleeptime = prevsleep;
            }
        } else {
            prevn = 0L;
        }
        prevn++;
    }

    *sleeptime = ((*sleeptime) > (long) PERMaccessconf->backoff_postfast)
                     ? (long) PERMaccessconf->backoff_postfast
                     : (*sleeptime);
    /* This ought to trap this bogon. */
    if ((*sleeptime) < 0L) {
        syslog(L_ERROR,
               "%s Negative sleeptime detected: %ld, prevsleep: %ld, N: %ld",
               Client.host, *sleeptime, prevsleep, n);
        *sleeptime = 0L;
    }

    /* Store the postrecord. */
    if (!StorePostRecord(path, now, *sleeptime, prevn)) {
        syslog(L_ERROR, "%s can't store post record: %s", Client.host,
               strerror(errno));
        return 0;
    }

    return 1;
}

#if defined(HAVE_SASL) || defined(HAVE_ZLIB)
/*
**  Check if the argument has a valid syntax.
**
**  Currently used for both SASL mechanisms (RFC 4643) and compression
**  algorithms.
**
**    algorithm = 1*20alg-char
**    alg-char = UPPER / DIGIT / "-" / "_"
*/
bool
IsValidAlgorithm(const char *string)
{
    size_t len = 0;
    const unsigned char *p;

    /* Not NULL. */
    if (string == NULL) {
        return false;
    }

    p = (const unsigned char *) string;

    for (; *p != '\0'; p++) {
        len++;

        if (!isupper((unsigned char) *p) && !isdigit((unsigned char) *p)
            && *p != '-' && *p != '_') {
            return false;
        }
    }

    if (len > 0 && len < 21) {
        return true;
    } else {
        return false;
    }
}
#endif /* HAVE_SASL || HAVE_ZLIB */

#if defined(HAVE_ZLIB)
/*
**  The COMPRESS command.  RFC 8054.
*/
void
CMDcompress(int ac, char *av[])
{
    bool result;

    /* Check the argument. */
    if (ac > 1) {
        if (!IsValidAlgorithm(av[1])) {
            Reply("%d Syntax error in compression algorithm name\r\n",
                  NNTP_ERR_SYNTAX);
            return;
        }
        if (strcasecmp(av[1], "DEFLATE") != 0) {
            Reply("%d Only the DEFLATE compression algorithm is supported\r\n",
                  NNTP_ERR_UNAVAILABLE);
            return;
        }
    }

    if (compression_layer_on) {
        Reply("%d Already using a compression layer\r\n", NNTP_ERR_ACCESS);
        return;
    }

    result = zlib_init();

    if (!result) {
        Reply("%d Impossible to activate compression\r\n", NNTP_FAIL_ACTION);
        return;
    }

    Reply("%d Compression now active; enjoy the speed!\r\n", NNTP_OK_COMPRESS);

    /* Flush any pending output, before enabling compression. */
    fflush(stdout);

    compression_layer_on = true;
}
#endif /* HAVE_ZLIB */

#if defined(HAVE_OPENSSL)
/*
**  The STARTTLS command.  RFC 4642.
*/
void
CMDstarttls(int ac UNUSED, char *av[] UNUSED)
{
    int result;
    bool boolval;

    if (encryption_layer_on) {
        Reply("%d Already using a security layer\r\n", NNTP_ERR_ACCESS);
        return;
    }

#    if defined(HAVE_ZLIB)
    /* If a compression layer is active, STARTTLS is not possible. */
    if (compression_layer_on) {
        Reply("%d Already using a compression layer\r\n", NNTP_ERR_ACCESS);
        return;
    }
#    endif /* HAVE_ZLIB */

    /* If the client is already authenticated, STARTTLS is not possible. */
    if (PERMauthorized && !PERMneedauth && !PERMcanauthenticate) {
        Reply(
            "%d Already authenticated without the use of a security layer\r\n",
            NNTP_ERR_ACCESS);
        return;
    }

    result = tls_init();

    if (result == -1) {
        /* No reply because tls_init() has already sent one. */
        return;
    }

    /* Close out any existing article, report group stats.
     * RFC 4642 requires the reset of any knowledge about the client. */
    if (GRPcur) {
        ARTclose();
        GRPreport();
        OVctl(OVCACHEFREE, &boolval);
        free(GRPcur);
        GRPcur = NULL;
        if (ARTcount) {
            syslog(L_NOTICE, "%s exit for STARTTLS articles %lu groups %ld",
                   Client.host, ARTcount, GRPcount);
        }
        GRPcount = 0;
        PERMgroupmadeinvalid = false;
    }

    /* We can now assume a secure connection will be negotiated because
     * nnrpd will exit if STARTTLS fails.
     * Check the permissions the client will have after having successfully
     * negotiated a TLS layer.  (There may be auth blocks requiring the
     * negotiation of a security layer in readers.conf that match the
     * connection.)
     * In case the client would no longer have access to the server, or an
     * authentication error happens, the connection aborts after a fatal 400
     * response code sent by PERMgetpermissions(). */
    encryption_layer_on = true;
    PERMgetaccess(false);
    PERMgetpermissions();

    Reply("%d Begin TLS negotiation now\r\n", NNTP_CONT_STARTTLS);
    fflush(stdout);

    /* Must flush our buffers before starting TLS. */

    result = tls_start_servertls(0,  /* Read. */
                                 1); /* Write. */
    if (result == -1) {
        /* No reply because we have already sent NNTP_CONT_STARTTLS.
         * We close the connection. */
        ExitWithStats(1, false);
    }

#    if defined(HAVE_SASL)
    /* Tell SASL about the negotiated layer. */
    result = sasl_setprop(sasl_conn, SASL_SSF_EXTERNAL,
                          (sasl_ssf_t *) &tls_cipher_usebits);
    if (result != SASL_OK) {
        syslog(L_NOTICE, "sasl_setprop() failed: CMDstarttls()");
    }

    result = sasl_setprop(sasl_conn, SASL_AUTH_EXTERNAL, tls_peer_CN);
    if (result != SASL_OK) {
        syslog(L_NOTICE, "sasl_setprop() failed: CMDstarttls()");
    }
#    endif /* HAVE_SASL */

#    if defined(HAVE_ZLIB) && OPENSSL_VERSION_NUMBER >= 0x00090800fL
    /* Check whether a compression layer has just been added.
     * SSL_get_current_compression() is defined in OpenSSL versions >= 0.9.8
     * final release, as well as LibreSSL. */
    tls_compression_on = (SSL_get_current_compression(tls_conn) != NULL);
    compression_layer_on = tls_compression_on;
#    endif /* HAVE_ZLIB && OPENSSL >= v0.9.8 */

    /* Reset our read buffer so as to prevent plaintext command injection. */
    line_reset(&NNTPline);
}
#endif /* HAVE_OPENSSL */

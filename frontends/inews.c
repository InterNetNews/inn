/*
**  Send an article (prepared by someone on the local site) to the
**  master news server.
*/

#include "portable/system.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <sys/stat.h>

#ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#endif
#include <time.h>

#include "inn/innconf.h"
#include "inn/libinn.h"
#include "inn/messages.h"
#include "inn/newsuser.h"
#include "inn/nntp.h"
#include "inn/paths.h"

/* Signature handling.  The separator will be appended before the signature,
   and at most SIG_MAXLINES will be appended. */
#define SIG_MAXLINES   4
#define SIG_SEPARATOR  "-- \n"

#define FLUSH_ERROR(F) (fflush((F)) == EOF || ferror((F)))
#define LPAREN         '(' /* For vi :-) */
#define HEADER_DELTA   20
#define GECOSTERM(c)   ((c) == ',' || (c) == ';' || (c) == ':' || (c) == LPAREN)

typedef enum _HEADERTYPE {
    HTobs,
    HTreq,
    HTstd
} HEADERTYPE;

typedef struct _HEADER {
    const char *Name;
    bool CanSet;
    HEADERTYPE Type;
    int Size;
    char *Value;
} HEADER;

static bool Dump;
static bool Revoked;
static bool Spooling;
static char **OtherHeaders;
static char SIGSEP[] = SIG_SEPARATOR;
static FILE *FromServer;
static FILE *ToServer;
static int OtherCount;
static int OtherSize;
static const char *Exclusions = "";
static const char *const BadDistribs[] = {BAD_DISTRIBS};

static void Usage(void) __attribute__((__noreturn__));

/* clang-format off */
static HEADER Table[] = {
    /* Name,           CanSet,  Type, Size, Value */
    {"Path",             true,   HTstd, 0, NULL},
#define _path          0
    {"From",             true,   HTstd, 0, NULL},
#define _from          1
    {"Newsgroups",       true,   HTreq, 0, NULL},
#define _newsgroups    2
    {"Subject",          true,   HTreq, 0, NULL},
#define _subject       3
    {"Control",          true,   HTstd, 0, NULL},
#define _control       4
    {"Supersedes",       true,   HTstd, 0, NULL},
#define _supersedes    5
    {"Followup-To",      true,   HTstd, 0, NULL},
#define _followupto    6
    {"Date",             true,   HTstd, 0, NULL},
#define _date          7
    {"Organization",     true,   HTstd, 0, NULL},
#define _organization  8
    {"Lines",            true,   HTstd, 0, NULL},
#define _lines         9
    {"Sender",           true,   HTstd, 0, NULL},
#define _sender       10
    {"Approved",         true,   HTstd, 0, NULL},
#define _approved     11
    {"Distribution",     true,   HTstd, 0, NULL},
#define _distribution 12
    {"Expires",          true,   HTstd, 0, NULL},
#define _expires      13
    {"Message-ID",       true,   HTstd, 0, NULL},
#define _messageid    14
    {"References",       true,   HTstd, 0, NULL},
#define _references   15
    {"Reply-To",         true,   HTstd, 0, NULL},
#define _replyto      16
    {"Also-Control",     true,   HTstd, 0, NULL},
#define _alsocontrol  17
    {"Xref",             false,  HTstd, 0, NULL},
    {"Summary",          true,   HTstd, 0, NULL},
    {"Keywords",         true,   HTstd, 0, NULL},
    {"Date-Received",    false,  HTobs, 0, NULL},
    {"Received",         false,  HTobs, 0, NULL},
    {"Posted",           false,  HTobs, 0, NULL},
    {"Posting-Version",  false,  HTobs, 0, NULL},
    {"Relay-Version",    false,  HTobs, 0, NULL},
};
/* clang-format on */

#define HDR(_x) (Table[(_x)].Value)


/*
**  Send the server a quit message, wait for a reply.
*/
__attribute__((__noreturn__)) static void
QuitServer(int x)
{
    char buff[MED_BUFFER];
    char *p;

    if (Spooling)
        exit(x);
    if (x)
        warn("article not posted");
    fprintf(ToServer, "quit\r\n");
    if (FLUSH_ERROR(ToServer))
        sysdie("cannot send quit to server");
    if (fgets(buff, sizeof buff, FromServer) == NULL)
        sysdie("warning: server did not reply to quit");
    if ((p = strchr(buff, '\r')) != NULL)
        *p = '\0';
    if ((p = strchr(buff, '\n')) != NULL)
        *p = '\0';
    if (atoi(buff) != NNTP_OK_QUIT)
        die("server did not reply to quit properly: %s", buff);
    fclose(FromServer);
    fclose(ToServer);
    exit(x);
}


/*
**  Failure handler, called by die.  Calls QuitServer to cleanly shut down the
**  connection with the remote server before exiting.
*/
static int
fatal_cleanup(void)
{
    /* Don't recurse. */
    message_fatal_cleanup = NULL;

    /* QuitServer does all the work. */
    QuitServer(1);
    /* NOTREACHED */
    return 1;
}


/*
**  Flush a stdio FILE; exit if there are any errors.
*/
static void
SafeFlush(FILE *F)
{
    if (FLUSH_ERROR(F))
        sysdie("cannot send text to server");
}


/*
**  Trim trailing spaces, return pointer to first non-space char.
*/
static char *
TrimSpaces(char *p)
{
    char *start;

    for (start = p; ISWHITE(*start); start++)
        continue;
    for (p = start + strlen(start);
         p > start && isspace((unsigned char) p[-1]);)
        *--p = '\0';
    return start;
}


/*
**  Mark the end of the header field starting at p, and return a pointer
**  to the start of the next one.  Handles continuations.
*/
static char *
NextHeader(char *p)
{
    char *q;
    for (q = p;; p++) {
        if ((p = strchr(p, '\n')) == NULL) {
            die("article is all headers");
        }
        /* Check the maximum length of a single line. */
        if (p - q + 1 > MAXARTLINELENGTH) {
            die("header line too long");
        }
        /* Check if there is a continuation line for the header field. */
        if (ISWHITE(p[1])) {
            q = p + 1;
            continue;
        }
        *p = '\0';
        return p + 1;
    }
}


/*
**  Strip any header fields off the article and dump them into the table.
*/
static char *
StripOffHeaders(char *article)
{
    char *p;
    char *q;
    HEADER *hp;
    char c;

    /* Set up the other header fields list. */
    OtherSize = HEADER_DELTA;
    OtherHeaders = xmalloc(OtherSize * sizeof(char *));
    OtherCount = 0;

    /* Scan through buffer, a header field at a time. */
    for (p = article;;) {
        if ((q = strchr(p, ':')) == NULL)
            die("no colon in header field \"%.30s...\"", p);
        if (q[1] == '\n' && !ISWHITE(q[2])) {
            /* Empty header field body; ignore this one, get next line. */
            p = NextHeader(p);
            if (*p == '\n')
                break;
        }

        if (q[1] != '\0' && !ISWHITE(q[1])) {
            if ((q = strchr(q, '\n')) != NULL)
                *q = '\0';
            die("no space after colon in \"%.30s...\"", p);
        }

        /* See if it's a known header field name. */
        c = islower((unsigned char) *p) ? toupper((unsigned char) *p) : *p;
        for (hp = Table; hp < ARRAY_END(Table); hp++)
            if (c == hp->Name[0] && p[hp->Size] == ':'
                && ISWHITE(p[hp->Size + 1])
                && strncasecmp(p, hp->Name, hp->Size) == 0) {
                if (hp->Type == HTobs)
                    die("obsolete header field: %s", hp->Name);
                if (hp->Value)
                    die("duplicate header field: %s", hp->Name);
                for (q = &p[hp->Size + 1]; ISWHITE(*q); q++)
                    continue;
                hp->Value = q;
                break;
            }

        /* No; add it to the set of other header fields. */
        if (hp == ARRAY_END(Table)) {
            if (OtherCount >= OtherSize - 1) {
                OtherSize += HEADER_DELTA;
                OtherHeaders =
                    xrealloc(OtherHeaders, OtherSize * sizeof(char *));
            }
            OtherHeaders[OtherCount++] = p;
        }

        /* Get start of next header field; if it's a blank line, we hit
         * the end. */
        p = NextHeader(p);
        if (*p == '\n')
            break;
    }

    return p + 1;
}


/*
**  See if the user is the news administrator.
*/
static bool
AnAdministrator(void)
{
    uid_t news_uid;
    gid_t news_gid;

    if (Revoked)
        return false;

    /* Find out who we are. */
    if (get_news_uid_gid(&news_uid, &news_gid, false) != 0) {
        /* Silent failure; clients might not have the group. */
        return false;
    }
    if (getuid() == news_uid)
        return true;

        /* See if we are in the right group and examine process
         * supplementary groups, rather than the group(5) file entry.
         */
#ifdef HAVE_GETGROUPS
    {
        int ngroups = getgroups(0, 0);
        GETGROUPS_T *groups, *gp;
        int rv;
        int rest;

        groups = (GETGROUPS_T *) xmalloc(ngroups * sizeof(GETGROUPS_T));
        if ((rv = getgroups(ngroups, groups)) < 0) {
            /* Silent failure; client doesn't have the group. */
            return false;
        }
        for (rest = ngroups, gp = groups; rest > 0; rest--, gp++) {
            if (*gp == (GETGROUPS_T) news_gid)
                return true;
        }
    }
#endif

    return false;
}


/*
**  Check the control message, and see if it's legit.
*/
static void
CheckControl(char *ctrl)
{
    char *p;
    char *q;
    char save;

    /* Snip off the first word. */
    for (p = ctrl; ISWHITE(*p); p++)
        continue;
    for (ctrl = p; *p && !ISWHITE(*p); p++)
        continue;
    if (p == ctrl)
        die("empty Control header field");
    save = *p;
    *p = '\0';

    if (strcasecmp(ctrl, "cancel") == 0) {
        for (q = p + 1; ISWHITE(*q); q++)
            continue;
        if (*q == '\0')
            die("message ID missing in cancel");
    } else if (strcasecmp(ctrl, "checkgroups") == 0
               || strcasecmp(ctrl, "ihave") == 0
               || strcasecmp(ctrl, "sendme") == 0
               || strcasecmp(ctrl, "newgroup") == 0
               || strcasecmp(ctrl, "rmgroup") == 0) {
        if (!AnAdministrator())
            die("ask your news administrator to do the %s for you", ctrl);
    } else {
        die("%s is not a valid control message", ctrl);
    }
    *p = save;
}


/*
**  Parse the GECOS field to get the user's full name.  This comes Sendmail's
**  buildfname routine.  Ignore leading stuff like "23-" "stuff]-" or
**  "stuff -" as well as trailing whitespace, or anything that comes after
**  a comma, semicolon, or in parentheses.  This seems to strip off most of
**  the UCB or ATT stuff people fill out the entries with.  Also, turn &
**  into the login name, with perhaps an initial capital.  (Everyone seems
**  to hate that, but everyone also supports it.)
*/
static char *
FormatUserName(struct passwd *pwp, char *node)
{
    char outbuff[SMBUF];
    char *buff;
    char *out;
    char *p;
#ifdef DO_MUNGE_GECOS
    int left = SMBUF - 1;
#endif

#if !defined(DONT_MUNGE_GETENV)
    memset(outbuff, 0, SMBUF);
    if ((p = getenv("NAME")) != NULL)
        strlcpy(outbuff, p, SMBUF);
    if (strlen(outbuff) == 0) {
#endif /* !defined(DONT_MUNGE_GETENV) */


#ifndef DO_MUNGE_GECOS
        strlcpy(outbuff, pwp->pw_gecos, SMBUF);
#else
    /* Be very careful here.  If we're not, we can potentially overflow our
     * buffer.  Remember that on some Unix systems, the content of the GECOS
     * field is under (untrusted) user control and we could be setgid. */
    p = pwp->pw_gecos;
    if (*p == '*')
        p++;
    for (out = outbuff; *p && !GECOSTERM(*p) && left; p++) {
        if (*p == '&') {
            strncpy(out, pwp->pw_name, left);
            if (islower((unsigned char) *out)
                && (out == outbuff || !isalpha((unsigned char) out[-1])))
                *out = toupper((unsigned char) *out);
            while (*out) {
                out++;
                left--;
            }
        } else if (*p == '-' && p > pwp->pw_gecos
                   && (isdigit((unsigned char) p[-1])
                       || isspace((unsigned char) p[-1]) || p[-1] == ']')) {
            out = outbuff;
            left = SMBUF - 1;
        } else {
            *out++ = *p;
            left--;
        }
    }
    *out = '\0';
#endif /* DO_MUNGE_GECOS */

#if !defined(DONT_MUNGE_GETENV)
    }
#endif /* !defined(DONT_MUNGE_GETENV) */

    out = TrimSpaces(outbuff);
    if (out[0])
        buff = concat(pwp->pw_name, "@", node, " (", out, ")", (char *) 0);
    else
        buff = concat(pwp->pw_name, "@", node, (char *) 0);
    return buff;
}


/*
**  Check the Distribution header field, and exit on error.
*/
static void
CheckDistribution(char *p)
{
    static char SEPS[] = " \t,";
    const char *const *dp;

    if ((p = strtok(p, SEPS)) == NULL)
        die("cannot parse Distribution header field");
    do {
        for (dp = BadDistribs; *dp; dp++)
            if (uwildmat(p, *dp))
                die("illegal distribution %s", p);
    } while ((p = strtok((char *) NULL, SEPS)) != NULL);
}


/*
**  Process all the headers.  FYI, they're done in RFC-order.
*/
static void
ProcessHeaders(bool AddOrg, bool AddSender, struct passwd *pwp)
{
    static char PATHFLUFF[] = PATHMASTER;
    static char *sendbuff = NULL;
    HEADER *hp;
    char *p;
    char buff[SMBUF];
    char from[SMBUF];

    /* Do some preliminary fix-ups. */
    for (hp = Table; hp < ARRAY_END(Table); hp++) {
        if (!hp->CanSet && hp->Value)
            die("cannot set system header field %s", hp->Name);
        if (hp->Value) {
            hp->Value = TrimSpaces(hp->Value);
            if (*hp->Value == '\0')
                hp->Value = NULL;
        }
    }

    /* Set From or Sender. */
    if ((p = innconf->fromhost) == NULL)
        sysdie("cannot get hostname");
    if (HDR(_from) == NULL)
        HDR(_from) = FormatUserName(pwp, p);
    else if (AddSender) {
        if (sendbuff != NULL)
            free(sendbuff);
        sendbuff = concat(pwp->pw_name, "%", p, (char *) 0);
        strlcpy(from, HDR(_from), SMBUF);
        HeaderCleanFrom(from);
        if (strcmp(from, sendbuff) != 0)
            HDR(_sender) = xstrdup(sendbuff);
    }

    if (HDR(_date) == NULL) {
        /* Set Date. */
        if (!makedate(-1, true, buff, sizeof(buff)))
            die("cannot generate Date header field body");
        HDR(_date) = xstrdup(buff);
    }

    /* Newsgroups are checked later. */

    /* Set Subject; Control overrides the subject. */
    if (HDR(_control)) {
        CheckControl(HDR(_control));
    } else {
        p = HDR(_subject);
        if (p == NULL)
            die("required Subject header field is missing or empty");
        else if (HDR(_alsocontrol))
            CheckControl(HDR(_alsocontrol));
    }

    /* Set Message-ID */
    if (HDR(_messageid) == NULL) {
        if ((p = GenerateMessageID(NULL)) == NULL)
            die("cannot generate Message-ID header field body");
        HDR(_messageid) = xstrdup(p);
    } else if ((p = strchr(HDR(_messageid), '@')) == NULL
               || strchr(++p, '@') != NULL) {
        die("Message-ID must have exactly one @");
    }

    /* Set Path */
    if (HDR(_path) == NULL) {
        HDR(_path) = concat(Exclusions, PATHFLUFF, (char *) 0);
    }

    /* Reply-To; left alone. */
    /* Sender; set above. */
    /* Followup-To; checked with Newsgroups. */

    /* Check Expires. */
    if (HDR(_expires) && parsedate_rfc5322_lax(HDR(_expires)) == -1)
        die("cannot parse \"%s\" as an expiration date", HDR(_expires));

    /* References; left alone. */
    /* Control; checked above. */

    /* Distribution. */
    if ((p = HDR(_distribution)) != NULL) {
        p = xstrdup(p);
        CheckDistribution(p);
        free(p);
    }

    /* Set Organization. */
    if (AddOrg && HDR(_organization) == NULL
        && (p = innconf->organization) != NULL) {
        HDR(_organization) = xstrdup(p);
    }

    /* Keywords; left alone. */
    /* Summary; left alone. */
    /* Approved; left alone. */
    /* Supersedes; left alone. */

    /* Now make sure everything is there. */
    for (hp = Table; hp < ARRAY_END(Table); hp++)
        if (hp->Type == HTreq && hp->Value == NULL)
            die("required header field %s is missing or empty", hp->Name);
}


/*
**  Try to append $HOME/.signature to the article.  When in doubt, exit
**  out in order to avoid postings like "Sorry, I forgot my .signature
**  -- here's the article again."
*/
static char *
AppendSignature(bool UseMalloc, char *article, char *homedir)
{
    int i;
    int length;
    size_t artsize;
    char *p;
    char buff[BUFSIZ];
    char *sigpath = NULL;
    FILE *F;

    /* Open the file. */
    sigpath = concatpath(homedir, ".signature");
    if ((F = fopen(sigpath, "r")) == NULL) {
        free(sigpath);
        if (errno == ENOENT)
            return article;
        fprintf(stderr, "Can't add your .signature (%s), article not posted",
                strerror(errno));
        QuitServer(1);
    }
    free(sigpath);

    /* Read it in. */
    length = fread(buff, 1, sizeof buff - 2, F);
    i = feof(F);
    fclose(F);
    if (length == 0)
        die("signature file is empty");
    if (length < 0)
        sysdie("cannot read signature file");
    if (length == sizeof buff - 2 && !i)
        die("signature is too large");

    /* Make sure the buffer ends with \n\0. */
    if (buff[length - 1] != '\n')
        buff[length++] = '\n';
    buff[length] = '\0';

    /* Count the lines. */
    for (i = 0, p = buff; (p = strchr(p, '\n')) != NULL; p++)
        if (++i > SIG_MAXLINES)
            die("signature has too many lines");

    /* Grow the article to have the signature. */
    i = strlen(article);
    artsize = i + sizeof(SIGSEP) - 1 + length + 1;
    if (UseMalloc) {
        p = xmalloc(artsize);
        strlcpy(p, article, artsize);
        article = p;
    } else
        article = xrealloc(article, artsize);
    strlcat(article, SIGSEP, artsize);
    strlcat(article, buff, artsize);
    return article;
}


/*
**  See if the user has more included text than new text.  Simple-minded, but
**  reasonably effective for catching neophyte's mistakes.  A line starting
**  with > is included text.  Decrement the count on lines starting with <
**  so that we don't reject diff(1) output.
*/
static void
CheckIncludedText(char *p, int lines)
{
    int i;

    for (i = 0;; p++) {
        switch (*p) {
        case '>':
            i++;
            break;
        case '|':
            i++;
            break;
        case ':':
            i++;
            break;
        case '<':
            i--;
            break;
        }
        if ((p = strchr(p, '\n')) == NULL)
            break;
    }
    if ((i * 2 > lines) && (lines > 40))
        die("more included text than new text");
}


/*
**  Read stdin into a string and return it.  Can't use ReadInDescriptor
**  since that will fail if stdin is a tty.
*/
static char *
ReadStdin(void)
{
    int size;
    char *p;
    char *article;
    char *end;
    int i;

    size = BUFSIZ;
    article = xmalloc(size);
    end = &article[size - 3];
    for (p = article; (i = getchar()) != EOF; *p++ = (char) i)
        if (p == end) {
            article = xrealloc(article, size + BUFSIZ);
            p = &article[size - 3];
            size += BUFSIZ;
            end = &article[size - 3];
        }

    /* Force a \n terminator. */
    if (p > article && p[-1] != '\n')
        *p++ = '\n';
    *p = '\0';
    return article;
}


/*
**  Offer the article to the server, return its reply.
*/
static int
OfferArticle(char *buff, bool Authorized)
{
    fprintf(ToServer, "post\r\n");
    SafeFlush(ToServer);
    if (fgets(buff, MED_BUFFER, FromServer) == NULL)
        sysdie(Authorized ? "Can't offer article to server (authorized)"
                          : "Can't offer article to server");
    return atoi(buff);
}


/*
**  Spool article to temp file.
*/
static void
Spoolit(char *article, size_t Length, char *deadfile)
{
    HEADER *hp;
    FILE *F;
    int i;

    /* Try to write to the deadfile. */
    if (deadfile == NULL)
        return;
    F = xfopena(deadfile);
    if (F == NULL)
        sysdie("cannot create spool file");

    /* Write the headers and a blank line. */
    for (hp = Table; hp < ARRAY_END(Table); hp++)
        if (hp->Value)
            fprintf(F, "%s: %s\n", hp->Name, hp->Value);
    for (i = 0; i < OtherCount; i++)
        fprintf(F, "%s\n", OtherHeaders[i]);
    fprintf(F, "\n");
    if (FLUSH_ERROR(F))
        sysdie("cannot write headers");

    /* Write the article and exit. */
    if (fwrite(article, 1, Length, F) != Length)
        sysdie("cannot write article");
    if (FLUSH_ERROR(F))
        sysdie("cannot write article");
    if (fclose(F) == EOF)
        sysdie("cannot close spool file");
}


/*
**  Print usage message and exit.
*/
static void
Usage(void)
{
    fprintf(stderr,
            "Usage: inews [-D] [-h] [other_options] [header_flags] [article]\n"
            "    Look at inews(1) man page for a complete list of recognized "
            "flags\n");
    /* Don't call QuitServer here -- connection isn't open yet. */
    exit(1);
}


int
main(int ac, char *av[])
{
    static char NOCONNECT[] = "cannot connect to server";
    int i;
    char *p;
    HEADER *hp;
    int j;
    int port;
    int Mode;
    struct passwd *pwp;
    char *article;
    char *deadfile;
    char buff[MED_BUFFER];
    char SpoolMessage[MED_BUFFER];
    bool DoSignature;
    bool AddOrg, AddSender;
    bool DiscardEmpty;
    bool ForceAuth;
    size_t Length;
    uid_t uid;

    /* First thing, set up logging and our identity. */
    message_program_name = "inews";

    /* Find out who we are. */
    uid = geteuid();
    if (uid == (uid_t) -1)
        sysdie("cannot get your user ID");
    if ((pwp = getpwuid(uid)) == NULL)
        sysdie("cannot get your passwd entry");

    /* Set defaults. */
    Mode = '\0';
    Dump = false;
    DoSignature = true;
    AddOrg = true;
    AddSender = true;
    DiscardEmpty = false;
    ForceAuth = false;
    port = 0;

    if (!innconf_read(NULL))
        exit(1);

    umask(NEWSUMASK);

    /* Parse command-line options.
     *
     * The flags are compatible with the original inews (C News) and the
     * improved inews-xt from Olaf Titz.
     * Also keep compatibility with tinews.pl (shipped with tin). */
    while ((i = getopt(ac, av,
                       "a:Ac:d:De:Ef:F:hHi:ILm:n:No:Op:Pr:Rs:St:vVw:Wx:XY"))
           != EOF) {
        switch (i) {
        case 'H':
        default:
            Usage();
            /* NOTREACHED */
        case 'D':
        case 'N':
            Dump = true;
            break;
        case 'A':
        case 'V':
        case 'W':
            /* Ignore C News options. */
            break;
        case 'i': /* 3 flags for PGP-signing messages. */
        case 's':
        case 'X':
        case 'I': /* Don't add Injection-Date. */
        case 'L': /* Don't add Cancel-Key. */
        case 'v': /* Verbose mode. */
            /* Ignore tinews.pl options. */
            break;
        case 'E':
            DiscardEmpty = true;
            break;
        case 'O':
            AddOrg = false;
            break;
        case 'P':
            AddSender = false;
            break;
        case 'R':
            Revoked = true;
            break;
        case 'S':
            DoSignature = false;
            break;
        case 'h':
            Mode = i;
            break;
        case 'x':
            Exclusions = concat(optarg, "!", (char *) 0);
            break;
        case 'Y':
            ForceAuth = true;
            break;
        case 'p':
            port = atoi(optarg);
            break;
            /* clang-format off */
        /* Header fields that can be specified on the command line. */
        case 'a':   HDR(_approved) = optarg;       break;
        case 'c':   HDR(_control) = optarg;        break;
        case 'd':   HDR(_distribution) = optarg;   break;
        case 'e':   HDR(_expires) = optarg;        break;
        case 'f':   HDR(_from) = optarg;           break;
        case 'm':   HDR(_messageid) = optarg;      break;
        case 'n':   HDR(_newsgroups) = optarg;     break;
        case 'r':   HDR(_replyto) = optarg;        break;
        case 't':   HDR(_subject) = optarg;        break;
        case 'F':   HDR(_references) = optarg;     break;
        case 'o':   HDR(_organization) = optarg;   break;
        case 'w':   HDR(_followupto) = optarg;     break;
            /* clang-format on */
        }
    }
    ac -= optind;
    av += optind;

    /* Parse positional arguments; at most one, the input file. */
    switch (ac) {
    default:
        Usage();
        /* NOTREACHED */
    case 0:
        /* Read stdin. */
        article = ReadStdin();
        break;
    case 1:
        /* Read named file. */
        article = ReadInFile(av[0], (struct stat *) NULL);
        if (article == NULL)
            sysdie("cannot read input file");
        break;
    }

    if (port == 0)
        port = NNTP_PORT;

    /* Try to open a connection to the server. */
    if (NNTPremoteopen(port, &FromServer, &ToServer, buff, sizeof(buff)) < 0) {
        Spooling = true;
        if ((p = strchr(buff, '\n')) != NULL)
            *p = '\0';
        if ((p = strchr(buff, '\r')) != NULL)
            *p = '\0';
        strlcpy(SpoolMessage, buff[0] ? buff : NOCONNECT,
                sizeof(SpoolMessage));
        deadfile = concatpath(pwp->pw_dir, "dead.article");
    } else {
        /* We now have an open server connection, so close it on failure. */
        message_fatal_cleanup = fatal_cleanup;

        /* See if we can post. */
        i = atoi(buff);

        /* Tell the server we're posting. */
        setbuf(FromServer, xmalloc(BUFSIZ));
        setbuf(ToServer, xmalloc(BUFSIZ));
        fprintf(ToServer, "mode reader\r\n");
        SafeFlush(ToServer);
        if (fgets(buff, MED_BUFFER, FromServer) == NULL)
            sysdie("cannot tell server we're reading");
        if ((j = atoi(buff)) != NNTP_ERR_COMMAND)
            i = j;

        if (i != NNTP_OK_BANNER_POST || ForceAuth) {
            /* We try to authenticate in case it is all the same possible
             * to post. */
            if (NNTPsendpassword((char *) NULL, FromServer, ToServer) < 0)
                die("you do not have permission to post");
        }
        deadfile = NULL;
    }

    /* Basic processing. */
    for (hp = Table; hp < ARRAY_END(Table); hp++)
        hp->Size = strlen(hp->Name);
    if (Mode == 'h')
        article = StripOffHeaders(article);
    for (i = 0, p = article; (p = strchr(p, '\n')) != NULL; i++, p++)
        continue;
    if (innconf->checkincludedtext)
        CheckIncludedText(article, i);
    if (DoSignature)
        article = AppendSignature(Mode == 'h', article, pwp->pw_dir);
    ProcessHeaders(AddOrg, AddSender, pwp);
    Length = strlen(article);
    if ((innconf->localmaxartsize != 0) && (Length > innconf->localmaxartsize))
        die("article is larger than local limit of %lu bytes",
            innconf->localmaxartsize);

    /* Do final checks. */
    if (i == 0 && HDR(_control) == NULL) {
        if (DiscardEmpty)
            exit(0);
        else
            die("article is empty");
    }

    if (Dump) {
        /* Write the headers and a blank line. */
        for (hp = Table; hp < ARRAY_END(Table); hp++)
            if (hp->Value)
                printf("%s: %s\n", hp->Name, hp->Value);
        for (i = 0; i < OtherCount; i++)
            printf("%s\n", OtherHeaders[i]);
        printf("\n");
        if (FLUSH_ERROR(stdout))
            sysdie("cannot write headers");

        /* Write the article and exit. */
        if (fwrite(article, 1, Length, stdout) != Length)
            sysdie("cannot write article");
        SafeFlush(stdout);
        QuitServer(0);
    }

    if (Spooling) {
        warn("warning: %s", SpoolMessage);
        warn("article will be spooled");
        Spoolit(article, Length, deadfile);
        exit(0);
    }

    /* Article is prepared, offer it to the server. */
    i = OfferArticle(buff, false);
    if (i == NNTP_FAIL_AUTH_NEEDED) {
        /* Posting not allowed, try to authorize. */
        if (NNTPsendpassword((char *) NULL, FromServer, ToServer) < 0)
            sysdie("authorization error");
        i = OfferArticle(buff, true);
    }
    if (i != NNTP_CONT_POST)
        die("server doesn't want the article: %s", buff);

    /* Write the headers, a blank line, then the article. */
    for (hp = Table; hp < ARRAY_END(Table); hp++)
        if (hp->Value)
            fprintf(ToServer, "%s: %s\r\n", hp->Name, hp->Value);
    for (i = 0; i < OtherCount; i++)
        fprintf(ToServer, "%s\r\n", OtherHeaders[i]);
    fprintf(ToServer, "\r\n");
    if (NNTPsendarticle(article, ToServer, true) < 0)
        sysdie("cannot send article to server");
    SafeFlush(ToServer);

    if (fgets(buff, sizeof buff, FromServer) == NULL)
        sysdie("no reply from server after sending the article");
    if ((p = strchr(buff, '\r')) != NULL)
        *p = '\0';
    if ((p = strchr(buff, '\n')) != NULL)
        *p = '\0';
    if (atoi(buff) != NNTP_OK_POST)
        die("cannot send article to server: %s", buff);

    /* Close up. */
    QuitServer(0);
    /* NOTREACHED */
    return 1;
}

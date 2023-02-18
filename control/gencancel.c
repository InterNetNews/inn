/*
**  Generate cancel control messages, with appropriate admin Cancel-Key.
**
**  Initial implementation in 2022 by Julien ÉLIE.
*/

#include "portable/system.h"

#include "inn/history.h"
#include "inn/innconf.h"
#include "inn/libinn.h"
#include "inn/messages.h"
#include "inn/paths.h"
#include "inn/secrets.h"
#include "inn/storage.h"
#include "inn/wire.h"

static const char usage[] = "\
Usage: gencancel [-adkLm] [-b body] [-c charset] [-f from] [-n newsgroups]\n\
                 [-s subject] '<Message-ID>'\n\
\n\
Write to standard output a cancel control article for <Message-Id>.\n\
\n\
    -a              Do not add an Approved header field\n\
    -b body         Use the argument as the body of the cancel article\n\
    -c charset      Use the argument for the Content-Type charset\n\
    -d              Do not use local time for the Date header field\n\
    -f from         Use the argument for the From and Approved header fields\n\
    -k              Only output the body of the Cancel-Key header field\n\
    -L              Do not generate a Cancel-Key header field\n\
    -m              Do not generate a Message-ID header field\n\
    -n newsgroups   Use the argument for the Newsgroups header field\n\
    -s subject      Use the argument for the Subject header field\n\
\n\
    Message-ID      The Message-ID to cancel (with quotes around for the\n\
                    shell, and angle brackets)\n";

static struct history *History;


/*
**  Retrieve the Newsgroups header field body of the article to be cancelled,
**  and return it as an allocated string.
**  If the original article is not found, we bail out (-n has to be used).
*/
static char *
getNewsgroups(char *mid)
{
    ARTHANDLE *art;
    TOKEN token;
    char *HistoryText;
    char *start, *end;
    char *newsgroups;
    bool flag = false;

    HistoryText = concatpath(innconf->pathdb, INN_PATH_HISTORY);
    History = HISopen(HistoryText, innconf->hismethod, HIS_RDONLY);

    if (History == NULL) {
        die("Cannot open history; you may want to use -n\n");
    }

    if (!HISlookup(History, mid, NULL, NULL, NULL, &token)) {
        HISclose(History);
        die("Cannot find article %s in history; you may want to use -n\n",
            mid);
    }

    if (!SMsetup(SM_RDWR, &flag) || !SMsetup(SM_PREOPEN, &flag)) {
        HISclose(History);
        die("Cannot set up storage manager; you may want to use -n\n");
    }

    if (!SMinit()) {
        HISclose(History);
        die("Cannot initialize storage manager: %s; you may want to use -n\n",
            SMerrorstr);
    }

    if ((art = SMretrieve(token, RETR_HEAD)) == NULL) {
        SMshutdown();
        HISclose(History);
        die("Cannot retrieve the article; you may want to use -n\n");
    }

    /* Extract the Newsgroups header field. */
    start = wire_findheader(art->data, art->len, "Newsgroups", true);

    if (start == NULL) {
        SMfreearticle(art);
        SMshutdown();
        HISclose(History);
        die("Cannot find any Newsgroups header field; "
            "you may want to use -n\n");
    }

    end = wire_endheader(start, art->data + art->len - 1);

    if (end == NULL) {
        SMfreearticle(art);
        SMshutdown();
        HISclose(History);
        die("Cannot retrieve the Newsgroups header field; "
            "you may want to use -n\n");
    }

    newsgroups = xstrndup(start, end - start);

    SMfreearticle(art);
    SMshutdown();
    HISclose(History);

    return newsgroups;
}


int
main(int ac, char *av[])
{
    int option;
    bool approved = true;
    char *body = NULL;
    char *charset = NULL;
    char datebuff[40];
    char *from = NULL;
    bool genmid = true;
    bool localtime = true;
    char *mid;
    char *newsgroups = NULL;
    char *originalmid;
    char *subject = NULL;
#if defined(HAVE_CANLOCK)
    bool cancelkeyonly = false;
    char *cankeybuff = NULL;
    bool gencankey = true;
#endif

    /* First thing, set up our identity. */
    message_program_name = "gencancel";

    /* Load inn.conf. */
    if (!innconf_read(NULL))
        exit(1);

    /* Read our secrets, if available.
     * Even if we fail reading them, we can just go on.  Don't bail out, the
     * related features will just not be activated. */
    secrets_read(NULL);

    /* Parse command-line arguments. */
    while ((option = getopt(ac, av, "ab:c:df:kLmn:s:")) != EOF) {
        switch (option) {
        case 'a':
            approved = false;
            break;
        case 'b':
            body = optarg;
            break;
        case 'c':
            charset = optarg;
            break;
        case 'd':
            localtime = false;
            break;
        case 'f':
            from = optarg;
            break;
        case 'k':
#if defined(HAVE_CANLOCK)
            cancelkeyonly = true;
            break;
#else
            die("INN was not built with Cancel-Lock support\n");
            /* NOTREACHED */
#endif
        case 'L':
#if defined(HAVE_CANLOCK)
            gencankey = false;
            break;
#else
            die("INN was not built with Cancel-Lock support\n");
            /* NOTREACHED */
#endif
        case 'm':
            genmid = false;
            break;
        case 'n':
            newsgroups = optarg;
            break;
        case 's':
            subject = optarg;
            break;
        default:
            die("Unexpected option\n\n%s", usage);
        }
    }
    ac -= optind;
    av += optind;

    /* One argument is expected: the Message-ID to cancel. */
    if (ac == 0) {
        die("No Message-ID provided\n\n%s", usage);
    } else if (ac == 1) {
        originalmid = av[0];
        if (!IsValidMessageID(originalmid, true, true))
            die("Invalid syntax for Message-ID %s\n\n%s", originalmid, usage);
    } else {
        die("Unexpected argument '%s'\n\n%s", av[1], usage);
    }

#if defined(HAVE_CANLOCK)
    /* Generate the admin Cancel-Key header field body. */
    if (!gen_cancel_key(NULL, originalmid, NULL, &cankeybuff)) {
        die("Cannot generate Cancel-Key header field body\n");
    }

    /* Only output the admin cancel key hashes, if desired.
     * This way, the user or an external program can easily make use of them
     * (for a supersede request for instance). */
    if (cancelkeyonly) {
        if (*cankeybuff != '\0') {
            printf("%s\n", cankeybuff);
            exit(0);
        } else {
            die("No canlockadmin secrets found in inn-secrets.conf\n");
        }
    }
#endif

    /* Try to find the newsgroup(s) the article has been posted to.
     * getNewsgroups() will die if it cannot retrieve the information. */
    if (newsgroups == NULL)
        newsgroups = getNewsgroups(originalmid);

    /* Write on standard output the cancel control message, header field
     * by header field. */
    if (from == NULL && innconf->complaints == NULL)
        die("complaints unset in inn.conf; you may want to use -f\n");
    printf("From: %s\n", from == NULL ? innconf->complaints : from);

    /* Taken from either -n or the original article. */
    printf("Newsgroups: %s\n", newsgroups);

    if (subject == NULL)
        printf("Subject: cmsg cancel %s\n", originalmid);
    else
        printf("Subject: %s\n", subject);

    printf("Control: cancel %s\n", originalmid);

    if (!makedate(-1, localtime, datebuff, sizeof(datebuff)))
        die("Can't generate Date header field body\n");
    printf("Date: %s\n", datebuff);

    if (approved) {
        printf("Approved: %s\n", from == NULL ? innconf->complaints : from);
    }

    if (genmid) {
        mid = GenerateMessageID(innconf->domain);
        if (mid == NULL)
            die("Cannot generate Message-ID\n");
        printf("Message-ID: %s\n", mid);
    }

    printf("MIME-Version: 1.0\n");
    printf("Content-Type: text/plain; charset=\"%s\"\n",
           charset == NULL ? "ISO-8859-1" : charset);
    printf("Content-Transfer-Encoding: 8bit\n");

#if defined(HAVE_CANLOCK)
    if (gencankey && *cankeybuff != '\0') {
        printf("Cancel-Key: %s\n", cankeybuff);
    }
#endif

    printf("\n%s\n", body == NULL ? "Admin cancel." : body);

    exit(0);
}

/*  $Id$
**
**  Provide a command line interface to the storage manager.
*/

#include "config.h"
#include "clibrary.h"
#include <sys/uio.h>

#include "inn/buffer.h"
#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/qio.h"
#include "inn/wire.h"
#include "inn/libinn.h"
#include "inn/storage.h"

static const char usage[] = "\
Usage: sm [-cdHiqrRSs] [token ...]\n\
\n\
Command-line interface to the INN storage manager.  The default action is\n\
to display the complete article associated with each token given.  If no\n\
tokens are specified on the command line, they're read from stdin, one per\n\
line.\n\
\n\
    -c          Show clear information about a token\n\
    -d, -r      Delete the articles associated with the given tokens\n\
    -H          Display the headers of articles only\n\
    -i          Translate tokens into newsgroup names and article numbers\n\
    -q          Suppress all error messages except usage\n\
    -R          Display the raw article rather than undoing wire format\n\
    -S          Output articles in rnews batch file format\n\
    -s          Store the article provided on stdin.\n";

/* The options that can be set on the command line, used to determine what to
   do with each token. */
struct options {
    bool artinfo;               /* Show newsgroup and article number. */
    bool clearinfo;             /* Show clear information about a token. */
    bool delete;                /* Delete articles instead of showing them. */
    bool header;                /* Display article headers only. */
    bool raw;                   /* Show the raw wire-format articles. */
    bool rnews;                 /* Output articles as rnews batch files. */
};


/*
**  Given a file descriptor, read a post from that file descriptor and then
**  store it.  This basically duplicates what INN does, except that INN has to
**  do more complex things to add the Path header.
**
**  Note that we make no attempt to add history or overview information, at
**  least right now.
*/
static bool
store_article(int fd)
{
    struct buffer *article;
    size_t size;
    char *text, *start, *end;
    ARTHANDLE handle;
    TOKEN token;

    /* Build the basic article handle. */
    article = buffer_new();
    if (!buffer_read_file(article, fd))
        sysdie("cannot read article");
    text = wire_from_native(article->data, article->left, &size);
    handle.type = TOKEN_EMPTY;
    handle.data = text;
    handle.iov = xmalloc(sizeof(struct iovec));
    handle.iov->iov_base = text;
    handle.iov->iov_len = size;
    handle.iovcnt = 1;
    handle.len = size;
    handle.arrived = 0;
    handle.expires = 0;
    buffer_free(article);

    /* Find the expiration time, if any. */
    start = wire_findheader(text, size, "Expires", true);
    if (start != NULL) {
        char *expires;

        end = wire_endheader(start, text + size - 1);
        if (end == NULL)
            die("cannot find end of Expires header");
        expires = xstrndup(start, end - start);
        handle.expires = parsedate_rfc5322_lax(expires);
        free(expires);
        if (handle.expires == (time_t) -1)
            handle.expires = 0;
    }

    /* Find the appropriate newsgroups header. */
    if (innconf->storeonxref) {
        start = wire_findheader(text, size, "Xref", true);
        if (start == NULL)
            die("no Xref header found in message");
        end = wire_endheader(start, text + size - 1);
        if (end == NULL)
            die("cannot find end of Xref header");
        for (; *start != ' ' && start < end; start++)
            ;
        if (start >= end)
            die("malformed Xref header");
        start++;
    } else {
        start = wire_findheader(text, size, "Newsgroups", true);
        if (start == NULL)
            die("no Newsgroups header found in message");
        end = wire_endheader(start, text + size - 1);
        if (end == NULL)
            die("cannot find end of Newsgroups header");
    }
    handle.groups = start;
    handle.groupslen = end - start;

    /* Store the article. */
    token = SMstore(handle);
    free(text);
    free(handle.iov);
    if (token.type == TOKEN_EMPTY) {
        warn("failed to store article: %s", SMerrorstr);
        return false;
    } else {
        printf("%s\n", TokenToText(token));
        return true;
    }
}


/*
**  Process a single token, performing the operations specified in the given
**  options struct.  Calls warn and die to display error messages; -q is
**  implemented by removing all the warn and die error handlers.
*/
static bool
process_token(const char *id, const struct options *options)
{
    TOKEN token;
    struct artngnum artinfo;
    ARTHANDLE *article;
    size_t length;
    char *text;

    if (!IsToken(id)) {
        warn("%s is not a storage token", id);
        return false;
    }
    token = TextToToken(id);

    if (options->artinfo) {
        if (!SMprobe(SMARTNGNUM, &token, &artinfo)) {
            warn("could not get article information for %s", id);
            return false;
        } else {
            printf("%s: %lu\n", artinfo.groupname, artinfo.artnum);
            free(artinfo.groupname);
        }
    } else if (options->clearinfo) {
        text = SMexplaintoken(token);
        printf("%s %s\n", id, text);
        free(text);
    } else if (options->delete) {
        if (!SMcancel(token)) {
            warn("could not remove %s: %s", id, SMerrorstr);
            return false;
        }
    } else {
        article = SMretrieve(token, options->header ? RETR_HEAD : RETR_ALL);
        if (article == NULL) {
            warn("could not retrieve %s", id);
            return false;
        }
        if (options->raw) {
            if (fwrite(article->data, article->len, 1, stdout) != 1)
                die("output failed");
        } else {
            text = wire_to_native(article->data, article->len, &length);
            if (options->rnews)
                printf("#! rnews %lu\n", (unsigned long) length);
            if (fwrite(text, length, 1, stdout) != 1)
                die("output failed");
            free(text);
        }
        SMfreearticle(article);
    }
    return true;
}


int
main(int argc, char *argv[])
{
    int option;
    bool okay, status;
    struct options options = { false, false, false, false, false, false };
    bool store = false;

    /* Suppress notice messages like tradspool rebuilding its map. */
    message_handlers_notice(0);

    message_program_name = "sm";

    if (!innconf_read(NULL))
        exit(1);

    while ((option = getopt(argc, argv, "cdHiqrRSs")) != EOF) {
        switch (option) {
        case 'c':
            options.clearinfo = true;
            break;
        case 'd':
        case 'r':
            options.delete = true;
            break;
        case 'H':
            options.header = true;
            break;
        case 'i':
            options.artinfo = true;
            break;
        case 'q':
            message_handlers_warn(0);
            message_handlers_die(0);
            break;
        case 'R':
            options.raw = true;
            break;
        case 'S':
            options.rnews = true;
            break;
        case 's':
            store = true;
            break;
        default:
            fprintf(stderr, usage);
            exit(1);
        }
    }

    /* Check options for consistency. */
    if (options.artinfo && options.delete)
        die("-i cannot be used with -r or -d");
    if (options.artinfo && (options.header || options.raw || options.rnews))
        die("-i cannot be used with -H, -R, or -S");
    if (options.delete && (options.header || options.rnews))
        die("-r or -d cannot be used with -H or -S");
    if (options.raw && options.rnews)
        die("-R cannot be used with -S");
    if (options.header && options.rnews)
        die("-H cannot be used with -S");
    if (store && (options.artinfo || options.delete || options.header))
        die("-s cannot be used with -i, -r, -d, -H, -R, or -S");
    if (store && (options.raw || options.rnews))
        die("-s cannot be used with -i, -r, -d, -H, -R, or -S");
    if (options.clearinfo && (options.artinfo || options.delete || options.header
                              || options.raw || options.rnews || store))
        die("-c cannot be used with -i, -r, -d, -H, -R, -S, or -s");

    /* Initialize the storage manager.  If we're doing article deletions, we
       need to open it read/write. */
    if (store || options.delete) {
        bool value = true;

        if (!SMsetup(SM_RDWR, &value))
            die("cannot set up storage manager");
    }
    if (!SMinit())
        die("cannot initialize storage manager: %s", SMerrorstr);

    /* If we're storing an article, do that and then exit. */
    if (store) {
        status = store_article(fileno(stdin));
        exit(status ? 0 : 1);
    }

    /* Process tokens.  If no arguments were given on the command line,
       process tokens from stdin.  Otherwise, walk through the remaining
       command line arguments. */
    okay = true;
    if (optind == argc) {
        QIOSTATE *qp;
        char *line;

        qp = QIOfdopen(fileno(stdin));
        for (line = QIOread(qp); line != NULL; line = QIOread(qp)) {
            status = process_token(line, &options);
            okay = okay && status;
        }
        if (QIOerror(qp)) {
            if (QIOtoolong(qp))
                die("input line too long");
            sysdie("error reading stdin");
        }
        QIOclose(qp);
    } else {
        int i;

        for (i = optind; i < argc; i++) {
            status = process_token(argv[i], &options);
            okay = okay && status;
        }
    }

    SMshutdown();
    exit(okay ? 0 : 1);
}

/*  $Id$
**
**  Provide a command line interface to the storage manager
*/

#include "config.h"
#include "clibrary.h"

#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/qio.h"
#include "storage.h"

static const char usage[] = "\
Usage: sm [-dHiqrRS] [token ...]\n\
\n\
Command-line interface to the INN storage manager.  The default action is\n\
to display the complete article associated with each token given.  If no\n\
tokens are specified on the command line, they're read from stdin, one per\n\
line.\n\
\n\
    -d, -r      Delete the articles associated with the given tokens\n\
    -H          Display the headers of articles only\n\
    -i          Translate tokens into newsgroup names and article numbers\n\
    -q          Suppress all error messages except usage\n\
    -R          Display the raw article rather than undoing wire format\n\
    -S          Output articles in rnews batch file format\n";

/* The options that can be set on the command line, used to determine what to
   do with each token. */
struct options {
    bool artinfo;               /* Show newsgroup and article number. */
    bool delete;                /* Delete articles instead of showing them. */
    bool header;                /* Display article headers only. */
    bool raw;                   /* Show the raw wire-format articles. */
    bool rnews;                 /* Output articles as rnews batch files. */
};


/*
**  Process a single token, performing the operations specified in the given
**  options struct.  Calls warn and die to display error messages; -q is
**  implemented by removing all the warn and die error handlers.
*/
static void
process_token(const char *id, const struct options *options)
{
    TOKEN token;
    struct artngnum artinfo;
    ARTHANDLE *article;
    size_t length;
    char *text;

    if (!IsToken(id)) {
        warn("%s is not a storage token", id);
        return;
    }
    token = TextToToken(id);

    if (options->artinfo) {
        if (!SMprobe(SMARTNGNUM, &token, &artinfo)) {
            warn("could not get article information for %s", id);
        } else {
            printf("%s: %lu\n", artinfo.groupname, artinfo.artnum);
            free(artinfo.groupname);
        }
    } else if (options->delete) {
        if (!SMcancel(token))
            warn("could not remove %s: %s", id, SMerrorstr);
    } else {
        article = SMretrieve(token, options->header ? RETR_HEAD : RETR_ALL);
        if (article == NULL) {
            warn("could not retrieve %s", id);
            return;
        }
        if (options->raw) {
            if (fwrite(article->data, article->len, 1, stdout) != 1)
                die("output failed");
        } else {
            text = FromWireFmt(article->data, article->len, &length);
            if (options->rnews)
                printf("#! rnews %lu\n", (unsigned long) length);
            if (fwrite(text, length, 1, stdout) != 1)
                die("output failed");
            free(text);
        }
        SMfreearticle(article);
    }
}


int
main(int argc, char *argv[])
{
    int option;
    struct options options = { false, false, false, false, false };

    message_program_name = "sm";

    if (!innconf_read(NULL))
        exit(1);

    while ((option = getopt(argc, argv, "iqrdR")) != EOF) {
        switch (option) {
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
        default:
            fprintf(stderr, usage);
            exit(1);
        }
    }

    /* Check options for consistency. */
    if (options.delete && (options.header || options.rnews))
        die("-r or -d cannot be used with -H or -S");
    if (options.raw && options.rnews)
        die("-R cannot be used with -S");
    if (options.header && options.rnews)
        die("-H cannot be used with -S");

    /* Initialize the storage manager.  If we're doing article deletions, we
       need to open it read/write. */
    if (options.delete) {
        bool value = true;

        if (!SMsetup(SM_RDWR, &value))
            die("cannot set up storage manager");
    }
    if (!SMinit())
        die("cannot initialize storage manager: %s", SMerrorstr);

    /* Process tokens.  If no arguments were given on the command line,
       process tokens from stdin.  Otherwise, walk through the remaining
       command line arguments. */
    if (optind == argc) {
        QIOSTATE *qp;
        char *line;

        qp = QIOfdopen(fileno(stdin));
        for (line = QIOread(qp); line != NULL; line = QIOread(qp))
            process_token(line, &options);
        if (QIOerror(qp)) {
            if (QIOtoolong(qp))
                die("input line too long");
            sysdie("error reading stdin");
        }
        QIOclose(qp);
    } else {
        int i;

        for (i = optind; i < argc; i++)
            process_token(argv[i], &options);
    }

    SMshutdown();
    exit(0);
}

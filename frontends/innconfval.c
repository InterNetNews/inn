/*  $Id$
**
**  Get a config value from INN.
*/

#include "config.h"
#include "clibrary.h"

#include "inn/innconf.h"
#include "inn/messages.h"
#include "libinn.h"

/*
**  Print the INN version string with appropriate quoting.
*/
static void
print_version(FILE *file, enum innconf_quoting quoting)
{
    switch (quoting) {
    case INNCONF_QUOTE_NONE:
        fprintf(file, "%s\n", inn_version_string);
        break;
    case INNCONF_QUOTE_SHELL:
        fprintf(file, "VERSION='%s'; export VERSION\n", inn_version_string);
        break;
    case INNCONF_QUOTE_PERL:
        fprintf(file, "$version = '%s';\n", inn_version_string);
        break;
    case INNCONF_QUOTE_TCL:
        fprintf(file, "set inn_version \"%s\"\n", inn_version_string);
        break;
    }
}


/*
**  Main routine.  Most of the real work is done by the innconf library
**  routines.
*/
int
main(int argc, char *argv[])
{
    int option, i;
    char *file = NULL;
    enum innconf_quoting quoting = INNCONF_QUOTE_NONE;
    bool okay = true;
    bool version = false;
    bool checking = false;

    message_program_name = "innconfval";

    while ((option = getopt(argc, argv, "ci:pstv")) != EOF)
        switch (option) {
        default:
            die("usage error");
            break;
        case 'c':
            checking = true;
            break;
        case 'i':
            file = optarg;
            break;
        case 'p':
            quoting = INNCONF_QUOTE_PERL;
            break;
        case 's':
            quoting = INNCONF_QUOTE_SHELL;
            break;
        case 't':
            quoting = INNCONF_QUOTE_TCL;
            break;
        case 'v':
            version = true;
            break;
        }
    argc -= optind;
    argv += optind;

    if (version) {
        print_version(stdout, quoting);
        exit(0);
    }
    if (checking)
        exit(innconf_check(file) ? 0 : 1);

    /* Read in the inn.conf file specified. */
    if (!innconf_read(file))
        exit(1);

    /* Perform the specified action. */
    if (argv[0] == NULL) {
        innconf_dump(stdout, quoting);
        print_version(stdout, quoting);
    } else {
        for (i = 0; i < argc; i++)
            if (strcmp(argv[i], "version") == 0)
                print_version(stdout, quoting);
            else if (!innconf_print_value(stdout, argv[i], quoting))
                okay = false;
    }
    exit(okay ? 0 : 1);
}

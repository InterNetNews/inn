/*
**  Get a config value from INN.
*/

#include "portable/system.h"

#include "inn/confparse.h"
#include "inn/innconf.h"
#include "inn/libinn.h"
#include "inn/messages.h"
#include "inn/secrets.h"
#include "inn/version.h"

/*
**  The list of recognized configuration files.
*/
enum configfile {
    INN_CONF,
    INN_SECRETS_CONF
};

/*
**  Print the INN version string with appropriate quoting.
*/
static void
print_version(FILE *file, enum confparse_quoting quoting)
{
    switch (quoting) {
    case CONFPARSE_QUOTE_NONE:
        fprintf(file, "%s\n", INN_VERSION_STRING);
        break;
    case CONFPARSE_QUOTE_SHELL:
        fprintf(file, "VERSION='%s'; export VERSION\n", INN_VERSION_STRING);
        break;
    case CONFPARSE_QUOTE_PERL:
        fprintf(file, "$version = '%s';\n", INN_VERSION_STRING);
        break;
    case CONFPARSE_QUOTE_TCL:
        fprintf(file, "set inn_version \"%s\"\n", INN_VERSION_STRING);
        break;
    }
}


/*
**  Main routine.  Most of the real work is done by the parseconf library
**  routines.
*/
int
main(int argc, char *argv[])
{
    int option, i;
    char *innconffile = NULL;
    char *configfile = NULL;
    enum confparse_quoting quoting = CONFPARSE_QUOTE_NONE;
    bool okay = true;
    bool version = false;
    bool checking = false;
    enum configfile requested_file = INN_CONF;

    message_program_name = "innconfval";

    while ((option = getopt(argc, argv, "Cf:F:i:pstv")) != EOF)
        switch (option) {
        default:
            die("Usage error");
            /* NOTREACHED */
        case 'C':
            checking = true;
            break;
        case 'f':
            configfile = optarg;
            break;
        case 'F':
            if (strcasecmp(optarg, "inn.conf") == 0)
                requested_file = INN_CONF;
            else if (strcasecmp(optarg, "inn-secrets.conf") == 0)
                requested_file = INN_SECRETS_CONF;
            else
                die("Unknown configuration file");
            break;
        case 'i':
            innconffile = optarg;
            break;
        case 'p':
            quoting = CONFPARSE_QUOTE_PERL;
            break;
        case 's':
            quoting = CONFPARSE_QUOTE_SHELL;
            break;
        case 't':
            quoting = CONFPARSE_QUOTE_TCL;
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

    /* Read in the inn.conf file specified.
     * It will notably set pathetc (used to find other configuration files). */
    if (!innconf_read(innconffile))
        exit(1);

    if (requested_file == INN_CONF && configfile == NULL)
        configfile = innconffile;

    if (requested_file == INN_SECRETS_CONF) {
        if (!secrets_read(configfile))
            exit(1);
    }

    if (checking) {
        bool status;
        switch (requested_file) {
        case INN_CONF:
            status = innconf_check(configfile);
            break;
        default:
            die("No check implemented for that configuration file");
        }
        exit(status ? 0 : 1);
    }

    /* Perform the specified action. */
    if (argv[0] == NULL) {
        switch (requested_file) {
        case INN_CONF:
            innconf_dump(stdout, quoting);
            print_version(stdout, quoting);
            break;
        case INN_SECRETS_CONF:
            secrets_dump(stdout, quoting);
            break;
        default:
            die("No dump implemented for that configuration file");
        }
    } else {
        for (i = 0; i < argc; i++)
            if (strcmp(argv[i], "version") == 0)
                print_version(stdout, quoting);
            else {
                switch (requested_file) {
                case INN_CONF:
                    if (!innconf_print_value(stdout, argv[i], quoting))
                        okay = false;
                    break;
                case INN_SECRETS_CONF:
                    if (!secrets_print_value(stdout, argv[i], quoting))
                        okay = false;
                    break;
                default:
                    die("No print implemented for that configuration file");
                }
            }
    }
    exit(okay ? 0 : 1);
}

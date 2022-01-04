/*
**  Manage the global secrets struct.
**
**  The functions in this file collapse the parse tree for inn-secrets.conf
**  into the secrets struct that's used throughout INN.
**
**  When adding new inn-secrets.conf parameters, make sure to add them in all
**  of the following places:
**
**   * The table in this file
**   * include/inn/secrets.h
**   * doc/pod/inn-secrets.conf.pod (and regenerate doc/man/inn-secrets.conf.5)
**   * Add the default value to samples/inn-secrets.conf
*/

#include "portable/system.h"

#include <ctype.h>

#include "inn/confparse.h"
#include "inn/innconf.h"
#include "inn/libinn.h"
#include "inn/messages.h"
#include "inn/paths.h"
#include "inn/secrets.h"
#include "inn/vector.h"

/* Instantiation of the global secrets variable. */
struct secrets *secrets = NULL;

#define K(name) (#name), offsetof(struct secrets, name)

/* clang-format off */
static const struct config config_table[] = {
    { K(canlockadmin),         LIST   (NULL) },
    { K(canlockuser),          LIST   (NULL) },
};
/* clang-format on */


/*
**  Given a config_group struct representing the inn-secrets.conf file, parse
**  that into an secrets struct and return the newly allocated struct.
*/
static struct secrets *
secrets_parse(struct config_group *group)
{
    unsigned int i, j;
    const char *char_ptr;
    char **string;
    const struct vector *vector_ptr;
    struct vector **list;
    struct secrets *config;

    config = xmalloc(sizeof(struct secrets));
    memset(config, 0, sizeof(struct secrets));
    for (i = 0; i < ARRAY_SIZE(config_table); i++)
        switch (config_table[i].type) {
        case TYPE_STRING:
            if (!config_param_string(group, config_table[i].name, &char_ptr))
                char_ptr = config_table[i].defaults.string;
            string = CONF_STRING(config, config_table[i].location);
            *string = (char_ptr == NULL) ? NULL : xstrdup(char_ptr);
            break;
        case TYPE_LIST:
            /* vector_ptr contains the value taken from inn-secrets.conf or the
             * default value from config_table; *list points to the
             * inn-secrets.conf structure in memory for this parameter.  We
             * have to do a deep copy of vector_ptr because, like char_ptr,
             * it is freed by config_free() called by other parts of INN. */
            if (!config_param_list(group, config_table[i].name, &vector_ptr))
                vector_ptr = config_table[i].defaults.list;
            list = CONF_LIST(config, config_table[i].location);
            *list = vector_new();
            if (vector_ptr != NULL && vector_ptr->strings != NULL) {
                vector_resize(*list, vector_ptr->count);
                for (j = 0; j < vector_ptr->count; j++) {
                    if (vector_ptr->strings[j] != NULL) {
                        vector_add(*list, vector_ptr->strings[j]);
                    }
                }
            }
            break;
        default:
            die("internal error: invalid type in row %u of config table", i);
        }
    return config;
}


/*
**  Read in inn-secrets.conf.  Takes a single argument, which is either NULL to
**  read the default configuration file or a path to an alternate configuration
**  file to read.  Returns true if the file was read successfully and false
**  otherwise.
*/
bool
secrets_read(const char *path)
{
    struct config_group *group;
    struct config_group *subgroup = NULL;
    char *configfile = NULL;
    bool parsed = false;

    if (secrets != NULL)
        secrets_free(secrets);
    configfile = concatpath(innconf->pathetc, INN_PATH_SECRETS);
    group = config_parse_file(path == NULL ? configfile : path);
    free(configfile);

    if (group != NULL) {
        parsed = true;

        /* We currently only have one type ("cancels").
         * So, just look inside it.  We'll improve that when other tags are
         * added.  A possibility would be to create get_secret_list() and
         * get_secret_string() functions, and call them in our code like:
         *   get_secret_list("cancels", "canlockadmin");
         * We may keep the secrets struct for secrets defined only once in the
         * inn-secrets.conf file, and use get_secret_x() functions for secrets
         * appearing several times at different scopes in the file.  (These
         * functions would take a variable number of arguments, all strings,
         * that represent the hierarchical path down to the key that we want.)
         */
        subgroup = config_find_group(group, "cancels");
    }

    /* Unconditionally parse subgroup, even if NULL, in order to initialize
     * vectors. */
    secrets = secrets_parse(subgroup);

    /* Free allocated memory (subgroup will be freed at the same time). */
    if (group != NULL)
        config_free(group);

    return parsed;
}


/*
**  Free secrets, requiring some complexity since all strings stored in the
**  secrets struct are allocated memory.  This routine is mostly generic to
**  any struct smashed down from a configuration file parse, though here we
**  explicitly erase the secrets from memory.
*/
void
secrets_free(struct secrets *config)
{
    unsigned int i;
    char *p;
    struct vector *q;

    for (i = 0; i < ARRAY_SIZE(config_table); i++) {
        if (config_table[i].type == TYPE_STRING) {
            p = *CONF_STRING(config, config_table[i].location);
            if (p != NULL) {
                explicit_bzero(p, strlen(p));
                free(p);
            }
        }
        if (config_table[i].type == TYPE_LIST) {
            q = *CONF_LIST(config, config_table[i].location);
            if (q != NULL) {
                size_t j;

                for (j = 0; j < q->count; j++) {
                    explicit_bzero(q->strings[j], strlen(q->strings[j]));
                    free(q->strings[j]);
                }
                free(q->strings);
                free(q);
            }
        }
    }
    free(config);
}

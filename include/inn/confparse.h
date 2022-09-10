/*
**  Configuration file parsing interface.
*/

#ifndef INN_CONFPARSE_H
#define INN_CONFPARSE_H 1

#include "inn/macros.h"
#include "inn/portable-stdbool.h"

/* Avoid including <inn/vector.h> unless the client needs it. */
struct vector;

/* The opaque data type representing a configuration tree. */
struct config_group;

/* Data types used to express the mappings from the configuration parse into
   the resulted configuration structs. */
enum type {
    TYPE_BOOLEAN,
    TYPE_NUMBER,
    TYPE_UNUMBER,
    TYPE_STRING,
    TYPE_LIST
};

/* Used to request various types of quoting when printing out values.
   Changes to that enum should be reflected into innconf.h and secrets.h (to
   avoid a forward declaration of an enum type in these files). */
enum confparse_quoting {
    CONFPARSE_QUOTE_NONE,
    CONFPARSE_QUOTE_SHELL,
    CONFPARSE_QUOTE_PERL,
    CONFPARSE_QUOTE_TCL
};
#define INN_CONFPARSE_QUOTING 1

struct config {
    const char *name;
    size_t location;
    enum type type;
    struct {
        bool boolean;
        long signed_number;
        unsigned long unsigned_number;
        const char *string;
        const struct vector *list;
    } defaults;
};

/* The following macros are helpers to make it easier to define the tables
   that specify how to convert the configuration file into a struct. */
/* clang-format off */
#define BOOL(def)       TYPE_BOOLEAN,   { (def),     0,     0,  NULL,  NULL }
#define NUMBER(def)     TYPE_NUMBER,    {     0, (def),     0,  NULL,  NULL }
#define UNUMBER(def)    TYPE_UNUMBER,   {     0,     0, (def),  NULL,  NULL }
#define STRING(def)     TYPE_STRING,    {     0,     0,     0, (def),  NULL }
#define LIST(def)       TYPE_LIST,      {     0,     0,     0,  NULL, (def) }

/* Accessor macros to get a pointer to a value inside a struct. */
#define CONF_BOOL(conf, offset) \
    (bool *)           (void *) ((char *) (conf) + (offset))
#define CONF_NUMBER(conf, offset) \
    (long *)           (void *) ((char *) (conf) + (offset))
#define CONF_UNUMBER(conf, offset) \
    (unsigned long *)  (void *) ((char *) (conf) + (offset))
#define CONF_STRING(conf, offset) \
    (char **)          (void *) ((char *) (conf) + (offset))
#define CONF_LIST(conf, offset) \
    (struct vector **) (void *) ((char *) (conf) + (offset))
/* clang-format on */

BEGIN_DECLS

/* Parse the given file and build a configuration tree.  This does purely
   syntactic parsing; no semantic checking is done.  After the file name, a
   NULL-terminated list of const char * pointers should be given, naming the
   top-level group types that the caller is interested in.  If none are given
   (if the second argument is NULL), the entire file is parsed.  (This is
   purely for efficiency reasons; if one doesn't care about speed, everything
   will work the same if no types are given.)

   Returns a config_group for the top-level group representing the entire
   file.  Generally one never wants to query parameters in this group;
   instead, the client should then call config_find_group for the group type
   of interest.  Returns NULL on failure to read the file or on a parse
   failure; errors are reported via warn. */
struct config_group *config_parse_file(const char *filename, /* types */...);

/* config_find_group returns the first group of the given type found in the
   tree rooted at its argument.  config_next_group returns the next group in
   the tree of the same type as the given group (or NULL if none is found).
   This can be used to do such things as enumerate all "peer" groups in a
   configuration file. */
struct config_group *config_find_group(struct config_group *,
                                       const char *type);
struct config_group *config_next_group(struct config_group *);

/* Accessor functions for group information. */
const char *config_group_type(struct config_group *);
const char *config_group_tag(struct config_group *);

/* Look up a parameter in a given config tree.  The second argument is the
   name of the parameter, and the result will be stored in the third argument
   if the function returns true.  If it returns false, the third argument is
   unchanged and that parameter wasn't set (or was set to an invalid value for
   the expected type). */
bool config_param_boolean(struct config_group *, const char *, bool *);
bool config_param_signed_number(struct config_group *, const char *, long *);
bool config_param_unsigned_number(struct config_group *, const char *,
                                  unsigned long *);
bool config_param_real(struct config_group *, const char *, double *);
bool config_param_string(struct config_group *, const char *, const char **);
bool config_param_list(struct config_group *, const char *,
                       const struct vector **);

/* Used for checking a configuration file, returns a vector of all parameters
   set for the given config_group, including inherited ones. */
struct vector *config_params(struct config_group *);

/* Used for reporting semantic errors, config_error_param reports the given
   error at a particular parameter in a config_group and config_error_group
   reports an error at the definition of that group.  The error is reported
   using warn. */
void config_error_group(struct config_group *, const char *format, ...)
    __attribute__((__format__(printf, 2, 3)));
void config_error_param(struct config_group *, const char *key,
                        const char *format, ...)
    __attribute__((__format__(printf, 3, 4)));

/* Free all space allocated by the tree rooted at config_group.  One normally
   never wants to do this.  WARNING: This includes the storage allocated for
   all strings returned by config_param_string and config_param_list for any
   configuration groups in this tree. */
void config_free(struct config_group *);

/* Used to print configuration values to a file or stdout. */
void print_boolean(FILE *file, const char *key, bool value,
                   enum confparse_quoting quoting);
void print_signed_number(FILE *file, const char *key, long value,
                         enum confparse_quoting quoting);
void print_unsigned_number(FILE *file, const char *key, unsigned long value,
                           enum confparse_quoting quoting);
void print_string(FILE *file, const char *key, const char *value,
                  enum confparse_quoting quoting);
void print_list(FILE *file, const char *key, const struct vector *value,
                enum confparse_quoting quoting);

END_DECLS

#endif /* INN_CONFPARSE_H */

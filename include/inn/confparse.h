/*  $Id$
**
**  Configuration file parsing interface.
*/

#ifndef INN_CONFPARSE_H
#define INN_CONFPARSE_H 1

#include <inn/defines.h>

/* Avoid including <inn/vector.h> unless the client needs it. */
struct vector;

/* The opaque data type representing a configuration tree. */
struct config_group;

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
struct config_group *config_parse_file(const char *filename, /* types */ ...);

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
bool config_param_unsigned_number(struct config_group *, const char *, unsigned long *);
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
void config_error_group(struct config_group *, const char *format, ...);
void config_error_param(struct config_group *, const char *key,
                        const char *format, ...);

/* Free all space allocated by the tree rooted at config_group.  One normally
   never wants to do this.  WARNING: This includes the storage allocated for
   all strings returned by config_param_string and config_param_list for any
   configuration groups in this tree. */
void config_free(struct config_group *);

END_DECLS

#endif /* INN_CONFPARSE_H */

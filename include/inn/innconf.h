/*  $Id$
**
**  inn.conf parser interface.
**
**  The interface to reading inn.conf configuration files and managing the
**  resulting innconf struct.
*/

#ifndef INN_INNCONF_H
#define INN_INNCONF_H 1

#include <inn/defines.h>
#include <stdio.h>

/* This is only for transition to prototype innconf_compare. */
struct conf_vars;

/* Used to request various types of quoting when printing out values. */
enum innconf_quoting {
    INNCONF_QUOTE_NONE,
    INNCONF_QUOTE_SHELL,
    INNCONF_QUOTE_PERL,
    INNCONF_QUOTE_TCL
};

BEGIN_DECLS

/* Parse the given file into innconf, using the default path if NULL. */
bool innconf_read(const char *path);

/* Free innconf and all allocated memory for it. */
void innconf_free(void);

/* Print a single value with appropriate quoting, return whether found. */
bool innconf_print_value(FILE *, const char *key, enum innconf_quoting);

/* Dump the entire configuration with appropriate quoting. */
void innconf_dump(FILE *, enum innconf_quoting);

/* Compare two instances of an innconf struct, for testing. */
bool innconf_compare(struct conf_vars *, struct conf_vars *);

END_DECLS

#endif /* INN_INNCONF_H */

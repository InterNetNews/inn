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

/* This is only for transition to prototype innconf_compare. */
struct conf_vars;

BEGIN_DECLS

bool innconf_read(const char *path);
void innconf_free(void);
bool innconf_compare(struct conf_vars *, struct conf_vars *);

END_DECLS

#endif /* INN_INNCONF_H */

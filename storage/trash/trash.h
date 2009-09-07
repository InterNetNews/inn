/*  $Id$
**
**  Storage manager module header for trash method.
*/

#ifndef __TRASH_H__
#define __TRASH_H__

#include "config.h"
#include "interface.h"

bool trash_init(SMATTRIBUTE *attr);
TOKEN trash_store(const ARTHANDLE article, const STORAGECLASS class);
ARTHANDLE *trash_retrieve(const TOKEN token, const RETRTYPE amount);
ARTHANDLE *trash_next(ARTHANDLE *article, const RETRTYPE amount);
void trash_freearticle(ARTHANDLE *article);
bool trash_cancel(TOKEN token);
bool trash_ctl(PROBETYPE type, TOKEN *token, void *value);
bool trash_flushcacheddata(FLUSHTYPE type);
void trash_printfiles(FILE *file, TOKEN token, char **xref, int ngroups);
void trash_shutdown(void);

#endif

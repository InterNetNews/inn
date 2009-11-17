/*  $Id$
**
**  Storage manager module header for traditional spool format.
*/

#ifndef __TRADSPOOL_H__
#define __TRADSPOOL_H__

#include "config.h"
#include "interface.h"

bool tradspool_init(SMATTRIBUTE *attr);
TOKEN tradspool_store(const ARTHANDLE article, const STORAGECLASS class);
ARTHANDLE *tradspool_retrieve(const TOKEN token, const RETRTYPE amount);
ARTHANDLE *tradspool_next(ARTHANDLE *article, const RETRTYPE amount);
void tradspool_freearticle(ARTHANDLE *article);
bool tradspool_cancel(TOKEN token);
bool tradspool_ctl(PROBETYPE type, TOKEN *token, void *value);
bool tradspool_flushcacheddata(FLUSHTYPE type);
void tradspool_printfiles(FILE *file, TOKEN token, char **xref, int ngroups);
char *tradspool_explaintoken(const TOKEN token);
void tradspool_shutdown(void);

#endif

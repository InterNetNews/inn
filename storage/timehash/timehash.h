/*  $Id$
**
**  Storage manager module header for timehash method.
*/

#ifndef __TIMEHASH_H__
#define __TIMEHASH_H__

#include "config.h"
#include "interface.h"

bool timehash_init(SMATTRIBUTE *attr);
TOKEN timehash_store(const ARTHANDLE article, const STORAGECLASS class);
ARTHANDLE *timehash_retrieve(const TOKEN token, const RETRTYPE amount);
ARTHANDLE *timehash_next(ARTHANDLE *article, const RETRTYPE amount);
void timehash_freearticle(ARTHANDLE *article);
bool timehash_cancel(TOKEN token);
bool timehash_ctl(PROBETYPE type, TOKEN *token, void *value);
bool timehash_flushcacheddata(FLUSHTYPE type);
void timehash_printfiles(FILE *file, TOKEN token, char **xref, int ngroups);
char *timehash_explaintoken(const TOKEN token);
void timehash_shutdown(void);

#endif

/*  $Id$
**
**  timehash based storing method header
*/

#ifndef __TIMEHASH_H__
#define __TIMEHASH_H__

#include <configdata.h>
#include <interface.h>

BOOL timehash_init(BOOL *selfexpire);
TOKEN timehash_store(const ARTHANDLE article, const STORAGECLASS class);
ARTHANDLE *timehash_retrieve(const TOKEN token, const RETRTYPE amount);
ARTHANDLE *timehash_next(const ARTHANDLE *article, const RETRTYPE amount);
void timehash_freearticle(ARTHANDLE *article);
BOOL timehash_cancel(TOKEN token);
BOOL timehash_ctl(PROBETYPE type, TOKEN *token, void *value);
void timehash_shutdown(void);

#endif

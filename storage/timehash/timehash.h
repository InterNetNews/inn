#ifndef __TIMEHASH_H__
#define __TIMEHASH_H__

#include <configdata.h>
#include <interface.h>

BOOL timehash_init(void);
TOKEN timehash_store(const ARTHANDLE article);
ARTHANDLE *timehash_retrieve(const TOKEN token, RETRTYPE amount);
ARTHANDLE *timehash_next(const ARTHANDLE *article, RETRTYPE amount);
void timehash_freearticle(ARTHANDLE *article);
BOOL timehash_cancel(TOKEN token);
void timehash_shutdown(void);

#endif

#ifndef __TIMEHASH_H__
#define __TIMEHASH_H__

#include <configdata.h>
#include <interface.h>

void *timehash_init(void);
TOKEN timehash_store(void *handle, const ARTHANDLE article, const STORAGECLASS class);
ARTHANDLE *timehash_retrieve(void *handle, const TOKEN token, RETRTYPE amount);
ARTHANDLE *timehash_next(void *handle, const ARTHANDLE *article, RETRTYPE amount);
void timehash_freearticle(void *handle, ARTHANDLE *article);
BOOL timehash_cancel(void *handle, TOKEN token);
void timehash_shutdown(void *handle);

#endif

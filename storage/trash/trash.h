#ifndef __TRASH_H__
#define __TRASH_H__

#include <configdata.h>
#include <interface.h>

BOOL trash_init(void);
TOKEN trash_store(const ARTHANDLE article, const STORAGECLASS class);
ARTHANDLE *trash_retrieve(const TOKEN token, RETRTYPE amount);
ARTHANDLE *trash_next(const ARTHANDLE *article, RETRTYPE amount);
void trash_freearticle(ARTHANDLE *article);
BOOL trash_cancel(TOKEN token);
void trash_shutdown(void);

#endif

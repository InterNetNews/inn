/*  $Id$
**
**  trashing articles method header
*/

#ifndef __TRASH_H__
#define __TRASH_H__

#include "config.h"
#include "interface.h"

bool trash_init(SMATTRIBUTE *attr);
TOKEN trash_store(const ARTHANDLE article, const STORAGECLASS class);
ARTHANDLE *trash_retrieve(const TOKEN token, const RETRTYPE amount);
ARTHANDLE *trash_next(const ARTHANDLE *article, const RETRTYPE amount);
void trash_freearticle(ARTHANDLE *article);
bool trash_cancel(TOKEN token);
bool trash_ctl(PROBETYPE type, TOKEN *token, void *value);
bool trash_flushcacheddata(FLUSHTYPE type);
void trash_shutdown(void);

#endif

/*
** $Id$
** tradspool -- storage manager for traditional spool format.
*/

#ifndef __TRADSPOOL_H__
#define __TRADSPOOL_H__

#include "config.h"
#include "interface.h"

BOOL tradspool_init(BOOL *selfexpire);
TOKEN tradspool_store(const ARTHANDLE article, const STORAGECLASS class);
ARTHANDLE *tradspool_retrieve(const TOKEN token, const RETRTYPE amount);
ARTHANDLE *tradspool_next(const ARTHANDLE *article, const RETRTYPE amount);
void tradspool_freearticle(ARTHANDLE *article);
BOOL tradspool_cancel(TOKEN token);
BOOL tradspool_ctl(PROBETYPE type, TOKEN *token, void *value);
BOOL tradspool_flushcacheddata(FLUSHTYPE type);
void tradspool_shutdown(void);

#endif

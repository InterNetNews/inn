/*
** $Id$
** timecaf -- like the timehash storage method (and heavily inspired
** by it), but uses the CAF library to store multiple articles in a
** single file. 
*/

#ifndef __TIMECAF_H__
#define __TIMECAF_H__

#include <configdata.h>
#include <interface.h>

BOOL timecaf_init(BOOL *selfexpire);
TOKEN timecaf_store(const ARTHANDLE article, const STORAGECLASS class);
ARTHANDLE *timecaf_retrieve(const TOKEN token, const RETRTYPE amount);
ARTHANDLE *timecaf_next(const ARTHANDLE *article, const RETRTYPE amount);
void timecaf_freearticle(ARTHANDLE *article);
BOOL timecaf_cancel(TOKEN token);
BOOL timecaf_ctl(PROBETYPE type, TOKEN *token, void *value);
void timecaf_shutdown(void);

#endif

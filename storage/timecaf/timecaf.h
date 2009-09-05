/*  $Id$
**
**  timecaf -- like the timehash storage method (and heavily inspired
**  by it), but uses the CAF library to store multiple articles in a
**  single file.
*/

#ifndef __TIMECAF_H__
#define __TIMECAF_H__

#include "config.h"
#include "interface.h"

bool timecaf_init(SMATTRIBUTE *attr);
TOKEN timecaf_store(const ARTHANDLE article, const STORAGECLASS class);
ARTHANDLE *timecaf_retrieve(const TOKEN token, const RETRTYPE amount);
ARTHANDLE *timecaf_next(ARTHANDLE *article, const RETRTYPE amount);
void timecaf_freearticle(ARTHANDLE *article);
bool timecaf_cancel(TOKEN token);
bool timecaf_ctl(PROBETYPE type, TOKEN *token, void *value);
bool timecaf_flushcacheddata(FLUSHTYPE type);
void timecaf_printfiles(FILE *file, TOKEN token, char **xref, int ngroups);
void timecaf_shutdown(void);

#endif

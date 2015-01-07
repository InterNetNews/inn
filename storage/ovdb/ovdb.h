/*  $Id$
*/

#ifndef _OVDB_H_
#define _OVDB_H_

#include "config.h"

BEGIN_DECLS

bool ovdb_open(int mode);
bool ovdb_groupstats(const char *group, int *lo, int *hi, int *count,
                     int *flag);
bool ovdb_groupadd(const char *group, ARTNUM lo, ARTNUM hi, char *flag);
bool ovdb_groupdel(const char *group);
bool ovdb_add(const char *group, ARTNUM artnum, TOKEN token, char *data,
              int len, time_t arrived, time_t expires);
bool ovdb_cancel(const char *group, ARTNUM artnum);
void *ovdb_opensearch(const char *group, int low, int high);
bool ovdb_search(void *handle, ARTNUM *artnum, char **data, int *len,
                 TOKEN *token, time_t *arrived);
void ovdb_closesearch(void *handle);
bool ovdb_getartinfo(const char *group, ARTNUM artnum, TOKEN *token);
bool ovdb_expiregroup(const char *group, int *lo, struct history *h);
bool ovdb_ctl(OVCTLTYPE type, void *val);
void ovdb_close(void);

END_DECLS

#endif /* _OVDB_H_ */

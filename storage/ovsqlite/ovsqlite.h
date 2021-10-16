#ifndef OVSQLITE_H
#define OVSQLITE_H 1

#include "config.h"

#include "inn/ov.h"
#include "inn/storage.h"

BEGIN_DECLS

bool ovsqlite_open(int mode);
bool ovsqlite_groupstats(const char *group, int *lo, int *hi, int *count,
                         int *flag);
bool ovsqlite_groupadd(const char *group, ARTNUM lo, ARTNUM hi, char *flag);
bool ovsqlite_groupdel(const char *group);
bool ovsqlite_add(const char *group, ARTNUM artnum, TOKEN token, char *data,
                  int len, time_t arrived, time_t expires);
bool ovsqlite_cancel(const char *group, ARTNUM artnum);
void *ovsqlite_opensearch(const char *group, int low, int high);
bool ovsqlite_search(void *handle, ARTNUM *artnum, char **data, int *len,
                     TOKEN *token, time_t *arrived);
void ovsqlite_closesearch(void *handle);
bool ovsqlite_getartinfo(const char *group, ARTNUM artnum, TOKEN *token);
bool ovsqlite_expiregroup(const char *group, int *lo, struct history *h);
bool ovsqlite_ctl(OVCTLTYPE type, void *val);
void ovsqlite_close(void);

END_DECLS

#endif /* OVSQLITE_H */

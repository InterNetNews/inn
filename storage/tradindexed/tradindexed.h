/*  $Id$
**
**  Public interface for the tradindexed overview method.
**
**  The exact API specified here must match the expectations of the overview
**  API.  Any changes here have to be made to all the overview methods at the
**  same time.
*/

#ifndef TRADINDEXED_H
#define TRADINDEXED_H 1

#include "config.h"
#include <sys/types.h>

#include "ov.h"
#include "storage.h"

BEGIN_DECLS

bool tradindexed_open(int mode);
bool tradindexed_groupstats(char *group, int *low, int *high, int *count,
                            int *flag);
bool tradindexed_groupadd(char *group, ARTNUM low, ARTNUM high, char *flag);
bool tradindexed_groupdel(char *group);
bool tradindexed_add(char *group, ARTNUM artnum, TOKEN token, char *data,
                     int length, time_t arrived, time_t expires);
bool tradindexed_cancel(TOKEN token);
void *tradindexed_opensearch(char *group, int low, int high);
bool tradindexed_search(void *handle, ARTNUM *artnum, char **data,
                        int *length, TOKEN *token, time_t *arrived);
void tradindexed_closesearch(void *handle);
bool tradindexed_getartinfo(char *group, ARTNUM artnum, TOKEN *token);
bool tradindexed_expiregroup(char *group, int *low, struct history *);
bool tradindexed_ctl(OVCTLTYPE type, void *val);
void tradindexed_close(void);

END_DECLS

#endif /* TRADINDEXED_H */

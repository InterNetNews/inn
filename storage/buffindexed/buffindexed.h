#ifndef _BUFFINDEXED_H_
#define _BUFFINDEXED_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

BOOL buffindexed_open(int mode);
BOOL buffindexed_groupstats(char *group, int *lo, int *hi, int *count, int *flag);
BOOL buffindexed_groupadd(char *group, ARTNUM lo, ARTNUM hi, char *flag);
BOOL buffindexed_groupdel(char *group);
BOOL buffindexed_add(TOKEN token, char *data, int len, time_t arrived);
BOOL buffindexed_cancel(TOKEN token);
void *buffindexed_opensearch(char *group, int low, int high);
BOOL buffindexed_search(void *handle, ARTNUM *artnum, char **data, int *len, TOKEN *token, time_t *arrived);
void buffindexed_closesearch(void *handle);
BOOL buffindexed_getartinfo(char *group, ARTNUM artnum, char **data, int *len, TOKEN *token);
BOOL buffindexed_expiregroup(char *group, int *lo);
BOOL buffindexed_ctl(OVCTLTYPE type, void *val);
void buffindexed_close(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _BUFFINDEXED_H_ */

#ifndef _OV3_H_
#define _OV3_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

BOOL tradindexed_open(int mode);
BOOL tradindexed_groupstats(char *group, int *lo, int *hi, int *count, int *flag);
BOOL tradindexed_groupadd(char *group, ARTNUM lo, ARTNUM hi, char *flag);
BOOL tradindexed_groupdel(char *group);
BOOL tradindexed_add(TOKEN token, char *data, int len, time_t arrived);
BOOL tradindexed_cancel(TOKEN token);
void *tradindexed_opensearch(char *group, int low, int high);
BOOL tradindexed_search(void *handle, ARTNUM *artnum, char **data, int *len, TOKEN *token, time_t *arrived);
void tradindexed_closesearch(void *handle);
BOOL tradindexed_getartinfo(char *group, ARTNUM artnum, char **data, int *len, TOKEN *token);
BOOL tradindexed_expiregroup(char *group, int *lo);
BOOL tradindexed_ctl(OVCTLTYPE type, void *val);
void tradindexed_close(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _OV3_H_ */

#ifndef _OV3_H_
#define _OV3_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

bool tradindexed_open(int mode);
bool tradindexed_groupstats(char *group, int *lo, int *hi, int *count, int *flag);
bool tradindexed_groupadd(char *group, ARTNUM lo, ARTNUM hi, char *flag);
bool tradindexed_groupdel(char *group);
bool tradindexed_add(char *group, ARTNUM artnum, TOKEN token, char *data, int len, time_t arrived, time_t expires);
bool tradindexed_cancel(TOKEN token);
void *tradindexed_opensearch(char *group, int low, int high);
bool tradindexed_search(void *handle, ARTNUM *artnum, char **data, int *len, TOKEN *token, time_t *arrived);
void tradindexed_closesearch(void *handle);
bool tradindexed_getartinfo(char *group, ARTNUM artnum, TOKEN *token);
bool tradindexed_expiregroup(char *group, int *lo, struct history *h);
bool tradindexed_ctl(OVCTLTYPE type, void *val);
void tradindexed_close(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _OV3_H_ */

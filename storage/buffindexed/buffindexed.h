#ifndef _BUFFINDEXED_H_
#define _BUFFINDEXED_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

bool buffindexed_open(int mode);
bool buffindexed_groupstats(const char *group, int *lo, int *hi, int *count,
                            int *flag);
bool buffindexed_groupadd(const char *group, ARTNUM lo, ARTNUM hi,
                          char *flag);
bool buffindexed_groupdel(const char *group);
bool buffindexed_add(const char *group, ARTNUM artnum, TOKEN token,
                     char *data, int len, time_t arrived, time_t expires);
bool buffindexed_cancel(const char *group, ARTNUM artnum);
void *buffindexed_opensearch(const char *group, int low, int high);
bool buffindexed_search(void *handle, ARTNUM *artnum, char **data, int *len,
                        TOKEN *token, time_t *arrived);
void buffindexed_closesearch(void *handle);
bool buffindexed_getartinfo(const char *group, ARTNUM artnum, TOKEN *token);
bool buffindexed_expiregroup(const char *group, int *lo, struct history *h);
bool buffindexed_ctl(OVCTLTYPE type, void *val);
void buffindexed_close(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _BUFFINDEXED_H_ */

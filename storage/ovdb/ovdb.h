#ifndef _OVDB_H_
#define _OVDB_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

bool ovdb_open(int mode);
bool ovdb_groupstats(char *group, int *lo, int *hi, int *count, int *flag);
bool ovdb_groupadd(char *group, ARTNUM lo, ARTNUM hi, char *flag);
bool ovdb_groupdel(char *group);
bool ovdb_add(char *group, ARTNUM artnum, TOKEN token, char *data, int len, time_t arrived, time_t expires);
bool ovdb_cancel(TOKEN token);
void *ovdb_opensearch(char *group, int low, int high);
bool ovdb_search(void *handle, ARTNUM *artnum, char **data, int *len, TOKEN *token, time_t *arrived);
void ovdb_closesearch(void *handle);
bool ovdb_getartinfo(char *group, ARTNUM artnum, TOKEN *token);
bool ovdb_expiregroup(char *group, int *lo, struct history *h);
bool ovdb_ctl(OVCTLTYPE type, void *val);
void ovdb_close(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _OVDB_H_ */

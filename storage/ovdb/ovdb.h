#ifndef _OVDB_H_
#define _OVDB_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

BOOL ovdb_open(int mode);
BOOL ovdb_groupstats(char *group, int *lo, int *hi, int *count, int *flag);
BOOL ovdb_groupadd(char *group, ARTNUM lo, ARTNUM hi, char *flag);
BOOL ovdb_groupdel(char *group);
BOOL ovdb_add(char *group, ARTNUM artnum, TOKEN token, char *data, int len, time_t arrived, time_t expires);
BOOL ovdb_cancel(TOKEN token);
void *ovdb_opensearch(char *group, int low, int high);
BOOL ovdb_search(void *handle, ARTNUM *artnum, char **data, int *len, TOKEN *token, time_t *arrived);
void ovdb_closesearch(void *handle);
BOOL ovdb_getartinfo(char *group, ARTNUM artnum, TOKEN *token);
BOOL ovdb_expiregroup(char *group, int *lo, struct history *h);
BOOL ovdb_ctl(OVCTLTYPE type, void *val);
void ovdb_close(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _OVDB_H_ */

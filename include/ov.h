#ifndef _OV_H_
#define _OV_H_

#include "storage.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define OV_READ  1
#define OV_WRITE 2

typedef enum {OVSPACE, OVSORT, OVCUTOFFLOW} OVCTLTYPE;
#define OV_NOSPACE 100
typedef enum {OVNEWSGROUP, OVARRIVED} OVSORTTYPE;

BOOL OVopen(int mode);
BOOL OVgroupstats(char *group, int *lo, int *hi, int *count, int *flag);
BOOL OVgroupadd(char *group, ARTNUM lo, ARTNUM hi, char *flag);
BOOL OVgroupdel(char *group);
BOOL OVadd(TOKEN token, char *data, int len, time_t arrived);
BOOL OVcancel(TOKEN token);
void *OVopensearch(char *group, int low, int high);
BOOL OVsearch(void *handle, ARTNUM *artnum, char **data, int *len, TOKEN *token, time_t *arrived);
void OVclosesearch(void *handle);
BOOL OVgetartinfo(char *group, ARTNUM artnum, char **data, int *len, TOKEN *token);
BOOL OVexpiregroup(char *group, int *lo);
BOOL OVctl(OVCTLTYPE type, void *val);
void OVclose(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _OV_H_ */

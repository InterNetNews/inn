#ifndef _OV_H_
#define _OV_H_

#include "storage.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define OV_READ  1
#define OV_WRITE 2

typedef enum {OVSPACE, OVSORT, OVCUTOFFLOW, OVGROUPBASEDEXPIRE, OVSTATICSEARCH} OVCTLTYPE;
#define OV_NOSPACE 100
typedef enum {OVNEWSGROUP, OVARRIVED, OVNOSORT} OVSORTTYPE;

typedef struct _OVGE {
    BOOL	delayrm;	  /* append tokens to filename if true */
    BOOL	usepost;	  /* posting date is used to determine expiry
				     time if true */
    BOOL	quiet;		  /* statistics will be suppressed if true */
    BOOL	keep;		  /* keep article so long as any of crossposted
				     newsgroups is not expired if true */
    BOOL	earliest;	  /* purge article any of crossposted
				     newsgroups is expired if true */
    BOOL	ignoreselfexpire; /* purge article even if storing method has
				     self expiry */
    char	*filename;	  /* used to append tokens to this file if
				     delayrm is true */
    time_t	now;		  /* used as current time */
    float	timewarp;	  /* used to bias expiry time */
} OVGE;

BOOL OVopen(int mode);
BOOL OVgroupstats(char *group, int *lo, int *hi, int *count, int *flag);
BOOL OVgroupadd(char *group, ARTNUM lo, ARTNUM hi, char *flag);
BOOL OVgroupdel(char *group);
BOOL OVadd(TOKEN token, char *data, int len, time_t arrived, time_t expires);
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

#ifndef _OV_H_
#define _OV_H_

#include "storage.h"
#include "inn/history.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define OV_READ  1
#define OV_WRITE 2

typedef enum {OVSPACE, OVSORT, OVCUTOFFLOW, OVGROUPBASEDEXPIRE, OVSTATICSEARCH, OVSTATALL, OVCACHEKEEP, OVCACHEFREE} OVCTLTYPE;
#define OV_NOSPACE 100
typedef enum {OVNEWSGROUP, OVARRIVED, OVNOSORT} OVSORTTYPE;
typedef enum {OVADDCOMPLETED, OVADDFAILED, OVADDGROUPNOMATCH} OVADDRESULT;

typedef struct _OVGE {
    bool	delayrm;	  /* append tokens to filename if true */
    bool	usepost;	  /* posting date is used to determine expiry
				     time if true */
    bool	quiet;		  /* statistics will be suppressed if true */
    bool	keep;		  /* keep article so long as any of crossposted
				     newsgroups is not expired if true */
    bool	earliest;	  /* purge article any of crossposted
				     newsgroups is expired if true */
    bool	ignoreselfexpire; /* purge article even if storing method has
				     self expiry */
    char	*filename;	  /* used to append tokens to this file if
				     delayrm is true */
    time_t	now;		  /* used as current time */
    float	timewarp;	  /* used to bias expiry time */
} OVGE;

extern bool	OVstatall;
bool OVopen(int mode);
bool OVgroupstats(char *group, int *lo, int *hi, int *count, int *flag);
bool OVgroupadd(char *group, ARTNUM lo, ARTNUM hi, char *flag);
bool OVgroupdel(char *group);
OVADDRESULT OVadd(TOKEN token, char *data, int len, time_t arrived, time_t expires);
bool OVcancel(TOKEN token);
void *OVopensearch(char *group, int low, int high);
bool OVsearch(void *handle, ARTNUM *artnum, char **data, int *len, TOKEN *token, time_t *arrived);
void OVclosesearch(void *handle);
bool OVgetartinfo(char *group, ARTNUM artnum, TOKEN *token);
bool OVexpiregroup(char *group, int *lo, struct history *h);
bool OVctl(OVCTLTYPE type, void *val);
void OVclose(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _OV_H_ */

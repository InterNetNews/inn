/*  $Id$
**
**  Overview interface header
*/

#ifndef __OVINTERFACE_H__
#define __OVINTERFACE_H__

#include "config.h"
#include "ov.h"
#include "storage.h"
#include "inn/history.h"

typedef struct {
    char	*name;
    bool	(*open)(int mode);
    bool	(*groupstats)(char *group, int *lo, int *hi, int *count, int *flag);
    bool	(*groupadd)(char *group, ARTNUM lo, ARTNUM hi, char *flag);
    bool	(*groupdel)(char *group);
    bool	(*add)(char *group, ARTNUM artnum, TOKEN token, char *data, int len, time_t arrived, time_t expires);
    bool	(*cancel)(TOKEN token);
    void	*(*opensearch)(char *group, int low, int high);
    bool	(*search)(void *handle, ARTNUM *artnum, char **data, int *len, TOKEN *token, time_t *arrived);
    void	(*closesearch)(void *handle);
    bool	(*getartinfo)(char *group, ARTNUM artnum, TOKEN *token);
    bool	(*expiregroup)(char *group, int *lo, struct history *h);
    bool	(*ctl)(OVCTLTYPE type, void *val);
    void	(*close)(void);
} OV_METHOD;

extern time_t	OVrealnow;
bool OVgroupbasedexpire(TOKEN token, char *group, char *data, int len, time_t arrived, time_t expires);
bool OVgroupmatch(char *group);
bool OVhisthasmsgid(struct history *, char *data);
void OVEXPremove(TOKEN token, bool deletedgroups, char **xref, int ngroups);

#define DEFAULT_MAX_XREF_LEN 8192

/*
**  Information about the schema of the news overview files.
*/
typedef struct _ARTOVERFIELD {  
    char	*Header;
    int		Length;
    bool	HasHeader;
    bool	NeedsHeader;
} ARTOVERFIELD;

#endif /* __OVINTERFACE_H__ */

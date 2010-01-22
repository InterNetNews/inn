/*  $Id$
**
**  Overview interface header
*/

#ifndef OVINTERFACE_H
#define OVINTERFACE_H 1

#include "config.h"
#include "inn/history.h"
#include "inn/ov.h"
#include "inn/storage.h"

struct buffer;
struct vector;

typedef struct overview_method {
    const char	*name;
    bool	(*open)(int mode);
    bool	(*groupstats)(const char *group, int *lo, int *hi, int *count,
                              int *flag);
    bool	(*groupadd)(const char *group, ARTNUM lo, ARTNUM hi,
                            char *flag);
    bool	(*groupdel)(const char *group);
    bool	(*add)(const char *group, ARTNUM artnum, TOKEN token,
                       char *data, int len, time_t arrived, time_t expires);
    bool	(*cancel)(const char *group, ARTNUM artnum);
    void	*(*opensearch)(const char *group, int low, int high);
    bool	(*search)(void *handle, ARTNUM *artnum, char **data, int *len,
                          TOKEN *token, time_t *arrived);
    void	(*closesearch)(void *handle);
    bool	(*getartinfo)(const char *group, ARTNUM artnum, TOKEN *token);
    bool	(*expiregroup)(const char *group, int *lo, struct history *h);
    bool	(*ctl)(OVCTLTYPE type, void *val);
    void	(*close)(void);
} OV_METHOD;

bool OVgroupbasedexpire(TOKEN token, const char *group, const char *data,
                        int len, time_t arrived, time_t expires);
bool OVhisthasmsgid(struct history *, const char *data);
void OVEXPremove(TOKEN token, bool deletedgroups, char **xref, int ngroups);
void OVEXPcleanup(void);

extern time_t	OVrealnow;
extern bool OVstatall;
extern time_t OVnow;
extern char *ACTIVE;
extern FILE *EXPunlinkfile;
extern bool OVignoreselfexpire;
extern bool OVusepost;
extern bool OVkeep;
extern bool OVearliest;
extern bool OVquiet;
extern time_t OVrealnow;
extern long EXPprocessed;
extern long EXPunlinked;
extern long EXPoverindexdrop;

#define DEFAULT_MAX_XREF_LEN 8192

#endif /* OVINTERFACE_H */

/*  $Id$
**
**  Overview interface header
*/

#ifndef __OVINTERFACE_H__
#define __OVINTERFACE_H__

#include <configdata.h>
#include <storage.h>

typedef struct {
    char	*name;
    BOOL	(*open)(int mode);
    BOOL	(*groupstats)(char *group, int *lo, int *hi, int *count, int *flag);
    BOOL	(*groupadd)(char *group, ARTNUM lo, char *flag);
    BOOL	(*groupdel)(char *group);
    BOOL	(*add)(TOKEN token, char *data, int len);
    BOOL	(*cancel)(TOKEN token);
    void	*(*opensearch)(char *group, int low, int high);
    BOOL	(*search)(void *handle, ARTNUM *artnum, char **data, int *len, TOKEN *token);
    void	(*closesearch)(void *handle);
    BOOL	(*getartinfo)(char *group, ARTNUM artnum, char **data, int *len, TOKEN *token);
    BOOL	(*expiregroup)(char *group, int *lo);
    void	(*close)(void);
} OV_METHOD;

#endif /* __OVINTERFACE_H__ */

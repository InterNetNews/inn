#ifndef __INTERFACE_H__
#define __INTERFACE_H__

#include <configdata.h>
#include <storage.h>

typedef struct {
    char                *name;
    unsigned char       type;
    BOOL                (*init)(void);
    TOKEN               (*store)(const ARTHANDLE article, STORAGECLASS storageclass);
    ARTHANDLE           *(*retrieve)(const TOKEN token, const RETRTYPE amount);
    ARTHANDLE           *(*next)(const ARTHANDLE *article, const RETRTYPE amount);
    void                (*freearticle)(ARTHANDLE *article);
    BOOL                (*cancel)(TOKEN token);
    void                (*shutdown)(void);
} STORAGE_METHOD;

char *SMFindBody(char *article, int len);
void SMseterror(int errno, char *error);

#endif /* __INTERFACE_H__ */

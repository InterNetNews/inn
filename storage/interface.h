#ifndef __INTERFACE_H__
#define __INTERFACE_H__

#include <configdata.h>
#include <storage.h>

typedef struct {
    char                *name;
    unsigned char       type;
    void                *(*init)(void);
    TOKEN               (*store)(void *handle, const ARTHANDLE article, STORAGECLASS storageclass);
    ARTHANDLE           *(*retrieve)(void *handle, const TOKEN token, const RETRTYPE amount);
    ARTHANDLE           *(*next)(void *handle, const ARTHANDLE *article, const RETRTYPE amount);
    void                (*freearticle)(void *handle, ARTHANDLE *article);
    BOOL                (*cancel)(void *handle, TOKEN token);
    void                (*shutdown)(void *handle);
} STORAGE_METHOD;

char *SMFindBody(char *article, int len);
void SMseterror(int errno, char *error);

#endif /* __INTERFACE_H__ */

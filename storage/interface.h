#ifndef __INTERFACE_H__
#define __INTERFACE_H__

#include <configdata.h>
#include <storage.h>

typedef struct {
    char                *name;
    unsigned char       type;
    BOOL                (*init)(void);
    TOKEN               (*store)(const ARTHANDLE article);
    ARTHANDLE           *(*retrieve)(const TOKEN token, const RETRTYPE amount);
    ARTHANDLE           *(*next)(const ARTHANDLE *article, const RETRTYPE amount);
    void                (*freearticle)(ARTHANDLE *article);
    BOOL                (*cancel)(TOKEN token);
    void                (*shutdown)(void);
} STORAGE_METHOD;

char *SMyFindBody(char *article, int len);

#endif __INTERFACE_H__

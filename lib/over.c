#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "configdata.h"
#include "clibrary.h"
#include "libinn.h"
#include "macros.h"


#define OFFSET_ARTNUM      0
#define OFFSET_HASH        4

/* 
** Pack the OVERINDEX structure into something more suitable for storing
** on disk.  Result is architecture independent.
*/
void PackOverIndex(OVERINDEX *index, char *result) {
    unsigned long       i;
    int                 intoverage;
    
    intoverage = sizeof(i) - 4;
    
    i = htonl(index->artnum & 0xffffffff);
    memcpy(&result[OFFSET_ARTNUM], &i, sizeof(i));
    memmove(&result[OFFSET_ARTNUM], &result[intoverage], 4);
    
    memcpy(&result[OFFSET_HASH], &index->hash, sizeof(HASH));
}


void UnpackOverIndex(char *packedindex, OVERINDEX *index) {
    unsigned long       i;
    char                buff[sizeof(i)];
    int                 intoverage;

    intoverage = sizeof(i) - 4;

    memset(buff, '\0', sizeof(buff));
    memcpy(&buff[intoverage], &packedindex[OFFSET_ARTNUM], 4);
    memcpy(&i, buff, sizeof(i));
    index->artnum = ntohl(i);

    memcpy(&index->hash, &packedindex[OFFSET_HASH], sizeof(HASH));
}


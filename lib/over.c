#include <stdio.h>
#include <netinet/in.h>
#include "configdata.h"
#include "clibrary.h"
#include "libinn.h"
#include "macros.h"


#define OFFSET_ARTNUM      0
#define OFFSET_OFFSET      4
#define OFFSET_TOKEN       8
#define OFFSET_CANCELLED   sizeof(TOKEN) + 8

/* 
** Pack the OVERINDEX structure into something more suitable for storing
** on disk.  Result is architecture independent.
*/
void PackOverIndex(OVERINDEX *index, char *result) {
    unsigned long       i;
    unsigned char       c;
    int                 intoverage;
    
    intoverage = sizeof(i) - 4;
    
    i = htonl(index->artnum & 0xffffffff);
    memcpy(&result[OFFSET_ARTNUM], &i, sizeof(i));
    memmove(&result[OFFSET_ARTNUM], &result[intoverage], 4);
    
    i = htonl(index->offset & 0xffffffff);
    memcpy(&result[OFFSET_OFFSET], &i, sizeof(i));
    memmove(&result[OFFSET_OFFSET],
	    &result[OFFSET_OFFSET + intoverage], 4);

    memcpy(&result[OFFSET_TOKEN], &index->token, sizeof(TOKEN));

    c = index->cancelled;
    memcpy(&result[OFFSET_CANCELLED], &c, 1);

}


void UnpackOverIndex(char *packedindex, OVERINDEX *index) {
    unsigned long       i;
    unsigned char       c;
    char                buff[sizeof(i)];
    int                 intoverage;

    intoverage = sizeof(i) - 4;

    memset(buff, '\0', sizeof(buff));
    memcpy(&buff[intoverage], &packedindex[OFFSET_ARTNUM], 4);
    memcpy(&i, buff, sizeof(i));
    index->artnum = ntohl(i);

    memset(buff, '\0', sizeof(buff));
    memcpy(&buff[intoverage], &packedindex[OFFSET_OFFSET], 4);
    memcpy(&i, buff, sizeof(i));
    index->offset = ntohl(i);

    memcpy(&index->token, &packedindex[OFFSET_TOKEN], sizeof(TOKEN));

    memcpy(&c, &packedindex[OFFSET_CANCELLED], 1);
    index->cancelled = c;
}


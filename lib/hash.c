/* This provides a generic hash function for use w/INN.  Currently
   is implemented using MD5, but should probably have a mechanism for
   choosing the hash algorithm and tagging the hash with the algorithm
   used */

#include <stdio.h>
#include "configdata.h"
#include "clibrary.h"
#include "libinn.h"
#include "md5.h"

STATIC HASH empty= { { 0, 0, 0, 0, 0, 0, 0, 0,
		       0, 0, 0, 0, 0, 0, 0, 0 }};

HASH Hash(const void *value, const size_t len) {
    MD5_CTX context;
    HASH hash;

    MD5Init(&context);
    MD5Update(&context, value, len);
    MD5Final(&context);
    memcpy(&hash,
	   &context.digest,
	   (sizeof(hash) < sizeof(context.digest)) ? sizeof(hash) : sizeof(context.digest));
    return hash;
}

BOOL HashEmpty(const HASH h) {
    return (memcmp(&empty, &h, sizeof(HASH)) == 0);
}

void HashClear(HASH *hash) {
    memset(hash, '\0', sizeof(HASH));
}

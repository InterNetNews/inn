/* $Id$ */
/* MD5 hashing test suite. */

#include "config.h"
#include "clibrary.h"
#include "inn/md5.h"
#include "libinn.h"

/* Used for converting digests to hex to make them easier to deal with. */
static const char hex[] = "0123456789abcdef";

/* A set of data strings and resulting digests to check.  It's not easy to
   get nulls into this data structure, so data containing nulls should be
   checked separately. */
static const char * const testdata[] = {
    /* First five tests of the MD5 test suite from RFC 1321. */
    "",
    "a",
    "abc",
    "message digest",
    "abcdefghijklmnopqrstuvwxyz",

    /* Three real message IDs to ensure compatibility with old INN versions;
       the corresponding MD5 hashes were taken directly out of the history
       file of a server running INN 2.3. */
    "<J3Ds5.931$Vg6.7556@news01.chello.no>",
    "<sr5v7ooea6e17@corp.supernews.com>",
    "<cancel.Y2Ds5.26391$oH5.540535@news-east.usenetserver.com>",

    /* Other random stuff, including high-bit characters. */
    "example.test",
    "||",
    "|||",
    "\375\277\277\277\277\276",
    "\377\277\277\277\277\277"
};

/* The hashes corresonding to the above data. */
static const char * const testhash[] = {
    "d41d8cd98f00b204e9800998ecf8427e",
    "0cc175b9c0f1b6a831c399e269772661",
    "900150983cd24fb0d6963f7d28e17f72",
    "f96b697d7cb7938d525a2f31aaf161d0",
    "c3fcd3d76192e4007dfb496cca67e13b",
    "c4a70fb19af37bed6b7c77f1e1187f00",
    "7f70531c7027c20b0ddba0a649cf8691",
    "9d9f0423f38b731c9bf69607cea6be76",
    "09952be409a7d6464cd7661beeeb966e",
    "7d010443693eec253a121e2aa2ba177c",
    "2edf2958166561c5c08cd228e53bbcdc",
    "c18293a6fe0a09720e841c8ebc697b97",
    "ce23eb027c63215b999b9f86d6a4f9cb"
};

#define ARRAY_SIZE(array)       sizeof(array) / sizeof(array[0])

void
digest2hex(const unsigned char *digest, char *result)
{
    const char *p;
    unsigned int i;

    for (p = digest, i = 0; i < 32; i += 2, p++) {
        result[i] = hex[(*p & 0xf0) >> 4];
        result[i + 1] = hex[*p & 0x0f];
    }
    result[32] = '\0';
}

static void
ok(int n, const unsigned char *expected, const unsigned char *data,
   size_t length)
{
    unsigned char digest[16];
    char hexdigest[33];

    md5_hash(data, length, digest);
    digest2hex(digest, hexdigest);
    if (!strcmp(expected, hexdigest)) {
        printf("ok %d\n", n);
    } else {
        printf("not ok %d\n   saw: %s\n  want: %s\n", n, hexdigest,
               expected);
    }
}

int
main(void)
{
    int i, j, n;
    char *data;

    printf("%d\n", 11 + ARRAY_SIZE(testdata));

    ok(1, "93b885adfe0da089cdf634904fd59f71", "\0", 1);
    ok(2, "e94a053c3fbfcfb22b4debaa11af7718", "\0ab\n", 4);

    data = xmalloc(64 * 1024);
    memset(data, 0, 64 * 1024);
    ok(3, "fcd6bcb56c1689fcef28b57c22475bad", data, 64 * 1024);
    memset(data, 1, 32 * 1024);
    ok(4, "3d8897b14254c9f86fbad3fe22f62edd", data, 64 * 1024);
    ok(5, "25364962aa23b187942a24ae736c4e8c", data, 65000);
    ok(6, "f9816b5d5363d15f14bb98d548309dcc", data, 55);
    ok(7, "5e99dfddfb51c18cfc55911dee24ae7b", data, 56);
    ok(8, "0871ffa021e2bc4da87eb93ac22d293c", data, 63);
    ok(9, "784d68ba9112308689114a6816c628ce", data, 64);

    /* Part of the MD5 test suite from RFC 1321. */
    for (i = 0, n = 'A'; n <= 'Z'; i++, n++)
        data[i] = n;
    for (i = 26, n = 'a'; n <= 'z'; i++, n++)
        data[i] = n;
    for (i = 52, n = '0'; n <= '9'; i++, n++)
        data[i] = n;
    ok(10, "d174ab98d277d9f5a5611c2c9f419d9f", data, 62);
    for (i = 0, j = 0; j < 8; j++) {
        for (n = '1'; n <= '9'; i++, n++)
            data[i] = n;
        data[i++] = '0';
    }
    ok(11, "57edf4a22be3c955ac49da2e2107b67a", data, 80);

    n = 12;
    for (i = 0; i < ARRAY_SIZE(testdata); i++)
        ok(n++, testhash[i], testdata[i], strlen(testdata[i]));

    return 0;
}

/*  $Id$
**
**  RSA Data Security, Inc. MD5 Message-Digest Algorithm
**
**  LANDON CURT NOLL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
**  INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN
**  NO EVENT SHALL LANDON CURT NOLL BE LIABLE FOR ANY SPECIAL, INDIRECT OR
**  CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
**  USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
**  OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
**  PERFORMANCE OF THIS SOFTWARE.
**
**  Copyright (C) 1990, RSA Data Security, Inc. All rights reserved.
**
**  License to copy and use this software is granted provided that it is
**  identified as the "RSA Data Security, Inc. MD5 Message-Digest
**  Algorithm" in all material mentioning or referencing this software or
**  this function.
**
**  License is also granted to make and use derivative works provided that
**  such works are identified as "derived from the RSA Data Security,
**  Inc. MD5 Message-Digest Algorithm" in all material mentioning or
**  referencing the derived work.
**
**  RSA Data Security, Inc. makes no representations concerning either the
**  merchantability of this software or the suitability of this software for
**  any particular purpose.  It is provided "as is" without express or
**  implied warranty of any kind.
**
**  These notices must be retained in any copies of any part of this
**  documentation and/or software.
*/

#ifndef INN_MD5_H
#define INN_MD5_H 1

#include <inn/defines.h>

/* Make sure we have uint32_t. */
#include <sys/types.h>
#if INN_HAVE_INTTYPES_H
# include <inttypes.h>
#endif

/* SCO OpenServer gets int32_t from here. */
#if INN_HAVE_SYS_BITYPES_H
# include <sys/bitypes.h>
#endif

/* Bytes to process at once, defined by the algorithm. */ 
#define MD5_CHUNKSIZE   (1 << 6)
#define MD5_CHUNKWORDS  (MD5_CHUNKSIZE / sizeof(uint32_t))

/* Length of the digest, defined by the algorithm. */
#define MD5_DIGESTSIZE  16
#define MD5_DIGESTWORDS (MD5_DIGESTSIZE / sizeof(uint32_t))

BEGIN_DECLS

/* Data structure for MD5 message-digest computation. */
struct md5_context {
    uint32_t count[2];                          /* A 64-bit byte count. */
    uint32_t buf[MD5_DIGESTWORDS];              /* Scratch buffer. */
    union {
        unsigned char byte[MD5_CHUNKSIZE];      /* Byte chunk buffer. */
        uint32_t word[MD5_CHUNKWORDS];          /* Word chunk buffer. */
    } in;
    unsigned int datalen;                       /* Length of data in in. */
    unsigned char digest[MD5_DIGESTSIZE];       /* Final digest. */
};

extern void md5_hash(const unsigned char *, size_t, unsigned char *);
extern void md5_init(struct md5_context *);
extern void md5_update(struct md5_context *, const unsigned char *, size_t);
extern void md5_final(struct md5_context *);

END_DECLS

#endif /* !INN_MD5_H */

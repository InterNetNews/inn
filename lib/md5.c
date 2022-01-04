/*
**  RSA Data Security, Inc. MD5 Message-Digest Algorithm
**
**  The standard MD5 message digest routines, from RSA Data Security, Inc.
**  by way of Landon Curt Noll, modified and integrated into INN by Clayton
**  O'Neill and then simplified somewhat, converted to INN coding style,
**  and commented by Russ Allbery.
**
**  To form the message digest for a message M:
**    (1) Initialize a context buffer md5_context using md5_init
**    (2) Call md5_update on md5_context and M
**    (3) Call md5_final on md5_context
**  The message digest is now in md5_context->digest[0...15].
**
**  Alternately, just call md5_hash on M, passing it a buffer of at least
**  16 bytes into which to put the digest.  md5_hash does the above
**  internally for you and is the most convenient interface; the interface
**  described above, however, is better when all of the data to hash isn't
**  available neatly in a single buffer (such as hashing data aquired a
**  block at a time).
**
**  For information about MD5, see RFC 1321.
**
**  LANDON CURT NOLL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
**  INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO
**  EVENT SHALL LANDON CURT NOLL BE LIABLE FOR ANY SPECIAL, INDIRECT OR
**  CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
**  USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
**  OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
**  PERFORMANCE OF THIS SOFTWARE.
**
**  Copyright (C) 1990, RSA Data Security, Inc.  All rights reserved.
**
**  License to copy and use this software is granted provided that it is
**  identified as the "RSA Data Security, Inc. MD5 Message-Digest Algorithm"
**  in all material mentioning or referencing this software or this
**  function.
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

/*
**  The actual mathematics and cryptography are at the bottom of this file,
**  as those parts should be fully debugged and very infrequently changed.
**  The core of actual work is done by md5_transform.  The beginning of this
**  file contains the infrastructure around the mathematics.
*/

#include "portable/system.h"

#include "inn/md5.h"

/* Rotate a 32-bit value left, used by both the MD5 mathematics and by the
   routine below to byteswap data on big-endian machines. */
#define ROT(X, n) (((X) << (n)) | ((X) >> (32 - (n))))

/* Almost zero fill padding, used by md5_final.  The 0x80 is part of the MD5
   hash algorithm, from RFC 1321 section 3.1:

     Padding is performed as follows: a single "1" bit is appended to the
     message, and then "0" bits are appended so that the length in bits of
     the padded message becomes congruent to 448, modulo 512.  In all, at
     least one bit and at most 512 bits are appended.

   Let the compiler zero the remainder of the array for us, guaranteed by
   ISO C99 6.7.8 paragraph 21. */
static const unsigned char padding[MD5_CHUNKSIZE] = {0x80, 0 /* 0, ... */};

/* Internal prototypes. */
static void md5_transform(uint32_t *, const uint32_t *);
static void md5_update_block(struct md5_context *, const unsigned char *,
                             size_t);

/* MD5 requires that input data be treated as words in little-endian byte
   order.  From RFC 1321 section 2:

     Similarly, a sequence of bytes can be interpreted as a sequence of
     32-bit words, where each consecutive group of four bytes is interpreted
     as a word with the low-order (least significant) byte given first.

   Input data must therefore be byteswapped on big-endian machines, as must
   the 16-byte result digest.  Since we have to make a copy of the incoming
   data anyway to ensure alignment for 4-byte access, we can byteswap it in
   place. */
#if !WORDS_BIGENDIAN
#    define decode(data)      /* empty */
#    define encode(data, out) memcpy((out), (data), MD5_DIGESTSIZE)
#else

/* The obvious way to do this is to pull single bytes at a time out of the
   input array and build words from them; this requires three shifts and
   three Boolean or operations, but it requires four memory reads per word
   unless the compiler is really smart.  Since we can assume four-byte
   alignment for the input data, use this optimized routine from J. Touch,
   USC/ISI.  This requires four shifts, two ands, and two ors, but only one
   memory read per word. */
#    define swap(word)              \
        do {                        \
            htmp = ROT((word), 16); \
            ltmp = htmp >> 8;       \
            htmp &= 0x00ff00ff;     \
            ltmp &= 0x00ff00ff;     \
            htmp <<= 8;             \
            (word) = htmp | ltmp;   \
        } while (0)

/* We process 16 words of data (one MD5 block) of data at a time, completely
   unrolling the loop manually since it should allow the compiler to take
   advantage of pipelining and parallel arithmetic units. */
static void
decode(uint32_t *data)
{
    uint32_t ltmp, htmp;

    swap(data[0]);
    swap(data[1]);
    swap(data[2]);
    swap(data[3]);
    swap(data[4]);
    swap(data[5]);
    swap(data[6]);
    swap(data[7]);
    swap(data[8]);
    swap(data[9]);
    swap(data[10]);
    swap(data[11]);
    swap(data[12]);
    swap(data[13]);
    swap(data[14]);
    swap(data[15]);
}

/* Used by md5_final to generate the final digest.  The digest is not
   guaranteed to be aligned for 4-byte access but we are allowed to be
   destructive, so byteswap first and then copy the result over. */
static void
encode(uint32_t *data, unsigned char *out)
{
    uint32_t ltmp, htmp;

    swap(data[0]);
    swap(data[1]);
    swap(data[2]);
    swap(data[3]);
    memcpy(out, data, MD5_DIGESTSIZE);
}

#endif /* WORDS_BIGENDIAN */


/*
**  That takes care of the preliminaries; now the real fun begins.  The
**  initialization function for a struct md5_context; set lengths to zero
**  and initialize our working buffer with the magic constants (c.f. RFC
**  1321 section 3.3).
*/
void
md5_init(struct md5_context *context)
{
    context->buf[0] = 0x67452301U;
    context->buf[1] = 0xefcdab89U;
    context->buf[2] = 0x98badcfeU;
    context->buf[3] = 0x10325476U;

    context->count[0] = 0;
    context->count[1] = 0;
    context->datalen = 0;
}


/*
**  The workhorse function, called for each chunk of data.  Update the
**  message-digest context to account for the presence of each of the
**  characters data[0 .. count - 1] in the message whose digest is being
**  computed.  Accepts any length of data; all whole blocks are processed
**  and the remaining data is buffered for later processing by either
**  another call to md5_update or by md5_final.
*/
void
md5_update(struct md5_context *context, const unsigned char *data,
           size_t count)
{
    unsigned int datalen = context->datalen;
    unsigned int left;
    size_t high_count, low_count, used;

    /* Update the count of hashed bytes.  The machinations here are to do
       the right thing on a platform where size_t > 32 bits without causing
       compiler warnings on platforms where it's 32 bits.  RFC 1321 section
       3.2 says:

         A 64-bit representation of b (the length of the message before the
         padding bits were added) is appended to the result of the previous
         step.  In the unlikely event that b is greater than 2^64, then only
         the low-order 64 bits of b are used.

       so we can just ignore the higher bits of count if size_t is larger
       than 64 bits.  (Think ahead!)  If size_t is only 32 bits, the
       compiler should kill the whole if statement as dead code. */
    if (sizeof(count) > 4) {
        high_count = count >> 31;
        context->count[1] += (high_count >> 1) & 0xffffffffU;
    }

    /* Now deal with count[0].  Add in the low 32 bits of count and
       increment count[1] if count[0] wraps.  Isn't unsigned arithmetic
       cool? */
    low_count = count & 0xffffffffU;
    context->count[0] += low_count;
    if (context->count[0] < low_count)
        context->count[1]++;

    /* See if we already have some data queued.  If so, try to complete a
       block and then deal with it first.  If the new data still doesn't
       fill the buffer, add it to the buffer and return.  Otherwise,
       transform that block and update data and count to account for the
       data we've already used. */
    if (datalen > 0) {
        left = MD5_CHUNKSIZE - datalen;
        if (left > count) {
            memcpy(context->in.byte + datalen, data, count);
            context->datalen += count;
            return;
        } else {
            memcpy(context->in.byte + datalen, data, left);
            decode(context->in.word);
            md5_transform(context->buf, context->in.word);
            data += left;
            count -= left;
            context->datalen = 0;
        }
    }

    /* If we have a block of data or more left, pass the rest off to
       md5_update_block to deal with all of the full blocks available. */
    if (count >= MD5_CHUNKSIZE) {
        md5_update_block(context, data, count);
        used = (count / MD5_CHUNKSIZE) * MD5_CHUNKSIZE;
        data += used;
        count -= used;
    }

    /* If there's anything left, buffer it until we can complete a block or
       for md5_final to deal with. */
    if (count > 0) {
        memcpy(context->in.byte, data, count);
        context->datalen = count;
    }
}


/*
**  Update the message-digest context to account for the presence of each of
**  the characters data[0 .. count - 1] in the message whose digest is being
**  computed, except that we only deal with full blocks of data.  The data
**  is processed one block at a time, and partial blocks at the end are
**  ignored (they're dealt with by md5_update, which calls this routine.
**
**  Note that we always make a copy of the input data into an array of
**  4-byte values.  If our input data were guaranteed to be aligned for
**  4-byte access, we could just use the input buffer directly on
**  little-endian machines, and in fact this implementation used to do that.
**  However, a requirement to align isn't always easily detectable, even by
**  configure (an Alpha running Tru64 will appear to allow unaligned
**  accesses, but will spew errors to the terminal if you do it).  On top of
**  that, even when it's allowed, unaligned access is quite frequently
**  slower; we're about to do four reads of each word of the input data for
**  the calculations, so doing one additional copy of the data up-front is
**  probably worth it.
*/
static void
md5_update_block(struct md5_context *context, const unsigned char *data,
                 size_t count)
{
    uint32_t in[MD5_CHUNKWORDS];

    /* Process data in MD5_CHUNKSIZE blocks. */
    while (count >= MD5_CHUNKSIZE) {
        memcpy(in, data, MD5_CHUNKSIZE);
        decode(in);
        md5_transform(context->buf, in);
        data += MD5_CHUNKSIZE;
        count -= MD5_CHUNKSIZE;
    }
}


/*
**  Terminates the message-digest computation, accounting for any final
**  trailing data and adding the message length to the hashed data, and ends
**  with the desired message digest in context->digest[0...15].
*/
void
md5_final(struct md5_context *context)
{
    unsigned int pad_needed, left;
    uint32_t count[2];
    uint32_t *countloc;

    /* Save the count before appending the padding. */
    count[0] = context->count[0];
    count[1] = context->count[1];

    /* Pad the final block of data.  RFC 1321 section 3.1:

         The message is "padded" (extended) so that its length (in bits) is
         congruent to 448, modulo 512.  That is, the message is extended so
         that it is just 64 bits shy of being a multiple of 512 bits long.
         Padding is always performed, even if the length of the message is
         already congruent to 448, modulo 512.

       The 64 bits (two words) left are for the 64-bit count of bits
       hashed.  We'll need at most 64 bytes of padding; lucky that the
       padding array is exactly that size! */
    left = context->datalen;
    pad_needed = (left < 64 - 8) ? (64 - 8 - left) : (128 - 8 - left);
    md5_update(context, padding, pad_needed);

    /* Append length in bits and transform.  Note that we cheat slightly
       here to save a few operations on big-endian machines; the algorithm
       says that we should add the length, in little-endian byte order, to
       the last block of data.  We'd then transform it into big-endian order
       on a big-endian machine.  But the count is *already* in big-endian
       order on a big-endian machine, so effectively we'd be byteswapping it
       twice.  Instead, add it to the block after doing byte swapping on the
       rest.

       Note that we've been counting *bytes*, but the algorithm demands the
       length in *bits*, so shift things around a bit. */
    decode(context->in.word);
    countloc = &context->in.word[MD5_CHUNKWORDS - 2];
    countloc[0] = count[0] << 3;
    countloc[1] = (count[1] << 3) | (count[0] >> 29);
    md5_transform(context->buf, context->in.word);

    /* Recover the final digest.  Whoo-hoo, we're done! */
    encode(context->buf, context->digest);
}


/*
**  A convenience wrapper around md5_init, md5_update, and md5_final.  Takes
**  a pointer to a buffer of data, the length of the data, and a pointer to
**  a buffer of at least 16 bytes into which to write the message digest.
*/
void
md5_hash(const unsigned char *data, size_t length, unsigned char *digest)
{
    struct md5_context context;

    md5_init(&context);
    md5_update(&context, data, length);
    md5_final(&context);
    memcpy(digest, context.digest, MD5_DIGESTSIZE);
}


/*
**  Look out, here comes the math.
**
**  There are no user-serviceable parts below this point unless you know
**  quite a bit about MD5 or optimization of integer math.  The only
**  function remaining, down below, is md5_transform; the rest of this is
**  setting up macros to make that function more readable.
**
**  The F, G, H and I are basic MD5 functions.  The following identity saves
**  one boolean operation.
**
**  F: (((x) & (y)) | (~(x) & (z))) == ((z) ^ ((x) & ((y) ^ (z))))
**  G: (((x) & (z)) | ((y) & ~(z))) == ((y) ^ ((z) & ((x) ^ (y))))
*/
#define F(x, y, z) ((z) ^ ((x) & ((y) ^ (z))))
#define G(x, y, z) ((y) ^ ((z) & ((x) ^ (y))))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

/*
**  FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4.  Rotation
**  is separate from addition to prevent recomputation.  S?? are the shift
**  values for each round.
*/
#define S11        7
#define S12        12
#define S13        17
#define S14        22
#define FF(a, b, c, d, x, s, ac)                         \
    {                                                    \
        (a) += F((b), (c), (d)) + (x) + (uint32_t) (ac); \
        (a) = ROT((a), (s));                             \
        (a) += (b);                                      \
    }

#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define GG(a, b, c, d, x, s, ac)                         \
    {                                                    \
        (a) += G((b), (c), (d)) + (x) + (uint32_t) (ac); \
        (a) = ROT((a), (s));                             \
        (a) += (b);                                      \
    }

#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define HH(a, b, c, d, x, s, ac)                         \
    {                                                    \
        (a) += H((b), (c), (d)) + (x) + (uint32_t) (ac); \
        (a) = ROT((a), (s));                             \
        (a) += (b);                                      \
    }

#define S41 6
#define S42 10
#define S43 15
#define S44 21
#define II(a, b, c, d, x, s, ac)                         \
    {                                                    \
        (a) += I((b), (c), (d)) + (x) + (uint32_t) (ac); \
        (a) = ROT((a), (s));                             \
        (a) += (b);                                      \
    }

/*
**  Basic MD5 step.  Transforms buf based on in.
*/
static void
md5_transform(uint32_t *buf, const uint32_t *in)
{
    uint32_t a = buf[0];
    uint32_t b = buf[1];
    uint32_t c = buf[2];
    uint32_t d = buf[3];

    /* clang-format off */
    /* Round 1 */
    FF(a, b, c, d, in[ 0], S11, 3614090360UL); /*  1 */
    FF(d, a, b, c, in[ 1], S12, 3905402710UL); /*  2 */
    FF(c, d, a, b, in[ 2], S13,  606105819UL); /*  3 */
    FF(b, c, d, a, in[ 3], S14, 3250441966UL); /*  4 */
    FF(a, b, c, d, in[ 4], S11, 4118548399UL); /*  5 */
    FF(d, a, b, c, in[ 5], S12, 1200080426UL); /*  6 */
    FF(c, d, a, b, in[ 6], S13, 2821735955UL); /*  7 */
    FF(b, c, d, a, in[ 7], S14, 4249261313UL); /*  8 */
    FF(a, b, c, d, in[ 8], S11, 1770035416UL); /*  9 */
    FF(d, a, b, c, in[ 9], S12, 2336552879UL); /* 10 */
    FF(c, d, a, b, in[10], S13, 4294925233UL); /* 11 */
    FF(b, c, d, a, in[11], S14, 2304563134UL); /* 12 */
    FF(a, b, c, d, in[12], S11, 1804603682UL); /* 13 */
    FF(d, a, b, c, in[13], S12, 4254626195UL); /* 14 */
    FF(c, d, a, b, in[14], S13, 2792965006UL); /* 15 */
    FF(b, c, d, a, in[15], S14, 1236535329UL); /* 16 */

    /* Round 2 */
    GG(a, b, c, d, in[ 1], S21, 4129170786UL); /* 17 */
    GG(d, a, b, c, in[ 6], S22, 3225465664UL); /* 18 */
    GG(c, d, a, b, in[11], S23,  643717713UL); /* 19 */
    GG(b, c, d, a, in[ 0], S24, 3921069994UL); /* 20 */
    GG(a, b, c, d, in[ 5], S21, 3593408605UL); /* 21 */
    GG(d, a, b, c, in[10], S22,   38016083UL); /* 22 */
    GG(c, d, a, b, in[15], S23, 3634488961UL); /* 23 */
    GG(b, c, d, a, in[ 4], S24, 3889429448UL); /* 24 */
    GG(a, b, c, d, in[ 9], S21,  568446438UL); /* 25 */
    GG(d, a, b, c, in[14], S22, 3275163606UL); /* 26 */
    GG(c, d, a, b, in[ 3], S23, 4107603335UL); /* 27 */
    GG(b, c, d, a, in[ 8], S24, 1163531501UL); /* 28 */
    GG(a, b, c, d, in[13], S21, 2850285829UL); /* 29 */
    GG(d, a, b, c, in[ 2], S22, 4243563512UL); /* 30 */
    GG(c, d, a, b, in[ 7], S23, 1735328473UL); /* 31 */
    GG(b, c, d, a, in[12], S24, 2368359562UL); /* 32 */

    /* Round 3 */
    HH(a, b, c, d, in[ 5], S31, 4294588738UL); /* 33 */
    HH(d, a, b, c, in[ 8], S32, 2272392833UL); /* 34 */
    HH(c, d, a, b, in[11], S33, 1839030562UL); /* 35 */
    HH(b, c, d, a, in[14], S34, 4259657740UL); /* 36 */
    HH(a, b, c, d, in[ 1], S31, 2763975236UL); /* 37 */
    HH(d, a, b, c, in[ 4], S32, 1272893353UL); /* 38 */
    HH(c, d, a, b, in[ 7], S33, 4139469664UL); /* 39 */
    HH(b, c, d, a, in[10], S34, 3200236656UL); /* 40 */
    HH(a, b, c, d, in[13], S31,  681279174UL); /* 41 */
    HH(d, a, b, c, in[ 0], S32, 3936430074UL); /* 42 */
    HH(c, d, a, b, in[ 3], S33, 3572445317UL); /* 43 */
    HH(b, c, d, a, in[ 6], S34,   76029189UL); /* 44 */
    HH(a, b, c, d, in[ 9], S31, 3654602809UL); /* 45 */
    HH(d, a, b, c, in[12], S32, 3873151461UL); /* 46 */
    HH(c, d, a, b, in[15], S33,  530742520UL); /* 47 */
    HH(b, c, d, a, in[ 2], S34, 3299628645UL); /* 48 */

    /* Round 4 */
    II(a, b, c, d, in[ 0], S41, 4096336452UL); /* 49 */
    II(d, a, b, c, in[ 7], S42, 1126891415UL); /* 50 */
    II(c, d, a, b, in[14], S43, 2878612391UL); /* 51 */
    II(b, c, d, a, in[ 5], S44, 4237533241UL); /* 52 */
    II(a, b, c, d, in[12], S41, 1700485571UL); /* 53 */
    II(d, a, b, c, in[ 3], S42, 2399980690UL); /* 54 */
    II(c, d, a, b, in[10], S43, 4293915773UL); /* 55 */
    II(b, c, d, a, in[ 1], S44, 2240044497UL); /* 56 */
    II(a, b, c, d, in[ 8], S41, 1873313359UL); /* 57 */
    II(d, a, b, c, in[15], S42, 4264355552UL); /* 58 */
    II(c, d, a, b, in[ 6], S43, 2734768916UL); /* 59 */
    II(b, c, d, a, in[13], S44, 1309151649UL); /* 60 */
    II(a, b, c, d, in[ 4], S41, 4149444226UL); /* 61 */
    II(d, a, b, c, in[11], S42, 3174756917UL); /* 62 */
    II(c, d, a, b, in[ 2], S43,  718787259UL); /* 63 */
    II(b, c, d, a, in[ 9], S44, 3951481745UL); /* 64 */
    /* clang-format on */

    buf[0] += a;
    buf[1] += b;
    buf[2] += c;
    buf[3] += d;
}

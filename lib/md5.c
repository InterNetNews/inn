/* @(#)md5.c	12.1 17 Nov 1995 04:22:34 */
/*
 * md5 - RSA Data Security, Inc. MD5 Message-Digest Algorithm
 *
 * LANDON CURT NOLL DISCLAIMS ALL WARRANTIES WITH  REGARD  TO
 * THIS  SOFTWARE,  INCLUDING  ALL IMPLIED WARRANTIES OF MER-
 * CHANTABILITY AND FITNESS.  IN NO EVENT SHALL  LANDON  CURT
 * NOLL  BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM  LOSS  OF
 * USE,  DATA  OR  PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR  IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * See md5drvr.c for version and modification history.
 */

/*
 ***********************************************************************
 ** Copyright (C) 1990, RSA Data Security, Inc. All rights reserved.  **
 **								      **
 ** License to copy and use this software is granted provided that    **
 ** it is identified as the "RSA Data Security, Inc. MD5 Message-     **
 ** Digest Algorithm" in all material mentioning or referencing this  **
 ** software or this function.					      **
 **								      **
 ** License is also granted to make and use derivative works	      **
 ** provided that such works are identified as "derived from the RSA  **
 ** Data Security, Inc. MD5 Message-Digest Algorithm" in all          **
 ** material mentioning or referencing the derived work.	      **
 **								      **
 ** RSA Data Security, Inc. makes no representations concerning       **
 ** either the merchantability of this software or the suitability    **
 ** of this software for any particular purpose.  It is provided "as  **
 ** is" without express or implied warranty of any kind.              **
 **								      **
 ** These notices must be retained in any copies of any part of this  **
 ** documentation and/or software.				      **
 ***********************************************************************
 */

/*
 ***********************************************************************
 **  Message-digest routines:					      **
 **  To form the message digest for a message M 		      **
 **    (1) Initialize a context buffer md5Ctx using MD5Init	      **
 **    (2) Call MD5Update on md5Ctx and M			      **
 **    (3) Call MD5Final on md5Ctx				      **
 **  The message digest is now in md5Ctx->digest[0...15]	      **
 ***********************************************************************
 */


#define MD5_IO
#include "md5.h"
#include "autoconfig.h"

char *MD5_what="@(#)";	/* #(@) if checked in */

/* almost zero fill */
static BYTE PADDING[MD5_CHUNKSIZE] = {
    0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/*
 * The F, G, H and I are basic MD5 functions.  The following
 * identity saves one boolean operation.
 *
 * F: (((x) & (y)) | (~(x) & (z))) == ((z) ^ ((x) & ((y) ^ (z))))
 * G: (((x) & (z)) | ((y) & ~(z))) == ((y) ^ ((z) & ((x) ^ (y))))
 */
/* F, G, H and I are basic MD5 functions */
#define F(x, y, z) ((z) ^ ((x) & ((y) ^ (z))))
#define G(x, y, z) ((y) ^ ((z) & ((x) ^ (y))))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

/* rotate a 32 bit value */
#define ROT(X,n) (((X)<<(n)) | ((X)>>(32-(n))))

/* FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4 */
/* Rotation is separate from addition to prevent recomputation */
#define S11 7
#define S12 12
#define S13 17
#define S14 22
#define FF(a, b, c, d, x, s, ac) \
    {(a) += F((b), (c), (d)) + (x) + (ULONG)(ac); \
     (a) = ROT((a), (s)); \
     (a) += (b); \
    }
#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define GG(a, b, c, d, x, s, ac) \
    {(a) += G((b), (c), (d)) + (x) + (ULONG)(ac); \
     (a) = ROT((a), (s)); \
     (a) += (b); \
    }
#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define HH(a, b, c, d, x, s, ac) \
    {(a) += H((b), (c), (d)) + (x) + (ULONG)(ac); \
     (a) = ROT((a), (s)); \
     (a) += (b); \
    }
#define S41 6
#define S42 10
#define S43 15
#define S44 21
#define II(a, b, c, d, x, s, ac) \
    {(a) += I((b), (c), (d)) + (x) + (ULONG)(ac); \
     (a) = ROT((a), (s)); \
     (a) += (b); \
    }

/* forward declaration */
static void MD5Transform P((ULONG*, ULONG*));
#if INN_BYTE_ORDER == INN_BIG_ENDIAN
static ULONG in[MD5_CHUNKWORDS];
#endif


/*
 * MD5Init - initiale the message-digest context
 *
 * The routine MD5Init initializes the message-digest context
 * md5Ctx. All fields are set to zero.
 */
void
MD5Init(md5Ctx)
    MD5_CTX *md5Ctx;
{
    /* load magic initialization constants */
    md5Ctx->buf[0] = (ULONG)0x67452301;
    md5Ctx->buf[1] = (ULONG)0xefcdab89;
    md5Ctx->buf[2] = (ULONG)0x98badcfe;
    md5Ctx->buf[3] = (ULONG)0x10325476;

    /* Initialise bit count */
    md5Ctx->countLo = 0L;
    md5Ctx->countHi = 0L;
    md5Ctx->datalen = 0L;
}


/*
 * MD5Update - update message-digest context
 *
 * The routine MD5Update updates the message-digest context to
 * account for the presence of each of the characters inBuf[0..inLen-1]
 * in the message whose digest is being computed.
 */
void
MD5Update(md5Ctx, inBuf, count)
    MD5_CTX *md5Ctx;
    BYTE *inBuf;
    UINT count;
{
    ULONG datalen = md5Ctx->datalen;
#if INN_BYTE_ORDER == INN_BIG_ENDIAN
    ULONG tmp;
    int cnt;
#endif

    /*
     * Catch the case of a non-empty data inBuf
     */
    if (datalen > 0) {

	/* determine the size we need to copy */
	ULONG cpylen = MD5_CHUNKSIZE - datalen;

	/* case: new data will not fill the inBuf */
	if (cpylen > count) {
	    memcpy((char *)(md5Ctx->in_byte)+datalen, (char *)inBuf, count);
	    md5Ctx->datalen = datalen+count;
	    return;

	/* case: inBuf will be filled */
	} else {
	    memcpy((char *)(md5Ctx->in_byte)+datalen, (char *)inBuf, cpylen);
#if INN_BYTE_ORDER == INN_BIG_ENDIAN
	    /* byte swap data into little endian order */
	    for (cnt=0; cnt < MD5_CHUNKWORDS; ++cnt) {
		tmp = (md5Ctx->in_ulong[cnt] << 16) |
			(md5Ctx->in_ulong[cnt] >> 16);
		in[cnt] = ((tmp & 0xFF00FF00L) >> 8) |
			  ((tmp & 0x00FF00FFL) << 8);
	    }
	    MD5Transform(md5Ctx->buf, in);
#else
	    MD5Transform(md5Ctx->buf, md5Ctx->in_ulong);
#endif
	    inBuf += cpylen;
	    count -= cpylen;
	    md5Ctx->datalen = 0;
	}
    }

    /*
     * Process data in MD5_CHUNKSIZE chunks
     */
    if (count >= MD5_CHUNKSIZE) {
        MD5fullUpdate(md5Ctx, inBuf, count);
	inBuf += (count/MD5_CHUNKSIZE)*MD5_CHUNKSIZE;
	count %= MD5_CHUNKSIZE;
    }

    /*
     * Handle any remaining bytes of data.
     * This should only happen once on the final lot of data
     */
    if (count > 0) {
	memcpy(md5Ctx->in_byte, inBuf, count);
    }
    md5Ctx->datalen = count;
}


/*
 * MD5fullUpdate - update message-digest context with a full set of chunks
 *
 * The routine MD5Update updates the message-digest context to
 * account for the presence of each of the characters inBuf[0..inLen-1]
 * in the message whose digest is being computed.
 *
 * Unlike MD5Update, this routine will assume that the input length is
 * a mulitple of MD5_CHUNKSIZE bytes long.
 */
void
MD5fullUpdate(md5Ctx, inBuf, count)
    MD5_CTX *md5Ctx;
    BYTE *inBuf;
    UINT count;
{
#if INN_BYTE_ORDER == INN_BIG_ENDIAN
    ULONG tmp;
    int cnt;
#endif

    /*
     * Process data in MD5_CHUNKSIZE chunks
     */
    while (count >= MD5_CHUNKSIZE) {
#if defined(INN_MUST_ALIGN)
	memcpy(in, inBuf, MD5_CHUNKSIZE);
#if INN_BYTE_ORDER == INN_BIG_ENDIAN
	/* byte swap data into little endian order */
	for (cnt=0; cnt < MD5_CHUNKWORDS; ++cnt) {
	    tmp = (in[cnt] << 16) | (in[cnt] >> 16);
	    in[cnt] = ((tmp & 0xFF00FF00L) >> 8) | ((tmp & 0x00FF00FFL) << 8);
	}
#endif
	MD5Transform(md5Ctx->buf, in);
#else
#if INN_BYTE_ORDER == INN_LITTLE_ENDIAN
	MD5Transform(md5Ctx->buf, (ULONG *)inBuf);
#else
	/* byte swap data into little endian order */
	for (cnt=0; cnt < MD5_CHUNKWORDS; ++cnt) {
	    tmp = (((ULONG *)inBuf)[cnt] << 16) | (((ULONG *)inBuf)[cnt] >> 16);
	    in[cnt] = ((tmp & 0xFF00FF00L) >> 8) | ((tmp & 0x00FF00FFL) << 8);
	}
	MD5Transform(md5Ctx->buf, in);
#endif
#endif
	inBuf += MD5_CHUNKSIZE;
	count -= MD5_CHUNKSIZE;
    }
}


/*
 * MD5Final - terminate the message-digest computation
 *
 * The routine MD5Final terminates the message-digest computation and
 * ends with the desired message digest in md5Ctx->digest[0...15].
 */
void
MD5Final(md5Ctx)
    MD5_CTX *md5Ctx;
{
    UINT padLen;
    int count = md5Ctx->datalen;
    ULONG lowBitcount = md5Ctx->countLo;
    ULONG highBitcount = md5Ctx->countHi;
#if INN_BYTE_ORDER == BIG_ENDIAN
    ULONG tmp;
    UINT i, ii;
#endif

    /* pad out to 56 mod MD5_CHUNKSIZE */
    padLen = (count < 56) ? (56 - count) : (120 - count);
    MD5Update(md5Ctx, PADDING, padLen);

    /* append length in bits and transform */
#if INN_BYTE_ORDER == BIG_ENDIAN
    for (i = 0; i < 14; i++) {
	tmp = (md5Ctx->in_ulong[i] << 16) | (md5Ctx->in_ulong[i] >> 16);
	md5Ctx->in_ulong[i] = ((tmp & 0xFF00FF00L) >> 8) |
			      ((tmp & 0x00FF00FFL) << 8);
    }
#endif
    md5Ctx->in_ulong[MD5_LOW] = (lowBitcount << 3);
    md5Ctx->in_ulong[MD5_HIGH] = (highBitcount << 3) | (lowBitcount >> 29);
    MD5Transform(md5Ctx->buf, md5Ctx->in_ulong);

    /* store buffer in digest */
#if INN_BYTE_ORDER == INN_LITTLE_ENDIAN
    memcpy((char *)md5Ctx->digest, (char *)md5Ctx->buf, sizeof(md5Ctx->digest));
#else
    for (i = 0, ii = 0; i < MD5_DIGESTWORDS; i++, ii += 4) {
	md5Ctx->digest[ii] = (BYTE)(md5Ctx->buf[i] & 0xFF);
	md5Ctx->digest[ii+1] = (BYTE)((md5Ctx->buf[i] >> 8) & 0xFF);
	md5Ctx->digest[ii+2] = (BYTE)((md5Ctx->buf[i] >> 16) & 0xFF);
	md5Ctx->digest[ii+3] = (BYTE)((md5Ctx->buf[i] >> 24) & 0xFF);
    }
#endif
}


/*
 * Basic MD5 step. Transforms buf based on in.
 */
static void
MD5Transform(buf, in)
    ULONG *buf;
    ULONG *in;
{
    ULONG a = buf[0], b = buf[1], c = buf[2], d = buf[3];

    /* Round 1 */
    FF( a, b, c, d, in[ 0], S11, 3614090360UL); /* 1 */
    FF( d, a, b, c, in[ 1], S12, 3905402710UL); /* 2 */
    FF( c, d, a, b, in[ 2], S13,  606105819UL); /* 3 */
    FF( b, c, d, a, in[ 3], S14, 3250441966UL); /* 4 */
    FF( a, b, c, d, in[ 4], S11, 4118548399UL); /* 5 */
    FF( d, a, b, c, in[ 5], S12, 1200080426UL); /* 6 */
    FF( c, d, a, b, in[ 6], S13, 2821735955UL); /* 7 */
    FF( b, c, d, a, in[ 7], S14, 4249261313UL); /* 8 */
    FF( a, b, c, d, in[ 8], S11, 1770035416UL); /* 9 */
    FF( d, a, b, c, in[ 9], S12, 2336552879UL); /* 10 */
    FF( c, d, a, b, in[10], S13, 4294925233UL); /* 11 */
    FF( b, c, d, a, in[11], S14, 2304563134UL); /* 12 */
    FF( a, b, c, d, in[12], S11, 1804603682UL); /* 13 */
    FF( d, a, b, c, in[13], S12, 4254626195UL); /* 14 */
    FF( c, d, a, b, in[14], S13, 2792965006UL); /* 15 */
    FF( b, c, d, a, in[15], S14, 1236535329UL); /* 16 */

    /* Round 2 */
    GG( a, b, c, d, in[ 1], S21, 4129170786UL); /* 17 */
    GG( d, a, b, c, in[ 6], S22, 3225465664UL); /* 18 */
    GG( c, d, a, b, in[11], S23,  643717713UL); /* 19 */
    GG( b, c, d, a, in[ 0], S24, 3921069994UL); /* 20 */
    GG( a, b, c, d, in[ 5], S21, 3593408605UL); /* 21 */
    GG( d, a, b, c, in[10], S22,   38016083UL); /* 22 */
    GG( c, d, a, b, in[15], S23, 3634488961UL); /* 23 */
    GG( b, c, d, a, in[ 4], S24, 3889429448UL); /* 24 */
    GG( a, b, c, d, in[ 9], S21,  568446438UL); /* 25 */
    GG( d, a, b, c, in[14], S22, 3275163606UL); /* 26 */
    GG( c, d, a, b, in[ 3], S23, 4107603335UL); /* 27 */
    GG( b, c, d, a, in[ 8], S24, 1163531501UL); /* 28 */
    GG( a, b, c, d, in[13], S21, 2850285829UL); /* 29 */
    GG( d, a, b, c, in[ 2], S22, 4243563512UL); /* 30 */
    GG( c, d, a, b, in[ 7], S23, 1735328473UL); /* 31 */
    GG( b, c, d, a, in[12], S24, 2368359562UL); /* 32 */

    /* Round 3 */
    HH( a, b, c, d, in[ 5], S31, 4294588738UL); /* 33 */
    HH( d, a, b, c, in[ 8], S32, 2272392833UL); /* 34 */
    HH( c, d, a, b, in[11], S33, 1839030562UL); /* 35 */
    HH( b, c, d, a, in[14], S34, 4259657740UL); /* 36 */
    HH( a, b, c, d, in[ 1], S31, 2763975236UL); /* 37 */
    HH( d, a, b, c, in[ 4], S32, 1272893353UL); /* 38 */
    HH( c, d, a, b, in[ 7], S33, 4139469664UL); /* 39 */
    HH( b, c, d, a, in[10], S34, 3200236656UL); /* 40 */
    HH( a, b, c, d, in[13], S31,  681279174UL); /* 41 */
    HH( d, a, b, c, in[ 0], S32, 3936430074UL); /* 42 */
    HH( c, d, a, b, in[ 3], S33, 3572445317UL); /* 43 */
    HH( b, c, d, a, in[ 6], S34,   76029189UL); /* 44 */
    HH( a, b, c, d, in[ 9], S31, 3654602809UL); /* 45 */
    HH( d, a, b, c, in[12], S32, 3873151461UL); /* 46 */
    HH( c, d, a, b, in[15], S33,  530742520UL); /* 47 */
    HH( b, c, d, a, in[ 2], S34, 3299628645UL); /* 48 */

    /* Round 4 */
    II( a, b, c, d, in[ 0], S41, 4096336452UL); /* 49 */
    II( d, a, b, c, in[ 7], S42, 1126891415UL); /* 50 */
    II( c, d, a, b, in[14], S43, 2878612391UL); /* 51 */
    II( b, c, d, a, in[ 5], S44, 4237533241UL); /* 52 */
    II( a, b, c, d, in[12], S41, 1700485571UL); /* 53 */
    II( d, a, b, c, in[ 3], S42, 2399980690UL); /* 54 */
    II( c, d, a, b, in[10], S43, 4293915773UL); /* 55 */
    II( b, c, d, a, in[ 1], S44, 2240044497UL); /* 56 */
    II( a, b, c, d, in[ 8], S41, 1873313359UL); /* 57 */
    II( d, a, b, c, in[15], S42, 4264355552UL); /* 58 */
    II( c, d, a, b, in[ 6], S43, 2734768916UL); /* 59 */
    II( b, c, d, a, in[13], S44, 1309151649UL); /* 60 */
    II( a, b, c, d, in[ 4], S41, 4149444226UL); /* 61 */
    II( d, a, b, c, in[11], S42, 3174756917UL); /* 62 */
    II( c, d, a, b, in[ 2], S43,  718787259UL); /* 63 */
    II( b, c, d, a, in[ 9], S44, 3951481745UL); /* 64 */

    buf[0] += a;
    buf[1] += b;
    buf[2] += c;
    buf[3] += d;
}

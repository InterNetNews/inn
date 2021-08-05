/*
**  COMPRESS functionality.  RFC 8054.
*/

#include "config.h"
#include "clibrary.h"

#include "inn/messages.h"
#include "nnrpd.h"

#if defined(HAVE_ZLIB)
# define ZBUFSIZE 65536
# define MEM_LEVEL 9
# define WINDOW_BITS (-15)      /* Raw deflate. */
bool compression_layer_on = false;
bool tls_compression_on = false;
z_stream *zstream_in = NULL;
z_stream *zstream_out = NULL;
bool zstream_inflate_needed = false;
bool zstream_flush_needed = false;
unsigned char *zbuf_in = NULL;
unsigned char *zbuf_out = NULL;
size_t zbuf_in_size = NNTP_MAXLEN_COMMAND;  /* Initial size of the input
                                             * buffer.  Can be reallocated. */
size_t zbuf_in_allocated = 0;
size_t zbuf_out_size = ZBUFSIZE;  /* Initial size of the output buffer.
                                   * Can be reallocated, when needed. */

/*
**  Wrappers for our memory management functions.
*/
static voidpf zalloc(voidpf opaque UNUSED, uInt items, uInt size)
{
    return (voidpf) xcalloc(items, size);
}

static void zfree(voidpf opaque UNUSED, voidpf address)
{
    free(address);
}


/*
**  The function called by nnrpd to initialize compression support.  Calls
**  both deflateInit2 and inflateInit2, and then checks the result.
**
**  Returns false on error.
*/
bool
zlib_init(void)
{
    int result;
    zstream_in = (z_stream *) xmalloc(sizeof(z_stream));
    zstream_out = (z_stream *) xmalloc(sizeof(z_stream));

    /* Allocate the buffer for compressed input data given to inflate(). */
    zbuf_in = (unsigned char *) xmalloc(zbuf_in_size);

    /* Allocate the buffer for compressed output data produced by deflate(). */
    zbuf_out = (unsigned char *) xmalloc(zbuf_out_size);

    zstream_in->zalloc = zalloc;
    zstream_in->zfree = zfree;
    zstream_in->opaque = Z_NULL;
    zstream_in->next_in = Z_NULL;
    zstream_in->avail_in = 0;

    zstream_out->zalloc = zalloc;
    zstream_out->zfree = zfree;
    zstream_out->opaque = Z_NULL;

    result = inflateInit2(zstream_in, WINDOW_BITS);

    if (result != Z_OK) {
        syslog(L_NOTICE, "inflateInit2() failed with error %d", result);
        free(zstream_in);
        free(zstream_out);
        free(zbuf_in);
        free(zbuf_out);
        return false;
    }

    result = deflateInit2(zstream_out, Z_BEST_COMPRESSION, Z_DEFLATED,
                          WINDOW_BITS, MEM_LEVEL, Z_DEFAULT_STRATEGY);

    if (result != Z_OK) {
        syslog(L_NOTICE, "deflateInit2() failed with error %d", result);
        inflateEnd(zstream_in);
        free(zstream_in);
        free(zstream_out);
        free(zbuf_in);
        free(zbuf_out);
        return false;
    }

    return true;
}

#endif /* HAVE_ZLIB */

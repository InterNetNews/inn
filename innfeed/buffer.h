/*  $Id$
**
**  The public interface to the Buffer class.
**
**  Written by James Brister <brister@vix.com>
**
**  The Buffer class encapsulates a region of memory. It provides reference
**  counting so that redundant memory allocation and copying is minimized. A
**  good example of this is the need to write the same data (e.g. an article
**  body) out more than one socket. The data is stored in a Buffer and the
**  Buffer is given to each EndPoint that is to write it. With the Refcount
**  appropriately set the EndPoints can release their references at will and
**  the Buffer will be cleaned up when necessary.
*/

#if ! defined ( buffer_h__ )
#define buffer_h__


#include <sys/types.h>
#include <stdio.h>

#include "misc.h"


/*
 * Create a new Buffer object and initialize it.
 */
Buffer newBuffer (size_t size) ;

/* Create a new Buffer object around the preallocted PTR, which is SIZE
   bytes long. The data size of the Buffer is set to DATASIZE. When then
   buffer is released it will not delete the ptr (this is useful to have
   Buffers around contant strings, or Buffers around other Buffers) */
Buffer newBufferByCharP (const char *ptr, size_t size, size_t dataSize) ;

/*
 * give up interest in the Buffer. Decrement refcount and delete if no
 * more referants
 */
void delBuffer (Buffer buff) ;

  /* print some debugging information about the buffer. */
void printBufferInfo (Buffer buffer, FILE *fp, unsigned int indentAmt) ;

  /* print debugging information about all outstanding buffers. */
void gPrintBufferInfo (FILE *fp, unsigned int indentAmt) ;

/* increment reference counts so that the buffer object can be */
/* handed off to another function without it being deleted when that */
/* function calls bufferDelete(). Returns buff, so the call can be */
/* used in function arg list. */
Buffer bufferTakeRef (Buffer buff) ;

/* returns the address of the base of the memory owned by the Buffer */
void *bufferBase (Buffer buff) ;

/* returns the size of the memory buffer has available. This always returns
   1 less than the real size so that there's space left for a null byte
   when needed. The extra byte is accounted for when the newBuffer function
   is called. */
size_t bufferSize (Buffer) ;

/* return the amount of data actually in the buffer */
size_t bufferDataSize (Buffer buff) ;

/* increment the size of the buffer's data region */
void bufferIncrDataSize (Buffer buff, size_t size) ;

/* decrement the size of the buffer's data region */
void bufferDecrDataSize (Buffer buff, size_t size) ;

/* set the size of the data actually in the buffer */
void bufferSetDataSize (Buffer buff, size_t size) ;

/* walk down the BUFFS releasing each buffer and then freeing BUFFS itself */
void freeBufferArray (Buffer *buffs) ;

/* All arguments are non-NULL Buffers, except for the last which must be
   NULL. Creates a free'able array and puts all the buffers into it (does
   not call bufferTakeRef on them). */
Buffer *makeBufferArray (Buffer buff, ...) ;

/* returns true if the buffer was created via
   newBuffer (vs. newBufferByCharP) */
bool isDeletable (Buffer buff) ;

/* Dups the array including taking out references on the Buffers
   inside. Return value must be freed (or given to freeBufferArray) */
Buffer *dupBufferArray (Buffer *array) ;

/* return the number of non-NULL elements in the array. */
unsigned int bufferArrayLen (Buffer *array) ;

/* copies the contents of the SRC buffer into the DEST buffer and sets the
   data size appropriately. Returns false if src is bigger than dest. */
bool copyBuffer (Buffer dest, Buffer src) ;

/* return the ref count on the buffer */
unsigned int bufferRefCount (Buffer buff) ;

/* insert a null byte at the end of the data region */
void bufferAddNullByte (Buffer buff) ;

/* append the data of src to the data of dest, if dest is big enough */
bool concatBuffer (Buffer dest, Buffer src) ;

/* expand the buffer's memory by the given AMT */
bool expandBuffer (Buffer buff, size_t amt) ;

/* Expand the buffer (if necessary) and insert carriage returns before very
   line feed. Adjusts the data size. The base address may change
   afterwards. */
bool nntpPrepareBuffer (Buffer buffer) ;

#endif /* buffer_h__ */

/*  $Id$
**
**  The Buffer class for innfeed.
**
**  Written by James Brister <brister@vix.com>
**
**  The implementation of the Buffer class.  Buffers are reference counted
**  objects that abstract memory regions in a way similar to struct iovec.
*/

#include "innfeed.h"
#include "config.h"
#include "clibrary.h"
#include <assert.h>

#include "inn/messages.h"
#include "libinn.h"

#include "buffer.h"


static Buffer gBufferList = NULL ;
static Buffer bufferPool = NULL ;
static unsigned int bufferCount = 0 ;
static unsigned int bufferByteCount = 0 ;


struct buffer_s 
{
    int refCount ;
    char *mem ;
    size_t memSize ;            /* the length of mem */
    size_t dataSize ;           /* amount that has actual data in it. */
    bool deletable ;
    struct buffer_s *next ;
    struct buffer_s *prev ;
};

#define BUFFER_POOL_SIZE ((4096 - 2 * (sizeof (void *))) / (sizeof (struct buffer_s)))

static void fillBufferPool (void)
{
  unsigned int i ;

  bufferPool = xmalloc (sizeof(struct buffer_s) * BUFFER_POOL_SIZE) ;

  for (i = 0; i < BUFFER_POOL_SIZE - 1; i++)
    bufferPool[i] . next = &(bufferPool [i + 1]) ;
  bufferPool [BUFFER_POOL_SIZE-1] . next = NULL ;
}


Buffer newBuffer (size_t size)
{
  Buffer nb ;

  if (bufferPool == NULL)
    fillBufferPool() ;

  nb = bufferPool;
  ASSERT (nb != NULL) ;
  bufferPool = bufferPool->next ;

  nb->refCount = 1 ;

  nb->mem = xmalloc (size + 1) ;
  
  nb->mem [size] = '\0' ;
  nb->memSize = size ;
  nb->dataSize = 0 ;
  nb->deletable = true ;

  bufferByteCount += size + 1 ;
  bufferCount++ ;

  nb->next = gBufferList ;
  nb->prev = NULL;
  if (gBufferList != NULL)
     gBufferList->prev = nb ;
  gBufferList = nb ;
  
#if 0
  d_printf (1,"Creating a DELETABLE buffer %p\n",nb) ;
#endif

  return nb ;
}


Buffer newBufferByCharP (const char *ptr, size_t size, size_t dataSize)
{
  Buffer nb ;

  if (bufferPool == NULL)
    fillBufferPool() ;

  nb = bufferPool;
  ASSERT (nb != NULL) ;
  bufferPool = bufferPool->next ;
  
  nb->refCount = 1 ;
  nb->mem = (char *) ptr ;      /* cast away const */
  nb->memSize = size ;
  nb->dataSize = dataSize ;
  nb->deletable = false ;

  nb->next = gBufferList ;
  nb->prev = NULL;
  if (gBufferList != NULL)
     gBufferList->prev = nb ;
  gBufferList = nb ;

  bufferCount++ ;
#if 0
  d_printf (1,"Creating a NON-DELETABLE buffer %p\n",nb) ;
#endif
  
  return nb ;
}


void delBuffer (Buffer buff) 
{
  if (buff != NULL && --(buff->refCount) == 0)
    {
#if 0
      d_printf (1,"Freeing a %s buffer (%p)\n",
               (buff->deletable ? "DELETABLE" : "NON-DELETABLE"), buff) ;
#endif

      bufferCount-- ;
      if (buff->deletable)
        {
          bufferByteCount -= (buff->memSize + 1) ;
          free (buff->mem) ;
          buff->mem = NULL ;
        }

      if (buff->next != NULL)
        buff->next->prev = buff->prev ;
      if (buff->prev != NULL)
        buff->prev->next = buff->next ;
      else
        {
          ASSERT(gBufferList == buff) ;
          gBufferList = buff->next ;
        }

      buff->next = bufferPool ;
      bufferPool = buff ;
    }
}

Buffer bufferTakeRef (Buffer buff) 
{
  ASSERT (buff != NULL) ;
  
  if (buff != NULL)
    buff->refCount++ ;

  return buff ;
}


void gPrintBufferInfo (FILE *fp, unsigned int indentAmt)
{
  Buffer b ;
  char indent [INDENT_BUFFER_SIZE] ;
  unsigned int i ;

  for (i = 0 ; i < MIN(INDENT_BUFFER_SIZE - 1,indentAmt) ; i++)
    indent [i] = ' ' ;
  indent [i] = '\0' ;
  
  fprintf (fp,"%sGlobal Buffer List : (count %d) {\n",indent,bufferCount) ;
  
  for (b = gBufferList ; b != NULL ; b = b->next)
    printBufferInfo (b,fp,indentAmt + INDENT_INCR) ;

  fprintf (fp,"%s}\n",indent) ;
}

void printBufferInfo (Buffer buffer, FILE *fp, unsigned int indentAmt)
{
  char indent [INDENT_BUFFER_SIZE] ;
  char bufferStart [256] ;
  unsigned int i ;

  for (i = 0 ; i < MIN(INDENT_BUFFER_SIZE - 1,indentAmt) ; i++)
    indent [i] = ' ' ;
  indent [i] = '\0' ;
  
  fprintf (fp,"%sBuffer : %p {\n",indent,(void *) buffer) ;

  if (buffer == NULL)
    {
      fprintf (fp,"%s}\n",indent) ;
      return ;
    }

  i = MIN(sizeof (bufferStart) - 1,buffer->dataSize) ;
  memcpy (bufferStart,buffer->mem,i) ;
  bufferStart[i] = '\0';
  
  fprintf (fp,"%s    refcount : %d\n",indent,buffer->refCount) ;
  fprintf (fp,"%s    data-size : %ld\n",indent,(long) buffer->dataSize) ;
  fprintf (fp,"%s    mem-size : %ld\n",indent,(long) buffer->memSize) ;
  fprintf (fp,"%s    base : %p\n", indent,(void *) buffer->mem) ;
  fprintf (fp,"%s    deletable : %s\n",indent,boolToString(buffer->deletable));
  fprintf (fp,"%s    buffer [0:%ld] : \"%s\"\n",
           indent, (long) i, bufferStart) ;
  fprintf (fp,"%s}\n",indent) ;
}


void *bufferBase (Buffer buff) 
{
  return buff->mem ;
}

size_t bufferSize (Buffer buff) 
{
  return buff->memSize ;
}

size_t bufferDataSize (Buffer buff) 
{
  return buff->dataSize ;
}

void bufferIncrDataSize (Buffer buff, size_t size) 
{
  if (buff->dataSize + size > buff->memSize)
    die ("Trying to make a buffer data size bigger than its memory alloc");
  else
    buff->dataSize += size ;
}

void bufferDecrDataSize (Buffer buff, size_t size) 
{
  ASSERT (size > buff->dataSize) ;
  
  buff->dataSize -= size ;
}

void bufferSetDataSize (Buffer buff, size_t size) 
{
  buff->dataSize = size ;

  ASSERT (buff->dataSize <= buff->memSize) ;
}


void freeBufferArray (Buffer *buffs)
{
  Buffer *b = buffs ;
  
  while (b && *b)
    {
      delBuffer (*b) ;
      b++ ;
    }

  if (buffs)
    free (buffs) ;
}


  /* Allocate an array and put all the arguments (the last of which must be
     NULL) into it. The terminating NULL is put in the returned array. */
Buffer *makeBufferArray (Buffer buff, ...)
{
  va_list ap ;
  size_t cLen = 10, idx = 0 ;
  Buffer *ptr, p ;

  ptr = xcalloc (cLen, sizeof(Buffer)) ;

  ptr [idx++] = buff ;

  va_start (ap, buff) ;
  do
    {
      p = va_arg (ap, Buffer) ;
      if (idx == cLen)
        {
          cLen += 10 ;
          ptr = xrealloc (ptr, sizeof(Buffer) * cLen) ;
        }
      ptr [idx++] = p ;
    }
  while (p != NULL) ;
  va_end (ap) ;

  return ptr ;
}


bool isDeletable (Buffer buff)
{
  return buff->deletable ;
}



  /* Dups the array including taking out references on the Buffers inside */
Buffer *dupBufferArray (Buffer *array)
{
  Buffer *newArr ;
  int count = 0 ;

  while (array && array [count] != NULL)
    count++ ;

  newArr = xmalloc (sizeof(Buffer) * (count + 1)) ;

  for (count = 0 ; array [count] != NULL ; count++)
    newArr [count] = bufferTakeRef (array [count]) ;

  newArr [count] = NULL ;

  return newArr ;
}


unsigned int bufferArrayLen (Buffer *array)
{
  unsigned int count = 0 ;

  if (array != NULL)
    while (*array != NULL)
      {
        count++ ;
        array++ ;
      }

  return count ;
}


bool copyBuffer (Buffer dest, Buffer src)
{
  char *baseDest = bufferBase (dest) ;
  char *baseSrc = bufferBase (src) ;
  unsigned int amt = bufferDataSize (src) ;

  if (amt > bufferSize (dest))
    return false ;

  memcpy (baseDest, baseSrc, amt) ;

  bufferSetDataSize (dest,amt) ;

  return true ;
}


unsigned int bufferRefCount (Buffer buf)
{
  return buf->refCount ;
}


void bufferAddNullByte (Buffer buff) 
{
  char *p = bufferBase (buff) ;

  p [buff->dataSize] = '\0' ;
}


  /* append the src buffer to the dest buffer growing the dest as needed.
     Can only be done to deletable buffers. */
bool concatBuffer (Buffer dest, Buffer src)
{
  ASSERT (dest->deletable) ;

  if ( !dest->deletable )
    return false ;              /* yeah, i know this is taken care of above */
      
  if ((dest->dataSize + src->dataSize) > dest->memSize)
    {
      char *newMem = xcalloc (dest->dataSize + src->dataSize + 1, 1) ;

      bufferByteCount += ((dest->dataSize + src->dataSize) - dest->memSize) ;
      
      memcpy (newMem, dest->mem, dest->dataSize) ;

      ASSERT (dest->mem != NULL) ;
      free (dest->mem) ;

      dest->mem = newMem ;
      dest->memSize = dest->dataSize + src->dataSize ; /* yep. 1 less */
    }

  memcpy (&dest->mem[dest->dataSize], src->mem, dest->dataSize) ;

  dest->dataSize += src->dataSize ;

  return true ;
}


  /* realloc the buffer's memory to increase the size by AMT */
bool expandBuffer (Buffer buff, size_t amt)
{
  d_printf (2,"Expanding buffer....\n") ;
  
  if (!buff->deletable)
    return false ;

  bufferByteCount += amt ;
  buff->memSize += amt ;

  buff->mem = xrealloc (buff->mem, buff->memSize + 1) ;

  return true ;
}


  /* Take a buffer and shift the contents around to add the necessary CR
     before every line feed and a '.' before every '.' at the start of a
     line. */
bool nntpPrepareBuffer (Buffer buffer)
{
  int msize, newDsize, dsize, extra ;
  char *base, p, *src, *dst ;
  bool needfinal = false ;

  ASSERT (buffer != NULL) ;

  dsize = buffer->dataSize ;
  msize = buffer->memSize - 1 ;
  base = buffer->mem ;

  extra = 3 ;
  p = '\0' ;
  for (src = base + dsize - 1 ; src > base ; )
    {
      if (*src == '\n')
        {
          extra++ ;
          if (p == '.')
            extra++ ;
        }
      p = *src-- ;
    }
  if (*src == '\n')
    {
      extra++ ;
      if (p == '.')
        extra++ ;
    }

  if (dsize > 0 && base [dsize - 1] != '\n')
    {
      needfinal = true ;
      extra += 2 ;
    }
    
  newDsize = dsize + extra ;
  
  if (msize - dsize < extra)
    {
      d_printf (2,"Expanding buffer in nntpPrepareBuffer (from %d to %d)\n",
               msize, msize + (extra - (msize - dsize))) ;

      if ( !expandBuffer (buffer, extra - (msize - dsize)) )
        {
          d_printf (1,"Expand failed...\n") ;
          return false ;
        }
      
      ASSERT (dsize == (int) buffer->dataSize) ;

      base = buffer->mem ;
    }

  base [newDsize] = '\0' ;
  base [newDsize - 1] = '\n' ;
  base [newDsize - 2] = '\r' ;
  base [newDsize - 3] = '.' ;
  newDsize -= 3 ;
  extra -= 3 ;
  
  if (needfinal)
    {
      base [newDsize - 1] = '\n' ;
      base [newDsize - 2] = '\r' ;
      newDsize -= 2 ;
      extra -= 2 ;
    }
  
  if (extra)
    {
      p = '\0';
      src = base + dsize - 1 ;
      dst = base + newDsize - 1 ;
      while (1)
        {
          if (*src == '\n')
            {
              if (p == '.')
                {
                  *dst-- = '.' ;
                  extra-- ;
                }
              *dst-- = '\n' ;
              *dst = '\r' ;
              if (--extra <= 0)
                 break ;
              p = '\0' ;
              dst-- ;
              src-- ;
            }
          else
            p = *dst-- = *src-- ;
        }
      ASSERT(dst >= base && src >= base) ; 
    }

  newDsize += 3;
  if (needfinal)
    newDsize += 2 ;
  
  bufferSetDataSize (buffer,newDsize) ;
  
  return true ;
}

/*  $Id$
**
**  Here be some useful macros.
*/

#ifndef __MACROS_H__
#define __MACROS_H__

/* We've probably already included this; only include it if we need it. */
#ifndef __CONFIG_H__
# include "config.h"
#endif

/* Memory allocation.  Wrappers around wrapper functions.  Don't replace any
   existing definitions, for use with malloc-debug packages, e.g. */
#ifdef _DEBUG_MALLOC_INC
# undef _DEBUG_MALLOC_INC
# include "malloc.h"
#else
# define malloc_enter(func)
# define malloc_leave(func)
# define malloc_chain_check()
# define malloc_dump(fd)
# define malloc_list(a,b,c)
# define malloc_size(hist)	(*(hist) = 0, 0)
#endif

/* Memory allocation macros. */
#define NEW(T, c)               xmalloc(sizeof(T) * (c), __FILE__, __LINE__)
#define COPY(p)                 xstrdup(p, __FILE__, __LINE__)
#define DISPOSE(p)              free(p)
#define RENEW(p, T, c)	\
    (p = xrealloc((p), sizeof(T) * (c), __FILE__, __LINE__))

#define ONALLOCFAIL(func)       (xmemfailure = (func))

/* Wrappers around str[n]cmp.  Don't add the ((a) == (b)) test here; it's
   already been done in places where it's time-critical. */
#define EQ(a, b)		(strcmp((a), (b)) == 0)
#define EQn(a, b, n)		(strncmp((a), (b), (n)) == 0)
#define caseEQ(a, b)		(strcasecmp((a), (b)) == 0)
#define caseEQn(a, b, n)	(strncasecmp((a), (b), (n)) == 0)

/* <ctype.h> usually includes \n, which is not what we want. */
#define ISWHITE(c)              ((c) == ' ' || (c) == '\t')

/* Get the number of elements in a fixed-size array, or a pointer just past
   the end of it. */
#define SIZEOF(array)           (sizeof array / sizeof array[0])
#define ENDOF(array)            (&array[SIZEOF(array)])

/* Get the length of a string constant. */
#define STRLEN(string)          (sizeof string - 1)

/* Turn a TIMEINFO into a floating point number. */
#define TIMEINFOasDOUBLE(t)	\
    ((double)(t).time + ((double)(t).usec) / 1000000.0)

/* Use a read or recv call to read a descriptor. */
#ifdef HAVE_UNIX_DOMAIN_SOCKETS
# define RECVorREAD(fd, p, s)   recv((fd), (p), (s), 0)
#else
# define RECVorREAD(fd, p, s)   read((fd), (p), (s))
#endif

#endif /* __MACROS_H__ */

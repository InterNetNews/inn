/*  $Revision$
**
**  Here be some useful macros.
*/

#ifdef __cplusplus
extern "C" {
#endif

/*
**  Memory allocation.  Wrappers around wrapper functions.
**  Don't replace any existing definitions, for use with malloc-debug
**  packages, e.g.
*/
#if	defined(_DEBUG_MALLOC_INC)
#undef _DEBUG_MALLOC_INC
#include "malloc.h"
#else
#define malloc_enter(func)
#define malloc_leave(func)
#define malloc_chain_check()
#define malloc_dump(fd)
#define malloc_list(a,b,c)
#define malloc_size(hist)	(*(hist) = 0, 0)
#endif	/* defined(_DEBUG_MALLOC_INC) */

#if	!defined(NEW)

#define NEW(T, c)			\
	((T *)xmalloc((unsigned int)(sizeof (T) * (c))))

#define RENEW(p, T, c)			\
        (p = (T *)xrealloc((char *)(p), (unsigned int)(sizeof (T) * (c))))

#define DISPOSE(p)		free(p)

    /* This properly belongs in libinn.h. */
extern int (*xmemfailure)();
#define ONALLOCFAIL(func)		(xmemfailure = (func))

#endif	/* !defined(NEW) */


/*
**  Copy a string to allocated memory.
*/
#define COPY(p)				\
	strcpy(NEW(char, strlen(p) + 1), p)


/*
**  Wrappers around str[n]cmp.  Don't add the ((a) == (b)) test here; it's
**  already been done in places where it's time-critical.
*/
#define EQ(a, b)		(strcmp((a), (b)) == 0)
#define EQn(a, b, n)		(strncmp((a), (b), (SIZE_T)(n)) == 0)
#define caseEQ(a, b)		(strcasecmp((a), (b)) == 0)
#define caseEQn(a, b, n)	(strncasecmp((a), (b), (SIZE_T)(n)) == 0)


/*
**  Cast a pointer into another point, but keep lint quiet.
*/
#if	!defined(lint)
#define CAST(t, p)	((t)(p))
#else
#define CAST(t, p)	((p) ? (t)NULL : (t)NULL)
#endif /* !defined(lint) */


/*
**  <ctype.h> usually includes \n, which is not what we want.
*/
#define ISWHITE(c)			((c) == ' ' || (c) == '\t')


/*
**  Get the number of elements in a fixed-size array, or a pointer just
**  past the end of it.
*/
#define SIZEOF(array)	((int)(sizeof array / sizeof array[0]))
#define ENDOF(array)	(&array[SIZEOF(array)])


/*
**  Get the length of a string constant.
*/
#define STRLEN(string)	((int)(sizeof string - 1))


/*
**  Turn a TIMEINFO into a floating point number.
*/
#define TIMEINFOasDOUBLE(t)	\
    ((double)(t).time + ((double)(t).usec) / 1000000.0)


/*
**  Use a read or recv call to read a descriptor.
*/
#if	defined(HAVE_UNIX_DOMAIN_SOCKETS)
#define RECVorREAD(fd, p, s)	recv((fd), (p), (s), 0)
#else
#define RECVorREAD(fd, p, s)	read((fd), (p), (s))
#endif	/* defined(HAVE_UNIX_DOMAIN_SOCKETS) */

#ifdef __cplusplus
}
#endif /* __cplusplus */

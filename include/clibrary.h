/*  $Revision$
**
**  Here be declarations of routines and variables in the C library.
**  You must #include <sys/types.h> and <stdio.h> before this file.
*/

#if defined(HAVE_UNISTD_H)
# include <unistd.h>
#endif	/* defined(HAVE_UNISTD_H) */

#if defined(HAVE_VFORK_H)
# include <vfork.h>
#endif	/* defined(HAVE_VFORK_H) */

#if ! defined (DO_NEED_BOOL) && ! defined (DONT_NEED_BOOL)
#define DO_NEED_BOOL 1
#endif

#include <sys/time.h>

    /* Generic pointer, used by memcpy, malloc, etc. */
    /* =()<typedef @<POINTER>@ *POINTER;>()= */
typedef void *POINTER;
    /* Const generic pointer, used by qsort. */
    /* =()<typedef const @<POINTER>@ *CPOINTER;>()= */
typedef const void *CPOINTER;
    /* What is a file offset?  Will not work unless long! */
    /* =()<typedef @<OFFSET_T>@ OFFSET_T;>()= */
typedef long OFFSET_T;
    /* What is the type of an object size? */
    /* =()<typedef @<SIZE_T>@ SIZE_T;>()= */
typedef size_t SIZE_T;
    /* What is the type of a passwd uid and gid, for use in chown(2)? */
    /* =()<typedef @<UID_T>@ UID_T;>()= */
typedef uid_t UID_T;
    /* =()<typedef @<GID_T>@ GID_T;>()= */
typedef gid_t GID_T;
    /* =()<typedef @<PID_T>@ PID_T;>()= */
typedef pid_t PID_T;
    /* =()<typedef @<U_INT32_T>@ U_INT32_T;>()= */
typedef unsigned int U_INT32_T;
    /* =()<typedef @<INT32_T>@ INT32_T;>()= */
typedef int INT32_T;

#include <signal.h>
    /* What should a signal handler return? */
    /* =()<#define SIGHANDLER	@<SIGHANDLER>@>()= */
#define SIGHANDLER	void

#if	defined(SIG_DFL)
    /* What types of variables can be modified in a signal handler? */
    /* =()<typedef @<SIGVAR>@ SIGVAR;>()= */
typedef sig_atomic_t SIGVAR;
#endif	/* defined(SIG_DFL) */

#ifdef HAVE_STRING_H
# include <string.h>
#else
# include "mystring.h"
#endif

#ifdef HAVE_MEMORY_H
# include <memory.h>
#else
# include "mymemory.h"
#endif

#include <stdlib.h>


/*
**  It's a pity we have to go through these contortions, for broken
**  systems that have fd_set but not the FD_SET.
*/
#if	defined(FD_SETSIZE)
#define FDSET		fd_set
#else
#include <sys/param.h>
#if	!defined(NOFILE)
	error -- #define NOFILE to the number of files allowed on your machine!
#endif	/* !defined(NOFILE) */
#if	!defined(howmany)
#define howmany(x, y)	(((x) + ((y) - 1)) / (y))
#endif	/* !defined(howmany) */
#define FD_SETSIZE	NOFILE
#define NFDBITS		(sizeof (long) * 8)
typedef struct _FDSET {
    long	fds_bits[howmany(FD_SETSIZE, NFDBITS)];
} FDSET;
#define FD_SET(n, p)	(p)->fds_bits[(n) / NFDBITS] |= (1 << ((n) % NFDBITS))
#define FD_CLR(n, p)	(p)->fds_bits[(n) / NFDBITS] &= ~(1 << ((n) % NFDBITS))
#define FD_ISSET(n, p)	((p)->fds_bits[(n) / NFDBITS] & (1 << ((n) % NFDBITS)))
#define FD_ZERO(p)	(void)memset((POINTER)(p), 0, sizeof *(p))
#endif	/* defined(FD_SETSIZE) */


#if	!defined(SEEK_SET)
#define SEEK_SET	0
#endif	/* !defined(SEEK_SET) */
#if	!defined(SEEK_END)
#define SEEK_END	2
#endif	/* !defined(SEEK_END) */

/*
**  We must use #define to set FREEVAL, since "typedef void FREEVAL;" doesn't
**  work on some broken compilers, sigh.
*/
/* =()<#define FREEVAL @<FREEVAL>@>()= */
#define FREEVAL void

extern int		optind;
extern char		*optarg;
#if	!defined(__STDC__)
extern int		errno;
#endif	/* !defined(__STDC__) */

extern char		*crypt();
extern char		*getenv();
extern char		*inet_ntoa();
extern char		*mktemp();
#if	!defined(strerror)
extern char		*strerror();
#endif	/* !defined(strerror) */
extern long		atol();
extern time_t		time();
extern unsigned long	inet_addr();
extern FREEVAL		free();
extern POINTER		malloc();
extern POINTER		realloc();

/* =()<typedef @<MMAP_PTR>@ MMAP_PTR;>()= */
typedef caddr_t MMAP_PTR;
#ifdef MAP_FILE
#define MAP__ARG        (MAP_FILE | MAP_SHARED)
#else
#define MAP__ARG         (MAP_SHARED)
#endif

/* Some backward systems need this. */
extern FILE	*popen();

/* This is in <mystring.h>, but not in some system string headers,
 * so we put it here just in case. */
extern int	strncasecmp();

/* =()<extern @<ABORTVAL>@	abort();>()= */
extern void	abort();
/* =()<extern @<ALARMVAL>@	alarm();>()= */
extern unsigned int	alarm();
/* =()<extern @<EXITVAL>@	exit();>()= */
extern void	exit();
/* =()<extern @<GETPIDVAL>@	getpid();>()= */
extern pid_t	getpid();
/* =()<extern @<LSEEKVAL>@	lseek();>()= */
extern off_t	lseek();
/* =()<extern @<QSORTVAL>@	qsort();>()= */
extern void	qsort();
/* =()<extern @<SLEEPVAL>@	sleep();>()= */
extern unsigned int	sleep();
/* =()<extern @<_EXITVAL>@	_exit();>()= */
extern void	_exit();

#if defined(HAVE_SETPROCTITLE)
extern void	setproctitle();
#endif	/* defined(HAVE_SETPROCTITLE) */

#ifdef __FreeBSD__
#include <osreldate.h>
#endif

#if defined (sun) && ! defined (__SVR4)
#define atexit(arg) on_exit (arg,0)
#define strtoul(a,b,c) strtol (a,b,c)
#endif

#if defined(UIO_MAXIOV) && !defined(IOV_MAX)
#define IOV_MAX		UIO_MAXIOV
#endif

#ifndef IOV_MAX
/* Solaris */
#if defined(sun) && defined(__SVR4)
#define IOV_MAX 16
#endif
/* FreeBSD 3.0 or above */
#if defined(__FreeBSD__) && (__FreeBSD_version >= 222000)
#define	IOV_MAX	1024
#endif

#if defined(__bsdi__)
#define IOV_MAX 1024
#endif

#if defined(_nec_ews)
#define IOV_MAX 16
#endif

#if (defined(HPUX) || defined(__hpux__)) && !defined(IOV_MAX)
#define IOV_MAX 16
#endif

#if defined(sgi) && !defined(IOV_MAX)
#define IOV_MAX 16
#endif

#if ! defined(IOV_MAX)
#define IOV_MAX 16		/* 16 is the lowest value known for any OS */
#endif

#endif

#if ! defined(INADDR_NONE)
#define		INADDR_NONE 	(unsigned long)0xffffffff
#endif

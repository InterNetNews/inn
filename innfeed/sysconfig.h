/* -*- c -*-
 *
 * Author:      James A. Brister <brister@vix.com> -- berkeley-unix --
 * Start Date:  Thu, 01 Feb 1996 22:17:51 +1100
 * Project:     INN -- innfeed
 * File:        sysconfig.h
 * RCSId:       $Id$
 *
 * Copyright:   Copyright (c) 1996 by Internet Software Consortium
 *
 *              Permission to use, copy, modify, and distribute this
 *              software for any purpose with or without fee is hereby
 *              granted, provided that the above copyright notice and this
 *              permission notice appear in all copies.
 *
 *              THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE
 *              CONSORTIUM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 *              SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *              MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET
 *              SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 *              INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *              WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 *              WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 *              TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
 *              USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Description: This file should be the only thing you need to touch when
 *              moving innfeed across platforms.
 *
 *              This file is broken into two sections. The first contains
 *              all defines etc. that are private to innfeed. The second
 *              contains defines etc. that identical to those in inn. By
 *              compiling with '-DUSE_INN_INCLUDES' and an appropriate '-I'
 *              you won't actually need to touch this second section.
 * 
 */

/* PLEASE FORWARD ANY CHANGES YOU HAVE TO MAKE TO <brister@vix.com> AND LET
   ME KNOW WHAT, WHY AND FOR WHAT PLATFORM. THANKS. */

#ifndef sysconfig_h__
#define sysconfig_h__ 1

/***************************************************************************
 **                      INNFEED PRIVATE SECTION                          **
 ***************************************************************************/

/*
** BSD/OS
*/

#if defined (__bsdi__)
#define MAX_WRITEV_VEC 1024
#define HAVE_MMAP
#endif

/*
** SCO ODT 5.0.4
*/
#if defined (_SCO_DS)
#define MAX_WRITEV_VEC 512
#define MAXPATHLEN 1024
extern int h_errno ;
#endif

/*
** FreeBSD
*/

#if defined (__FreeBSD__)
#define MAX_WRITEV_VEC 1024
#define HAVE_MMAP
#endif

/*
** NetBSD
**
*/

#if defined (__NetBSD__)
#define MAX_WRITEV_VEC 1024
#define HAVE_MMAP
#endif


/*
** LINUX
*/

#if defined (linux)

/* Note: If you are running version 5.4.3 or better of libc, then 16 is the
   number to use. Lower than that, and you should use 1. */
#define MAX_WRITEV_VEC 1
#undef DO_NEED_SYS_SELECT 
#define HAVE_MMAP 1
#define USE_SIGACTION
#endif


/*
** DEC Unix
**
*/

#if defined (__osf__)
#define GETSOCKOPT_ARG  char *
#define MAX_WRITEV_VEC  1024
#endif


/*
** SOLARIS
**
*/

#if defined (sun) && defined (__SVR4)
/* #define DO_NEED_STRERROR 1 */  /* this is needed for version < 2.5 */
#define GETSOCKOPT_ARG  char *
#define MAX_WRITEV_VEC  16
#define DO_NEED_STREAM 1
#define HAVE_MMAP
#define MAX_STDIO_FD 256

 /* this may be needed for solaris version < 2.6 */
/* #define wait3(a,b,c) waitpid(-1,a,b) */

#endif /* defined (S) */


/*
** SunOS 4.x
*/

#if defined (sun) && ! defined (__SVR4)
#define MAX_WRITEV_VEC 16
#define DO_NEED_STRERROR 1
#define HAVE_MMAP
#define MAX_STDIO_FD 128
#define atexit(arg) on_exit (arg,0)
#endif


/*
** IRIX
**
*/

#if defined (__sgi)
#define MAX_WRITEV_VEC 16    /* actually bigger on 5.2, but this on 5.3 and 6.x */
#define HAVE_MMAP
#endif


/*
** NEC UX/4800
**
*/
#if defined (_nec_ews)
#define DO_NEED_STRDUP 1
#define MAX_WRITEV_VEC 16
#define DO_NEED_STREAM 1
#define NO_SBRK 1
#endif


/*
** AIX 3.2.x, 4.1.3
** 
** NOTE FOR 3.2.x. Be careful ! GCC ONLY !!  If you compile
** innfeed 0.9.1 with AIX cc, machine will be rebooted during building
** connection.o. You can't believe it but I tested it with two different
** AIX 3.2.5 boxes 10 times !! :)
** 1996/11/10, seokchan lee <chan@plaza.snu.ac.kr>
*/

#if defined (_AIX32)
#define DO_NEED_STREAM 1
#undef HAVE_UNION_WAIT 
#define FDCOUNT_GETDTAB 1
#define MAX_WRITEV_VEC 16
#define UIO_MAXIOV 16
#define GETSOCKOPT_ARG  char *
/* If UIO_MAXIOV not defined, AIX 4.1.x defines UIO_MAXIOV as 1024. (see
   /usr/include/sys/socket.h). But "make check-maxiov" says it should
   be 16 and it works. There is no UIO_MAXIOV predefines for 3.2.x */
#define LOG_PERROR 0
#define _BSD 44
#endif

#if ! defined (USE_SIGSET) && ! defined(USE_SIGVEC)
#define USE_SIGACTION
#endif



  /* Defaults below here. If you need to change something it should really
     be done in the architecture specific section just above. */

/* If you compiler doesn't support `volatile' then add a line like
              #define VOLATILE
   above */
#if ! defined (VOLATILE)
#define VOLATILE volatile
#endif


/* Some broken system (all SunOS versions) have a lower limit for the
   maximum number of stdio files that can be open, than the limit of open
   file the OS will let you have. If this value is > 0 (and ``stdio-fdmax''
   is *not* used in the config file), then all non-stdio file descriptors
   will be kept above this value (by dup'ing them). */
#if ! defined (MAX_STDIO_FD)
#define MAX_STDIO_FD 0
#endif

/* define DONT_NEED_SYS_SELECT or DO_NEED_SYS_SELECT depending on whether
   endpoint.c fails to compile properly and you have
   /usr/include/sys/select.h */
#if ! defined (DO_NEED_SYS_SELECT) && !defined (DONT_NEED_SYS_SELECT)
#define DONT_NEED_SYS_SELECT 1
#endif

/* define DONT_NEED_STRDUP or DO_NEED_STRDUP depending on
   if you a strdup on your machine or not. */
#if ! defined (DO_NEED_STRDUP) && ! defined (DONT_NEED_STRDUP)
#define DONT_NEED_STRDUP 1
#endif

/* define DONT_NEED_U_INT or DO_NEED_U_INT depending on if you
   have a `u_long', `u_int', `u_short' in your system include path or not */
#if ! defined (DO_NEED_U_INT) && ! defined (DONT_NEED_U_INT)
#define DONT_NEED_U_INT 1
#endif

/* define DONT_NEED_BOOL or DO_NEED_BOOL depending on if you have a `bool'
   in your include path or not */
#if ! defined (DO_NEED_BOOL) && ! defined (DONT_NEED_BOOL)
#define DO_NEED_BOOL 1
#endif


/* maximum number of struct iovec in a writev.  Build and run the program
   uio_maxiov to determine the proper value to use here (or look for the
   define for UIO_MAXIOV in your system's include file).. Please send me
   <brister@vix.com> value you determined and the appropriate CPP symbols
   to test for your system. 16 is the smallest number I've come across
   yet. Having a number that's too small won't break anything. Having a
   number that's too big will. */
#if ! defined (MAX_WRITEV_VEC)
#define MAX_WRITEV_VEC 16
#endif


/* Defined DO_NEED_STREAM or DONT_NEED_STREAM depending on if you need to
   include <sys/stream.h> included (Solaris and other SVR4(?)) */
#if ! defined (DO_NEED_STREAM) && ! defined (DONT_NEED_STREAM)
#define DONT_NEED_STREAM 1
#endif


/* Define DONT_NEED_STRERROR or DO_NEED_STRERROR depending on if you have
   strerror() in your libraries */
#if ! defined (DO_NEED_STRERROR) && ! defined (DONT_NEED_STRERROR)
#define DONT_NEED_STRERROR 1
#endif


/* argument type for 4th argument to getsockopt. */
#if ! defined (GETSOCKOPT_ARG)
#define GETSOCKOPT_ARG void *
#endif

#if ! defined (USE_SIGSET) && ! defined(USE_SIGVEC)
#define USE_SIGACTION
#endif

#include "configdata.h"
#include <syslog.h> 
#endif /* sysconfig_h__ */

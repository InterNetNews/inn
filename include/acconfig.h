/*  $Revision$
**
**  Here be configuration data used by various InterNetNews programs.  This
**  file is used as source for the autoheader script, which from it and
**  configure.in generates a config.h.in file that will be used by autoconf
**  as the repository of all wisdom.
**
**  Stuff before TOP and after BOTTOM is copied verbatim to config.h.in;
**  the rest of this file is in the desired form for autoconfiscation.
**  Only stuff that autoconf 2.13 couldn't figure out for itself or that
**  needs a larger comment is included here.
*/

#ifndef __CONFIG_H__
#define __CONFIG_H__

/*
**  SITE SETTINGS
**
**  Look over these settings and make sure they're correct for your site.
**  These values don't come from configure and therefore may need manual
**  editing.
*/

/* What to use for a Path tail for local posts.  (FIXME: This should be
   configurable in inn.conf.) */
#define PATHMASTER	"not-for-mail"


/*
**  CONFIGURE RESULTS
**
**  From this point down are only legacy settings and the results of
**  configure.  None of this should require manual editing; if anything
**  here is wrong, see if you should be passing a flag to configure to set
**  it correctly for your system.
*/
@TOP@

/* Mode that incoming articles are created with.  */
#undef ARTFILE_MODE

/* Mode that batch files are created with.  */
#undef BATCHFILE_MODE

/* Define to `char *' if <sys/types.h> doesn't define.  */
#undef caddr_t

/* Define one of the following for the close-on-exec style to use.  */
#undef CLX_FCNTL
#undef CLX_IOCTL

/* Define to compile in Python filter support.  */
#undef DO_PYTHON

/* The syslog facility to use.  */
#undef FACILITY_NEWS

/* Mode that directories are created with.  */
#undef GROUPDIR_MODE

/* Define if your msync() takes three arguments.  */
#undef HAVE_MSYNC_3_ARG

/* Define if you have the <ndbm.h> header file.  */
#undef HAVE_NDBM_H

/* Define if you have both setrlimit() and getrlimit().  */
#undef HAVE_RLIMIT

/* Define if you have the setproctitle function.  */
#undef HAVE_SETPROCTITLE

/* Define if you have the statvfs function.  */
#undef HAVE_STATVFS

/* Define if your `struct tm' has a `tm_gmtoff' member.  */
#undef HAVE_TM_GMTOFF

/* Define if you have union wait.  */
#undef HAVE_UNION_WAIT

/* Define if you have unix domain sockets.  */
#undef HAVE_UNIX_DOMAIN_SOCKETS

/* Define to `(unsigned long) -1' if <netinet/in.h> doesn't define.  */
#undef INADDR_NONE

/* Additional valid low-numbered port for inndstart.  */
#undef INND_PORT

/* Define to a suitable 32-bit type if standard headers don't define.  */
#undef int32_t

/* The log facility to use for INN logging.  Server is for innd itself.  */
#undef LOG_INN_PROG
#undef LOG_INN_SERVER

/* Define if you need to msync() after writes.  */
#undef MMAP_MISSES_WRITES

/* Define one of the following for the non-blocking I/O style to use.  */
#undef NBIO_FCNTL
#undef NBIO_IOCTL

/* The user who gets all INN-related e-mail. */
#undef NEWSMASTER

/* The umask used by all INN programs. */
#undef NEWSUMASK

/* The username and group name that INN should run under. */
#undef NEWSUSER
#undef NEWSGRP

/* Define to `int' if <signal.h> doesn't define.  */
#undef sig_atomic_t

/* sort program for makehistory.  */
#undef SORT_PGM

/* Define to a suitable 32-bit type if standard headers don't define.  */
#undef uint32_t

@BOTTOM@

/*
**  LEGACY
**
**  Everything below this point is here so that parts of INN that haven't
**  been tweaked to use more standard constructs don't break.  Don't count
**  on any of this staying around, and if you're knee-deep in a file that
**  uses any of this, please consider fixing it.
*/

/* All occurrances of these typedefs anywhere should be replaced by their
   ANSI/ISO/standard C definitions given in these typedefs. */
typedef char const *    STRING;
typedef char * const    CSTRING;
typedef void *          ALIGNPTR;

/* Anything that uses this should instead duplicate the logic used here and
   define what it really wants to define; the logic isn't that complex. */
#if defined(STDC_HEADERS) || defined(HAVE_STDARG_H)
# define VAR_STDARGS 1
#else
# ifdef HAVE_VARARGS_H
#  define VAR_VARARGS 1
# else
#  define VAR_NONE 1
# endif
#endif

/* Just use vfork() directly rather than FORK(). */
#define FORK() vfork()

/* There's no reason to make all of these #defines except possibly for
   L_CC_CMD and even that's a stretch.  Since we're logging to our own
   distinguished log facility, provided that we spread things out between a
   reasonable variety of log levels, the sysadmin shouldn't have to change
   any of this.  (Some of this is arguably wrong; L_NOTICE should be
   LOG_NOTICE, for example.) */

/* Flags to use in opening the logs; some programs add LOG_PID. */
#define L_OPENLOG_FLAGS         (LOG_CONS | LOG_NDELAY)

/* Fatal error, program is about to exit. */
#define L_FATAL                 LOG_CRIT

/* Log an error that might mean one or more articles get lost. */
#define L_ERROR                 LOG_ERR

/* Informational notice, usually not worth caring about. */
#define L_NOTICE                LOG_WARNING

/* A protocol trace. */
#define L_TRACE                 LOG_DEBUG

/* All incoming control commands (ctlinnd, etc). */
#define L_CC_CMD                LOG_INFO

#endif /* !__CONFIG_H__ */

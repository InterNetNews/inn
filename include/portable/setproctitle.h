/*  $Id$
**
**  Set things up for setproctitle portably.
**
**  If the system supports setproctitle, we need to define away
**  setproctitle_init.  Otherwise, we have to prototype setproctitle (which is
**  normally prototyped in stdlib.h).
*/

#ifndef PORTABLE_SETPROCTITLE_H
#define PORTABLE_SETPROCTITLE_H 1

#include "config.h"

#if !HAVE_SETPROCTITLE || !HAVE_DECL_SETPROCTITLE
void setproctitle(const char *format, ...);
#endif

#if HAVE_SETPROCTITLE || HAVE_PSTAT
# define setproctitle_init(argc, argv)   /* empty */
#else
void setproctitle_init(int argc, char *argv[]);
#endif

#endif /* !PORTABLE_SETPROCTITLE_H */

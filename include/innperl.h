/*  $Id$
**
**  Declarations for embedded Perl.
*/

#ifndef INNPERL_H
#define INNPERL_H 1

/* Skip this entire file if DO_PERL (./configure --with-perl) isn't set. */
#if DO_PERL

#include "config.h"

BEGIN_DECLS

/* Provide an alternate version of newXS that takes const char strings for the
   first and third parameters and casts them to the char * that Perl
   expects. */
#define inn_newXS(name, func, file) \
    newXS((char *)(name), func, (char *)(file))

extern bool PerlFilterActive;

extern bool PerlFilter(bool value);
extern void PerlClose(void);
extern void PERLsetup(char *startupfile, char *filterfile,
                      const char *function);
extern int  PERLreadfilter(char *filterfile, const char *function);

END_DECLS

#endif /* DO_PERL */
#endif /* !INNPERL_H */

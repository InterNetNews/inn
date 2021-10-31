/*
**  Declarations for embedded Perl.
*/

#ifndef INNPERL_H
#define INNPERL_H 1

/* Skip this entire file if DO_PERL (./configure --with-perl) isn't set. */
#if DO_PERL

#    include "config.h"

/* Suppress the float-equal gcc warning because of SvTRUE that
   compares floats to 0.0 in Perl core code. */
#    pragma GCC diagnostic ignored "-Wfloat-equal"

BEGIN_DECLS

/* Provide an alternate version of newXS that takes const char strings for the
   first and third parameters and casts them to the char * that Perl
   expects. */
#    define inn_newXS(name, func, file) \
        newXS((char *) (name), func, (char *) (file))

extern bool PerlFilterActive;

extern bool PerlFilter(bool value);
extern void PerlClose(void);
extern void PERLsetup(char *startupfile, char *filterfile,
                      const char *function);
extern int PERLreadfilter(char *filterfile, const char *function);

END_DECLS

#endif /* DO_PERL */
#endif /* !INNPERL_H */

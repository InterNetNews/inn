/*  $Id$
**
**  Declarations for embedded Perl.
*/

/* Skip this entire file if DO_PERL (./configure --with-perl) isn't set. */
#if DO_PERL

/* from the embedded Perl source */

extern void loadPerl(void);


/* from lib/perl.c */

extern bool PerlFilterActive;

extern void PerlFilter(bool value);
extern void PerlClose(void);
extern void PERLsetup(char *startupfile, char *filterfile,
                      const char *function);
extern int  PERLreadfilter(char *filterfile, const char *function);

#endif


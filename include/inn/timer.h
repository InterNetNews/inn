/*  $Id$
**
**  Timer library interface.
**
**  An interface to a simple profiling library.  An application can declare
**  its intent to use n timers by calling TMRinit(n), and then start and
**  stop numbered timers with TMRstart and TMRstop.  TMRsummary logs the
**  results to syslog given labels for each numbered timer.
*/

#ifndef INN_TIMER_H
#define INN_TIMER_H

#include <inn/defines.h>

BEGIN_DECLS

enum {
    TMR_HISHAVE,                /* Looking up ID in history (yes/no). */
    TMR_HISGREP,                /* Looking up ID in history (data). */
    TMR_HISWRITE,               /* Writing to history. */
    TMR_HISSYNC,                /* Syncing history to disk. */
    TMR_APPLICATION             /* Application numbering starts here. */
};

void            TMRinit(unsigned int);
void            TMRstart(unsigned int);
void            TMRstop(unsigned int);
void            TMRsummary(const char *prefix, const char *const *labels);
unsigned long   TMRnow(void);
void            TMRfree(void);

/* Return the current time as a double of seconds and fractional sections. */
double TMRnow_double(void);

END_DECLS

#endif /* INN_TIMER_H */

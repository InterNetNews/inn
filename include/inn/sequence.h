/*
**  Sequence space arithmetic routines.
**
**  This is a set of routines for implementing so called sequence
**  space arithmetic (typically used for DNS serial numbers). The
**  implementation here is taken from RFC 1982.
*/

#ifndef INN_SEQUENCE_H
#define INN_SEQUENCE_H 1

#include "inn/macros.h"
#include "inn/portable-stdbool.h"
#include "inn/system.h"

BEGIN_DECLS

int seq_lcompare(unsigned long, unsigned long);

END_DECLS

#endif /* INN_SEQUENCE_H */

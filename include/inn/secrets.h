/*
**  inn-secrets.conf parser interface.
**
**  The interface to reading inn-secrets.conf configuration file and managing
**  the resulting secrets struct.
*/

#ifndef INN_SECRETS_H
#define INN_SECRETS_H 1

#include "inn/macros.h"
#include "inn/portable-stdbool.h"
#include "inn/system.h"
#include <stdio.h>

/*
**  This structure is organized in the same order as the variables contained
**  in it are mentioned in the inn-secrets.conf documentation, and broken down
**  into the same sections.  Note that due to the implementation, only two
**  types of variables are permissible here: char * and struct vector *.
*/
struct secrets {
    /* Cancel-Lock Settings */
    struct vector *canlockadmin; /* For the news administrator */
    struct vector *canlockuser;  /* For authenticated users */
};

/* The global secrets variable used in programs. */
extern struct secrets *secrets;

BEGIN_DECLS

/* Parse the given file into secrets, using the default path if NULL. */
bool secrets_read(const char *path);

/* Free a secrets struct and all allocated memory for it. */
void secrets_free(struct secrets *);

END_DECLS

#endif /* INN_SECRETS_H */

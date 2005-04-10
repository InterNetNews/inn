/*  $Id$
**
**  Some utility functions for writing authenticators and resolvers.
*/

#ifndef LIBAUTH_H
#define LIBAUTH_H 1

#include "config.h"
#include "portable/socket.h"
#include <stdio.h>
#include <inn/defines.h>

/* Holds the resolver information from nnrpd. */
struct res_info {
    char *clienthostname;
    char *clientip;
    char *clientport;
    char *localip;
    char *localport;
};

/* Holds the authentication information from nnrpd. */
struct auth_info {
    char *username;
    char *password;
};

BEGIN_DECLS

/* Reads connection information from a file descriptor (normally stdin, when
   talking to nnrpd) and returns a new res_info or auth_info struct, or
   returns NULL on failure.  Note that the fields will never be NULL; if the
   corresponding information is missing, it is an error (which will be logged
   and NULL will be returned).  The client is responsible for freeing the
   struct and its fields; this can be done by calling the appropriate
   destruction function below. */
extern struct auth_info *get_auth_info(FILE *);
extern struct res_info  *get_res_info (FILE *);

/* Free a res_info or auth_info struct. */
extern void free_auth_info(struct auth_info *);
extern void free_res_info (struct res_info  *);

END_DECLS

#endif /* !LIBAUTH_H */

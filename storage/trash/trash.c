/*  $Id$
**
**  trashing articles method
*/

#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <clibrary.h>
#include <macros.h>
#include <configdata.h>
#include <libinn.h>
#include <methods.h>
#include "paths.h"

BOOL timehash_init(void) {
    return TRUE;
}

TOKEN timehash_store(const ARTHANDLE article, STORAGECLASS class) {
    TOKEN               token;

    if (article.token == (TOKEN *)NULL)
	memset(&token.token, '\0', sizeof(token));
    else {
	memcpy(&token, article.token, sizeof(token));
	memset(&token.token, '\0', STORAGE_TOKEN_LENGTH);
    }
    token.type = TOKEN_TRASH;
    token.class = class;
    return token;
}

ARTHANDLE *timehash_retrieve(const TOKEN token, RETRTYPE amount) {
    if (token.type != TOKEN_TRASH) {
	SMseterror(SMERR_INTERNAL, NULL);
	return (ARTHANDLE *)NULL;
    }
    SMseterror(SMERR_UNDEFINED, NULL);
    return (ARTHANDLE *)NULL;
}

void timehash_freearticle(ARTHANDLE *article) {
}

BOOL timehash_cancel(TOKEN token) {
    SMseterror(SMERR_UNDEFINED, NULL);
    return FALSE;
}

ARTHANDLE *timehash_next(const ARTHANDLE *article, RETRTYPE amount) {
    return (ARTHANDLE *)NULL;
}

void timehash_shutdown(void) {
}

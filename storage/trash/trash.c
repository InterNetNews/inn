/*  $Id$
**
**  trashing articles method
*/

#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <configdata.h>
#include <clibrary.h>
#include <macros.h>
#include <libinn.h>
#include <methods.h>
#include "paths.h"

BOOL trash_init(BOOL *selfexpire) {
    *selfexpire = TRUE;
    return TRUE;
}

TOKEN trash_store(const ARTHANDLE article, const STORAGECLASS class) {
    TOKEN               token;

    if (article.token == (TOKEN *)NULL)
	memset(&token, '\0', sizeof(token));
    else {
	memcpy(&token, article.token, sizeof(token));
	memset(&token.token, '\0', STORAGE_TOKEN_LENGTH);
    }
    token.type = TOKEN_TRASH;
    token.class = class;
    return token;
}

ARTHANDLE *trash_retrieve(const TOKEN token, const RETRTYPE amount) {
    if (token.type != TOKEN_TRASH) {
	SMseterror(SMERR_INTERNAL, NULL);
	return (ARTHANDLE *)NULL;
    }
    SMseterror(SMERR_NOENT, NULL);
    return (ARTHANDLE *)NULL;
}

void trash_freearticle(ARTHANDLE *article) {
}

BOOL trash_cancel(TOKEN token) {
    SMseterror(SMERR_NOENT, NULL);
    return FALSE;
}

ARTHANDLE *trash_next(const ARTHANDLE *article, const RETRTYPE amount) {
    return (ARTHANDLE *)NULL;
}

void trash_shutdown(void) {
}

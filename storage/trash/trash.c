/*  $Id$
**
**  Trashing articles method
*/
#include "config.h"
#include "clibrary.h"
#include "libinn.h"
#include "methods.h"

bool trash_init(SMATTRIBUTE *attr) {
    if (attr == NULL) {
	SMseterror(SMERR_INTERNAL, "attr is NULL");
	return FALSE;
    }
    attr->selfexpire = TRUE;
    attr->expensivestat = FALSE;
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

bool trash_cancel(TOKEN token) {
    SMseterror(SMERR_NOENT, NULL);
    return FALSE;
}

bool trash_ctl(PROBETYPE type, TOKEN *token, void *value) {
    switch (type) {
    case SMARTNGNUM:
    default:
	return FALSE;
    }
}

bool trash_flushcacheddata(FLUSHTYPE type) {
    return TRUE;
}

void trash_printfiles(FILE *file, TOKEN token, char **xref, int ngroups) {
}

ARTHANDLE *trash_next(const ARTHANDLE *article, const RETRTYPE amount) {
    return (ARTHANDLE *)NULL;
}

void trash_shutdown(void) {
}

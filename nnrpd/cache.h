#ifndef CACHE_H
#define CACHE_H

#include "libinn.h"
#include "storage.h"

BEGIN_DECLS

void cache_add(const HASH, const TOKEN);
TOKEN cache_get(const HASH, bool final);

END_DECLS

#endif /* CACHE_H */

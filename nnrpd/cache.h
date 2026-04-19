/*
**  Message-ID to storage token cache.
**
**  Written by Alex Kiernan (alex.kiernan@thus.net) in 2002.
**
**  Various bug fixes, code and documentation improvements since then
**  in 2003, 2006, 2008, 2020, 2021, 2026.
*/

#ifndef CACHE_H
#define CACHE_H

#include "inn/libinn.h"
#include "inn/storage.h"

BEGIN_DECLS

void cache_add(const HASH, const TOKEN);
TOKEN cache_get(const HASH, bool final);

END_DECLS

#endif /* CACHE_H */

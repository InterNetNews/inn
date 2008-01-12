/* $Id$
 *
 * Declarations of functions to ensure running as "news" user/group.
 *
 * By Ivan Shmakov, 2007.
 * This code is in the public domain.
 *
 */

#ifndef RUNASUSER_H
#define RUNASUSER_H 1

#include "config.h"
#include "clibrary.h"

int get_news_uid_gid(uid_t *uid, gid_t *gid, bool may_die);

/* The following functions die() on failure. */
void ensure_news_user(bool may_setuid);
void ensure_news_grp(bool may_setgid);
void ensure_news_user_grp(bool may_setuid, bool may_setgid);

#endif


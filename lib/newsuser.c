/* $Id$
 *
 * Ensure running as "news" user/group.
 *
 * By Ivan Shmakov, 2007.
 * This code is in the public domain.
 *
 */

#include "config.h"
#include "clibrary.h"

#include <pwd.h>
#include <grp.h>

#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/newsuser.h"

/*
 * Resolve runasuser to a UID and runasgroup to a GID.
 */
int
get_news_uid_gid(uid_t *uid, gid_t *gid, bool may_die)
{
    /* NB:  get_news_uid_gid() may be called before innconf_read(). */
    const char *runasuser = innconf != NULL ? innconf->runasuser : RUNASUSER;
    const char *runasgroup  = innconf != NULL ? innconf->runasgroup  : RUNASGROUP;
    bool fail = false;
    struct passwd *pwd;
    struct group *grp;

    /* Resolve runasuser to a UID. */
    if (!uid) {
        /* Do nothing. */
    } else if ((pwd = getpwnam(runasuser)) != 0) {
        *uid = pwd->pw_uid;
    } else if (may_die) {
        die ("can't resolve %s to a UID"
              " (account doesn't exist?)", runasuser);
    } else {
        fail = true;
    }
    
    /* Resolve runasgroup to a GID. */
    if (!gid) {
        /* Do nothing. */
    } else if ((grp = getgrnam(runasgroup)) != 0) {
        *gid = grp->gr_gid;
    } else if (may_die) {
        die ("can't resolve %s to a GID"
              " (group doesn't exist?)", runasgroup);
    } else {
        fail = true;
    }

    return fail ? -1 : 0;
}

/*
 * Ensure running as runasuser user.
 */
void
ensure_news_user(bool may_setuid)
{
    uid_t uid;

    get_news_uid_gid(&uid, false, true);
    if (geteuid() == 0) {
        if (! may_setuid) {
            /* NB:  mustn't be run as root, unless "may_setuid" is true. */
            die("must be run as %s, not as root", innconf->runasuser);
        }
        setuid(uid);
    }
    if (geteuid() != uid || getuid() != uid) {
        die("must be run as %s", innconf->runasuser);
    }
}

/*
 * Ensure running as runasgroup group.
 */
void
ensure_news_grp(bool may_setgid)
{
    gid_t gid;

    get_news_uid_gid(false, &gid, true);
    if (may_setgid && geteuid() == 0) {
        setgid(gid);
    }
    if (getegid() != gid || getgid() != gid) {
        die ("must be run as %s group", innconf->runasgroup);
    }
}

/*
 * Ensure running as runasuser user and runasgroup group.
 */
void
ensure_news_user_grp(bool may_setuid, bool may_setgid)
{
  /* NB:  changing the group first to lose root privileges last. */
  ensure_news_grp(may_setgid);
  ensure_news_user(may_setuid);
}


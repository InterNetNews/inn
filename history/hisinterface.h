/* $Id$
**
** Interface to history API modules
*/

#ifndef HISINTERFACE_H
#define HISINTERFACE_H

struct token;
struct histopts;

typedef struct hismethod {
    const char *name;
    void *(*open)(const char *path, int flags);
    bool (*close)(void *);
    bool (*sync)(void *);
    bool (*lookup)(void *, const char *, time_t *, time_t *, time_t *,
		   struct token *);
    bool (*check)(void *, const char *);
    bool (*write)(void *, const char *, time_t, time_t, time_t,
		  const struct token *);
    bool (*replace)(void *, const char *, time_t, time_t, time_t,
		    const struct token *);
    bool (*expire)(void *, const char *, const char *, bool, void *, time_t,
		   bool (*)(void *, time_t, time_t, time_t,
			    struct token *));
    bool (*walk)(void *, const char *, void *,
		 bool (*)(void *, time_t, time_t, time_t,
			  const struct token *));
    const char *(*error)(void *);
    bool (*remember)(void *, const char *, time_t);
    bool (*ctl)(void *, int, void *);
} HIS_METHOD;

enum { S_HIScacheadd, S_HIScachelookup, S_HISsetup, S_HISsync,
       S_HISlogstats, S_HISclose, S_HISfilesfor, S_HIShavearticle,
       S_HISwrite, S_HISremember, S_HIS_MAX };

/* fine grained history logging */
void his_logger(char *s, int code);
#endif

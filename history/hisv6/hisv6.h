/* $Id$
**
** Internal history API interface exposed to HISxxx
*/

#ifndef HISV6_H
#define HISV6_H

struct token;
struct histopts;
struct history;

void *hisv6_open(const char *path, int flags, struct history *);

bool hisv6_close(void *);

bool hisv6_sync(void *);

bool hisv6_lookup(void *, const char *key, time_t *arrived,
		  time_t *posted, time_t *expires, struct token *token);

bool hisv6_check(void *, const char *key);

bool hisv6_write(void *, const char *key, time_t arrived,
		 time_t posted, time_t expires, const struct token *token);

bool hisv6_replace(void *, const char *key, time_t arrived,
		 time_t posted, time_t expires, const struct token *token);

bool hisv6_expire(void *, const char *, const char *, bool,
		  void *, time_t threshold,
		  bool (*exists)(void *, time_t, time_t, time_t,
				 struct token *));

bool hisv6_walk(void *, const char *, void *,
		bool (*)(void *, time_t, time_t, time_t,
			 const struct token *));

const char *hisv6_error(void *);

bool hisv6_remember(void *, const char *key, time_t arrived);

bool hisv6_ctl(void *, int, void *);

#endif

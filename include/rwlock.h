#ifndef _RWLOCK_H_
#define _RWLOCK_H_

#if defined(_REENTRANT) || defined(_POSIX_THREAD_SEMANTICS)

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct {
    pthread_mutex_t     m;               /* read/write monitor lock */
    int                 rwlock;          /* >0=# rdrs, <0=wrtr, 0=none */
    pthread_cond_t      readers_ok;      /* start waiting readers */
    unsigned int        waiting_writers; /* # of waiting writers */
    pthread_cond_t      writer_ok;       /* start a waiting writer */
} rwl_t;

void rwl_init(rwl_t *rwlp);
void rwl_rdlock(rwl_t *rwlp);
void rwl_wrlock(rwl_t *rwlp);
void rwl_unlock(rwl_t *rwlp);

#else

typedef int rwl_t;       /* Dummy variable */

#define rwl_init(x)
#define rwl_rdlock(x)
#define rwl_wrlock(x)
#define rwl_unlock(x)

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _REENTRANT || _POSIX_THREAD_SEMANTICS */

#endif /* _RWLOCK_H_ */

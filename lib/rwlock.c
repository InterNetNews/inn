#include "rwlock.h"

#if defined(_REENTRANT) || defined(_POSIX_THREAD_SEMANTICS)

void rwl_init(rwl_t *rwlp) {
    pthread_mutex_init(&rwlp->m, NULL);
    pthread_cond_init(&rwlp->readers_ok, NULL);
    pthread_cond_init(&rwlp->writer_ok, NULL);
    rwlp->rwlock = 0;
    rwlp->waiting_writers = 0;
}

/*
** Acquire a read lock.  Multiple readers can go if there are no writers.
*/
void rwl_rdlock(rwl_t *rwlp) {
    pthread_mutex_lock(&rwlp->m);
    while ((rwlp->rwlock < 0) || (rwlp->waiting_writers))
	pthread_cond_wait(&rwlp->readers_ok, &rwlp->m);
    rwlp->rwlock++;
    pthread_mutex_unlock(&rwlp->m);
}

/*
** Acquire a write lock.  Only a single writer can proceed.
*/
void rwl_wrlock(rwl_t *rwlp) {
    pthread_mutex_lock(&rwlp->m);
    while (rwlp->rwlock != 0) {
	rwlp->waiting_writers++;	
	pthread_cond_wait(&rwlp->writer_ok, &rwlp->m);
	rwlp->waiting_writers--;
    }
    rwlp->rwlock = -1;
    pthread_mutex_unlock(&rwlp->m);
}

/*
** Unlock the read/write lock
*/
void rwl_unlock(rwl_t *rwlp) {
    int                 ww, wr;

    pthread_mutex_lock(&rwlp->m);
    if (rwlp->rwlock < 0)
	rwlp->rwlock = 0;
    else
	rwlp->rwlock--;
    /* Keep flags that show if there are waiting readers or writers so that
       we can wake them up outside the monitor lock */
    ww = (rwlp->waiting_writers && (rwlp->rwlock == 0));
    wr = (rwlp->waiting_writers == 0);
    pthread_mutex_unlock(&rwlp->m);
    /* wakeup a waiting writer first.  Otherwise wakeup all readers */
    if (ww)
	pthread_cond_signal(&rwlp->writer_ok);
    else if (wr)
	pthread_cond_broadcast(&rwlp->readers_ok);
}

#endif /* _REENTRANT || _POSIX_THREAD_SEMANTICS */
#include "rwlock.h"

#if defined(_REENTRANT) || defined(_POSIX_THREAD_SEMANTICS)

void rwl_init(rwl_t *rwlp) {
    pthread_mutex_init(&rwlp->m, NULL);
    pthread_cond_init(&rwlp->readers_ok, NULL);
    pthread_cond_init(&rwlp->writer_ok, NULL);
    rwlp->rwlock = 0;
    rwlp->waiting_writers = 0;
}

/*
** Acquire a read lock.  Multiple readers can go if there are no writers.
*/
void rwl_rdlock(rwl_t *rwlp) {
    pthread_mutex_lock(&rwlp->m);
    while ((rwlp->rwlock < 0) || (rwlp->waiting_writers))
	pthread_cond_wait(&rwlp->readers_ok, &rwlp->m);
    rwlp->rwlock++;
    pthread_mutex_unlock(&rwlp->m);
}

/*
** Acquire a write lock.  Only a single writer can proceed.
*/
void rwl_wrlock(rwl_t *rwlp) {
    pthread_mutex_lock(&rwlp->m);
    while (rwlp->rwlock != 0) {
	rwlp->waiting_writers++;	
	pthread_cond_wait(&rwlp->writer_ok, &rwlp->m);
	rwlp->waiting_writers--;
    }
    rwlp->rwlock = -1;
    pthread_mutex_unlock(&rwlp->m);
}

/*
** Unlock the read/write lock
*/
void rwl_unlock(rwl_t *rwlp) {
    int                 ww, wr;

    pthread_mutex_lock(&rwlp->m);
    if (rwlp->rwlock < 0)
	rwlp->rwlock = 0;
    else
	rwlp->rwlock--;
    /* Keep flags that show if there are waiting readers or writers so that
       we can wake them up outside the monitor lock */
    ww = (rwlp->waiting_writers && (rwlp->rwlock == 0));
    wr = (rwlp->waiting_writers == 0);
    pthread_mutex_unlock(&rwlp->m);
    /* wakeup a waiting writer first.  Otherwise wakeup all readers */
    if (ww)
	pthread_cond_signal(&rwlp->writer_ok);
    else if (wr)
	pthread_cond_broadcast(&rwlp->readers_ok);
}

#endif /* _REENTRANT || _POSIX_THREAD_SEMANTICS */
#include "rwlock.h"

#if defined(_REENTRANT) || defined(_POSIX_THREAD_SEMANTICS)

void rwl_init(rwl_t *rwlp) {
    pthread_mutex_init(&rwlp->m, NULL);
    pthread_cond_init(&rwlp->readers_ok, NULL);
    pthread_cond_init(&rwlp->writer_ok, NULL);
    rwlp->rwlock = 0;
    rwlp->waiting_writers = 0;
}

/*
** Acquire a read lock.  Multiple readers can go if there are no writers.
*/
void rwl_rdlock(rwl_t *rwlp) {
    pthread_mutex_lock(&rwlp->m);
    while ((rwlp->rwlock < 0) || (rwlp->waiting_writers))
	pthread_cond_wait(&rwlp->readers_ok, &rwlp->m);
    rwlp->rwlock++;
    pthread_mutex_unlock(&rwlp->m);
}

/*
** Acquire a write lock.  Only a single writer can proceed.
*/
void rwl_wrlock(rwl_t *rwlp) {
    pthread_mutex_lock(&rwlp->m);
    while (rwlp->rwlock != 0) {
	rwlp->waiting_writers++;	
	pthread_cond_wait(&rwlp->writer_ok, &rwlp->m);
	rwlp->waiting_writers--;
    }
    rwlp->rwlock = -1;
    pthread_mutex_unlock(&rwlp->m);
}

/*
** Unlock the read/write lock
*/
void rwl_unlock(rwl_t *rwlp) {
    int                 ww, wr;

    pthread_mutex_lock(&rwlp->m);
    if (rwlp->rwlock < 0)
	rwlp->rwlock = 0;
    else
	rwlp->rwlock--;
    /* Keep flags that show if there are waiting readers or writers so that
       we can wake them up outside the monitor lock */
    ww = (rwlp->waiting_writers && (rwlp->rwlock == 0));
    wr = (rwlp->waiting_writers == 0);
    pthread_mutex_unlock(&rwlp->m);
    /* wakeup a waiting writer first.  Otherwise wakeup all readers */
    if (ww)
	pthread_cond_signal(&rwlp->writer_ok);
    else if (wr)
	pthread_cond_broadcast(&rwlp->readers_ok);
}

#endif /* _REENTRANT || _POSIX_THREAD_SEMANTICS */

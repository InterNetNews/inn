#ifndef _THREADS_H_
#define _THREADS_H_

#if defined (NOTHING) || defined(_REENTRANT) || (_THREAD_SAFE) || (_POSIX_THREAD_SEMANTICS)
#define USING_THREADS
#include <pthread.h>
#else

/* Setup a bunch of dummy definitions so that code that doesn't use
   threads will still work */

#define pthread_mutex_lock(x)
#define pthread_mutex_unlock(x)

#define PTHREAD_MUTEX_INITIALIZER (0)

typedef int pthread_mutex_t;

#endif

#include "rwlock.h"

#endif /* _THREADS_H_ */
#ifndef _THREADS_H_
#define _THREADS_H_

#if defined (NOTHING) || defined(_REENTRANT) || (_THREAD_SAFE) || (_POSIX_THREAD_SEMANTICS)
#define USING_THREADS
#include <pthread.h>
#else

/* Setup a bunch of dummy definitions so that code that doesn't use
   threads will still work */

#define pthread_mutex_lock(x)
#define pthread_mutex_unlock(x)

#define PTHREAD_MUTEX_INITIALIZER (0)

typedef int pthread_mutex_t;

#endif

#include "rwlock.h"

#endif /* _THREADS_H_ */
#ifndef _THREADS_H_
#define _THREADS_H_

#if defined (NOTHING) || defined(_REENTRANT) || (_THREAD_SAFE) || (_POSIX_THREAD_SEMANTICS)
#define USING_THREADS
#include <pthread.h>
#else

/* Setup a bunch of dummy definitions so that code that doesn't use
   threads will still work */

#define pthread_mutex_lock(x)
#define pthread_mutex_unlock(x)

#define PTHREAD_MUTEX_INITIALIZER (0)

typedef int pthread_mutex_t;

#endif

#include "rwlock.h"

#endif /* _THREADS_H_ */

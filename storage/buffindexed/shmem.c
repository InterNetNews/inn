/* Shared memory control utility. */

#include "portable/system.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>

#include "inn/libinn.h"
#include "inn/messages.h"
#include "shmem.h"

#ifdef _TEST_
#    include <sys/file.h> /* for flock */
#    include <syslog.h>   /* for openlog */
#endif

#ifndef MAP_FAILED
#    define MAP_FAILED ((caddr_t) -1)
#endif

#define SEM_RWPERMS (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)

static int
smcGetSemaphore(const char *name)
{
    key_t kt = ftok((char *) name, 0);
    int id = semget(kt, 0, SEM_RWPERMS);

    if (id < 0) {
        syswarn("semget failed to get semaphore for %s", name);
    }
    return id;
}

static int
smcCreateSemaphore(const char *name)
{
    key_t kt = ftok((char *) name, 0);
    int id = semget(kt, 2, IPC_CREAT | SEM_RWPERMS);

    if (id < 0) {
        if (errno == EACCES || errno == EINVAL) {
            /* looks like a wrong semaphore exists. remove it. */
            id = semget(kt, 0, SEM_RWPERMS);
            if (id < 0) {
                /* couldn't even retrieve it. */
                syswarn("cant get semaphore using %s", name);
                return id;
            }
            /* try to remove it */
#ifdef HAVE_UNION_SEMUN
            {
                union semun semArg;
                semArg.val = 1;
                if (semctl(id, 0, IPC_RMID, semArg) < 0) {
                    syswarn("cant remove semaphore %s", name);
                    return -1;
                }
            }
#else
            if (semctl(id, 0, IPC_RMID, NULL) < 0) {
                syswarn("cant remove semaphore %s", name);
                return -1;
            }
#endif
            /* and retry creating it */
            id = semget(kt, 2, IPC_CREAT | SEM_RWPERMS);
        }
    }
    if (id < 0)
        syswarn("cant create semaphore using %s", name);
    return id;
}

int
smcGetExclusiveLock(smcd_t *this)
{
    struct sembuf sops[3] = {
        {0, 0, SEM_UNDO}, /* wait for exclusive lock. */
        {0, 1, SEM_UNDO}, /* lock */
        {1, 0, SEM_UNDO}  /* wait for shared lock */
    };

    /* Get a lock for the buffer. Try again if it fails because our
       SIGHUP may interrupt this semop() call */
    if (semop(this->semap, sops, 3) < 0 && semop(this->semap, sops, 3) < 0) {
        syswarn("semop failed to getExclusiveLock");
        return (-1);
    }
    return (0);
}

int
smcGetSharedLock(smcd_t *this)
{
    struct sembuf sops[2] = {
        {0, 0, SEM_UNDO}, /* wait for exclusive lock. */
        {1, 1, SEM_UNDO}  /* increase access count */
    };

    /* Get a lock for the buffer. Try again if it fails because our
       SIGHUP may interrupt this semop() call */
    if (semop(this->semap, sops, 2) < 0 && semop(this->semap, sops, 2) < 0) {
        syswarn("semop failed to getSharedLock");
        return (-1);
    }
    return (0);
}

int
smcReleaseSharedLock(smcd_t *this)
{
    struct sembuf sops = {1, -1, SEM_UNDO | IPC_NOWAIT};

    /* Release the lock */
    if (semop(this->semap, &sops, 1) < 0) {
        syswarn("semop failed to release shared lock");
        return (-1);
    }
    return (0);
}

int
smcReleaseExclusiveLock(smcd_t *this)
{
    struct sembuf sops = {0, -1, SEM_UNDO | IPC_NOWAIT};

    /* Release the lock */
    if (semop(this->semap, &sops, 1) < 0) {
        syswarn("semop failed to release exclusive lock");
        return (-1);
    }
    return (0);
}

/*
** Get an existing shared memory buffer
*/
smcd_t *
smcGetShmemBuffer(const char *name, int size)
{
    int shmid, semap;
    smcd_t *this;
    caddr_t addr;
    key_t fk = ftok((char *) name, 0);

    /* create shared memory buffer */
    shmid = shmget(fk, size, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (shmid < 0) {
        /* this is normal */
        return NULL;
    }

    /* attach to shared memory buffer */
    if ((addr = (caddr_t) shmat(shmid, 0, 0)) == MAP_FAILED) {
        syswarn("cant attach shared memory");
        if (shmctl(shmid, IPC_RMID, 0) < 0)
            syswarn("cant remove shared memory");
        return NULL;
    }

    /* Get control semaphore */
    if ((semap = smcGetSemaphore(name)) < 0) {
        warn("failed to get semaphore for key %s", name);
        if (shmdt(addr) < 0)
            syswarn("cant detach shared memory");
        if (shmctl(shmid, IPC_RMID, 0) < 0)
            syswarn("cant remove shared memory");
        return NULL;
    }

    this = xmalloc(sizeof(smcd_t));
    this->addr = addr;
    this->size = size;
    this->shmid = shmid;
    this->semap = semap;

    /* This makes news log file huge if enabled */
    debug("got shmid %d semap %d addr %p size %d", shmid, semap, (void *) addr,
          size);

    return this;
}

/*
** Create a shared memory buffer
*/
smcd_t *
smcCreateShmemBuffer(const char *name, int size)
{
    int shmid, semap;
    smcd_t *this;
    caddr_t addr;
    key_t fk = ftok((char *) name, 0);

    /* create shared memory buffer */
    shmid = shmget(fk, size, IPC_CREAT | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (shmid < 0) {
        /* try to get existing segment */
        shmid = shmget(fk, 4, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (shmid >= 0) {
            syswarn("shmem segment already exists name %s", name);
            /* try to delete old segment */
            if (shmctl(shmid, IPC_RMID, 0) < 0) {
                syswarn("cant delete old memory segment");
                return NULL;
            }
            notice("recreating another shmem segment");
            shmid = shmget(fk, size, IPC_CREAT | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        }
    }
    if (shmid < 0) {
        syswarn("cant create shared memory segment");
        return NULL;
    }

    /* attach to shared memory buffer */
    if ((addr = (caddr_t) shmat(shmid, 0, 0)) == MAP_FAILED) {
        syswarn("cant attach shared memory");
        if (shmctl(shmid, IPC_RMID, 0) < 0)
            syswarn("cant remove shared memory");
        return NULL;
    }
    /* clear the data */
    memset(addr, 0, size);

    /* Create control semaphore */
    if ((semap = smcCreateSemaphore(name)) < 0) {
        warn("failed to create semaphore for %s", name);
        if (shmdt(addr) < 0)
            syswarn("cant detach shared memory");
        if (shmctl(shmid, IPC_RMID, 0) < 0)
            syswarn("cant remove shared memory");
        return NULL;
    }

    this = xmalloc(sizeof(smcd_t));
    this->addr = addr;
    this->size = size;
    this->shmid = shmid;
    this->semap = semap;

    debug("created shmid %d semap %d addr %p size %d", shmid, semap,
          (void *) addr, size);

    return this;
}

void
smcClose(smcd_t *this)
{
    struct shmid_ds buf;

    if (this->addr != MAP_FAILED) {
        /* detach shared memory segment */
        if (shmdt(this->addr) < 0)
            syswarn("cant detach shared memory segment");
        this->addr = MAP_FAILED;
    }

    /* delete shm if no one has attached it */
    if (shmctl(this->shmid, IPC_STAT, &buf) < 0) {
        syswarn("cant stat shmid %d", this->shmid);
    } else if (buf.shm_nattch == 0) {
        if (shmctl(this->shmid, IPC_RMID, 0) < 0)
            syswarn("cant delete shmid %d", this->shmid);
        else
            debug("shmid %d deleted", this->shmid);
            /* Delete the semaphore too */
#ifdef HAVE_UNION_SEMUN
        {
            union semun semArg;
            semArg.val = 0;
            if (semctl(this->semap, 0, IPC_RMID, semArg) < 0) {
                syswarn("can't remove semaphore %d", this->semap);
            }
        }
#else
        if (semctl(this->semap, 0, IPC_RMID, NULL) < 0) {
            syswarn("can't remove semaphore %d", this->semap);
        }
#endif
    }
    free(this);
}

#ifdef _TEST_

/* Check if the testfile exists.
** If the file is absent, create one with size 1M, and fill the contents
** with all zero.
** for (i=0; i<100; i++)
**   add 1 to the content;
*/
static const char *testfile = "testfile";
#    define TESTSIZE (1024 * 1024)
#    define MAXCOUNT 100

static smcd_t *this;
static void
myexit(void)
{
    if (this) {
        smcClose(this);
    }
}

int
main(int argc UNUSED, char **argv UNUSED)
{
    int fd, i, k;
    int *x;
    int len, xmin, xmax;

    atexit(myexit);
    openlog("shmemtest", LOG_PID, LOG_DAEMON);

    /* open the testfile */
    fd = creat(testfile, 0660);
    if (fd < 0) {
        printf("cant open %s", testfile);
        exit(1);
    }

    /* lock the file */
    if (flock(fd, LOCK_EX) < 0) {
        printf("cant get flock");
        exit(1);
    }

    /* try to get shared memory buffer */
    this = smcGetShmemBuffer(testfile, TESTSIZE);
    if (!this) {
        /* because there's no shared memory, create one. */
        this = smcCreateShmemBuffer(testfile, TESTSIZE);
        if (!this) {
            printf("cant create shmem buffer");
            exit(1);
        }
    }

    /* unlock the file */
    if (flock(fd, LOCK_UN) < 0) {
        printf("cant unflock %s", testfile);
        exit(1);
    }

    x = (int *) this->addr;
    len = this->size / sizeof(int);
    for (k = 0; k < MAXCOUNT; k++) {
        if (smcGetExclusiveLock(this) < 0) {
            printf("cant get exclusive lock");
            exit(1);
        }
        for (i = 0; i < len; i++)
            x[i] += 1;
        if (write(fd, this->addr, this->size) != (signed int) this->size) {
            printf("cant write");
            exit(1);
        }
        if (smcReleaseExclusiveLock(this)) {
            printf("cant release exclusive lock");
            exit(1);
        }
    }
    /* write the minimum and maximum */
    xmin = xmax = x[0];
    for (i = 1; i < len; i++) {
        if (x[i] < xmin)
            xmin = x[i];
        if (x[i] > xmax)
            xmax = x[i];
    }
    printf("min %d max %d\n", xmin, xmax);

    return (0);
}
#endif /* _TEST_ */

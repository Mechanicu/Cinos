#ifndef PTHREAD_SPINLOCK_H
#define PTHREAD_SPINLOCK_H
#include <pthread.h>
typedef volatile int pthread_spinlock_t;
extern int           pthread_spin_init(pthread_spinlock_t *__lock, int __pshared)
    __THROW __nonnull((1));

/* Destroy the spinlock LOCK.  */
extern int pthread_spin_destroy(pthread_spinlock_t *__lock)
    __THROW __nonnull((1));

/* Wait until spinlock LOCK is retrieved.  */
extern int pthread_spin_lock(pthread_spinlock_t *__lock)
    __THROWNL __nonnull((1));

/* Try to lock spinlock LOCK.  */
extern int pthread_spin_trylock(pthread_spinlock_t *__lock)
    __THROWNL __nonnull((1));

/* Release spinlock LOCK.  */
extern int pthread_spin_unlock(pthread_spinlock_t *__lock)
    __THROWNL __nonnull((1));
#endif
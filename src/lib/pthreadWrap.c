/* pthreadWrap - error checking wrappers around Posix
 * thread functions.  Most of the errors here are invariably
 * fatal, but shouldn't happen unless the kernal or
 * the program is hosed. */

#include "common.h"
#include "errabort.h"
#include "pthreadWrap.h"


static void pwarn(char *function, int err)
/* Print a warning message on non-zero error code. */
{
if (err != 0)
    warn("Couldn't %s: %s\n", function, strerror(err));
}

static void perr(char *function, int err)
/* Print out error for function and abort on
 * non-zero error code.. */
{
if (err != 0)
    {
    pwarn(function, err);
    noWarnAbort();
    }
}

void pthreadCreate(pthread_t *thread, const pthread_attr_t *attr,
	void *(*start_routine)(void *), void *arg)
/* Create a thread or squawk and die. */
{
int err = pthread_create(thread, attr, start_routine, arg);
perr("pthread_create", err);
}

boolean pthreadMayCreate(pthread_t *thread, const pthread_attr_t *attr,
	void *(*start_routine)(void *), void *arg)
/* Create a thread.  Warn and return FALSE if there is a problem. */
{
int err = pthread_create(thread, attr, start_routine, arg);
pwarn("pthread_create", err);
return err == 0;
}

void pthreadMutexInit(pthread_mutex_t *mutex)
/* Initialize mutex or die trying */
{
int err = pthread_mutex_init(mutex, NULL);
perr("pthread_mutex_init", err);
}

void pthreadMutexDestroy(pthread_mutex_t *mutex)
/* Free up mutex. */
{
int err = pthread_mutex_destroy(mutex);
perr("pthread_mutex_destroy", err);
}

void pthreadMutexLock(pthread_mutex_t *mutex)
/* Lock a mutex to gain exclusive access or die trying. */
{
int err = pthread_mutex_lock(mutex);
perr("pthread_mutex_lock", err);
}

void pthreadMutexUnlock(pthread_mutex_t *mutex)
/* Unlock a mutex or die trying. */
{
int err = pthread_mutex_unlock(mutex);
perr("pthread_mutex_unlock", err);
}

void pthreadCondInit(pthread_cond_t *cond)
/* Initialize pthread conditional. */
{
int err = pthread_cond_init(cond, NULL);
perr("pthread_cond_init", err);
}

void pthreadCondDestroy(pthread_cond_t *cond)
/* Free up conditional. */
{
int err = pthread_cond_destroy(cond);
perr("pthread_cond_destroy", err);
}

void pthreadCondSignal(pthread_cond_t *cond)
/* Set conditional signal to wake up a sleeping thread, or
 * die trying. */
{
int err = pthread_cond_signal(cond);
perr("pthread_cond_signal", err);
}

void pthreadCondWait(pthread_cond_t *cond, pthread_mutex_t *mutex)
/* Wait for conditional signal. */
{
int err = pthread_cond_wait(cond, mutex);
perr("pthread_cond_wait", err);
}


/* pthreadWrap - error checking wrappers around Posix
 * thread functions.  Most of the errors here are invariably
 * fatal, but shouldn't happen unless the kernal or
 * the program is hosed. */

#ifndef PTHREADWRAP_H
#define PTHREADWRAP_H

#include <pthread.h>

void pthreadCreate(pthread_t *thread, const pthread_attr_t *attr,
	void *(*start_routine)(void *), void *arg);
/* Create a thread or squawk and die. */

boolean pthreadMayCreate(pthread_t *thread, const pthread_attr_t *attr,
	void *(*start_routine)(void *), void *arg);
/* Create a thread.  Warn and return FALSE if there is a problem. */

void pthreadMutexInit(pthread_mutex_t *mutex);
/* Initialize mutex or die trying */

void pthreadMutexDestroy(pthread_mutex_t *mutex);
/* Free up mutex. */

void pthreadMutexLock(pthread_mutex_t *mutex);
/* Lock a mutex to gain exclusive access or die trying. */

void pthreadMutexUnlock(pthread_mutex_t *mutex);
/* Unlock a mutex or die trying. */

void pthreadCondInit(pthread_cond_t *cond);
/* Initialize pthread conditional. */

void pthreadCondDestroy(pthread_cond_t *cond);
/* Free up conditional. */

void pthreadCondSignal(pthread_cond_t *cond);
/* Set conditional signal to wake up a sleeping thread, or
 * die trying. */

void pthreadCondWait(pthread_cond_t *cond, pthread_mutex_t *mutex);
/* Wait for conditional signal. */

#endif /* PTHREADWRAP_H */


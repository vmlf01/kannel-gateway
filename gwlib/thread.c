#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "config.h"
#include "gwmem.h"
#include "log.h"
#include "thread.h"
#include "gwassert.h"



/*
 * Start a new thread, running function func, and giving it the argument
 * `arg'. If `size' is 0, `arg' is given as is; otherwise, `arg' is copied
 * into a memory area of size `size'.
 * 
 * If `detached' is non-zero, the thread is created detached, otherwise
 * it is created detached.
 */
pthread_t start_thread(int detached, Threadfunc *func, void *arg, size_t size)
{
	void *copy;
	pthread_t id;
	pthread_attr_t attr;
	int ret;
	
	if (size == 0)
		copy = arg;
	else {
		copy = gw_malloc(size);
		memcpy(copy, arg, size);
	}
	
	pthread_attr_init(&attr);
	if (detached)
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	do {
		ret = pthread_create(&id, &attr, func, copy);
		if (ret == EAGAIN) {
			error(0, "Too many threads, waiting to create one...");
			sleep(1);
		}
	} while (ret == EAGAIN);
	pthread_attr_destroy(&attr);
	if (ret != 0) {
		error(ret, "pthread_create failed");
		goto error;
	}

	return id;

error:
	return (pthread_t) -1;
}

#ifdef MUTEX_STATS
Mutex *mutex_create_measured(unsigned char *filename, int lineno) {
	Mutex *mutex;

	mutex = mutex_create_real();
	mutex->filename = filename;
	mutex->lineno = lineno;
	mutex->locks = 0;
	mutex->collisions = 0;
	return mutex;
}
#endif

Mutex *mutex_create_real(void) {
	Mutex *mutex;
	
	mutex = gw_malloc(sizeof(Mutex));
	pthread_mutex_init(&mutex->mutex, NULL);
	mutex->owner = (pthread_t) -1;
	return mutex;
}

void mutex_destroy(Mutex *mutex) {
	if (mutex == NULL)
		return;

#ifdef MUTEX_STATS
	if (mutex->locks > 0 || mutex->collisions > 0) {
		info(0, "Mutex %s:%d: %ld locks, %ld collisions.",
			mutex->filename, mutex->lineno,
			mutex->locks, mutex->collisions);
	}
#endif

	pthread_mutex_destroy(&mutex->mutex);
	gw_free(mutex);
}


void mutex_lock(Mutex *mutex)
{
	int ret;

	gw_assert(mutex != NULL);

#ifdef MUTEX_STATS
	ret = pthread_mutex_trylock(&mutex->mutex);
	if (ret != 0) {
		ret = pthread_mutex_lock(&mutex->mutex);
		mutex->collisions++;
	}
	mutex->locks++;
#else
	ret = pthread_mutex_lock(&mutex->mutex);
#endif
	if (ret != 0)
		panic(ret, "mutex_lock: Mutex failure!");
	if (mutex->owner == pthread_self())
		panic(0, "mutex_lock: Managed to lock the mutex twice!");
	mutex->owner = pthread_self();
}


int mutex_try_lock(Mutex *mutex)
{
	int ret;

	gw_assert(mutex != NULL);
    
	/* Let's try to lock it. */
	ret = pthread_mutex_trylock(&mutex->mutex);

	if (ret == EBUSY)
		return -1; /* Oops, didn't succeed, someone else locked it. */

	if (ret != 0) {
		/* Oops, some real error occured. The only known case
		 * where this happens is when a mutex object is
		 * initialized badly or not at all. In that case,
		 * we're stupid and don't deserve to live. */
		panic(ret, "mutex_try_lock: Mutex failure!");
	}

	if (pthread_equal(mutex->owner, pthread_self())) {
		/* The lock succeeded, but some thread systems allow
		 * the locking thread to lock it a second time.  We
		 * don't want that because it's not portable, so we
		 * pretend it didn't happen. */
		pthread_mutex_unlock(&mutex->mutex);
		return -1;
	}

	/* Hey, it's ours! Let's remember that... */
	mutex->owner = pthread_self();
	return 0;
}


void mutex_unlock(Mutex *mutex)
{
	int ret;
	gw_assert(mutex != NULL);
	mutex->owner = (pthread_t) -1;
	ret = pthread_mutex_unlock(&mutex->mutex);
	if (ret != 0)
		panic(ret, "mutex_unlock: Mutex failure!");
}

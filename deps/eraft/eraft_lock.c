/*********************************************************************************************/
/************************	Created by 钱慧奇 on 18/04/23.	******************************/
/*********	 Copyright © 2016年 xuli. All rights reserved.	******************************/
/*********************************************************************************************/
#include <time.h>

#include "eraft_lock.h"

bool eraft_lock_init(struct eraft_lock *lock)
{
	if (pthread_mutex_init(&lock->mutex, NULL) != 0) {
		return false;
	}

	if (pthread_cond_init(&lock->cond, NULL) != 0) {
		pthread_mutex_destroy(&lock->mutex);
		return false;
	}

	lock->init = true;
	return true;
}

inline bool eraft_lock_lock(struct eraft_lock *lock)
{
	assert(lock && lock->init);
	return pthread_mutex_lock(&lock->mutex) == 0;
}

inline bool eraft_lock_trylock(struct eraft_lock *lock)
{
	assert(lock && lock->init);
	return pthread_mutex_trylock(&lock->mutex) == 0;
}

inline bool eraft_lock_unlock(struct eraft_lock *lock)
{
	assert(lock && lock->init);
	return pthread_mutex_unlock(&lock->mutex) == 0;
}

void eraft_lock_destroy(struct eraft_lock *lock)
{
	if (lock && lock->init) {
		pthread_mutex_destroy(&lock->mutex);
		pthread_cond_destroy(&lock->cond);
		lock->init = false;
	}
}

static void __pthread_exit_todo(void *data)
{
	pthread_mutex_unlock((pthread_mutex_t *)data);
}

/* 填充绝对时间，精度为毫秒 */
static void _fill_absolute_time(struct timespec *tmspec, long value)
{
	assert(tmspec);

	clock_gettime(CLOCK_REALTIME, tmspec);

	tmspec->tv_sec += value / 1000;
	tmspec->tv_nsec = (long)((value % 1000) * 1000000);
}

bool eraft_lock_wait(struct eraft_lock *lock, int timeout)
{
	assert(lock && lock->init);

	pthread_cleanup_push(__pthread_exit_todo, &lock->mutex);

	if (timeout > 0) {
		struct timespec tm = {};
		_fill_absolute_time(&tm, timeout);
		/* 设置了超时等待，则选择带超时功能设置的条件变量等待函数 */
		pthread_cond_timedwait(&lock->cond, &lock->mutex, &tm);
	} else {
		pthread_cond_wait(&lock->cond, &lock->mutex);
	}

	pthread_cleanup_pop(0);

	return true;
}

bool eraft_lock_wake(struct eraft_lock *lock)
{
	assert(lock && lock->init);

	/* 返回的值为@addr地址上的旧值，说明设置成功,唤醒等待线程 */
	pthread_cond_broadcast(&lock->cond);

	return true;
}


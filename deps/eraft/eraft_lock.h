/*********************************************************************************************/
/************************	Created by 许莉 on 16/04/08.	******************************/
/*********	 Copyright © 2016年 xuli. All rights reserved.	******************************/
/*********************************************************************************************/
#ifndef __ERAFT_LOCK_H__
#define __ERAFT_LOCK_H__

#include <pthread.h>

#include "eraft_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

struct eraft_lock
{
	bool            init;		/* 锁的初始化标志，1为已初始化 */
	pthread_mutex_t mutex;		/* 互斥量 */
	pthread_cond_t  cond;		/* 条件变量 */
};

/* 初始化锁 */
bool eraft_lock_init(struct eraft_lock *lock);

/* 销毁锁 */
void eraft_lock_destroy(struct eraft_lock *lock);

/* 获取锁并锁住，如果锁被占用则阻塞到锁可用 */
bool eraft_lock_lock(struct eraft_lock *lock);

/* 尝试获取锁，如果锁被占用则立即返回 */
bool eraft_lock_trylock(struct eraft_lock *lock);

/* 解锁 */
bool eraft_lock_unlock(struct eraft_lock *lock);

/* 如果超时没有设置，则阻塞等待 */
bool eraft_lock_wait(struct eraft_lock *lock, int timeout);

/* 唤醒等待线程 */
bool eraft_lock_wake(struct eraft_lock *lock);

#ifdef __cplusplus
}
#endif
#endif	/* ifndef __ERAFT_LOCK_H__ */


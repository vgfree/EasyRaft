#pragma once

#include "eraft_lock.h"
#include "eraft_evts.h"

#ifdef __cplusplus
extern "C" {
#endif

/* easy_raft模块的上下文环境结构体 */
struct eraft_context
{
	pthread_t               ptid;		/* 新线程的pid */

	struct eraft_evts       evts;		/* 事件驱动 */

	enum
	{
		ERAFT_STAT_NONE,
		ERAFT_STAT_INIT,
		ERAFT_STAT_RUN,
		ERAFT_STAT_STOP
	}                       stat;			/* 线程的状态 */
	struct eraft_lock       statlock;		/* 用来同步stat的状态 */
};

/* 创建一个easy_raft上下文的结构体 */
struct eraft_context    *eraft_context_create(int port);

/* 销毁一个easy_raft上下文的结构体 */
void eraft_context_destroy(struct eraft_context *ctx);

void eraft_context_dispose_del_group(struct eraft_context *ctx, char *identity);

void eraft_context_dispose_add_group(struct eraft_context *ctx, struct eraft_group *group);

void eraft_context_dispose_send_entry(struct eraft_context *ctx, char *identity, msg_entry_t *entry);

#ifdef __cplusplus
}
#endif


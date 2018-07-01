#include <dirent.h>
#include <stdlib.h>

#include "eraft_context.h"

/* 一个新的线程开始运行 */
static void *_start_main_loop(void *usr)
{
	struct eraft_context *ctx = (struct eraft_context *)usr;

	assert(ctx);

	/* evts init */
	assert(eraft_evts_make(&ctx->evts, ctx->port));
	ctx->evts.ctx = ctx;


	/* 状态设置为ERAFT_STAT_RUN，唤醒等待线程 */
	eraft_lock_lock(&ctx->statlock);
	ctx->stat = ERAFT_STAT_RUN;
	eraft_lock_wake(&ctx->statlock);
	eraft_lock_unlock(&ctx->statlock);

	do {
		/* 线程状态为STOP的时候则将状态设置为NONE，返回真，则代表设置成功，退出循环 */
		if (unlikely(ctx->stat == ERAFT_STAT_STOP)) {
			ATOMIC_SET(&ctx->stat, ERAFT_STAT_NONE);
			break;
		}

		eraft_evts_once(&ctx->evts);
	} while (1);

	eraft_evts_free(&ctx->evts);
	return NULL;
}

struct eraft_context *eraft_context_create(int port)
{
	struct eraft_context *ctx = NULL;

	New(ctx);

	if (unlikely(!ctx)) {
		goto error;
	}

	ctx->port = port;

	/* stat init */
	ctx->stat = ERAFT_STAT_INIT;

	if (unlikely(!eraft_lock_init(&ctx->statlock))) {
		goto error;
	}

	if (unlikely((pthread_create(&ctx->ptid, NULL, _start_main_loop, (void *)ctx))) != 0) {
		goto error;
	}

	eraft_lock_lock(&ctx->statlock);
	do {
		if (ctx->stat == ERAFT_STAT_RUN) {
			break;
		}

		eraft_lock_wait(&ctx->statlock, -1);
	} while (1);
	eraft_lock_unlock(&ctx->statlock);

	return ctx;

error:

	if (ctx) {
		eraft_lock_destroy(&ctx->statlock);
		Free(ctx);
	}

	return NULL;
}

void eraft_context_destroy(struct eraft_context *ctx)
{
	if (ctx) {
		ATOMIC_SET(&ctx->stat, ERAFT_STAT_STOP);

		/* 等待子线程退出然后再继续销毁数据 */
		pthread_join(ctx->ptid, NULL);
		assert(ctx->stat == ERAFT_STAT_NONE);

		/*释放事件及数据*/
		eraft_evts_free(&ctx->evts);

		eraft_lock_destroy(&ctx->statlock);
		Free(ctx);
	}
}

void eraft_context_dispose_del_group(struct eraft_context *ctx, char *identity)
{
	eraft_task_dispose_del_group(&ctx->evts, identity);
}

void eraft_context_dispose_add_group(struct eraft_context *ctx, struct eraft_group *group)
{
	eraft_task_dispose_add_group(&ctx->evts, group);
}

void eraft_context_dispose_send_entry(struct eraft_context *ctx, char *identity, msg_entry_t *entry)
{
	eraft_task_dispose_send_entry(&ctx->evts, identity, entry);
}


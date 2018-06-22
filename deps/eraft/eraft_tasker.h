#pragma once

#include "ev.h"

#include "eraft_lock.h"
#include "eraft_task.h"

struct eraft_once_tasker;
typedef void (*ERAFT_ONCE_TASKER_WORK)(struct eraft_once_tasker *tasker, struct eraft_task *task, void *usr);

struct eraft_once_tasker
{
	struct ev_async         async_watcher;
	struct list_head        list;
	struct eraft_lock       lock;

	struct ev_loop *loop;

	ERAFT_ONCE_TASKER_WORK       fcb;
	void                    *usr;
};

void eraft_once_tasker_init(struct eraft_once_tasker *tasker, struct ev_loop *loop, ERAFT_ONCE_TASKER_WORK fcb, void *usr);

void eraft_once_tasker_call(struct eraft_once_tasker *tasker);

void eraft_once_tasker_stop(struct eraft_once_tasker *tasker);

void eraft_once_tasker_free(struct eraft_once_tasker *tasker);

void eraft_once_tasker_give(struct eraft_once_tasker *tasker, struct eraft_task *task);


#pragma once

#include "ev.h"
#include "libevcoro.h"

#include "etask.h"
#include "eraft_confs.h"
#include "eraft_lock.h"
#include "eraft_dotask.h"

struct eraft_tasker_once
{
#ifdef USE_LIBEVCORO
	evcoro_locks_t lock;
	struct evcoro_scheduler *scheduler;
#else
	struct ev_async         async_watcher;
	struct list_head        list;
	struct eraft_lock       lock;

	struct ev_loop          *loop;
#endif
};

void eraft_tasker_once_init(struct eraft_tasker_once *tasker, struct ev_loop *loop);

void eraft_tasker_once_call(struct eraft_tasker_once *tasker);

void eraft_tasker_once_stop(struct eraft_tasker_once *tasker);

void eraft_tasker_once_free(struct eraft_tasker_once *tasker);

void eraft_tasker_once_give(struct eraft_tasker_once *tasker, struct eraft_dotask *task);


struct eraft_tasker_each
{
#ifdef USE_LIBEVCORO
	evcoro_locks_t lock;
	struct evcoro_scheduler *scheduler;
#else
	struct etask            etask;
	struct ev_io            io_watcher;
	struct list_head        list;
	struct eraft_lock       lock;

	struct ev_loop          *loop;
#endif
};

void eraft_tasker_each_init(struct eraft_tasker_each *tasker, struct ev_loop *loop);

void eraft_tasker_each_call(struct eraft_tasker_each *tasker);

void eraft_tasker_each_stop(struct eraft_tasker_each *tasker);

void eraft_tasker_each_free(struct eraft_tasker_each *tasker);

void eraft_tasker_each_give(struct eraft_tasker_each *tasker, struct eraft_dotask *task);


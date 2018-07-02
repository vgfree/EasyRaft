#include "eraft_worker.h"

static void one_loop_cb(struct evcoro_scheduler *scheduler, void *usr)
{}

static void *_worker_start(void *arg)
{
	struct eraft_worker *worker = (struct eraft_worker *)arg;
	/*初始化事件loop*/
	struct evcoro_scheduler *p_scheduler = evcoro_get_default_scheduler();

	assert(p_scheduler);
	worker->scheduler = p_scheduler;
	worker->loop = p_scheduler->listener;

	eraft_tasker_once_init(&worker->tasker, worker->loop);

	do {
		if (worker->exit) {
			break;
		}

		evcoro_once(worker->scheduler, one_loop_cb, NULL);
	} while (1);
	return NULL;
}

int eraft_worker_init(struct eraft_worker *worker)
{
	worker->exit = false;

	/*
	 * set only one thread to do write the same journal file.
	 * set only one queue for worker because we use one event loop to do all task.
	 */
	assert(pthread_create(&worker->pid, NULL, &_worker_start, worker) == 0);

	return 0;
}

int eraft_worker_free(struct eraft_worker *worker)
{
	worker->exit = true;
	// TODO: safe pthread_exit
	pthread_join(worker->pid, NULL);
	// TODO: clean list
	eraft_tasker_once_free(&worker->tasker);
	return 0;
}

void eraft_worker_give(struct eraft_worker *worker, struct eraft_dotask *task)
{
	eraft_tasker_once_give(&worker->tasker, task);
}


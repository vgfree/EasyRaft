#include "eraft_worker.h"

static void *_worker_start(void *arg);

int eraft_worker_init(struct eraft_worker *worker, ERAFT_WORKER_WORK fcb, void *usr)
{
	worker->exit = false;
	etask_make(&worker->etask);
	INIT_LIST_HEAD(&worker->list);
	eraft_lock_init(&worker->lock);

	worker->fcb = fcb;
	worker->usr = usr;
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
	etask_awake(&worker->etask);
	pthread_join(worker->pid, NULL);
	eraft_lock_destroy(&worker->lock);
	//TODO: clean list
	etask_free(&worker->etask);
	return 0;
}

void *_worker_start(void *arg)
{
	struct eraft_worker *worker = (struct eraft_worker *)arg;
	do {
		eraft_lock_lock(&worker->lock);
		bool is_empty = list_empty(&worker->list);
		eraft_lock_unlock(&worker->lock);
		if (is_empty) {
			if (worker->exit) {
				break;
			}
			etask_sleep(&worker->etask);
		}

		eraft_lock_lock(&worker->lock);
		struct eraft_task *task = list_first_entry(&worker->list, struct eraft_task, node);
                list_del(&task->node);
		eraft_lock_unlock(&worker->lock);

		worker->fcb(worker, task, worker->usr);
	} while(1);
	return NULL;
}

void eraft_worker_give(struct eraft_worker *worker, struct eraft_task *task)
{
	eraft_lock_lock(&worker->lock);
	list_add_tail(&task->node, &worker->list);
	eraft_lock_unlock(&worker->lock);

	etask_awake(&worker->etask);
}


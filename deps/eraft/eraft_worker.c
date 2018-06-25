#include "eraft_worker.h"

static void *_worker_start(void *arg)
{
	struct eraft_worker *worker = (struct eraft_worker *)arg;

	eraft_tasker_once_call(&worker->tasker);

	do {
		if (worker->exit) {
			eraft_tasker_once_stop(&worker->tasker);
			break;
		}

		ev_loop(worker->loop, EVRUN_ONCE);
	} while(1);
	return NULL;
}


int eraft_worker_init(struct eraft_worker *worker, ERAFT_TASKER_ONCE_FCB fcb, void *usr)
{
	worker->exit = false;

	/*初始化事件loop*/
	worker->loop = ev_loop_new(EVBACKEND_EPOLL | EVFLAG_NOENV);

	eraft_tasker_once_init(&worker->tasker, worker->loop, fcb, usr);
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
	//TODO: safe pthread_exit
	pthread_join(worker->pid, NULL);
	//TODO: clean list
	eraft_tasker_once_free(&worker->tasker);
	return 0;
}

void eraft_worker_give(struct eraft_worker *worker, struct eraft_dotask *task)
{
	eraft_tasker_once_give(&worker->tasker, task);
}


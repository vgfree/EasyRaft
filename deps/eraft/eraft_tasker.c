#include "eraft_tasker.h"


#ifdef USE_LIBEVCORO
static void dotask_handle(struct evcoro_scheduler *scheduler, void *usr)
{
	struct eraft_dotask *task = (struct eraft_dotask *)usr;

	task->_fcb(task, task->_usr);
}
#else
static void __tasker_async_cb(struct ev_loop *loop, struct ev_async *w, int revents)
{
	struct eraft_tasker_once *tasker = w->data;

	LIST_HEAD(do_list);
	eraft_lock_lock(&tasker->lock);
	list_splice_init(&tasker->list, &do_list);
	eraft_lock_unlock(&tasker->lock);

	while (!list_empty(&do_list)) {
		struct eraft_dotask *first = list_first_entry(&do_list, struct eraft_dotask, node);
		list_del(&first->node);

		assert(sizeof(struct list_head) == sizeof(struct list_node));
		struct list_head *head = (struct list_head *)&first->node;
		INIT_LIST_HEAD(head);

		first->_fcb(first, first->_usr);
	}
}
static void __tasker_io_cb(struct ev_loop *loop, struct ev_io *w, int revents)
{
	struct eraft_tasker_each        *tasker = w->data;
	eventfd_t                       val = 0;

	eventfd_xrecv(tasker->etask.efd, &val);

	LIST_HEAD(do_list);
	eraft_lock_lock(&tasker->lock);
	list_splice_init(&tasker->list, &do_list);
	eraft_lock_unlock(&tasker->lock);

	if (!list_empty(&do_list)) {
		/*摘取第一个task*/
		struct eraft_dotask *first = list_first_entry(&do_list, struct eraft_dotask, node);
		list_del(&first->node);

		/*放置回去*/
		eraft_lock_lock(&tasker->lock);
		list_splice_init(&do_list, &tasker->list);
		eraft_lock_unlock(&tasker->lock);

		first->_fcb(first, first->_usr);
	}
}
#endif

void eraft_tasker_once_init(struct eraft_tasker_once *tasker, struct ev_loop *loop)
{
#ifdef USE_LIBEVCORO
	evcoro_locks_init(&tasker->lock, NULL, MUTEX_LOCK_TYPE);
	tasker->scheduler = evcoro_get_default_scheduler();
#else
	tasker->async_watcher.data = tasker;
	tasker->loop = loop;
	ev_async_init(&tasker->async_watcher, __tasker_async_cb);
	INIT_LIST_HEAD(&tasker->list);
	eraft_lock_init(&tasker->lock);

	ev_async_start(tasker->loop, &tasker->async_watcher);
#endif
}

void eraft_tasker_once_call(struct eraft_tasker_once *tasker)
{
#ifdef USE_LIBEVCORO
	struct evcoro_scheduler *p_scheduler = evcoro_get_default_scheduler();
	evcoro_locks_unlock(p_scheduler, &tasker->lock);
#else
	ev_async_start(tasker->loop, &tasker->async_watcher);
#endif
}

void eraft_tasker_once_stop(struct eraft_tasker_once *tasker)
{
#ifdef USE_LIBEVCORO
	struct evcoro_scheduler *p_scheduler = evcoro_get_default_scheduler();
	evcoro_locks_lock(p_scheduler, &tasker->lock, 0);
#else
	ev_async_stop(tasker->loop, &tasker->async_watcher);
#endif
}

void eraft_tasker_once_free(struct eraft_tasker_once *tasker)
{
#ifdef USE_LIBEVCORO
	evcoro_locks_destroy(&tasker->lock);
#else
	ev_async_stop(tasker->loop, &tasker->async_watcher);

	eraft_lock_destroy(&tasker->lock);
#endif
}

void eraft_tasker_once_give(struct eraft_tasker_once *tasker, struct eraft_dotask *task)
{
#ifdef USE_LIBEVCORO
	struct evcoro_scheduler *p_scheduler = evcoro_get_default_scheduler();
	struct ev_coro          *cursor = evcoro_list_cursor(p_scheduler->working);

	if (_evcoro_subctx(cursor)) {
		evcoro_transfer_goto(p_scheduler, tasker->scheduler);

		task->_fcb(task, task->_usr);
	} else {
		struct ev_coro *giving = evcoro_open(p_scheduler, dotask_handle, (void *)task, 0);
		
		evcoro_join(tasker->scheduler, giving);
	}
#else
	eraft_lock_lock(&tasker->lock);
	list_add_tail(&task->node, &tasker->list);
	eraft_lock_unlock(&tasker->lock);

	ev_async_send(tasker->loop, &tasker->async_watcher);
#endif
}


void eraft_tasker_each_init(struct eraft_tasker_each *tasker, struct ev_loop *loop)
{
#ifdef USE_LIBEVCORO
	evcoro_locks_init(&tasker->lock, NULL, MUTEX_LOCK_TYPE);
	tasker->scheduler = evcoro_get_default_scheduler();
#else
	tasker->io_watcher.data = tasker;
	tasker->loop = loop;
	etask_make(&tasker->etask);
	ev_io_init(&tasker->io_watcher, __tasker_io_cb, tasker->etask.efd, EV_READ);
	INIT_LIST_HEAD(&tasker->list);
	eraft_lock_init(&tasker->lock);

	ev_io_start(tasker->loop, &tasker->io_watcher);
#endif
}

void eraft_tasker_each_call(struct eraft_tasker_each *tasker)
{
#ifdef USE_LIBEVCORO
	struct evcoro_scheduler *p_scheduler = evcoro_get_default_scheduler();
	evcoro_locks_unlock(p_scheduler, &tasker->lock);
#else
	ev_io_start(tasker->loop, &tasker->io_watcher);
#endif
}

void eraft_tasker_each_stop(struct eraft_tasker_each *tasker)
{
#ifdef USE_LIBEVCORO
	struct evcoro_scheduler *p_scheduler = evcoro_get_default_scheduler();
	evcoro_locks_lock(p_scheduler, &tasker->lock, 0);
#else
	ev_io_stop(tasker->loop, &tasker->io_watcher);
#endif
}

void eraft_tasker_each_free(struct eraft_tasker_each *tasker)
{
#ifdef USE_LIBEVCORO
	evcoro_locks_destroy(&tasker->lock);
#else
	ev_io_stop(tasker->loop, &tasker->io_watcher);

	etask_free(&tasker->etask);
	eraft_lock_destroy(&tasker->lock);
#endif
}

void eraft_tasker_each_give(struct eraft_tasker_each *tasker, struct eraft_dotask *task)
{
#ifdef USE_LIBEVCORO
	struct evcoro_scheduler *p_scheduler = evcoro_get_default_scheduler();
	struct ev_coro          *cursor = evcoro_list_cursor(p_scheduler->working);

	if (_evcoro_subctx(cursor)) {
		evcoro_transfer_goto(p_scheduler, tasker->scheduler);

		task->_fcb(task, task->_usr);
	} else {
		struct ev_coro *giving = evcoro_open(p_scheduler, dotask_handle, (void *)task, 0);
		
		evcoro_join(tasker->scheduler, giving);
	}
#else
	eraft_lock_lock(&tasker->lock);
	list_add_tail(&task->node, &tasker->list);
	eraft_lock_unlock(&tasker->lock);

	eventfd_xsend(tasker->etask.efd, 1);
#endif
}


#include "eraft_tasker.h"


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

		tasker->fcb(tasker, first, tasker->usr);
	}
}

void eraft_tasker_once_init(struct eraft_tasker_once *tasker, struct ev_loop *loop, ERAFT_TASKER_ONCE_FCB fcb, void *usr)
{
	tasker->async_watcher.data = tasker;
	tasker->loop = loop;
	tasker->fcb = fcb;
	tasker->usr = usr;
	ev_async_init(&tasker->async_watcher, __tasker_async_cb);
	INIT_LIST_HEAD(&tasker->list);
	eraft_lock_init(&tasker->lock);
}

void eraft_tasker_once_call(struct eraft_tasker_once *tasker)
{
	ev_async_start(tasker->loop, &tasker->async_watcher);
}

void eraft_tasker_once_stop(struct eraft_tasker_once *tasker)
{
	ev_async_stop(tasker->loop, &tasker->async_watcher);
}

void eraft_tasker_once_free(struct eraft_tasker_once *tasker)
{
	eraft_lock_destroy(&tasker->lock);
}

void eraft_tasker_once_give(struct eraft_tasker_once *tasker, struct eraft_dotask *task)
{
	eraft_lock_lock(&tasker->lock);
	list_add_tail(&task->node, &tasker->list);
	eraft_lock_unlock(&tasker->lock);

	ev_async_send(tasker->loop, &tasker->async_watcher);
}





static void __tasker_io_cb(struct ev_loop *loop, struct ev_io *w, int revents)
{
	struct eraft_tasker_each *tasker = w->data;
	eventfd_t val = 0;
	eventfd_xrecv(tasker->etask.efd, &val);

	LIST_HEAD(do_list);
	eraft_lock_lock(&tasker->lock);
	list_splice_init(&tasker->list, &do_list);
	eraft_lock_unlock(&tasker->lock);

	if (!list_empty(&do_list)) {
		/*摘取第一个task*/
		struct eraft_dotask *first = list_first_entry(&do_list, struct eraft_dotask, node);
		list_del(&first->node);

		assert(sizeof(struct list_head) == sizeof(struct list_node));
		struct list_head *head = (struct list_head *)&first->node;
		INIT_LIST_HEAD(head);

		/*摘取一致的task*/
		if (first->merge) {
			struct eraft_dotask *child = NULL;
			list_for_each_entry(child, &do_list, node) {
				if (first->type == child->type) {
					list_del(&child->node);
					list_add_tail(&child->node, head);
				} else {
					break;
				}
			}
		}
		/*放置回去*/
		eraft_lock_lock(&tasker->lock);
		list_splice_init(&do_list, &tasker->list);
		eraft_lock_unlock(&tasker->lock);

		tasker->fcb(tasker, first, tasker->usr);
	}
}

void eraft_tasker_each_init(struct eraft_tasker_each *tasker, struct ev_loop *loop, ERAFT_TASKER_EACH_FCB fcb, void *usr)
{
	tasker->io_watcher.data = tasker;
	tasker->loop = loop;
	tasker->fcb = fcb;
	tasker->usr = usr;
	etask_make(&tasker->etask);
	ev_io_init(&tasker->io_watcher, __tasker_io_cb, tasker->etask.efd, EV_READ);
	INIT_LIST_HEAD(&tasker->list);
	eraft_lock_init(&tasker->lock);
}

void eraft_tasker_each_call(struct eraft_tasker_each *tasker)
{
	ev_io_start(tasker->loop, &tasker->io_watcher);
}

void eraft_tasker_each_stop(struct eraft_tasker_each *tasker)
{
	ev_io_stop(tasker->loop, &tasker->io_watcher);
}

void eraft_tasker_each_free(struct eraft_tasker_each *tasker)
{
	etask_free(&tasker->etask);
	eraft_lock_destroy(&tasker->lock);
}

void eraft_tasker_each_give(struct eraft_tasker_each *tasker, struct eraft_dotask *task)
{
	eraft_lock_lock(&tasker->lock);
	list_add_tail(&task->node, &tasker->list);
	eraft_lock_unlock(&tasker->lock);

	eventfd_xsend(tasker->etask.efd, 1);
}


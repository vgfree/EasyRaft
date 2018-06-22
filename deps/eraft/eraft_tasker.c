#include "eraft_tasker.h"


static void __tasker_async_cb(struct ev_loop *loop, ev_async *w, int revents)
{
	struct eraft_once_tasker *tasker = w->data;

	LIST_HEAD(do_list);
	eraft_lock_lock(&tasker->lock);
	list_splice_init(&tasker->list, &do_list);
	eraft_lock_unlock(&tasker->lock);

	while (!list_empty(&do_list)) {
		struct eraft_task *task = list_first_entry(&do_list, struct eraft_task, node);
		list_del(&task->node);
		assert(sizeof(struct list_head) == sizeof(struct list_node));
		INIT_LIST_HEAD((struct list_head *)&task->node);

		if (task->type == ERAFT_TASK_ENTRY_SEND) {
			struct eraft_task *child = NULL;
			list_for_each_entry(child, &do_list, node) {
				if (strcmp(task->identity, child->identity) == 0) {
					list_del(&child->node);
					list_add_tail(&child->node, (struct list_head *)&task->node);
				} else {
					break;
				}
			}
		}
		tasker->fcb(tasker, task, tasker->usr);
	}
}

void eraft_once_tasker_init(struct eraft_once_tasker *tasker, struct ev_loop *loop, ERAFT_ONCE_TASKER_WORK fcb, void *usr)
{
	tasker->async_watcher.data = tasker;
	tasker->loop = loop;
	tasker->fcb = fcb;
	tasker->usr = usr;
	ev_async_init(&tasker->async_watcher, __tasker_async_cb);
	INIT_LIST_HEAD(&tasker->list);
	eraft_lock_init(&tasker->lock);
}

void eraft_once_tasker_call(struct eraft_once_tasker *tasker)
{
	ev_async_start(tasker->loop, &tasker->async_watcher);
}

void eraft_once_tasker_stop(struct eraft_once_tasker *tasker)
{
	ev_async_stop(tasker->loop, &tasker->async_watcher);
}

void eraft_once_tasker_free(struct eraft_once_tasker *tasker)
{
	eraft_lock_destroy(&tasker->lock);
}

void eraft_once_tasker_give(struct eraft_once_tasker *tasker, struct eraft_task *task)
{
	eraft_lock_lock(&tasker->lock);
	list_add_tail(&task->node, &tasker->list);
	eraft_lock_unlock(&tasker->lock);

	ev_async_send(tasker->loop, &tasker->async_watcher);
}


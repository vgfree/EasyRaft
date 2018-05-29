#include "eraft_tasker.h"

struct eraft_task_add_group *eraft_task_add_group_make(struct eraft_group *group)
{
	struct eraft_task_add_group *object = calloc(1, sizeof(*object));

	object->type = ERAFT_TASK_GROUP_ADD;
	INIT_LIST_NODE(&object->node);

	/*设置属性*/
	object->group = group;
	return object;
}

void eraft_task_add_group_free(struct eraft_task_add_group *object)
{
	free(object);
}

struct eraft_task_del_group *eraft_task_del_group_make(char *identity)
{
	struct eraft_task_del_group *object = calloc(1, sizeof(*object));

	object->type = ERAFT_TASK_GROUP_DEL;
	INIT_LIST_NODE(&object->node);

	/*设置属性*/
	object->identity = strdup(identity);
	return object;
}

void eraft_task_del_group_free(struct eraft_task_del_group *object)
{
	free(object->identity);
	free(object);
}

struct eraft_task_send_entry *eraft_task_send_entry_make(char *identity, msg_entry_t *entry, msg_entry_response_t *entry_response)
{
	struct eraft_task_send_entry *object = calloc(1, sizeof(*object));

	object->type = ERAFT_TASK_ENTRY_SEND;
	INIT_LIST_NODE(&object->node);

	/*设置属性*/
	object->identity = strdup(identity);
	object->entry_response = entry_response;
	object->entry = entry;
	etask_make(&object->etask);
	object->efd = -1;
	object->idx = -1;
	return object;
}

void eraft_task_send_entry_free(struct eraft_task_send_entry *object)
{
	free(object->identity);
	etask_free(&object->etask);
	free(object);
}

static void __tasker_async_cb(uv_async_t *handle)
{
	struct eraft_tasker *tasker = handle->data;

	LIST_HEAD(do_list);
	eraft_lock_lock(&tasker->lock);
	list_splice_init(&tasker->list, &do_list);
	eraft_lock_unlock(&tasker->lock);

	struct eraft_task *task = NULL;
	list_for_each_entry(task, &do_list, node)
	{
		list_del(&task->node);

		tasker->fcb(tasker, task, tasker->usr);
	}
}

void eraft_tasker_init(struct eraft_tasker *tasker, uv_loop_t *loop, ERAFT_TASKER_WORK fcb, void *usr)
{
	tasker->async.data = tasker;
	tasker->loop = loop;
	tasker->fcb = fcb;
	tasker->usr = usr;
	uv_async_init(loop, &tasker->async, __tasker_async_cb);
	INIT_LIST_HEAD(&tasker->list);
	eraft_lock_init(&tasker->lock);
}

static void __empty_close_cb(uv_handle_t *handle) {}

void eraft_tasker_free(struct eraft_tasker *tasker)
{
	uv_close((uv_handle_t *)&tasker->async, __empty_close_cb);
	eraft_lock_destroy(&tasker->lock);
}

void eraft_tasker_give(struct eraft_tasker *tasker, struct eraft_task *task)
{
	eraft_lock_lock(&tasker->lock);
	list_add_tail(&task->node, &tasker->list);
	eraft_lock_unlock(&tasker->lock);

	uv_async_send(&tasker->async);
}


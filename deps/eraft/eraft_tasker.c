#include "eraft_tasker.h"

struct eraft_task_group_add *eraft_task_group_add_make(struct eraft_group *group)
{
	struct eraft_task_group_add *object = calloc(1, sizeof(*object));

	object->type = ERAFT_TASK_GROUP_ADD;
	INIT_LIST_NODE(&object->node);

	/*设置属性*/
	object->group = group;
	return object;
}

void eraft_task_group_add_free(struct eraft_task_group_add *object)
{
	free(object);
}

struct eraft_task_group_del *eraft_task_group_del_make(char *identity)
{
	struct eraft_task_group_del *object = calloc(1, sizeof(*object));

	object->type = ERAFT_TASK_GROUP_DEL;
	INIT_LIST_NODE(&object->node);

	/*设置属性*/
	object->identity = strdup(identity);
	return object;
}

void eraft_task_group_del_free(struct eraft_task_group_del *object)
{
	free(object->identity);
	free(object);
}

struct eraft_task_entry_send *eraft_task_entry_send_make(char *identity, msg_entry_t *entry)
{
	struct eraft_task_entry_send *object = calloc(1, sizeof(*object));

	object->type = ERAFT_TASK_ENTRY_SEND;
	INIT_LIST_NODE(&object->node);

	/*设置属性*/
	object->identity = strdup(identity);
	object->entry = entry;
	etask_make(&object->etask);
	object->efd = -1;
	object->idx = -1;
	return object;
}

void eraft_task_entry_send_free(struct eraft_task_entry_send *object)
{
	free(object->identity);
	etask_free(&object->etask);
	free(object);
}

struct eraft_task_log_retain    *eraft_task_log_retain_make(char *identity, struct eraft_evts *evts, struct eraft_journal *journal, raft_batch_t *batch, raft_index_t start_idx, void *usr)
{
	struct eraft_task_log_retain *object = calloc(1, sizeof(*object));

	object->type = ERAFT_TASK_LOG_RETAIN;
	INIT_LIST_NODE(&object->node);

	/*设置属性*/
	object->identity = strdup(identity);
	object->evts = evts;
	object->journal = journal;
	object->batch = batch;
	object->start_idx = start_idx;
	object->usr = usr;
	return object;
}

void eraft_task_log_retain_free(struct eraft_task_log_retain *object)
{
	free(object->identity);
	free(object);
}

struct eraft_task_log_retain_done    *eraft_task_log_retain_done_make(char *identity, raft_batch_t *batch, raft_index_t start_idx, void *usr)
{
	struct eraft_task_log_retain_done *object = calloc(1, sizeof(*object));

	object->type = ERAFT_TASK_LOG_RETAIN_DONE;
	INIT_LIST_NODE(&object->node);

	/*设置属性*/
	object->identity = strdup(identity);
	object->batch = batch;
	object->start_idx = start_idx;
	object->usr = usr;
	return object;
}

void eraft_task_log_retain_done_free(struct eraft_task_log_retain_done *object)
{
	free(object->identity);
	free(object);
}

struct eraft_task_log_append    *eraft_task_log_append_make(char *identity, struct eraft_evts *evts, struct eraft_journal *journal, raft_batch_t *batch, raft_index_t start_idx, raft_node_t *node, raft_index_t    leader_commit, raft_index_t    rsp_first_idx)
{
	struct eraft_task_log_append *object = calloc(1, sizeof(*object));

	object->type = ERAFT_TASK_LOG_APPEND;
	INIT_LIST_NODE(&object->node);

	/*设置属性*/
	object->identity = strdup(identity);
	object->evts = evts;
	object->journal = journal;
	object->batch = batch;
	object->start_idx = start_idx;
	object->raft_node = node;
	object->leader_commit = leader_commit;
	object->rsp_first_idx = rsp_first_idx;
	return object;
}

void eraft_task_log_append_free(struct eraft_task_log_append *object)
{
	free(object->identity);
	free(object);
}

struct eraft_task_log_append_done    *eraft_task_log_append_done_make(char *identity, struct eraft_evts *evts, raft_batch_t *batch, raft_index_t start_idx, raft_node_t *node, raft_index_t    leader_commit, raft_index_t    rsp_first_idx)
{
	struct eraft_task_log_append_done *object = calloc(1, sizeof(*object));

	object->type = ERAFT_TASK_LOG_APPEND_DONE;
	INIT_LIST_NODE(&object->node);

	/*设置属性*/
	object->identity = strdup(identity);
	object->evts = evts;
	object->batch = batch;
	object->start_idx = start_idx;
	object->raft_node = node;
	object->leader_commit = leader_commit;
	object->rsp_first_idx = rsp_first_idx;
	return object;
}

void eraft_task_log_append_done_free(struct eraft_task_log_append_done *object)
{
	free(object->identity);
	free(object);
}

struct eraft_task_log_apply    *eraft_task_log_apply_make(char *identity, struct eraft_evts *evts, raft_batch_t *batch, raft_index_t start_idx)
{
	struct eraft_task_log_apply *object = calloc(1, sizeof(*object));

	object->type = ERAFT_TASK_LOG_APPLY;
	INIT_LIST_NODE(&object->node);

	/*设置属性*/
	object->identity = strdup(identity);
	object->evts = evts;
	object->batch = batch;
	object->start_idx = start_idx;
	return object;
}

void eraft_task_log_apply_free(struct eraft_task_log_apply *object)
{
	free(object->identity);
	free(object);
}

struct eraft_task_log_apply_done    *eraft_task_log_apply_done_make(char *identity, raft_batch_t *batch, raft_index_t start_idx)
{
	struct eraft_task_log_apply_done *object = calloc(1, sizeof(*object));

	object->type = ERAFT_TASK_LOG_APPLY_DONE;
	INIT_LIST_NODE(&object->node);

	/*设置属性*/
	object->identity = strdup(identity);
	object->batch = batch;
	object->start_idx = start_idx;
	return object;
}

void eraft_task_log_apply_done_free(struct eraft_task_log_apply_done *object)
{
	free(object->identity);
	free(object);
}

static void __tasker_async_cb(uv_async_t *handle)
{
	struct eraft_tasker *tasker = handle->data;

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


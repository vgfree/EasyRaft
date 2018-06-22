#include "eraft_taskis.h"

struct eraft_taskis_group_add *eraft_taskis_group_add_make(struct eraft_group *group)
{
	struct eraft_taskis_group_add *object = calloc(1, sizeof(*object));

	eraft_dotask_init(&object->base, ERAFT_TASK_GROUP_ADD, false, group->identity);

	/*设置属性*/
	object->group = group;
	return object;
}

void eraft_taskis_group_add_free(struct eraft_taskis_group_add *object)
{
	eraft_dotask_free(&object->base);
	free(object);
}

struct eraft_taskis_group_del *eraft_taskis_group_del_make(char *identity)
{
	struct eraft_taskis_group_del *object = calloc(1, sizeof(*object));

	eraft_dotask_init(&object->base, ERAFT_TASK_GROUP_DEL, false, identity);
	return object;
}

void eraft_taskis_group_del_free(struct eraft_taskis_group_del *object)
{
	eraft_dotask_free(&object->base);
	free(object);
}

struct eraft_taskis_entry_send *eraft_taskis_entry_send_make(char *identity, msg_entry_t *entry)
{
	struct eraft_taskis_entry_send *object = calloc(1, sizeof(*object));

	eraft_dotask_init(&object->base, ERAFT_TASK_ENTRY_SEND, true, identity);

	object->entry = entry;
	etask_make(&object->etask);
	object->efd = -1;
	object->idx = -1;
	return object;
}

void eraft_taskis_entry_send_free(struct eraft_taskis_entry_send *object)
{
	eraft_dotask_free(&object->base);

	etask_free(&object->etask);
	free(object);
}

struct eraft_taskis_log_retain    *eraft_taskis_log_retain_make(char *identity, struct eraft_evts *evts, struct eraft_journal *journal, raft_batch_t *batch, raft_index_t start_idx, void *usr)
{
	struct eraft_taskis_log_retain *object = calloc(1, sizeof(*object));

	eraft_dotask_init(&object->base, ERAFT_TASK_LOG_RETAIN, false, identity);

	object->evts = evts;
	object->journal = journal;
	object->batch = batch;
	object->start_idx = start_idx;
	object->usr = usr;
	return object;
}

void eraft_taskis_log_retain_free(struct eraft_taskis_log_retain *object)
{
	eraft_dotask_free(&object->base);

	free(object);
}

struct eraft_taskis_log_retain_done    *eraft_taskis_log_retain_done_make(char *identity, raft_batch_t *batch, raft_index_t start_idx, void *usr)
{
	struct eraft_taskis_log_retain_done *object = calloc(1, sizeof(*object));

	eraft_dotask_init(&object->base, ERAFT_TASK_LOG_RETAIN_DONE, false, identity);

	/*设置属性*/
	object->batch = batch;
	object->start_idx = start_idx;
	object->usr = usr;
	return object;
}

void eraft_taskis_log_retain_done_free(struct eraft_taskis_log_retain_done *object)
{
	eraft_dotask_free(&object->base);

	free(object);
}

struct eraft_taskis_log_append    *eraft_taskis_log_append_make(char *identity, struct eraft_evts *evts, struct eraft_journal *journal, raft_batch_t *batch, raft_index_t start_idx, raft_node_t *node, raft_index_t    leader_commit, raft_index_t    rsp_first_idx)
{
	struct eraft_taskis_log_append *object = calloc(1, sizeof(*object));

	eraft_dotask_init(&object->base, ERAFT_TASK_LOG_APPEND, false, identity);

	/*设置属性*/
	object->evts = evts;
	object->journal = journal;
	object->batch = batch;
	object->start_idx = start_idx;
	object->raft_node = node;
	object->leader_commit = leader_commit;
	object->rsp_first_idx = rsp_first_idx;
	return object;
}

void eraft_taskis_log_append_free(struct eraft_taskis_log_append *object)
{
	eraft_dotask_free(&object->base);

	free(object);
}

struct eraft_taskis_log_append_done    *eraft_taskis_log_append_done_make(char *identity, struct eraft_evts *evts, raft_batch_t *batch, raft_index_t start_idx, raft_node_t *node, raft_index_t    leader_commit, raft_index_t    rsp_first_idx)
{
	struct eraft_taskis_log_append_done *object = calloc(1, sizeof(*object));

	eraft_dotask_init(&object->base, ERAFT_TASK_LOG_APPEND_DONE, false, identity);

	/*设置属性*/
	object->evts = evts;
	object->batch = batch;
	object->start_idx = start_idx;
	object->raft_node = node;
	object->leader_commit = leader_commit;
	object->rsp_first_idx = rsp_first_idx;
	return object;
}

void eraft_taskis_log_append_done_free(struct eraft_taskis_log_append_done *object)
{
	eraft_dotask_free(&object->base);

	free(object);
}

struct eraft_taskis_log_apply    *eraft_taskis_log_apply_make(char *identity, struct eraft_evts *evts, raft_batch_t *batch, raft_index_t start_idx)
{
	struct eraft_taskis_log_apply *object = calloc(1, sizeof(*object));

	eraft_dotask_init(&object->base, ERAFT_TASK_LOG_APPLY, false, identity);

	/*设置属性*/
	object->evts = evts;
	object->batch = batch;
	object->start_idx = start_idx;
	return object;
}

void eraft_taskis_log_apply_free(struct eraft_taskis_log_apply *object)
{
	eraft_dotask_free(&object->base);

	free(object);
}

struct eraft_taskis_log_apply_done    *eraft_taskis_log_apply_done_make(char *identity, raft_batch_t *batch, raft_index_t start_idx)
{
	struct eraft_taskis_log_apply_done *object = calloc(1, sizeof(*object));

	eraft_dotask_init(&object->base, ERAFT_TASK_LOG_APPLY_DONE, false, identity);

	/*设置属性*/
	object->batch = batch;
	object->start_idx = start_idx;
	return object;
}

void eraft_taskis_log_apply_done_free(struct eraft_taskis_log_apply_done *object)
{
	eraft_dotask_free(&object->base);

	free(object);
}


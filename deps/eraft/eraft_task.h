#pragma once

#include "raft.h"
#include "list.h"
#include "etask.h"
#include "eraft_multi.h"

enum eraft_task_type
{
	ERAFT_TASK_ENTRY_SEND,

	ERAFT_TASK_GROUP_ADD,
	ERAFT_TASK_GROUP_DEL,
	ERAFT_TASK_GROUP_EMPTY,
	ERAFT_TASK_LOG_RETAIN,
	ERAFT_TASK_LOG_RETAIN_DONE,
	ERAFT_TASK_LOG_APPEND,
	ERAFT_TASK_LOG_APPEND_DONE,
	ERAFT_TASK_LOG_APPLY,
	ERAFT_TASK_LOG_APPLY_DONE,
};

struct eraft_task
{
	struct list_node        node;
	enum eraft_task_type    type;
	char                    *identity;

	char                    object[0];
};

/*********************************************************/
struct eraft_task_group_add
{
	struct list_node        node;
	enum eraft_task_type    type;
	char                    *identity;

	struct eraft_group      *group;
};

struct eraft_task_group_add     *eraft_task_group_add_make(struct eraft_group *group);

void eraft_task_group_add_free(struct eraft_task_group_add *object);


struct eraft_task_group_del
{
	struct list_node        node;
	enum eraft_task_type    type;
	char                    *identity;
};

struct eraft_task_group_del     *eraft_task_group_del_make(char *identity);

void eraft_task_group_del_free(struct eraft_task_group_del *object);


struct eraft_task_entry_send
{
	struct list_node        node;
	enum eraft_task_type    type;
	char                    *identity;

	msg_entry_t             *entry;
	struct etask            etask;/*remain done call*/
	int                     efd;/*entry commit call*/
	int                     idx;
};

struct eraft_task_entry_send    *eraft_task_entry_send_make(char *identity, msg_entry_t *entry);

void eraft_task_entry_send_free(struct eraft_task_entry_send *object);



struct eraft_task_log_retain
{
	struct list_node        node;
	enum eraft_task_type    type;
	char                    *identity;

	struct eraft_evts	*evts;
	struct eraft_journal	*journal;
	raft_batch_t    *batch;
	raft_index_t    start_idx;
	void *usr;
};

struct eraft_task_log_retain    *eraft_task_log_retain_make(char *identity, struct eraft_evts *evts, struct eraft_journal *journal, raft_batch_t *batch, raft_index_t start_idx, void *usr);

void eraft_task_log_retain_free(struct eraft_task_log_retain *object);

struct eraft_task_log_retain_done
{
	struct list_node        node;
	enum eraft_task_type    type;
	char                    *identity;

	raft_batch_t    *batch;
	raft_index_t    start_idx;
	void *usr;
};

struct eraft_task_log_retain_done    *eraft_task_log_retain_done_make(char *identity, raft_batch_t *batch, raft_index_t start_idx, void *usr);

void eraft_task_log_retain_done_free(struct eraft_task_log_retain_done *object);


struct eraft_task_log_append
{
	struct list_node        node;
	enum eraft_task_type    type;
	char                    *identity;

	struct eraft_evts	*evts;
	struct eraft_journal	*journal;
	raft_batch_t    *batch;
	raft_index_t    start_idx;
	raft_node_t *raft_node;
    raft_index_t    leader_commit;
    raft_index_t    rsp_first_idx;
};

struct eraft_task_log_append    *eraft_task_log_append_make(char *identity, struct eraft_evts *evts, struct eraft_journal *journal, raft_batch_t *batch, raft_index_t start_idx, raft_node_t *node, raft_index_t    leader_commit, raft_index_t    rsp_first_idx);

void eraft_task_log_append_free(struct eraft_task_log_append *object);

struct eraft_task_log_append_done
{
	struct list_node        node;
	enum eraft_task_type    type;
	char                    *identity;

	struct eraft_evts	*evts;
	raft_batch_t    *batch;
	raft_index_t    start_idx;
	raft_node_t *raft_node;
    raft_index_t    leader_commit;
    raft_index_t    rsp_first_idx;
};

struct eraft_task_log_append_done    *eraft_task_log_append_done_make(char *identity, struct eraft_evts *evts, raft_batch_t *batch, raft_index_t start_idx, raft_node_t *node, raft_index_t    leader_commit, raft_index_t    rsp_first_idx);

void eraft_task_log_append_done_free(struct eraft_task_log_append_done *object);


struct eraft_task_log_apply
{
	struct list_node        node;
	enum eraft_task_type    type;
	char                    *identity;

	struct eraft_evts	*evts;
	raft_batch_t    *batch;
	raft_index_t    start_idx;
};

struct eraft_task_log_apply    *eraft_task_log_apply_make(char *identity, struct eraft_evts *evts, raft_batch_t *batch, raft_index_t start_idx);

void eraft_task_log_apply_free(struct eraft_task_log_apply *object);


struct eraft_task_log_apply_done
{
	struct list_node        node;
	enum eraft_task_type    type;
	char                    *identity;

	raft_batch_t    *batch;
	raft_index_t    start_idx;
};

struct eraft_task_log_apply_done    *eraft_task_log_apply_done_make(char *identity, raft_batch_t *batch, raft_index_t start_idx);

void eraft_task_log_apply_done_free(struct eraft_task_log_apply_done *object);


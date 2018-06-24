#pragma once

#include "raft.h"
#include "list.h"
#include "etask.h"
#include "eraft_multi.h"
#include "eraft_dotask.h"

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
	ERAFT_TASK_NET_APPEND,
	ERAFT_TASK_NET_APPEND_RESPONSE,
	ERAFT_TASK_NET_VOTE,
	ERAFT_TASK_NET_VOTE_RESPONSE,
};


struct eraft_taskis_group_add
{
	struct eraft_dotask	base;

	struct eraft_group      *group;
};

struct eraft_taskis_group_add     *eraft_taskis_group_add_make(struct eraft_group *group);

void eraft_taskis_group_add_free(struct eraft_taskis_group_add *object);


struct eraft_taskis_group_del
{
	struct eraft_dotask	base;
};

struct eraft_taskis_group_del     *eraft_taskis_group_del_make(char *identity);

void eraft_taskis_group_del_free(struct eraft_taskis_group_del *object);


struct eraft_taskis_entry_send
{
	struct eraft_dotask	base;

	msg_entry_t             *entry;
	struct etask            etask;/*remain done call*/
	int                     efd;/*entry commit call*/
	int                     idx;
};

struct eraft_taskis_entry_send    *eraft_taskis_entry_send_make(char *identity, msg_entry_t *entry);

void eraft_taskis_entry_send_free(struct eraft_taskis_entry_send *object);



struct eraft_taskis_log_retain
{
	struct eraft_dotask	base;

	struct eraft_evts	*evts;
	struct eraft_journal	*journal;
	raft_batch_t    *batch;
	raft_index_t    start_idx;
	void *usr;
};

struct eraft_taskis_log_retain    *eraft_taskis_log_retain_make(char *identity, struct eraft_evts *evts, struct eraft_journal *journal, raft_batch_t *batch, raft_index_t start_idx, void *usr);

void eraft_taskis_log_retain_free(struct eraft_taskis_log_retain *object);

struct eraft_taskis_log_retain_done
{
	struct eraft_dotask	base;

	raft_batch_t    *batch;
	raft_index_t    start_idx;
	void *usr;
};

struct eraft_taskis_log_retain_done    *eraft_taskis_log_retain_done_make(char *identity, raft_batch_t *batch, raft_index_t start_idx, void *usr);

void eraft_taskis_log_retain_done_free(struct eraft_taskis_log_retain_done *object);


struct eraft_taskis_log_append
{
	struct eraft_dotask	base;

	struct eraft_evts	*evts;
	struct eraft_journal	*journal;
	raft_batch_t    *batch;
	raft_index_t    start_idx;
	raft_node_t *raft_node;
	raft_index_t    leader_commit;
	raft_index_t    rsp_first_idx;
};

struct eraft_taskis_log_append    *eraft_taskis_log_append_make(char *identity, struct eraft_evts *evts, struct eraft_journal *journal, raft_batch_t *batch, raft_index_t start_idx, raft_node_t *node, raft_index_t    leader_commit, raft_index_t    rsp_first_idx);

void eraft_taskis_log_append_free(struct eraft_taskis_log_append *object);

struct eraft_taskis_log_append_done
{
	struct eraft_dotask	base;

	struct eraft_evts	*evts;
	raft_batch_t    *batch;
	raft_index_t    start_idx;
	raft_node_t *raft_node;
	raft_index_t    leader_commit;
	raft_index_t    rsp_first_idx;
};

struct eraft_taskis_log_append_done    *eraft_taskis_log_append_done_make(char *identity, struct eraft_evts *evts, raft_batch_t *batch, raft_index_t start_idx, raft_node_t *node, raft_index_t    leader_commit, raft_index_t    rsp_first_idx);

void eraft_taskis_log_append_done_free(struct eraft_taskis_log_append_done *object);


struct eraft_taskis_log_apply
{
	struct eraft_dotask	base;

	struct eraft_evts	*evts;
	raft_batch_t    *batch;
	raft_index_t    start_idx;
};

struct eraft_taskis_log_apply    *eraft_taskis_log_apply_make(char *identity, struct eraft_evts *evts, raft_batch_t *batch, raft_index_t start_idx);

void eraft_taskis_log_apply_free(struct eraft_taskis_log_apply *object);


struct eraft_taskis_log_apply_done
{
	struct eraft_dotask	base;

	raft_batch_t    *batch;
	raft_index_t    start_idx;
};

struct eraft_taskis_log_apply_done    *eraft_taskis_log_apply_done_make(char *identity, raft_batch_t *batch, raft_index_t start_idx);

void eraft_taskis_log_apply_done_free(struct eraft_taskis_log_apply_done *object);

struct eraft_taskis_net_append_response
{
	struct eraft_dotask	base;

	raft_node_t             *node;
	msg_appendentries_response_t    *aer;
};

struct eraft_taskis_net_append_response *eraft_taskis_net_append_response_make(char *identity, msg_appendentries_response_t *aer, raft_node_t *node);

void eraft_taskis_net_append_response_free(struct eraft_taskis_net_append_response *object);

struct eraft_taskis_net_append
{
	struct eraft_dotask	base;

	raft_node_t             *node;
	msg_appendentries_t     *ae;
};

struct eraft_taskis_net_append *eraft_taskis_net_append_make(char *identity, msg_appendentries_t *ae, raft_node_t *node);

void eraft_taskis_net_append_free(struct eraft_taskis_net_append *object);


struct eraft_taskis_net_vote
{
	struct eraft_dotask	base;

	raft_node_t             *node;
	msg_requestvote_t       *rv;
};

struct eraft_taskis_net_vote *eraft_taskis_net_vote_make(char *identity, msg_requestvote_t *rv, raft_node_t *node);

void eraft_taskis_net_vote_free(struct eraft_taskis_net_vote *object);


struct eraft_taskis_net_vote_response
{
	struct eraft_dotask	base;

	raft_node_t             *node;
	msg_requestvote_response_t      *rvr;
};

struct eraft_taskis_net_vote_response *eraft_taskis_net_vote_response_make(char *identity, msg_requestvote_response_t *rvr, raft_node_t *node);

void eraft_taskis_net_vote_response_free(struct eraft_taskis_net_vote_response *object);


#pragma once

#include "uv.h"
#include "uv_helpers.h"
#include "uv_multiplex.h"

#include "raft.h"
#include "list.h"
#include "etask.h"
#include "eraft_lock.h"
#include "eraft_multi.h"

enum eraft_task_type
{
	ERAFT_TASK_GROUP_ADD,
	ERAFT_TASK_GROUP_DEL,
	ERAFT_TASK_GROUP_EMPTY,
	ERAFT_TASK_ENTRY_SEND,
};

struct eraft_task
{
	struct list_node        node;
	enum eraft_task_type    type;

	char                    object[0];
};

struct eraft_task_add_group
{
	struct list_node        node;
	enum eraft_task_type    type;

	struct eraft_group      *group;
};

struct eraft_task_del_group
{
	struct list_node        node;
	enum eraft_task_type    type;

	char                    *identity;
};

struct eraft_task_send_entry
{
	struct list_node        node;
	enum eraft_task_type    type;

	char                    *identity;
	msg_entry_t             *entry;
	msg_entry_response_t    *entry_response;
	struct etask            etask;
	int                     efd;
	int                     idx;
};

struct eraft_task_add_group     *eraft_task_add_group_make(struct eraft_group *group);

void eraft_task_add_group_free(struct eraft_task_add_group *object);

struct eraft_task_del_group     *eraft_task_del_group_make(char *identity);

void eraft_task_del_group_free(struct eraft_task_del_group *object);

struct eraft_task_send_entry    *eraft_task_send_entry_make(char *identity, msg_entry_t *entry, msg_entry_response_t *entry_response);

void eraft_task_send_entry_free(struct eraft_task_send_entry *object);

struct eraft_tasker;
typedef void (*ERAFT_TASKER_WORK)(struct eraft_tasker *tasker, struct eraft_task *task, void *usr);

struct eraft_tasker
{
	uv_async_t              async;
	struct list_head        list;
	struct eraft_lock       lock;

	uv_loop_t               *loop;

	ERAFT_TASKER_WORK       fcb;
	void                    *usr;
};

void eraft_tasker_init(struct eraft_tasker *tasker, uv_loop_t *loop, ERAFT_TASKER_WORK fcb, void *usr);

void eraft_tasker_free(struct eraft_tasker *tasker);

void eraft_tasker_give(struct eraft_tasker *tasker, struct eraft_task *task);


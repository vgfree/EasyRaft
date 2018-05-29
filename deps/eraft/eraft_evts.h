#pragma once

#include "uv.h"
#include "uv_helpers.h"
#include "uv_multiplex.h"

#include "eraft_tasker.h"
#include "eraft_multi.h"
#include "eraft_network.h"

#define PERIOD_MSEC 1000

struct eraft_evts
{
	bool                    init;
	bool                    canfree;

	struct eraft_multi      multi;
	struct eraft_network    network;

	uv_timer_t              periodic_timer;

	uv_loop_t               loop;

	/* Raft isn't multi-threaded */
	struct eraft_tasker     tasker;

	void                    *wait_idx_tree;

	void                    *ctx;
};

/* 初始化事件结构体 */
struct eraft_evts       *eraft_evts_make(struct eraft_evts *evts, int self_port);

/* 销毁一个事件结构体 */
void eraft_evts_free(struct eraft_evts *evts);

/* 执行一次事件循环 */
void eraft_evts_once(struct eraft_evts *evts);

void eraft_task_dispose_del_group(struct eraft_evts *evts, char *identity);

void eraft_task_dispose_add_group(struct eraft_evts *evts, struct eraft_group *group);

void eraft_task_dispose_send_entry(struct eraft_evts *evts, char *identity, msg_entry_t *entry);

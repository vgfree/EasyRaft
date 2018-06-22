#pragma once

#include "ev.h"

#include "eraft_tasker.h"
#include "eraft_worker.h"
#include "eraft_multi.h"
#include "eraft_network.h"
#include "eraft_network_ext.h"

#define PERIOD_MSEC 1000
#define MAX_APPLY_WORKER	32
#define MAX_JOURNAL_WORKER	32


struct eraft_evts
{
	bool                    init;
	bool                    canfree;

	struct eraft_multi      multi;
	struct eraft_network    network;

	struct ev_periodic      periodic_watcher;

	struct ev_loop *loop;

	/* Raft isn't multi-threaded */
	struct eraft_once_tasker     tasker;
	struct eraft_worker	journal_worker[MAX_JOURNAL_WORKER];
	struct eraft_worker	apply_worker[MAX_APPLY_WORKER];

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


#pragma once

#include "raft.h"
#include "eraft_confs.h"
#include "eraft_lock.h"
#include "eraft_tasker.h"
#include "eraft_journal.h"
#include "eraft_journal_ext.h"

struct eraft_node
{
	int     node_id;
	char    raft_host[IPV4_HOST_LEN];
	char    raft_port[IPV4_PORT_LEN];
};

struct eraft_conf
{
	char                    *cluster;

	int                     selfidx;

	int                     num_nodes;
	struct eraft_node       *nodes;
};

struct eraft_conf       *eraft_conf_make(char *cluster, int selfidx);

void eraft_conf_free(struct eraft_conf *conf);

struct eraft_group;

typedef int (*ERAFT_LOG_APPLY_FCB)(struct eraft_group *group, raft_batch_t *batch, raft_index_t start_idx);

struct eraft_group
{
	char                            *identity;
	/* the server's node ID */
	int                             node_id;

	raft_server_t                   *raft;

	struct eraft_conf               *conf;

	struct eraft_tasker_each        self_tasker;
	struct eraft_tasker_each        peer_tasker;

	struct eraft_journal            journal;

	ERAFT_LOG_APPLY_FCB             log_apply_fcb;

	void                            *evts;
};

struct eraft_group      *eraft_group_make(char *identity, int selfidx, char *db_path, int db_size, ERAFT_LOG_APPLY_FCB fcb);

struct eraft_node       *eraft_group_get_self_node(struct eraft_group *group);

void eraft_group_free(struct eraft_group *group);

struct eraft_multi
{
	void *rbt_handle;
};

int eraft_multi_init(struct eraft_multi *multi);

int eraft_multi_free(struct eraft_multi *multi);

int eraft_multi_add_group(struct eraft_multi *multi, struct eraft_group *group);

struct eraft_group      *eraft_multi_get_group(struct eraft_multi *multi, char *identity);

typedef bool (*ERAFT_MULTI_TRAVEL_FOR_LOOKUP_FCB)(struct eraft_group *group, size_t idx, void *usr);
typedef bool (*ERAFT_MULTI_TRAVEL_FOR_DELETE_FCB)(struct eraft_group *group, size_t idx, void *usr);

int eraft_multi_foreach_group(struct eraft_multi        *multi,
	ERAFT_MULTI_TRAVEL_FOR_LOOKUP_FCB               lfcb,
	ERAFT_MULTI_TRAVEL_FOR_DELETE_FCB               dfcb,
	void                                            *usr);


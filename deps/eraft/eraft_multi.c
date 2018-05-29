#include "rbtree_cache.h"
#include "eraft_multi.h"

static void eraft_node_init(struct eraft_node *node, char *node_str, int node_id)
{
	node->node_id = node_id;

	char *tmp = strdup(node_str);

	int     step = 0;
	char    *savep, *split;
	split = strtok_r(tmp, ":", &savep);

	while (split != NULL) {
		if (0 == step) {
			snprintf(node->raft_host, sizeof(node->raft_host), "%s", split);
		} else {
			snprintf(node->raft_port, sizeof(node->raft_port), "%s", split);
		}

		step++;

		split = strtok_r(NULL, ",", &savep);
	}

	free(tmp);
}

struct eraft_conf *eraft_conf_make(char *cluster, int selfidx)
{
	struct eraft_conf *conf = calloc(1, sizeof(*conf));

	conf->cluster = strdup(cluster);
	conf->selfidx = selfidx;

	char *tmp = strdup(cluster);

	char *savep, *split;
	split = strtok_r(tmp, ",", &savep);

	while (split != NULL) {
		if (0 == conf->num_nodes) {
			conf->nodes = calloc(1, sizeof(struct eraft_node));
		} else {
			conf->nodes = realloc(conf->nodes, sizeof(struct eraft_node) * (conf->num_nodes + 1));
		}

		assert(conf->nodes);

		eraft_node_init(&conf->nodes[conf->num_nodes], split, conf->num_nodes);
		conf->num_nodes++;

		split = strtok_r(NULL, ",", &savep);
	}

	free(tmp);

	assert(selfidx < conf->num_nodes);
	return conf;
}

void eraft_conf_free(struct eraft_conf *conf)
{
	if (conf->cluster) {
		free(conf->cluster);
	}

	if (conf->nodes) {
		free(conf->nodes);
	}

	free(conf);
}

static void _load_each_entry(struct eraft_journal    *journal, raft_entry_t *entry, void *usr)
{
	struct eraft_group *group = usr;

	raft_append_entry(group->raft, entry);
}

extern raft_cbs_t g_default_raft_funcs;

struct eraft_group *eraft_group_make(char *identity, int selfidx, char *db_path, int db_size, ERAFT_APPLYLOG_FCB fcb)
{
	struct eraft_group      *group = calloc(1, sizeof(*group));
	struct eraft_conf       *conf = eraft_conf_make(identity, selfidx);

	group->conf = conf;
	group->identity = strdup(identity);
	group->node_id = selfidx;
	group->applylog_fcb = fcb;

	/*加载原有信息*/
	eraft_journal_init(&group->journal, selfidx, db_path, db_size, ERAFT_JOURNAL_TYPE_BDB);
	eraft_journal_open(&group->journal);


	/*创建raft服务*/
	raft_server_t *raft = raft_new();
	raft_set_callbacks(raft, &g_default_raft_funcs, group);
	raft_set_election_timeout(raft, 2000);

	/* add self */
	// raft_add_node(raft, NULL, selfidx, 1);
	for (int i = 0; i < conf->num_nodes; i++) {
		raft_add_node(raft, NULL, i, (selfidx == i) ? 1 : 0);
	}

	group->raft = raft;

	/* Reload cluster information */
	//__load_foreach_append_log(group->lmdb, _load_each_entry, group);

	int     commit_idx = 0;
	eraft_journal_get_state(&group->journal, "commit_idx", strlen("commit_idx") + 1, (char *)&commit_idx, sizeof(commit_idx));
	raft_set_commit_idx(group->raft, commit_idx);
	raft_apply_all(group->raft);

	int     voted_for = -1;
	eraft_journal_get_state(&group->journal, "voted_for", strlen("voted_for") + 1, (char *)&voted_for, sizeof(voted_for));
	raft_vote_for_nodeid(group->raft, voted_for);

	int     term = -1;
	eraft_journal_get_state(&group->journal, "term", strlen("term") + 1, (char *)&term, sizeof(term));
	raft_set_current_term(group->raft, term);

	return group;
}

struct eraft_node *eraft_group_get_self_node(struct eraft_group *group)
{
	return &group->conf->nodes[group->conf->selfidx];
}

void eraft_group_free(struct eraft_group *group)
{
	eraft_journal_close(&group->journal);
	eraft_journal_free(&group->journal);

	eraft_conf_free(group->conf);

	free(group->identity);

	free(group);
}

int eraft_multi_init(struct eraft_multi *multi)
{
	return RBTCacheCreate(&multi->rbt_handle);
}

int eraft_multi_free(struct eraft_multi *multi)
{
	return RBTCacheDestory(&multi->rbt_handle);
}

int eraft_multi_add_group(struct eraft_multi *multi, struct eraft_group *group)
{
	struct eraft_group      *find = NULL;
	int                     ret = RBTCacheGet(multi->rbt_handle, group->identity, strlen(group->identity) + 1, &find, sizeof(find));

	if (ret == sizeof(find)) {
		return -1;
	} else {
		printf("add [%s]\n", group->identity);
		ret = RBTCacheSet(multi->rbt_handle, group->identity, strlen(group->identity) + 1, &group, sizeof(group));
		assert(ret == sizeof(group));
		return 0;
	}
}

struct eraft_group *eraft_multi_get_group(struct eraft_multi *multi, char *identity)
{
	struct eraft_group *group = NULL;
	// printf("get [%s]\n", identity);
	int ret = RBTCacheGet(multi->rbt_handle, identity, strlen(identity) + 1, &group, sizeof(group));

	if (ret == sizeof(group)) {
		return group;
	} else {
		return NULL;
	}
}

struct _travel_from_info
{
	ERAFT_MULTI_TRAVEL_FOR_LOOKUP_FCB       lfcb;
	ERAFT_MULTI_TRAVEL_FOR_DELETE_FCB       dfcb;
	void                                    *usr;
};

static bool _travel_from_lfcb(const void *key, size_t klen, void *val, size_t vlen, size_t idx, void *usr)
{
	struct _travel_from_info *info = (struct _travel_from_info *)usr;

	struct eraft_group *group = NULL;

	assert(vlen == sizeof(group));
	group = *((struct eraft_group **)val);

	return info->lfcb(group, idx, info->usr);
}

static bool _travel_from_dfcb(const void *key, size_t klen, void *val, size_t vlen, size_t idx, void *usr)
{
	struct _travel_from_info *info = (struct _travel_from_info *)usr;

	struct eraft_group *group = NULL;

	assert(vlen == sizeof(group));
	group = *((struct eraft_group **)val);

	return info->dfcb(group, idx, info->usr);
}

int eraft_multi_foreach_group(struct eraft_multi        *multi,
	ERAFT_MULTI_TRAVEL_FOR_LOOKUP_FCB               lfcb,
	ERAFT_MULTI_TRAVEL_FOR_DELETE_FCB               dfcb,
	void                                            *usr)
{
	struct _travel_from_info info = {
		.lfcb   = lfcb,
		.dfcb   = dfcb,
		.usr    = usr,
	};

	return RBTCacheTravel(multi->rbt_handle, lfcb ? _travel_from_lfcb : NULL, dfcb ? _travel_from_dfcb : NULL, &info);
}


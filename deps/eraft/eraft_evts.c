#include "etask_tree.h"
#include "eraft_evts.h"
#include "eraft_taskis.h"
#include "eraft_confs.h"

typedef enum
{
	HANDSHAKE_FAILURE,
	HANDSHAKE_SUCCESS,
} handshake_state_e;

/** Message types used for peer to peer traffic
 * These values are used to identify message types during deserialization */
typedef enum
{
	/** Handshake is a special non-raft message type
	 * We send a handshake so that we can identify ourselves to our peers */
	MSG_HANDSHAKE,
	/** Successful responses mean we can start the Raft periodic callback */
	MSG_HANDSHAKE_RESPONSE,
	/** Tell leader we want to leave the cluster */
	/* When instance is ctrl-c'd we have to gracefuly disconnect */
	MSG_LEAVE,
	/* Receiving a leave response means we can shutdown */
	MSG_LEAVE_RESPONSE,
	MSG_REQUESTVOTE,
	MSG_REQUESTVOTE_RESPONSE,
	MSG_APPENDENTRIES,
	MSG_APPENDENTRIES_RESPONSE,
} peer_message_type_e;

#include "eraft_network.h"

/** Peer protocol handshake
 * Send handshake after connecting so that our peer can identify us */
typedef struct
{
	int     raft_port;
	int     http_port;
	int     node_id;
} msg_handshake_t;

typedef struct
{
	int     success;

	/* leader's Raft port */
	int     leader_port;

	/* the responding node's HTTP port */
	int     http_port;

	/* my Raft node ID.
	 * Sometimes we don't know who we did the handshake with */
	int     node_id;

	char    leader_host[IPV4_HOST_LEN];
} msg_handshake_response_t;

typedef struct
{
	int     type;
	int     node_id;
	char    identity[MAX_GROUP_IDENTITY_LEN];
	union
	{
		msg_handshake_t                 hs;
		msg_handshake_response_t        hsr;
		msg_requestvote_t               rv;
		msg_requestvote_response_t      rvr;
		msg_appendentries_t             ae;
		msg_appendentries_response_t    aer;
	};
	int     padding[100];
} msg_t;

/** Add/remove Raft peer */
typedef struct
{
	int     raft_port;
	int     http_port;
	int     node_id;
	char    host[IPV4_HOST_LEN];
} entry_cfg_change_t;

static int __append_cfg_change(struct eraft_group *group,
	raft_logtype_e change_type,
	char *host,
	int raft_port, int http_port,
	int node_id)
{
	struct eraft_evts       *evts = group->evts;
	entry_cfg_change_t      *change = calloc(1, sizeof(*change));

	change->raft_port = raft_port;
	change->http_port = http_port;
	change->node_id = node_id;
	strcpy(change->host, host);
	change->host[IPV4_HOST_LEN - 1] = 0;

	msg_entry_t entry = {};
	entry.id = rand();
	entry.data.buf = (void *)change;
	entry.data.len = sizeof(*change);
	entry.type = change_type;

	struct iovec request = { .iov_base = entry.data.buf, .iov_len = entry.data.len };

	raft_batch_t    *bat = raft_batch_make(1);
	raft_entry_t    *ety = raft_entry_make(entry.term, entry.id, entry.type, entry.data.buf, entry.data.len);
	raft_batch_join_entry(bat, 0, ety);

	struct etask                            *etask = etask_make(NULL);
	struct eraft_taskis_request_write       *object = eraft_taskis_request_write_make(group->identity, eraft_evts_dispose_dotask, evts, &request, etask);
	int                                     e = raft_retain_entries(group->raft, bat, object);	// FIXME: raft thread may hung by this.
	// etask_sleep(etask);
	// etask_free(etask);

	if (0 != e) {
		return -1;
	}

	return 0;
}

int __raft_log_get_node_id(
	raft_server_t   *raft,
	void            *user_data,
	raft_entry_t    *entry,
	raft_index_t    entry_idx
	)
{
	entry_cfg_change_t *change = (entry_cfg_change_t *)entry->data.buf;

	return change->node_id;
}

static int __send_handshake_response(struct eraft_group *group,
	eraft_connection_t                              *conn,
	handshake_state_e                               success,
	raft_node_t                                     *leader)
{
	struct eraft_evts       *evts = group->evts;
	msg_t                   msg = {};

	msg.type = MSG_HANDSHAKE_RESPONSE;
	msg.node_id = group->node_id;
	snprintf(msg.identity, sizeof(msg.identity), "%s", group->identity);
	msg.hsr.success = success;
	msg.hsr.leader_port = 0;
	msg.hsr.node_id = group->node_id;

	/* allow the peer to redirect to the leader */
	if (leader) {
#if 0
		eraft_connection_t *leader_conn = raft_node_get_udata(leader);

		if (leader_conn) {
			msg.hsr.leader_port = atoi(leader_conn->port);
			snprintf(msg.hsr.leader_host, IPV4_HOST_LEN, "%s",
				inet_ntoa(leader_conn->addr.sin_addr));
		}
#else
		raft_node_id_t          id = raft_node_get_id(leader);
		struct eraft_node       *enode = &group->conf->nodes[id];
		msg.hsr.leader_port = atoi(enode->raft_port);
		snprintf(msg.hsr.leader_host, IPV4_HOST_LEN, "%s", enode->raft_host);
#endif
	}

	msg.hsr.http_port = msg.hsr.leader_port + 1000;

	struct iovec bufs[1];
	bufs[0].iov_base = (char *)&msg;
	bufs[0].iov_len = sizeof(msg_t);
	eraft_network_transmit_connection(&evts->network, conn, bufs, 1);

	return 0;
}

struct _on_network_info
{
	struct eraft_evts       *evts;
	eraft_connection_t      *conn;
};

/** Parse raft peer traffic using binary protocol, and respond to message */
static int _on_transmit_fcb(eraft_connection_t *conn, char *data, uint64_t size, void *usr)
{
	struct eraft_evts *evts = usr;

	msg_t m = *(msg_t *)data;

	struct eraft_group *group = eraft_multi_get_group(&evts->multi, m.identity);

	if (!group) {	// 丢弃还是暂留?
		return -1;
	}

	raft_node_t *node = raft_get_node(group->raft, m.node_id);
#ifdef JUST_FOR_TEST
#else
	struct eraft_node *enode = &group->conf->nodes[m.node_id];
	conn = eraft_network_find_connection(&evts->network, enode->raft_host, enode->raft_port);
#endif
	switch (m.type)
	{
		case MSG_HANDSHAKE:
		{
			// conn->http_port = m.hs.http_port;
			// conn->raft_port = m.hs.raft_port;

			/* Is this peer in our configuration already? */
			node = raft_get_node(group->raft, m.hs.node_id);

			if (node) {
				// raft_node_set_udata(node, conn);
			}

			raft_node_t *leader = raft_get_current_leader_node(group->raft);

			if (!leader) {
				return __send_handshake_response(group, conn, HANDSHAKE_FAILURE, NULL);
			} else if (raft_node_get_id(leader) != group->node_id) {
				return __send_handshake_response(group, conn, HANDSHAKE_FAILURE, leader);
			} else if (node) {
				return __send_handshake_response(group, conn, HANDSHAKE_SUCCESS, NULL);
			} else {
				char    host[IPV4_HOST_LEN] = { 0 };
				char    port[IPV4_PORT_LEN] = { 0 };
				eraft_network_info_connection(&evts->network, conn, host, port);
				int e = __append_cfg_change(group, RAFT_LOGTYPE_ADD_NONVOTING_NODE,
						host,
						m.hs.raft_port, m.hs.http_port,
						m.hs.node_id);

				if (0 != e) {
					return __send_handshake_response(group, conn, HANDSHAKE_FAILURE, NULL);
				}

				return __send_handshake_response(group, conn, HANDSHAKE_SUCCESS, NULL);
			}
		}
		break;

		case MSG_HANDSHAKE_RESPONSE:

			if (0 == m.hsr.success) {
				// conn->http_port = m.hsr.http_port;

				/* We're being redirected to the leader */
				if (m.hsr.leader_port) {
					printf("Redirecting to %s:%d...\n", m.hsr.leader_host, m.hsr.leader_port);
					char port[IPV4_PORT_LEN] = {};
					snprintf(port, sizeof(port), "%d", m.hsr.leader_port);
					eraft_network_find_connection(&evts->network, m.hsr.leader_host, port);
				}
			} else {
				char    host[IPV4_HOST_LEN] = { 0 };
				char    port[IPV4_PORT_LEN] = { 0 };
				eraft_network_info_connection(&evts->network, conn, host, port);
				printf("Connected to leader: %s:%s\n", host, port);

				// if (!conn->node) {
				//	conn->node = raft_get_node(group->raft, m.hsr.node_id);
				// }
			}

			break;

		case MSG_LEAVE:
		{
			// if (!conn->node) {
			//	printf("ERROR: no node\n");
			//	return 0;
			// }

			int                     id = raft_node_get_id(node);
			struct eraft_node       *enode = &group->conf->nodes[id];
			char                    host[IPV4_HOST_LEN] = { 0 };
			char                    port[IPV4_PORT_LEN] = { 0 };
			eraft_network_info_connection(&evts->network, conn, host, port);
			int e = __append_cfg_change(group, RAFT_LOGTYPE_REMOVE_NODE,
					host,
					atoi(enode->raft_port),
					atoi(enode->raft_port) + 1000,
					raft_node_get_id(node));

			if (0 != e) {
				printf("ERROR: Leave request failed\n");
			}
		}
		break;

		case MSG_LEAVE_RESPONSE:
		{
			// __drop_db(group->lmdb);
			printf("Shutdown complete. Quitting...\n");
			exit(0);
		}
		break;

		case MSG_REQUESTVOTE:
		{
			printf("===========node id %d ask me vote ============\n", m.node_id);

			struct eraft_taskis_net_vote *task = eraft_taskis_net_vote_make(group->identity, eraft_evts_dispose_dotask, evts, &m.rv, node);
			eraft_tasker_once_give(&evts->tasker, (struct eraft_dotask *)task);
		}
		break;

		case MSG_REQUESTVOTE_RESPONSE:
		{
			struct eraft_taskis_net_vote_response *task = eraft_taskis_net_vote_response_make(group->identity, eraft_evts_dispose_dotask, evts, &m.rvr, node);
			eraft_tasker_once_give(&evts->tasker, (struct eraft_dotask *)task);
			printf("===========node id %d for me vote ============\n", m.node_id);
		}
		break;

		case MSG_APPENDENTRIES:
		{
			// printf("unpack count ---------------------------------------------------->%d\n", m.ae.n_entries);
			if (0 < m.ae.n_entries) {
				/* handle appendentries payload */
				m.ae.bat = raft_batch_make(m.ae.n_entries);

				char *p = ((char *)data) + sizeof(msg_t);

				for (int i = 0; i < m.ae.n_entries; i++) {
					msg_entry_t     *ety = (msg_entry_t *)p;
					int             len = ety->data.len;
					p += sizeof(msg_entry_t);

					raft_entry_t *new = raft_entry_make(ety->term, ety->id, ety->type, p, len);
					raft_batch_join_entry(m.ae.bat, i, new);

					p += len;
				}
			}

			struct eraft_taskis_net_append *task = eraft_taskis_net_append_make(group->identity, eraft_evts_dispose_dotask, evts, &m.ae, node);
			eraft_tasker_each_give(&group->peer_tasker, (struct eraft_dotask *)task);
		}
		break;

		case MSG_APPENDENTRIES_RESPONSE:
		{
			struct eraft_taskis_net_append_response *task = eraft_taskis_net_append_response_make(group->identity, eraft_evts_dispose_dotask, evts, &m.aer, node);
			eraft_tasker_once_give(&evts->tasker, (struct eraft_dotask *)task);
		}
		break;

		default:
			printf("unknown msg\n");
			exit(0);
	}
	return 0;
}

/** Raft callback for sending request vote message */
static int __raft_send_requestvote(
	raft_server_t           *raft,
	void                    *user_data,
	raft_node_t             *node,
	msg_requestvote_t       *m
	)
{
	struct eraft_group      *group = raft_get_udata(raft);
	int                     id = raft_node_get_id(node);
	struct eraft_node       *enode = &group->conf->nodes[id];
	struct eraft_evts       *evts = group->evts;

	eraft_connection_t *conn = eraft_network_find_connection(&evts->network, enode->raft_host, enode->raft_port);

	if (!eraft_network_usable_connection(&evts->network, conn)) {
		return 0;
	}

	msg_t msg = {};
	msg.type = MSG_REQUESTVOTE;
	msg.node_id = group->node_id;
	snprintf(msg.identity, sizeof(msg.identity), "%s", group->identity);
	msg.rv = *m;

	struct iovec bufs[1];
	bufs[0].iov_base = (char *)&msg;
	bufs[0].iov_len = sizeof(msg_t);
	eraft_network_transmit_connection(&evts->network, conn, bufs, 1);
	return 0;
}

int __raft_send_requestvote_response(
	raft_server_t                   *raft,
	void                            *user_data,
	raft_node_t                     *node,
	msg_requestvote_response_t      *m
	)
{
	struct eraft_group      *group = raft_get_udata(raft);
	int                     id = raft_node_get_id(node);
	struct eraft_node       *enode = &group->conf->nodes[id];
	struct eraft_evts       *evts = group->evts;

	eraft_connection_t *conn = eraft_network_find_connection(&evts->network, enode->raft_host, enode->raft_port);

	if (!eraft_network_usable_connection(&evts->network, conn)) {
		return 0;
	}

	msg_t msg = {};
	msg.type = MSG_REQUESTVOTE_RESPONSE;
	msg.node_id = group->node_id;
	snprintf(msg.identity, sizeof(msg.identity), "%s", group->identity);
	msg.rvr = *m;

	struct iovec bufs[1];
	bufs[0].iov_base = (char *)&msg;
	bufs[0].iov_len = sizeof(msg_t);
	eraft_network_transmit_connection(&evts->network, conn, bufs, 1);
	return 0;
}

/** Raft callback for sending appendentries message */
static int __raft_send_appendentries(
	raft_server_t           *raft,
	void                    *user_data,
	raft_node_t             *node,
	msg_appendentries_t     *m
	)
{
	struct eraft_group      *group = raft_get_udata(raft);
	int                     id = raft_node_get_id(node);
	struct eraft_node       *enode = &group->conf->nodes[id];
	struct eraft_evts       *evts = group->evts;

	eraft_connection_t *conn = eraft_network_find_connection(&evts->network, enode->raft_host, enode->raft_port);

	if (!eraft_network_usable_connection(&evts->network, conn)) {
		return 0;
	}

	msg_t msg = {};
	msg.type = MSG_APPENDENTRIES;
	msg.node_id = group->node_id;
	snprintf(msg.identity, sizeof(msg.identity), "%s", group->identity);
	msg.ae.term = m->term;
	msg.ae.prev_log_idx = m->prev_log_idx;
	msg.ae.prev_log_term = m->prev_log_term;
	msg.ae.leader_commit = m->leader_commit;
	msg.ae.n_entries = m->n_entries;

	struct iovec bufs[(m->n_entries * 2) + 1];
	bufs[0].iov_base = (char *)&msg;
	bufs[0].iov_len = sizeof(msg_t);

	if (0 < m->n_entries) {
		/* appendentries with payload */
		//	printf("pack count ---------------------------------------------------->%d\n", m->n_entries);
		for (int i = 0; i < m->n_entries; i++) {
			bufs[(i * 2) + 1].iov_base = (char *)m->bat->entries[i];
			bufs[(i * 2) + 1].iov_len = sizeof(raft_entry_t);
			bufs[(i * 2) + 2].iov_base = (char *)m->bat->entries[i]->data.buf;
			bufs[(i * 2) + 2].iov_len = m->bat->entries[i]->data.len;
		}

		eraft_network_transmit_connection(&evts->network, conn, bufs, (m->n_entries * 2) + 1);
		// TODO: del bat
	} else {
		/* keep alive appendentries only */
		eraft_network_transmit_connection(&evts->network, conn, bufs, 1);
	}

	return 0;
}

int __raft_send_appendentries_response(
	raft_server_t                   *raft,
	void                            *user_data,
	raft_node_t                     *node,
	msg_appendentries_response_t    *m
	)
{
	struct eraft_group      *group = raft_get_udata(raft);
	int                     id = raft_node_get_id(node);
	struct eraft_node       *enode = &group->conf->nodes[id];
	struct eraft_evts       *evts = group->evts;

	eraft_connection_t *conn = eraft_network_find_connection(&evts->network, enode->raft_host, enode->raft_port);

	if (!eraft_network_usable_connection(&evts->network, conn)) {
		return 0;
	}

	msg_t msg = {};
	msg.type = MSG_APPENDENTRIES_RESPONSE;
	msg.node_id = group->node_id;
	snprintf(msg.identity, sizeof(msg.identity), "%s", group->identity);
	msg.aer = *m;

	/* send response */
	struct iovec bufs[1];
	bufs[0].iov_base = (char *)&msg;
	bufs[0].iov_len = sizeof(msg_t);
	eraft_network_transmit_connection(&evts->network, conn, bufs, 1);
	return 0;
}

static int __send_leave_response(struct eraft_group *group, eraft_connection_t *conn)
{
	struct eraft_evts *evts = group->evts;

	if (!conn) {
		printf("no connection??\n");
		return -1;
	}

	// if (!conn->stream) {
	//	return -1;
	// }

	msg_t msg = {};
	msg.type = MSG_LEAVE_RESPONSE;
	msg.node_id = group->node_id;
	snprintf(msg.identity, sizeof(msg.identity), "%s", group->identity);

	struct iovec bufs[1];
	bufs[0].iov_base = (char *)&msg;
	bufs[0].iov_len = sizeof(msg_t);
	eraft_network_transmit_connection(&evts->network, conn, bufs, 1);
	return 0;
}

static long str2num(char *s)
{
	long num = 0;

	while (*s != '\0') {
		num += *s;
		s++;
	}

	return num;
}

/** Raft callback for applying an entry to the finite state machine */
static int __raft_log_apply(
	raft_server_t   *raft,
	void            *udata,
	raft_batch_t    *batch,
	raft_index_t    start_idx
	)
{
	struct eraft_group      *group = raft_get_udata(raft);
	struct eraft_evts       *evts = group->evts;

	/* Check if it's a configuration change */
	if (batch->n_entries == 1) {
		raft_entry_t *ety = raft_batch_view_entry(batch, 0);

		if (raft_entry_is_cfg_change(ety)) {
			entry_cfg_change_t *change = ety->data.buf;

			if ((RAFT_LOGTYPE_REMOVE_NODE == ety->type) && raft_is_leader(group->raft)) {
				char port[IPV4_PORT_LEN] = {};
				snprintf(port, sizeof(port), "%d", change->raft_port);
				eraft_connection_t *conn = eraft_network_find_connection(&evts->network, change->host, port);
				__send_leave_response(group, conn);
			}

			goto commit;
		}
	}

	struct eraft_taskis_log_apply   *object = eraft_taskis_log_apply_make(group->identity, eraft_evts_dispose_dotask, evts, evts, batch, start_idx);
	long                            hash_key = str2num(group->identity);
	long                            hash_idx = hash_key % MAX_APPLY_WORKER;
	eraft_worker_give(&evts->apply_worker[hash_idx], (struct eraft_dotask *)object);

commit:
	;

	/* We save the commit idx for performance reasons.
	 * Note that Raft doesn't require this as it can figure it out itself. */
	// TODO: open
	// int commit_idx = raft_get_commit_idx(raft);
	// eraft_journal_set_state(&group->journal, "commit_idx", strlen("commit_idx") + 1, (char *)&commit_idx, sizeof(commit_idx));//非sync操作,快照时需要sync操作.

	return 0;
}

/** Raft callback for saving voted_for field to disk.
 * This only returns when change has been made to disk. */
static int __raft_persist_vote(
	raft_server_t   *raft,
	void            *udata,
	raft_node_id_t  voted_for
	)
{
	struct eraft_group *group = raft_get_udata(raft);

	return eraft_journal_set_state(&group->journal, "voted_for", strlen("voted_for") + 1, (char *)&voted_for, sizeof(voted_for));
}

/** Raft callback for saving term field to disk.
 * This only returns when change has been made to disk. */
static int __raft_persist_term(
	raft_server_t   *raft,
	void            *udata,
	raft_term_t     term,
	raft_node_id_t  vote
	)
{
	struct eraft_group *group = raft_get_udata(raft);

	return eraft_journal_set_state(&group->journal, "term", strlen("term") + 1, (char *)&term, sizeof(term));
}

static int __offer_cfg_change(struct eraft_group        *group,
	raft_server_t                                   *raft,
	const unsigned char                             *data,
	raft_logtype_e                                  change_type)
{
	entry_cfg_change_t      *change = (void *)data;
	struct eraft_evts       *evts = group->evts;

	/* Node is being removed */
	if (RAFT_LOGTYPE_REMOVE_NODE == change_type) {
		raft_remove_node(raft, change->node_id);

		// TODO: if all no use delete the connection
		return 0;
	}

	/* Node is being added */
	char raft_port[IPV4_PORT_LEN];
	snprintf(raft_port, sizeof(raft_port), "%d", change->raft_port);
	eraft_connection_t *conn = eraft_network_find_connection(&evts->network, change->host, raft_port);

	// conn->http_port = change->http_port;

	int is_self = change->node_id == group->node_id;

	raft_node_t *node = NULL;
	switch (change_type)
	{
		case RAFT_LOGTYPE_ADD_NONVOTING_NODE:
			node = raft_add_non_voting_node(raft, conn, change->node_id, is_self);
			break;

		case RAFT_LOGTYPE_ADD_NODE:
			node = raft_add_node(raft, conn, change->node_id, is_self);
			break;

		default:
			assert(0);
	}

	raft_node_set_udata(node, conn);

	return 0;
}

int __set_append_log_batch(struct eraft_journal *store, raft_batch_t *bat, int start_idx)
{
#ifdef TEST_NETWORK_ONLY
	return 0;
#endif
	void *txn = eraft_journal_tx_begin(store);

	for (int i = 0; i < bat->n_entries; i++) {
		struct eraft_entry eentry;
		eentry.entry = *bat->entries[i];
		eentry.aid = 0;
		eentry.iid = start_idx + i;

		int num = eraft_journal_set_record(store, txn, start_idx + i, &eentry);

		if (0 == num) {
			eraft_journal_tx_abort(store, txn);
			return -1;
		}
	}

	int e = eraft_journal_tx_commit(store, txn);
	assert(e == 0);

	return 0;
}

int __get_append_log(struct eraft_journal *store, raft_entry_t *ety, int ety_idx)
{
#ifdef TEST_NETWORK_ONLY
	return 0;
#endif
	void *txn = eraft_journal_tx_begin(store);

	struct eraft_entry eentry;

	int num = eraft_journal_get_record(store, txn, ety_idx, &eentry);

	if (num) {
		*ety = eentry.entry;
	}

	int e = eraft_journal_tx_commit(store, txn);
	assert(e == 0);

	return 0;
}

/** Raft callback for appending an item to the log */
static int __raft_log_append(
	raft_server_t   *raft,
	void            *user_data,
	raft_batch_t    *batch,
	raft_index_t    start_idx,
	raft_node_t     *node,
	raft_index_t    leader_commit,
	raft_index_t    rsp_first_idx
	)
{
	struct eraft_group      *group = raft_get_udata(raft);
	struct eraft_evts       *evts = (struct eraft_evts *)group->evts;

	if (batch->n_entries == 1) {
		raft_entry_t *ety = raft_batch_view_entry(batch, 0);

		if (raft_entry_is_cfg_change(ety)) {
			__offer_cfg_change(group, raft, ety->data.buf, ety->type);
		}
	}

	struct eraft_taskis_log_append *object = eraft_taskis_log_append_make(group->identity, eraft_evts_dispose_dotask, evts, evts, &group->journal, batch, start_idx, node, leader_commit, rsp_first_idx);

	long    hash_key = str2num(group->identity);
	long    hash_idx = hash_key % MAX_JOURNAL_WORKER;
	eraft_worker_give(&evts->journal_worker[hash_idx], (struct eraft_dotask *)object);

	return 0;
}

int __raft_log_retain(
	raft_server_t   *raft,
	void            *user_data,
	raft_batch_t    *batch,
	raft_index_t    start_idx,
	void            *usr
	)
{
	struct eraft_group      *group = raft_get_udata(raft);
	struct eraft_evts       *evts = (struct eraft_evts *)group->evts;

	if (batch->n_entries == 1) {
		raft_entry_t *ety = raft_batch_view_entry(batch, 0);

		if (raft_entry_is_cfg_change(ety)) {
			__offer_cfg_change(group, raft, ety->data.buf, ety->type);
		}
	}

	printf("log_retain working!\n");
	struct eraft_taskis_log_retain *object = eraft_taskis_log_retain_make(group->identity, eraft_evts_dispose_dotask, evts, evts, &group->journal, batch, start_idx, usr);

	long    hash_key = str2num(group->identity);
	long    hash_idx = hash_key % MAX_JOURNAL_WORKER;
	eraft_worker_give(&evts->journal_worker[hash_idx], (struct eraft_dotask *)object);

	return 0;
}

int __raft_log_retain_done(
	raft_server_t   *raft,
	void            *user_data,
	int             result,
	raft_term_t     term,
	raft_index_t    start_idx,
	raft_index_t    end_idx,
	void            *usr
	)
{
	struct eraft_group      *group = raft_get_udata(raft);
	struct eraft_evts       *evts = (struct eraft_evts *)group->evts;

	// TODO: if result is not ok;
	struct eraft_taskis_request_write *object = (struct eraft_taskis_request_write *)usr;

	LIST_HEAD(do_list);
	list_splice_init(((struct list_head *)&object->base.node), &do_list);

	int idx = start_idx;
	// printf("--->%d\n", idx);
	object->efd = etask_tree_make_task(evts->wait_idx_tree, &idx, sizeof(idx));
	object->idx = idx;
	etask_awake(object->etask);

	struct eraft_taskis_request_write *child = NULL;
	list_for_each_entry(child, &do_list, base.node)
	{
		idx++;
		assert(idx <= end_idx);

		// printf("--->%d\n", idx);
		child->efd = etask_tree_make_task(evts->wait_idx_tree, &idx, sizeof(idx));
		child->idx = idx;
		etask_awake(child->etask);
	}
	return 0;
}

int __raft_log_remind(
	raft_server_t   *raft,
	void            *user_data,
	raft_batch_t    *batch,
	raft_index_t    start_idx,
	void            *usr
	)
{
	struct eraft_group      *group = raft_get_udata(raft);
	struct eraft_evts       *evts = (struct eraft_evts *)group->evts;

	printf("log_remind working!\n");
	struct eraft_taskis_log_remind *object = eraft_taskis_log_remind_make(group->identity, eraft_evts_dispose_dotask, evts, evts, batch, start_idx, usr);

	long    hash_key = str2num(group->identity);
	long    hash_idx = hash_key % MAX_JOURNAL_WORKER;
	eraft_worker_give(&evts->journal_worker[hash_idx], (struct eraft_dotask *)object);

	return 0;
}

/** Raft callback for removing the first entry from the log
 * @note this is provided to support log compaction in the future */
static int __raft_logentry_poll(
	raft_server_t   *raft,
	void            *udata,
	raft_entry_t    *entry,
	raft_index_t    ety_idx
	)
{
	// struct eraft_group *group = raft_get_udata(raft);

	// __pop_oldest_log(group->lmdb);

	return 0;
}

/** Raft callback for deleting the most recent entry from the log.
 * This happens when an invalid leader finds a valid leader and has to delete
 * superseded log entries. */
static int __raft_logentry_pop(
	raft_server_t   *raft,
	void            *udata,
	raft_entry_t    *entry,
	raft_index_t    ety_idx
	)
{
	// struct eraft_group *group = raft_get_udata(raft);

	// __pop_newest_log(group->lmdb);

	return 0;
}

/** Non-voting node now has enough logs to be able to vote.
 * Append a finalization cfg log entry. */
static int __raft_node_has_sufficient_logs(
	raft_server_t   *raft,
	void            *user_data,
	raft_node_t     *node)
{
	struct eraft_group      *group = raft_get_udata(raft);
	int                     id = raft_node_get_id(node);
	struct eraft_node       *enode = &group->conf->nodes[id];

	// struct eraft_evts *evts = (struct eraft_evts *)group->evts;
	// eraft_connection_t *conn = raft_node_get_udata(node);
	// inet_ntoa(conn->addr.sin_addr)

	__append_cfg_change(group, RAFT_LOGTYPE_ADD_NODE,
		enode->raft_host,
		atoi(enode->raft_port),
		atoi(enode->raft_port) + 1000,
		id);
	return 0;
}

/** Raft callback for displaying debugging information */
void __raft_log(raft_server_t *raft, raft_node_t *node, void *udata,
	const char *buf)
{
	if (1) {
		printf("raft: %s\n", buf);
	}
}

raft_cbs_t g_default_raft_funcs = {
	.send_requestvote               = __raft_send_requestvote,
	.send_requestvote_response      = __raft_send_requestvote_response,
	.send_appendentries             = __raft_send_appendentries,
	.send_appendentries_response    = __raft_send_appendentries_response,
	.persist_vote                   = __raft_persist_vote,
	.persist_term                   = __raft_persist_term,

	.log_retain                     = __raft_log_retain,
	.log_retain_done                = __raft_log_retain_done,
	.log_remind                     = __raft_log_remind,
	.log_append                     = __raft_log_append,
	.log_poll                       = __raft_logentry_poll,
	.log_pop                        = __raft_logentry_pop,
	.log_apply                      = __raft_log_apply,
	.log_get_node_id                = __raft_log_get_node_id,

	.node_has_sufficient_logs       = __raft_node_has_sufficient_logs,
	.log                            = __raft_log,
};

/*****************************************************************************/
static void __send_handshake(struct eraft_evts *evts, struct eraft_group *group, eraft_connection_t *conn)
{
	struct eraft_node *enode = eraft_group_get_self_node(group);

	msg_t msg = {};

	msg.type = MSG_HANDSHAKE;
	msg.node_id = group->node_id;
	snprintf(msg.identity, sizeof(msg.identity), "%s", group->identity);
	msg.hs.raft_port = atoi(enode->raft_port);
	msg.hs.http_port = atoi(enode->raft_port) + 1000;
	msg.hs.node_id = group->node_id;

	struct iovec bufs[1];
	bufs[0].iov_base = (char *)&msg;
	bufs[0].iov_len = sizeof(msg_t);
	eraft_network_transmit_connection(&evts->network, conn, bufs, 1);
}

static bool __connected_for_lookup_fcb(struct eraft_group *group, size_t idx, void *usr)
{
	struct _on_network_info *info = usr;

	__send_handshake(info->evts, group, info->conn);
	return true;
}

void _on_connected_fcb(eraft_connection_t *conn, void *usr)
{
	struct eraft_evts *evts = usr;

	struct _on_network_info info = { .evts = evts, .conn = conn };

	eraft_multi_foreach_group(&evts->multi, __connected_for_lookup_fcb, NULL, &info);
}

/*****************************************************************************/
bool __periodic_for_lookup_fcb(struct eraft_group *group, size_t idx, void *usr)
{
	// struct eraft_evts *evts = usr;

	if (0 || (raft_get_current_leader(group->raft) == -1)) {
		raft_periodic(group->raft, PERIOD_MSEC);
	}

	return true;
}

/** Raft callback for handling periodic logic */
static void _periodic_evcb(struct ev_loop *loop, ev_periodic *w, int revents)
{
	struct eraft_evts *evts = w->data;

	for (int i = 0; i < 0; ++i) {
		// TODO: if not connect, reconnect.
	}

	eraft_multi_foreach_group(&evts->multi, __periodic_for_lookup_fcb, NULL, evts);
}

static void _start_raft_periodic_timer(struct eraft_evts *evts)
{
	struct ev_periodic *p_periodic_watcher = &evts->periodic_watcher;

	p_periodic_watcher->data = evts;
	ev_periodic_init(p_periodic_watcher, _periodic_evcb, 0, PERIOD_MSEC / 1000, 0);
	ev_periodic_start(evts->loop, p_periodic_watcher);
}

static void _stop_raft_periodic_timer(struct eraft_evts *evts)
{
	struct ev_periodic *p_periodic_watcher = &evts->periodic_watcher;

	ev_periodic_stop(evts->loop, p_periodic_watcher);
}

/*****************************************************************************/
struct eraft_evts *eraft_evts_make(struct eraft_evts *evts, int self_port)
{
	if (evts) {
		bzero(evts, sizeof(*evts));
		evts->canfree = false;
	} else {
		New(evts);
		assert(evts);
		evts->canfree = true;
	}

	/* eventfd by callback register in rbtree. */
	evts->wait_idx_tree = etask_tree_make();

	/*初始化事件loop*/
	struct evcoro_scheduler *p_scheduler = evcoro_get_default_scheduler();
	assert(p_scheduler);
	evts->scheduler = p_scheduler;
	evts->loop = p_scheduler->listener;

	/*开启周期定时器*/
	_start_raft_periodic_timer(evts);

	/*绑定端口,开启raft服务*/
	int e = eraft_network_init(&evts->network, ERAFT_NETWORK_TYPE_LIBCOMM, self_port, _on_connected_fcb, NULL, NULL, _on_transmit_fcb, evts);
	assert(0 == e);

	eraft_tasker_once_init(&evts->tasker, evts->loop);

	for (int i = 0; i < MAX_JOURNAL_WORKER; i++) {
		eraft_worker_init(&evts->journal_worker[i]);
	}

	for (int i = 0; i < MAX_APPLY_WORKER; i++) {
		eraft_worker_init(&evts->apply_worker[i]);
	}

	eraft_multi_init(&evts->multi);

	evts->init = true;

	return evts;
}

void eraft_evts_free(struct eraft_evts *evts)
{
	if (evts && evts->init) {
		etask_tree_free(evts->wait_idx_tree);

		_stop_raft_periodic_timer(evts);

		eraft_network_free(&evts->network);

		eraft_tasker_once_free(&evts->tasker);

		for (int i = 0; i < MAX_JOURNAL_WORKER; i++) {
			eraft_worker_free(&evts->journal_worker[i]);
		}

		for (int i = 0; i < MAX_APPLY_WORKER; i++) {
			eraft_worker_free(&evts->apply_worker[i]);
		}

		eraft_multi_free(&evts->multi);

		evts->init = false;

		if (evts->canfree) {
			Free(evts);
		}
	}
}

static void one_loop_cb(struct evcoro_scheduler *scheduler, void *usr)
{}

void eraft_evts_once(struct eraft_evts *evts)
{
	evcoro_once(evts->scheduler, one_loop_cb, NULL);
}

/*****************************************************************************/
#if 0
static void __send_leave(eraft_connection_t *conn)
{
	msg_t msg = {};

	msg.type = MSG_LEAVE;

	struct iovec bufs[1];
	bufs[0].iov_base = (char *)&msg;
	bufs[0].iov_len = sizeof(msg_t);
	eraft_network_transmit_connection(&evts->network, conn, bufs, 1);
}

#endif

void do_merge_task(struct eraft_group *group)
{
	if (list_empty(&group->merge_list)) {
		return;
	}

	/*没有冲突的读写可以并行执行*/
	struct eraft_dotask *first = list_first_entry(&group->merge_list, struct eraft_dotask, node);

	if (first->type == ERAFT_TASK_REQUEST_WRITE) {
		/*写需要串行执行*/
		if (group->merge_task_state == MERGE_TASK_STATE_STOP) {
			return;
		}
	}

	/*采取所有执行任务*/
	LIST_HEAD(do_list);
	list_splice_init(&group->merge_list, &do_list);

	/*摘取第一个task*/
	first = list_first_entry(&do_list, struct eraft_dotask, node);
	list_del(&first->node);

	assert(sizeof(struct list_head) == sizeof(struct list_node));
	struct list_head *head = (struct list_head *)&first->node;
	INIT_LIST_HEAD(head);

	/*摘取一致的task*/
	struct eraft_dotask *child = NULL;
	list_for_each_entry(child, &do_list, node)
	{
		if (first->type == child->type) {
			list_del(&child->node);
			list_add_tail(&child->node, head);
		} else {
			break;
		}
	}

	/*放置回去*/
	list_splice_init(&do_list, &group->merge_list);

	int type = first->type;

	if (type == ERAFT_TASK_REQUEST_WRITE) {
		/*统计条数*/
		int num = 1;

		if (!list_empty(head)) {
			list_for_each_entry(child, head, node)
			{
				num++;
			}
		}

		/*合并请求*/
		raft_batch_t *bat = raft_batch_make(num);

		struct eraft_taskis_request_write       *object = list_entry(first, struct eraft_taskis_request_write, base);
		raft_entry_t                            *ety = raft_entry_make(raft_get_current_term(group->raft),
				rand(), RAFT_LOGTYPE_NORMAL,
				object->request->iov_base, object->request->iov_len);
		raft_batch_join_entry(bat, 0, ety);

		if (!list_empty(head)) {
			int i = 1;
			list_for_each_entry(child, head, node)
			{
				object = list_entry(child, struct eraft_taskis_request_write, base);
				ety = raft_entry_make(raft_get_current_term(group->raft),
						rand(), RAFT_LOGTYPE_NORMAL,
						object->request->iov_base, object->request->iov_len);
				raft_batch_join_entry(bat, i++, ety);
			}
		}

		/*停止新请求*/
		group->merge_task_state = MERGE_TASK_STATE_STOP;
		printf("===stop self===\n");

		/*if success, log_retain will be called.*/
		g_default_raft_funcs.log_retain = __raft_log_retain;
		/*if failed, log_retain_done will be called.*/
		g_default_raft_funcs.log_retain_done = __raft_log_retain_done;
		/*start*/
		int e = raft_retain_entries(group->raft, bat, first);

		if (0 != e) {
			printf("errno is %d\n", e);
		}
	}

	if (type == ERAFT_TASK_REQUEST_READ) {
		int e = raft_remind_entries(group->raft, first);

		if (0 != e) {
			abort();
		}
	}
}

void eraft_evts_dispose_dotask(struct eraft_dotask *task, void *usr)
{
	struct eraft_evts *evts = usr;

	switch (task->type)
	{
		/*====================each worker====================*/
		case ERAFT_TASK_REQUEST_WRITE:
		case ERAFT_TASK_REQUEST_READ:
		{
			struct eraft_group *group = eraft_multi_get_group(&evts->multi, task->identity);

			list_add_tail(&task->node, &group->merge_list);

			do_merge_task(group);
		}
		break;

		case ERAFT_TASK_NET_APPEND:
		{
			struct eraft_taskis_net_append  *object = (struct eraft_taskis_net_append *)task;
			struct eraft_group              *group = eraft_multi_get_group(&evts->multi, object->base.identity);

			if (object->ae->n_entries) {
				eraft_tasker_each_stop(&group->peer_tasker);
				printf("===stop peer===\n");
			}

			/* this is a keep alive message */	// TODO:call?
			int e = raft_recv_appendentries(group->raft, object->node, object->ae);
			assert(e == 0);
			eraft_taskis_net_append_free(object);
		}
		break;

		/*====================once worker====================*/
		case ERAFT_TASK_GROUP_ADD:
		{
			struct eraft_taskis_group_add   *object = (struct eraft_taskis_group_add *)task;
			struct eraft_group              *group = object->group;

			// eraft_tasker_each_init(&group->self_tasker, evts->loop);
			eraft_tasker_each_init(&group->peer_tasker, evts->loop);
			/* Rejoin cluster */
			eraft_multi_add_group(&evts->multi, group);

			/*连接其它节点*/
			for (int i = 0; i < raft_get_num_nodes(group->raft); i++) {
				raft_node_t *node = raft_get_node_by_idx(group->raft, i);

				if (raft_node_get_id(node) == group->node_id) {
					continue;
				}

				/*创建连接*/
				assert(i < group->conf->num_nodes);
				struct eraft_node *enode = &group->conf->nodes[i];

				eraft_connection_t *conn = eraft_network_find_connection(&evts->network, enode->raft_host, enode->raft_port);
				raft_node_set_udata(node, conn);
			}

			etask_awake(object->etask);
		}
		break;

		case ERAFT_TASK_GROUP_DEL:
		{
			struct eraft_taskis_group_del *object = (struct eraft_taskis_group_del *)task;

			struct eraft_group *group = eraft_multi_del_group(&evts->multi, object->base.identity);
			eraft_group_free(group);

			etask_awake(object->etask);
		}
		break;

#if 0
		case ERAFT_TASK_RELATIONSHIP_DEL:
			raft_node_t *leader = raft_get_current_leader_node(group->raft);

			if (leader) {
				if (raft_node_get_id(leader) == group->node_id) {
					printf("I'm the leader, leave the cluster...\n");
				} else {
					eraft_connection_t *leader_conn = raft_node_get_udata(leader);

					if (leader_conn) {
						printf("Leaving cluster...\n");
						__send_leave(leader_conn);
					}
				}
			} else {
				printf("Try again no leader at the moment...\n");
			}
			break;
#endif
		case ERAFT_TASK_LOG_RETAIN_DONE:
		{
			struct eraft_taskis_log_retain_done     *object = (struct eraft_taskis_log_retain_done *)task;
			struct eraft_group                      *group = eraft_multi_get_group(&evts->multi, object->base.identity);

			int n_entries = object->batch->n_entries;
			raft_dispose_entries_cache(group->raft, true, object->batch, object->start_idx);

			/*first will call send_appendentries*/
			g_default_raft_funcs.send_appendentries = __raft_send_appendentries;
			/*then will call log_retain_done*/
			g_default_raft_funcs.log_retain_done = __raft_log_retain_done;
			/*start*/
			raft_async_retain_entries_finish(group->raft, 0, n_entries, object->usr);

			eraft_taskis_log_retain_done_free(object);

			group->merge_task_state = MERGE_TASK_STATE_WORK;
			printf("===call self===\n");
			do_merge_task(group);
		}
		break;

		case ERAFT_TASK_LOG_APPEND_DONE:
		{
			struct eraft_taskis_log_append_done     *object = (struct eraft_taskis_log_append_done *)task;
			struct eraft_group                      *group = eraft_multi_get_group(&evts->multi, object->base.identity);

			raft_index_t curr_idx = raft_dispose_entries_cache(group->raft, true, object->batch, object->start_idx);

			raft_async_append_entries_finish(group->raft, object->raft_node, true, object->leader_commit, 1, curr_idx, object->rsp_first_idx);

			eraft_taskis_log_append_done_free(object);

			eraft_tasker_each_call(&group->peer_tasker);
			printf("===call peer===\n");
		}
		break;

		case ERAFT_TASK_LOG_APPLY_DONE:
		{
			struct eraft_taskis_log_apply_done      *object = (struct eraft_taskis_log_apply_done *)task;
			struct eraft_group                      *group = eraft_multi_get_group(&evts->multi, object->base.identity);

			raft_async_apply_entries_finish(group->raft, true, object->batch, object->start_idx);

			eraft_taskis_log_apply_done_free(object);
		}
		break;

		case ERAFT_TASK_NET_APPEND_RESPONSE:
		{
			struct eraft_taskis_net_append_response *object = (struct eraft_taskis_net_append_response *)task;
			struct eraft_group                      *group = eraft_multi_get_group(&evts->multi, object->base.identity);
			int                                     e = raft_recv_appendentries_response(group->raft, object->node, object->aer);
			assert(e == 0);
			/*FIXME*/
			int     first_idx = object->aer->first_idx;
			int     over_idx = raft_get_commit_idx(group->raft);

			for (int id = first_idx; id <= over_idx; id++) {
				// printf("<---%d\n", id);
				etask_tree_awake_task(evts->wait_idx_tree, &id, sizeof(id));
			}

			eraft_taskis_net_append_response_free(object);
		}
		break;

		case ERAFT_TASK_NET_VOTE:
		{
			struct eraft_taskis_net_vote    *object = (struct eraft_taskis_net_vote *)task;
			struct eraft_group              *group = eraft_multi_get_group(&evts->multi, object->base.identity);
			int                             e = raft_recv_requestvote(group->raft, object->node, object->rv);
			assert(e == 0);
			eraft_taskis_net_vote_free(object);
		}
		break;

		case ERAFT_TASK_NET_VOTE_RESPONSE:
		{
			struct eraft_taskis_net_vote_response   *object = (struct eraft_taskis_net_vote_response *)task;
			struct eraft_group                      *group = eraft_multi_get_group(&evts->multi, object->base.identity);
			int                                     e = raft_recv_requestvote_response(group->raft, object->node, object->rvr);
			assert(e == 0);
			printf("Leader is %d\n", raft_get_current_leader(group->raft));
			eraft_taskis_net_vote_response_free(object);
		}
		break;

		/*====================log worker====================*/
		case ERAFT_TASK_LOG_RETAIN:
		{
			struct eraft_taskis_log_retain *object = (struct eraft_taskis_log_retain *)task;
			printf("-----%d\n", object->start_idx);
			int e = __set_append_log_batch(object->journal, object->batch, object->start_idx);
			assert(0 == e);

			/*移交给raft线程去处理*/
			struct eraft_taskis_log_retain_done *new_task = eraft_taskis_log_retain_done_make(object->base.identity, eraft_evts_dispose_dotask, evts, object->batch, object->start_idx, object->usr);
			eraft_tasker_once_give(&object->evts->tasker, (struct eraft_dotask *)new_task);

			eraft_taskis_log_retain_free(object);
		}
		break;

		case ERAFT_TASK_LOG_REMIND:
		{
			struct eraft_taskis_log_remind *object = (struct eraft_taskis_log_remind *)task;
			printf("-----%d\n", object->start_idx);

			struct eraft_dotask *first = (struct eraft_dotask *)object->usr;
			assert(first->type == ERAFT_TASK_REQUEST_READ);
			struct list_head *head = (struct list_head *)&first->node;

			struct eraft_dotask *child = NULL;
			/*统计条数*/
			int new_count = 1;

			if (!list_empty(head)) {
				list_for_each_entry(child, head, node)
				{
					new_count++;
				}
			}

			struct iovec *new_requests = calloc(new_count, sizeof(struct iovec));

			struct eraft_taskis_request_read *obj = list_entry(first, struct eraft_taskis_request_read, base);
			new_requests[0].iov_base = obj->request->iov_base;
			new_requests[0].iov_len = obj->request->iov_len;

			if (!list_empty(head)) {
				int i = 1;
				list_for_each_entry(child, head, node)
				{
					obj = list_entry(child, struct eraft_taskis_request_read, base);
					new_requests[i].iov_base = obj->request->iov_base;
					new_requests[i].iov_len = obj->request->iov_len;
					i++;
				}
			}

			struct eraft_group *group = eraft_multi_get_group(&object->evts->multi, object->base.identity);

			{
				raft_batch_t *bat = object->batch;

				int             old_count = bat->n_entries;
				struct iovec    *old_requests = NULL;

				if (old_count) {
					old_requests = calloc(old_count, sizeof(struct iovec));

					for (int i = 0; i < old_count; i++) {
						raft_entry_t *ety = raft_batch_view_entry(bat, i);

						old_requests[i].iov_base = ety->data.buf;
						old_requests[i].iov_len = ety->data.len;
					}
				}

				if (group->log_apply_rfcb) {
					group->log_apply_rfcb(group, old_requests, old_count, new_requests, new_count);
				}

				if (old_count) {
					free(old_requests);

					for (int i = 0; i < old_count; i++) {
						raft_entry_t *ety = raft_batch_take_entry(bat, i);

						raft_entry_free(ety);
					}
				}

				raft_batch_free(bat);
			}

			if (!list_empty(head)) {
				list_for_each_entry(child, head, node)
				{
					list_del(&child->node);
					obj = list_entry(child, struct eraft_taskis_request_read, base);
					etask_awake(obj->etask);
				}
			}

			obj = list_entry(first, struct eraft_taskis_request_read, base);
			etask_awake(obj->etask);

			free(new_requests);

			eraft_taskis_log_remind_free(object);
		}
		break;

		case ERAFT_TASK_LOG_APPEND:
		{
			struct eraft_taskis_log_append *object = (struct eraft_taskis_log_append *)task;
			// printf("-----%d\n", start_idx);
			int e = __set_append_log_batch(object->journal, object->batch, object->start_idx);
			assert(0 == e);

			/*移交给raft线程去处理*/
			struct eraft_taskis_log_append_done *new_task = eraft_taskis_log_append_done_make(object->base.identity, eraft_evts_dispose_dotask, evts, object->evts, object->batch, object->start_idx, object->raft_node, object->leader_commit, object->rsp_first_idx);
			eraft_tasker_once_give(&object->evts->tasker, (struct eraft_dotask *)new_task);

			eraft_taskis_log_append_free(object);
		}
		break;

		case ERAFT_TASK_LOG_APPLY:
		{
			struct eraft_taskis_log_apply   *object = (struct eraft_taskis_log_apply *)task;
			struct eraft_group              *group = eraft_multi_get_group(&object->evts->multi, object->base.identity);

			raft_batch_t *bat = object->batch;

			int             new_count = bat->n_entries;
			struct iovec    *new_requests = NULL;

			if (new_count) {
				new_requests = calloc(new_count, sizeof(struct iovec));

				for (int i = 0; i < new_count; i++) {
					raft_entry_t *ety = raft_batch_view_entry(bat, i);

					new_requests[i].iov_base = ety->data.buf;
					new_requests[i].iov_len = ety->data.len;
				}
			}

			if (group->log_apply_wfcb) {
				group->log_apply_wfcb(group, new_requests, new_count);
			}

			if (new_count) {
				free(new_requests);
			}

			/*移交给raft线程去处理*/
			struct eraft_taskis_log_apply_done *new_task = eraft_taskis_log_apply_done_make(group->identity, eraft_evts_dispose_dotask, evts, object->batch, object->start_idx);
			eraft_tasker_once_give(&object->evts->tasker, (struct eraft_dotask *)new_task);

			eraft_taskis_log_apply_free(object);
		}
		break;

		default:
			abort();
	}
}


/**
 * Copyright (c) 2015, Willem-Hendrik Thiart
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include "lmdb.h"
#include "lmdb_helpers.h"

#include "usage.h"
#include "timeopt.h"
#include "eraft_api.h"
#include "http_context.h"

typedef struct
{
	struct eraft_context    *eraft_ctx;

	struct http_context     *http_ctx;

	/* LMDB database environment */
	MDB_env                 *db_env;

	/* Set of tickets that have been issued
	 * We store unsigned ints in here */
	MDB_dbi                 tickets;
} service_t;

static options_t        g_opts;
static service_t        g_serv;

static void service_store_init(service_t *service, char *db_path, int db_size)
{
	mdb_db_env_create(&service->db_env, 0, db_path, db_size);
	mdb_db_create(&service->tickets, service->db_env, "tickets");
}

/** Check if the ticket has already been issued
 * @return true if not unique; otherwise false */
static bool __check_if_ticket_exists(service_t *service, const unsigned int ticket)
{
	MDB_val key = { .mv_size = sizeof(ticket), .mv_data = (void *)&ticket };
	MDB_val val;

	MDB_txn *txn;

	int e = mdb_txn_begin(service->db_env, NULL, MDB_RDONLY, &txn);

	if (0 != e) {
		mdb_fatal(e);
	}

	e = mdb_get(txn, service->tickets, &key, &val);
	switch (e)
	{
		case 0:
			e = mdb_txn_commit(txn);

			if (0 != e) {
				mdb_fatal(e);
			}

			return true;

		case MDB_NOTFOUND:
			e = mdb_txn_commit(txn);

			if (0 != e) {
				mdb_fatal(e);
			}

			return false;

		default:
			mdb_fatal(e);
	}
	return false;
}

static unsigned int __generate_ticket(service_t *service)
{
	unsigned int ticket;

	do {
		ticket = rand();
	} while (__check_if_ticket_exists(service, ticket));
	return ticket;
}

static int __save_ticket(service_t *service, const unsigned int ticket)
{
	MDB_val key = { .mv_size = sizeof(ticket), .mv_data = (void *)&ticket };
	MDB_val val = { .mv_size = 0, .mv_data = "\0" };

	MDB_txn *txn;

	int e = mdb_txn_begin(service->db_env, NULL, 0, &txn);

	if (0 != e) {
		mdb_fatal(e);
	}

	e = mdb_put(txn, service->tickets, &key, &val, 0);
	switch (e)
	{
		case 0:
			break;

		case MDB_MAP_FULL:
		{
			mdb_txn_abort(txn);
			return -1;
		}

		default:
			mdb_fatal(e);
	}

	e = mdb_txn_commit(txn);

	if (0 != e) {
		mdb_fatal(e);
	}

	return 0;
}

static int __log_apply_fcb(struct eraft_group *group, raft_batch_t *batch, raft_index_t start_idx)
{
#if 0
	assert(entry->data.len == sizeof(unsigned int));
	unsigned int ticket = *(unsigned int *)entry->data.buf;

	/* This log affects the ticketd state machine */
	__save_ticket(&g_serv, ticket);
#endif
	return 0;
}

/** HTTP POST entry point for receiving entries from client
 * Provide the user with an ID */
static int __http_get_id(h2o_handler_t *self, h2o_req_t *req)
{
	static h2o_generator_t generator = { NULL, NULL };

	if (!h2o_memis(req->method.base, req->method.len, H2O_STRLIT("POST"))) {
		return -1;
	}

	struct eraft_group      *group = eraft_multi_get_group(&g_serv.eraft_ctx->evts.multi, g_opts.cluster);
	raft_node_t             *leader = raft_get_current_leader_node(group->raft);

	if (!leader) {
		return h2oh_respond_with_error(req, 503, "Leader unavailable");
	} else if (raft_node_get_id(leader) != group->node_id) {
		int                     id = raft_node_get_id(leader);
		struct eraft_node       *enode = &group->conf->nodes[id];
		char                    *host = enode->raft_host;
		char                    *port = enode->raft_port;

		static h2o_generator_t  generator = { NULL, NULL };
		static h2o_iovec_t      body = { .base = "", .len = 0 };
		req->res.status = 301;
		req->res.reason = "Moved Permanently";
		h2o_start_response(req, &generator);

#define LEADER_URL_LEN 512
		char leader_url[LEADER_URL_LEN];
		snprintf(leader_url, LEADER_URL_LEN, "http://%s:%d/", host, atoi(port) + 1000);

		h2o_add_header(&req->pool,
			&req->res.headers,
			H2O_TOKEN_LOCATION,
			NULL,
			leader_url,
			strlen(leader_url));
		h2o_send(req, &body, 1, 1);
		return 0;
	} else {
		unsigned int ticket = __generate_ticket(&g_serv);

		struct iovec request = { .iov_base = (void *)&ticket, .iov_len = sizeof(ticket) };

		/* block until the request is committed */
		erapi_write_request(g_serv.eraft_ctx, g_opts.cluster, &request);

		/* serialize ID */
		char id_str[100];
		sprintf(id_str, "%ld", ticket);
		h2o_iovec_t body = h2o_iovec_init(id_str, strlen(id_str));

		req->res.status = 200;
		req->res.reason = "OK";
		h2o_start_response(req, &generator);
		h2o_send(req, &body, 1, 1);
		return 0;
	}
}

static void _int_handler(int dummy)
{
	// eraft_context_task_give(g_serv.eraft_ctx, NULL, ERAFT_TASK_GROUP_EMPTY);

	erapi_ctx_destroy(g_serv.eraft_ctx);
}

static void _main_env_init(void)
{
	/*设置随机种子*/
	srand(time(NULL));

	service_store_init(&g_serv, "store", 1000);
}

static void _main_env_exit(void)
{}

int main(int argc, const char *const argv[])
{
	memset(&g_serv, 0, sizeof(service_t));

	int e = parse_options(argc, argv, &g_opts);

	if (-1 == e) {
		exit(-1);
	}

	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, _int_handler);

	_main_env_init();

	char    self_host[IPV4_HOST_LEN] = { 0 };
	char    self_port[IPV4_PORT_LEN] = { 0 };
	erapi_get_node_info(g_opts.cluster, atoi(g_opts.id), self_host, self_port);

	int raft_port = atoi(self_port);

	/*创建eraft上下文*/
	g_serv.eraft_ctx = erapi_ctx_create(raft_port);
	/*创建cluster服务*/
	struct eraft_group *group = erapi_add_group(g_serv.eraft_ctx, g_opts.cluster, atoi(g_opts.id),
			g_opts.db_path, atoi(g_opts.db_size), __log_apply_fcb);
	assert(group);

	/*设置http_port*/
	int http_port = raft_port + 1000;

	/*创建http服务*/
	g_serv.http_ctx = http_context_create(http_port, __http_get_id);

	do {
		sleep(1);
	} while (1);

	erapi_del_group(g_serv.eraft_ctx, g_opts.cluster);
	erapi_ctx_destroy(g_serv.eraft_ctx);
	_main_env_exit();
}


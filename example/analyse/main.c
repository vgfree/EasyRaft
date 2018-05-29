/**
 * Copyright (c) 2015, Willem-Hendrik Thiart
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include "usage.h"
#include "timeopt.h"
#include "eraft_confs.h"
#include "eraft_context.h"


//#define TEST_TWO_NET

typedef struct
{
	struct eraft_context	*eraft_ctx;
#ifdef TEST_TWO_NET
	struct eraft_context	*eraft_ctx2;
#endif

} service_t;


static options_t       g_opts;
static service_t       g_serv;





static int __applylog_fcb(struct eraft_group *group, raft_entry_t *entry)
{
	return 0;
}

static char g_send_data[4 << 10] = {0};
//static char g_send_data[256] = {0};
static void *_start_test(void *usr)
{
#ifdef TEST_TWO_NET
	int *idx = (int *)usr;
#endif
	for (int i = 0; i < 2000; i ++) {
		msg_entry_t entry = {};
		//entry.id = 0;
		entry.id = rand();
		entry.data.buf = (void *)g_send_data;
		entry.data.len = sizeof(g_send_data);

		/* block until the entry is committed */
#ifdef TEST_TWO_NET
		if (*idx % 2) {
			eraft_context_dispose_send_entry(g_serv.eraft_ctx, g_opts.cluster, &entry);
		} else {
			eraft_context_dispose_send_entry(g_serv.eraft_ctx2, "192.168.108.108:8000,192.168.108.109:8001,192.168.108.110:8002", &entry);
		}
#else
		eraft_context_dispose_send_entry(g_serv.eraft_ctx, g_opts.cluster, &entry);
#endif
		//usleep(100);
		//printf("???????????????????????\n");
	}
	return NULL;
}
void test_iops(void)
{
	struct timespec beg;
	time_now(&beg);

#define MAX_TEST_CLI	4096
	pthread_t ptids[MAX_TEST_CLI] = {0};
	int idx[MAX_TEST_CLI] = {0};
	for (int i = 0; i < MAX_TEST_CLI; i++) {
		idx[i] = i;
		pthread_create(&ptids[i], NULL, _start_test, &idx[i]);
	}
	for (int i = 0; i < MAX_TEST_CLI; i++) {
		if (pthread_join(ptids[i], NULL) != 0) {
			abort();
		}
	}

	struct timespec end;
	time_now(&end);

	printf("test use (%ld)!\n", time_diff(&beg, &end));
}





static void _int_handler(int dummy)
{
	//eraft_context_task_give(g_serv.eraft_ctx, NULL, ERAFT_TASK_GROUP_EMPTY);

	eraft_context_destroy(g_serv.eraft_ctx);
}



static void _main_env_init(void)
{
	/*设置随机种子*/
	srand(time(NULL));
}

static void _main_env_exit(void)
{
}

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

	/*创建cluster*/
	struct eraft_group *group = eraft_group_make(g_opts.cluster, atoi(g_opts.id), g_opts.db_path, atoi(g_opts.db_size), __applylog_fcb);

	int raft_port = atoi(eraft_group_get_self_node(group)->raft_port);

	/*创建eraft上下文*/
	g_serv.eraft_ctx = eraft_context_create(raft_port);
	/*运行cluster*/
	eraft_context_dispose_add_group(g_serv.eraft_ctx, group);
#ifdef TEST_TWO_NET
	struct eraft_group *group2 = eraft_group_make("192.168.108.108:8000,192.168.108.109:8001,192.168.108.110:8002", atoi(g_opts.id), "store2", atoi(g_opts.db_size), __applylog_fcb);
	g_serv.eraft_ctx2 = eraft_context_create(raft_port + 2000);
	eraft_context_dispose_add_group(g_serv.eraft_ctx2, group2);
#endif


	sleep(30);
	do {
		struct eraft_group *group = eraft_multi_get_group(&g_serv.eraft_ctx->evts.multi, g_opts.cluster);
		if (!group) {
			sleep(1);
			continue;
		}
		raft_node_t *leader = raft_get_current_leader_node(group->raft);
		if (!leader) {
			sleep(1);
			continue;
		} else if (raft_node_get_id(leader) != group->node_id) {
			//int id = raft_node_get_id(leader);
			//struct eraft_node *enode = &group->conf->nodes[id];
			//char *host = enode->raft_host;
			//char *port = enode->raft_port;

			sleep(1);
			continue;
		} else {
			test_iops();
			break;
		}
	} while (1);




	eraft_context_dispose_del_group(g_serv.eraft_ctx, g_opts.cluster);//FIXME:can't use stack string
	eraft_context_destroy(g_serv.eraft_ctx);
	_main_env_exit();
}


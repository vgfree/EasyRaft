# 简介

EasyRaft是基于raft协议实现的一套lib库，内置网络模块、日志模块、调度模块等。
用户只需通过eraft_api.h里提供的接口，即可快速搭建分布式应用。

详细使用方法，可以产考example里的ticketd程序。

# 编译

./build.sh

# 示例

	char    *cluster = "127.0.0.1:6000,127.0.0.1:6001,127.0.0.1:6002";
	int     self_id = 1;
	char    self_host[IPV4_HOST_LEN] = { 0 };
	char    self_port[IPV4_PORT_LEN] = { 0 };
	erapi_get_node_info(cluster, self_id, self_host, self_port);

	int raft_port = atoi(self_port);

	/*创建eraft上下文*/
	struct eraft_context *ctx = erapi_ctx_create(raft_port);
	/*创建cluster服务*/
	struct eraft_group *group = erapi_add_group(ctx, cluster, self_id,
			"journal_data_path", 4 << 20, __log_apply_wfcb, __log_apply_rfcb);

	do {
		raft_node_t *leader = raft_get_current_leader_node(group->raft);

		if (!leader) {
			/*还在选举状态*/
			sleep(1);
			continue;
		} else if (raft_node_get_id(leader) != group->node_id) {
			/*本节点非leader节点*/
			sleep(1);
			continue;
		} else {
			/*本节点非leader节点*/
			char data[100] = "1->你好!";
			struct iovec request = { .iov_base = (void *)data, .iov_len = sizeof(data) };

			erapi_write_request(ctx, cluster, &request);
			break;
		}
	} while (1);

	erapi_del_group(ctx, cluster);
	erapi_ctx_destroy(ctx);

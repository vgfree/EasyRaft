#include <dirent.h>
#include <stdlib.h>

#include "etask_tree.h"
#include "eraft_taskis.h"
#include "eraft_api.h"
#include "liblogger.h"

static int _logger_impl(short syslv, const char *func, const char *file, int line, const char *format, va_list *ap)
{
	return SLogPrintf(syslv2loglv(syslv), func, file, line, format, ap);
}

void erapi_env_init(void)
{
	raft_logger_setup(_logger_impl);
}

struct eraft_context *erapi_ctx_create(int port)
{
	return eraft_context_create(port);
}

void erapi_ctx_destroy(struct eraft_context *ctx)
{
	eraft_context_destroy(ctx);
}

int erapi_get_node_info(char *cluster, int idx, char host[IPV4_HOST_LEN], char port[IPV4_PORT_LEN])
{
	struct eraft_conf *conf = eraft_conf_make(cluster, idx);

	snprintf(host, IPV4_HOST_LEN, "%s", conf->nodes[idx].raft_host);
	snprintf(port, IPV4_PORT_LEN, "%s", conf->nodes[idx].raft_port);

	eraft_conf_free(conf);
	return 0;
}

struct eraft_group *erapi_add_group(struct eraft_context *ctx, char *cluster, int selfidx,
	char *db_path, int db_size,
	ERAFT_LOG_APPLY_WFCB wfcb, ERAFT_LOG_APPLY_RFCB rfcb)
{
	struct eraft_evts *evts = &ctx->evts;

	struct eraft_group *group = eraft_group_make(cluster, selfidx, db_path, db_size, wfcb, rfcb);

	group->evts = evts;

	struct etask                    *etask = etask_make(NULL);
	struct eraft_taskis_group_add   *task = eraft_taskis_group_add_make(cluster, eraft_evts_dispose_dotask, evts, group, etask);

	eraft_tasker_once_give(&evts->tasker, (struct eraft_dotask *)task);

	etask_sleep(etask);
	etask_free(etask);

	eraft_taskis_group_add_free(task);
	return group;
}

void erapi_del_group(struct eraft_context *ctx, char *cluster)
{
	struct eraft_evts *evts = &ctx->evts;

	struct etask                    *etask = etask_make(NULL);
	struct eraft_taskis_group_del   *task = eraft_taskis_group_del_make(cluster, eraft_evts_dispose_dotask, evts, etask);

	eraft_tasker_once_give(&evts->tasker, (struct eraft_dotask *)task);

	etask_sleep(etask);
	etask_free(etask);

	eraft_taskis_group_del_free(task);
}

int erapi_write_request(struct eraft_context *ctx, char *cluster, struct iovec *request)
{
	struct eraft_evts                       *evts = &ctx->evts;
	struct etask                            *etask = etask_make(NULL);
	struct eraft_taskis_request_write       *task = eraft_taskis_request_write_make(cluster, eraft_evts_dispose_dotask, evts, request, etask);

	eraft_tasker_once_give(&evts->tasker, (struct eraft_dotask *)task);

	etask_sleep(etask);
	etask_free(etask);

	int result = eraft_errno_by_raft(task->result);

	if (result == 0) {
		/* When we receive an request from the client we need to block until the
		 * request has been committed. This efd is used to wake us up. */
		int ret = etask_tree_await_task(evts->wait_idx_tree, &task->idx, sizeof(task->idx), task->efd, -1);
		assert(ret == 0);
	}

	result = eraft_errno_by_raft(task->result);

	eraft_taskis_request_write_free(task);
	return result;
}

int erapi_read_request(struct eraft_context *ctx, char *cluster, struct iovec *request)
{
	struct eraft_evts                       *evts = &ctx->evts;
	struct etask                            *etask = etask_make(NULL);
	struct eraft_taskis_request_read        *task = eraft_taskis_request_read_make(cluster, eraft_evts_dispose_dotask, evts, request, etask);

	eraft_tasker_once_give(&evts->tasker, (struct eraft_dotask *)task);

	etask_sleep(etask);
	etask_free(etask);

	int result = eraft_errno_by_raft(task->result);

	eraft_taskis_request_read_free(task);
	return result;
}


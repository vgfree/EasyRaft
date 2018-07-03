#pragma once

#include "eraft_errno.h"
#include "eraft_confs.h"
#include "eraft_context.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 创建一个eraft context */
struct eraft_context    *erapi_ctx_create(int port);

/* 销毁一个eraft context */
void erapi_ctx_destroy(struct eraft_context *ctx);

/* 获取节点信息 */
int erapi_get_node_info(char *cluster, int idx, char host[IPV4_HOST_LEN], char port[IPV4_PORT_LEN]);

/* 加载或创建一个eraft group */
struct eraft_group      *erapi_add_group(struct eraft_context *ctx, char *cluster, int selfidx, char *db_path, int db_size, ERAFT_LOG_APPLY_FCB fcb);

/* 删除一个eraft group */
void erapi_del_group(struct eraft_context *ctx, char *cluster);

/* 写数据 */

/*
 * errno:
 *      ERAFT_ERR_NOT_LEADER
 *      ERAFT_ERR_TIME_OUT
 */
int erapi_write_request(struct eraft_context *ctx, char *cluster, struct iovec *request);

/* 读数据 */

#ifdef __cplusplus
}
#endif


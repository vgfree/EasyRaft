#pragma once

enum
{
	ERAFT_ERR_NOT_LEADER = -2,	/* 本节点不是leader */
	ERAFT_ERR_SNAPSHOT_IN_PROGRESS = -3,
	ERAFT_ERR_TIME_OUT = -4,	/* 请求超时 */
};

int eraft_errno_by_raft(int err);


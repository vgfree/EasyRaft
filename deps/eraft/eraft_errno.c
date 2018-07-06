#include <assert.h>

#include "raft.h"
#include "eraft_errno.h"

int eraft_errno_by_raft(int err)
{
	switch (err)
	{
		case RAFT_ERR_SNAPSHOT_IN_PROGRESS:
			return ERAFT_ERR_SNAPSHOT_IN_PROGRESS;

		case RAFT_ERR_NOT_LEADER:
			return ERAFT_ERR_NOT_LEADER;

		default:
			assert(0);
			return 0;
	}
}


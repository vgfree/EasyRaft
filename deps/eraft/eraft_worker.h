#include <stdbool.h>

#include "list.h"
#include "etask.h"
#include "eraft_tasker.h"

struct eraft_worker;
typedef void (*ERAFT_WORKER_WORK)(struct eraft_worker *tasker, struct eraft_task *task, void *usr);
struct eraft_worker {
	struct etask etask;
	struct list_head        list;
	struct eraft_lock       lock;
	ERAFT_WORKER_WORK       fcb;
	void                    *usr;
	/*FIXME: ahead is eraft_tasker*/

	pthread_t pid;
	bool exit;
};

int eraft_worker_init(struct eraft_worker *worker, ERAFT_WORKER_WORK fcb, void *usr);

int eraft_worker_free(struct eraft_worker *worker);

void eraft_worker_give(struct eraft_worker *worker, struct eraft_task *task);

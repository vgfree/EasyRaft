#include "eraft_dotask.h"

void eraft_dotask_init(struct eraft_dotask *task, int type, bool merge, char *identity, ERAFT_DOTASK_FCB _fcb, void *_usr)
{
	INIT_LIST_NODE(&task->node);
	task->type = type;
	task->merge = merge;

	/*设置属性*/
	task->identity = strdup(identity);
	task->_fcb = _fcb;
	task->_usr = _usr;
}

void eraft_dotask_free(struct eraft_dotask *task)
{
	free(task->identity);
}

